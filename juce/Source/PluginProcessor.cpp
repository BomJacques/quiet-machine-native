#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
float get(const juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (const auto* value = state.getRawParameterValue(id))
        return value->load();
    return 0.0f;
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat(const char* id, const char* name,
                                                     float minimum, float maximum, float step,
                                                     float defaultValue, float centre = 0.0f)
{
    juce::NormalisableRange<float> range { minimum, maximum, step };
    if (centre > minimum && centre < maximum)
        range.setSkewForCentre(centre);
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name, range, defaultValue);
}

std::unique_ptr<juce::AudioParameterBool> makeBool(const char* id, const char* name, bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, defaultValue);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice(const char* id, const char* name,
                                                       juce::StringArray choices, int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id, 1 }, name, std::move(choices), defaultIndex);
}


}

QuietMachineAudioProcessor::BusesProperties QuietMachineAudioProcessor::createBusesProperties()
{
    BusesProperties buses;
    buses = buses.withOutput("Main Processed", juce::AudioChannelSet::stereo(), true);
    constexpr std::array<const char*, qm::sourceCount> names {
        "Tape 1", "Tape 2", "Tape 3", "Tape 4", "Tape 5", "Swell",
        "Rumble", "Piano", "Drone", "Sub", "Static", "Hiss"
    };
    for (const auto* name : names)
        buses = buses.withOutput(name, juce::AudioChannelSet::stereo(), false);
    return buses;
}

QuietMachineAudioProcessor::QuietMachineAudioProcessor()
    : AudioProcessor(createBusesProperties()),
      parameters(*this, nullptr, "QuietMachineState", createParameterLayout())
{

    midiCaptureCache.reserve(midiCaptureCapacity);
    initialiseStructuredState();
}

void QuietMachineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    nativeAmbientEngine.prepare(sampleRate, samplesPerBlock);
    nativeTextureEngine.prepare(sampleRate, samplesPerBlock);
    nativeMasterFx.prepare(sampleRate, samplesPerBlock);
    ampEnvelope.setSampleRate(sampleRate);
    ampEnvelope.setParameters(ampEnvelope.getParameters());
    ampEnvelope.reset();
    const auto preparedSamples = juce::jmax(8192, samplesPerBlock);
    for (auto& source : sourceBuffers)
        source.setSize(2, preparedSamples, false, true, false);
    ampEnvelopeBuffer.setSize(1, preparedSamples, false, true, false);
    synthesisMidi.ensureSize(synthesisMidiReserveBytes);
    deferredHostMidi.ensureSize(synthesisMidiReserveBytes);
    deferredHostMidi.clear();
    performanceMidi.ensureSize(synthesisMidiReserveBytes);
    generatedMidi.ensureSize(generatedMidiReserveBytes);
    envelopeMidi.ensureSize(synthesisMidiReserveBytes + generatedMidiReserveBytes);
    generatorWasRunning = false;
    generatedNoteCount = 0;
    performanceHeldNoteCounts.fill(0);
    performanceHeldNoteCount = 0;
    clearPerformanceMidiMappings();
    clearCaptureInput();
    for (auto& channel : ampHeldNoteCounts)
        channel.fill(0);
    ampHeldNoteCount = 0;
    ampGeneratorResumePending = false;
    midiOutWasEnabled = get(parameters, "midiOutEnabled") > 0.5f;
    midiRecordWasEnabled = get(parameters, "midiRecordEnabled") > 0.5f;
    midiChordModeWasEnabled = get(parameters, "midiChordMode") > 0.5f;
    bypassWasEnabled = get(parameters, "bypass") > 0.5f;
    hostTransportKnown = false;
    hostWasPlaying = false;
}

void QuietMachineAudioProcessor::releaseResources()
{
    nativeAmbientEngine.reset();
    nativeTextureEngine.reset();
    nativeMasterFx.reset();
    ampEnvelope.reset();
    generatedNoteCount = 0;
    performanceHeldNoteCounts.fill(0);
    performanceHeldNoteCount = 0;
    clearPerformanceMidiMappings();
    clearCaptureInput();
    for (auto& channel : ampHeldNoteCounts)
        channel.fill(0);
    ampHeldNoteCount = 0;
    ampGeneratorResumePending = false;
    generatorWasRunning = false;
}

bool QuietMachineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
    {
        const auto channels = layouts.getChannelSet(false, bus);
        if (! channels.isDisabled() && channels != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void QuietMachineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    buffer.clear();
    if (numSamples == 0)
    {
        auto count = deferredHostMidi.getNumEvents();
        for (const auto event : midi)
        {
            if (event.numBytes > 3)
                continue;
            if (count >= maxIncomingMidiEventsPerBlock)
            {
                panicRequested.store(true, std::memory_order_release);
                break;
            }
            deferredHostMidi.addEvent(event.getMessage(), 0);
            ++count;
        }
        midi.clear();
        return;
    }

    std::array<int, 16> stateRestoreCleanupNotes {};
    auto stateRestoreCleanupCount = 0;
    auto allSoundOffSampleOffset = -1;

    synthesisMidi.clear();
    performanceMidi.clear();
    generatedMidi.clear();
    envelopeMidi.clear();

    if (engineResetRequested.exchange(false, std::memory_order_acq_rel))
    {
        deferredHostMidi.clear();
        clearCaptureInput();
        if (midiOutWasEnabled)
        {
            stateRestoreCleanupCount = generatedNoteCount;
            std::copy_n(generatedMidiNotes.begin(), generatedNoteCount,
                        stateRestoreCleanupNotes.begin());
        }
        generatedNoteCount = 0;
        nativeAmbientEngine.reset();
        nativeTextureEngine.reset();
        nativeMasterFx.reset();
        ampEnvelope.reset();
        for (auto& channel : ampHeldNoteCounts)
            channel.fill(0);
        ampHeldNoteCount = 0;
        ampGeneratorResumePending = false;
        performanceHeldNoteCounts.fill(0);
        performanceHeldNoteCount = 0;
        clearPerformanceMidiMappings();
        generatorWasRunning = false;
    }

    int incomingEventCount = 0;
    for (const auto metadata : deferredHostMidi)
    {
        auto message = metadata.getMessage();
        trackCaptureInput(message, false);
        if (message.getChannel() > 0)
            message.setChannel(1);
        if (message.isAllSoundOff())
            allSoundOffSampleOffset = 0;
        synthesisMidi.addEvent(message, 0);
        ++incomingEventCount;
    }
    deferredHostMidi.clear();
    for (const auto metadata : midi)
    {
        if (incomingEventCount >= maxIncomingMidiEventsPerBlock)
        {
            panicRequested.store(true, std::memory_order_release);
            continue;
        }

        // The synth only consumes short MIDI channel/system messages. Ignoring SysEx
        // keeps this copy bounded by the storage reserved in prepareToPlay().
        if (metadata.numBytes <= 3)
        {
            auto message = metadata.getMessage();
            trackCaptureInput(message, false);
            if (message.getChannel() > 0)
                message.setChannel(1);
            const auto sampleOffset = juce::jlimit(0, juce::jmax(0, numSamples - 1),
                                                   metadata.samplePosition);
            if (message.isAllSoundOff())
                allSoundOffSampleOffset = allSoundOffSampleOffset < 0
                    ? sampleOffset : juce::jmin(allSoundOffSampleOffset, sampleOffset);
            synthesisMidi.addEvent(message, sampleOffset);
            ++incomingEventCount;
        }
    }

    // Host input is deliberately removed from the output buffer. Only events made
    // by the generator are eligible for MIDI output.
    midi.clear();
    for (int index = 0; index < stateRestoreCleanupCount; ++index)
        midi.addEvent(juce::MidiMessage::noteOff(
            16, stateRestoreCleanupNotes[(size_t) index]), 0);
    drainUiMidiEvents(synthesisMidi, numSamples);

    if (const auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            const auto hostIsPlaying = position->getIsPlaying();
            if (hostTransportKnown && hostWasPlaying && ! hostIsPlaying)
            {
                synthesisMidi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
                clearCaptureInput();
            }
            hostWasPlaying = hostIsPlaying;
            hostTransportKnown = true;
        }

    drainGeneratorSnapshots();
    const auto midiChordModeEnabled = get(parameters, "midiChordMode") > 0.5f;
    buildPerformanceMidi(synthesisMidi, performanceMidi, midiChordModeEnabled, numSamples);
    midiChordModeWasEnabled = midiChordModeEnabled;

    const auto tempo = getEffectiveTempo();
    lastRenderedTempo.store(tempo, std::memory_order_relaxed);
    const auto midiOutEnabled = get(parameters, "midiOutEnabled") > 0.5f;
    const auto midiRecordEnabled = get(parameters, "midiRecordEnabled") > 0.5f;

    if (midiOutEnabled != midiOutWasEnabled)
        appendActiveGeneratedNotes(midi, 0, midiOutEnabled);
    const auto captureRestartRequested = midiCaptureRestartRequested.exchange(
        false, std::memory_order_acq_rel);
    if (midiRecordEnabled != midiRecordWasEnabled)
        captureActiveGeneratedNotes(midiRecordEnabled, 0, tempo);
    else if (midiRecordEnabled && captureRestartRequested)
        captureActiveGeneratedNotes(true, 0, tempo);

    midiOutWasEnabled = midiOutEnabled;
    midiRecordWasEnabled = midiRecordEnabled;

    renderGenerator(generatedMidi, performanceMidi, numSamples);

    if (panicRequested.exchange(false, std::memory_order_acq_rel))
    {
        clearCaptureInput();
        generatedMidi.clear();
        performanceMidi.clear();
        stopGeneratedNotes(generatedMidi, 0);
        nativeAmbientEngine.reset();
        nativeTextureEngine.reset();
        nativeMasterFx.reset();
        ampEnvelope.reset();
        generatorWasRunning = false;
        performanceMidi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        performanceMidi.addEvent(juce::MidiMessage::allNotesOff(16), 0);
        performanceHeldNoteCounts.fill(0);
        performanceHeldNoteCount = 0;
        clearPerformanceMidiMappings();
        for (auto& channel : ampHeldNoteCounts)
            channel.fill(0);
        ampHeldNoteCount = 0;
        ampGeneratorResumePending = false;
    }

    if (! synthesisMidi.isEmpty())
    {
        uint64_t held = 0;
        for (int note = 0; note < 128; ++note)
            if (captureInputNotes[(size_t) note] != 0)
                held |= uint64_t { 1 } << (note >= 48 && note <= 83 ? note - 48 : 63);
        heldInputChord.store(held, std::memory_order_release);
    }

    for (const auto metadata : generatedMidi)
    {
        const auto message = metadata.getMessage();
        if (midiOutEnabled)
            midi.addEvent(message, metadata.samplePosition);
        if (midiRecordEnabled && (message.isNoteOn() || message.isNoteOff()))
            captureGeneratedEvent(message, metadata.samplePosition, tempo);
    }

    envelopeMidi.addEvents(performanceMidi, 0, numSamples, 0);
    envelopeMidi.addEvents(generatedMidi, 0, numSamples, 0);
    // The master envelope owns every source and effect tail. Once it is idle,
    // there is no audible work until a MIDI event arrives.
    if (! ampEnvelope.isActive() && ! ampGeneratorResumePending && envelopeMidi.isEmpty())
    {
        outputPeak.store(0.0f, std::memory_order_relaxed);
        renderedSeconds += (double) numSamples / currentSampleRate;
        captureEndpointSeconds.store(renderedSeconds, std::memory_order_release);
        return;
    }
    if (appliedVoiceProfileRevision != activeGeneratorSnapshot.revision)
    {
        nativeAmbientEngine.setVoiceProfiles(activeGeneratorSnapshot.keyVoices);
        appliedVoiceProfileRevision = activeGeneratorSnapshot.revision;
    }
    nativeAmbientEngine.consumeGeneratedMidi(envelopeMidi, performanceSourceEvents.data(),
                                             performanceSourceEventCount);
    nativeTextureEngine.consumeGeneratedMidi(envelopeMidi);

    const auto requiredSamples = juce::jmax(1, numSamples);
    if (sourceBuffers[0].getNumSamples() < requiredSamples)
    {
        for (auto& source : sourceBuffers)
            source.setSize(2, requiredSamples, false, true, false);
        ampEnvelopeBuffer.setSize(1, requiredSamples, false, true, false);
    }

    qm::SourceOutput sourceOutput;
    for (std::size_t source = 0; source < sourceBuffers.size(); ++source)
    {
        sourceBuffers[source].clear(0, numSamples);
        sourceOutput.left[source] = sourceBuffers[source].getWritePointer(0);
        sourceOutput.right[source] = sourceBuffers[source].getWritePointer(1);
    }

    nativeAmbientEngine.process(sourceOutput, numSamples, parameters);
    nativeTextureEngine.process(sourceOutput, numSamples, parameters);

    juce::ADSR::Parameters envelopeParameters;
    envelopeParameters.attack = get(parameters, "ampAttack");
    envelopeParameters.decay = get(parameters, "ampDecay");
    envelopeParameters.sustain = get(parameters, "ampSustain");
    envelopeParameters.release = get(parameters, "ampRelease");
    const auto& previousEnvelopeParameters = ampEnvelope.getParameters();
    // Recalculating unchanged parameters overwrites noteOff's release rate
    // from the current level with a rate based on sustain instead.
    if (envelopeParameters.attack != previousEnvelopeParameters.attack
        || envelopeParameters.decay != previousEnvelopeParameters.decay
        || envelopeParameters.sustain != previousEnvelopeParameters.sustain
        || envelopeParameters.release != previousEnvelopeParameters.release)
        ampEnvelope.setParameters(envelopeParameters);

    auto envelopeEvent = envelopeMidi.cbegin();
    auto inputEvent = synthesisMidi.cbegin();
    const auto inputEnd = synthesisMidi.cend();
    const bool legato = get(parameters, "ampLegato") > 0.5f;
    const auto envelopeEnd = envelopeMidi.cend();
    auto* envelope = ampEnvelopeBuffer.getWritePointer(0);
    const auto channelHasNotes = [this](size_t channel)
    {
        return std::any_of(ampHeldNoteCounts[channel].begin(), ampHeldNoteCounts[channel].end(),
                           [](int count) { return count > 0; });
    };
    auto performanceGateIsOpen = channelHasNotes(0);
    auto generatorGateIsOpen = channelHasNotes(15);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto envelopeGateWasOpen = ampHeldNoteCount > 0;
        const auto performanceGateWasOpen = performanceGateIsOpen;
        auto gatesChanged = false;
        auto hardEnvelopeStop = false;
        bool inputNoteOn = false;
        while (inputEvent != inputEnd && (*inputEvent).samplePosition <= sample)
        {
            inputNoteOn = inputNoteOn || (*inputEvent).getMessage().isNoteOn();
            ++inputEvent;
        }
        while (envelopeEvent != envelopeEnd && (*envelopeEvent).samplePosition <= sample)
        {
            gatesChanged = true;
            const auto message = (*envelopeEvent).getMessage();
            if (message.isNoteOn())
            {
                const auto channel = (size_t) juce::jlimit(1, 16, message.getChannel()) - 1;
                auto& count = ampHeldNoteCounts[channel][(size_t) message.getNoteNumber()];
                if (count == 0)
                {
                    count = 1;
                    ++ampHeldNoteCount;
                }
            }
            else if (message.isNoteOff())
            {
                const auto channel = (size_t) juce::jlimit(1, 16, message.getChannel()) - 1;
                auto& count = ampHeldNoteCounts[channel][(size_t) message.getNoteNumber()];
                if (count > 0)
                {
                    count = 0;
                    --ampHeldNoteCount;
                }
            }
            else if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                const auto channel = (size_t) juce::jlimit(1, 16, message.getChannel()) - 1;
                for (const auto count : ampHeldNoteCounts[channel])
                    ampHeldNoteCount -= count;
                ampHeldNoteCounts[channel].fill(0);
                ampHeldNoteCount = juce::jmax(0, ampHeldNoteCount);
                if (message.isAllSoundOff())
                {
                    ampEnvelope.reset();
                    ampGeneratorResumePending = false;
                    hardEnvelopeStop = true;
                }
            }
            ++envelopeEvent;
        }

        const auto envelopeGateIsOpen = ampHeldNoteCount > 0;
        if (gatesChanged)
        {
            performanceGateIsOpen = channelHasNotes(0);
            generatorGateIsOpen = channelHasNotes(15);
        }
        if (! hardEnvelopeStop)
        {
            // Aggregate the gate across all MIDI channels before changing
            // the ADSR. Generator chord swaps can contain note-off/note-on
            // pairs at the same sample and must not restart the envelope.
            // A physical note release has priority over an auto-generator
            // resumption: let the configured release finish first, then
            // hand the envelope back to the generator.
            const auto performanceReleased = performanceGateWasOpen
                                           && ! performanceGateIsOpen;
            if (performanceReleased)
            {
                ampGeneratorResumePending = generatorGateIsOpen;
                ampEnvelope.noteOff();
            }
            else if (performanceGateIsOpen && (! performanceGateWasOpen || (! legato && inputNoteOn)))
            {
                ampGeneratorResumePending = false;
                // Retrigger from the current level, never an abrupt reset to zero.
                // Raw input triggers once even when chord expansion shares pitches.
                ampEnvelope.noteOn();
            }
            else if (! performanceGateIsOpen
                     && ampGeneratorResumePending)
            {
                // Hold the release even if the generator has already
                // replaced its note. It will be restarted below at zero.
            }
            else if (! envelopeGateWasOpen && envelopeGateIsOpen)
                ampEnvelope.noteOn();
            else if (envelopeGateWasOpen && ! envelopeGateIsOpen)
                ampEnvelope.noteOff();
        }

        envelope[sample] = ampEnvelope.getNextSample();
        if (! hardEnvelopeStop && ampGeneratorResumePending)
        {
            if (! generatorGateIsOpen)
            {
                ampGeneratorResumePending = false;
            }
            else if (envelope[sample] <= 0.000001f)
            {
                if (! performanceGateIsOpen && generatorGateIsOpen)
                {
                    ampGeneratorResumePending = false;
                    ampEnvelope.noteOn();
                }
            }
        }
    }

    auto mainOutput = getBusBuffer(buffer, false, 0);
    for (std::size_t source = 0; source < sourceBuffers.size(); ++source)
    {
        auto stemBus = getBusBuffer(buffer, false, static_cast<int>(source + 1));
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto* input = sourceBuffers[source].getReadPointer(channel);
            auto* main = mainOutput.getWritePointer(channel);
            auto* stem = stemBus.getNumChannels() > channel ? stemBus.getWritePointer(channel) : nullptr;
            juce::FloatVectorOperations::add(main, input, numSamples);
            if (stem != nullptr)
                juce::FloatVectorOperations::multiply(stem, input, envelope, numSamples);
        }
    }
    for (int sample = 0; sample < numSamples; ++sample)
        if (envelope[sample] <= 0.0f)
            for (int channel = 0; channel < mainOutput.getNumChannels(); ++channel)
                mainOutput.setSample(channel, sample, 0.0f);

    const auto bypassEnabled = get(parameters, "bypass") > 0.5f;
    if (bypassEnabled)
    {
        if (! bypassWasEnabled)
            nativeMasterFx.reset();
        nativeAmbientEngine.reset();
        nativeTextureEngine.reset();
        ampEnvelope.reset();
        ampGeneratorResumePending = false;
        bypassWasEnabled = true;
        buffer.clear();
        outputPeak.store(0.0f, std::memory_order_relaxed);
        renderedSeconds += (double) numSamples / currentSampleRate;
        captureEndpointSeconds.store(renderedSeconds, std::memory_order_release);
        return;
    }

    if (bypassWasEnabled)
        nativeMasterFx.reset();
    bypassWasEnabled = false;

    if (allSoundOffSampleOffset >= 0)
    {
        if (allSoundOffSampleOffset > 0)
        {
            juce::AudioBuffer<float> beforeAllSoundOff(
                mainOutput.getArrayOfWritePointers(), mainOutput.getNumChannels(),
                0, allSoundOffSampleOffset);
            nativeMasterFx.process(beforeAllSoundOff, parameters);
        }

        nativeMasterFx.reset();
        if (allSoundOffSampleOffset < numSamples)
        {
            juce::AudioBuffer<float> afterAllSoundOff(
                mainOutput.getArrayOfWritePointers(), mainOutput.getNumChannels(),
                allSoundOffSampleOffset, numSamples - allSoundOffSampleOffset);
            nativeMasterFx.process(afterAllSoundOff, parameters);
        }
    }
    else
    {
        nativeMasterFx.process(mainOutput, parameters);
    }

    // Shape the complete processed output once, including reverb and freeze.
    // Stems receive the same envelope independently above.
    for (int channel = 0; channel < mainOutput.getNumChannels(); ++channel)
        juce::FloatVectorOperations::multiply(mainOutput.getWritePointer(channel), envelope, numSamples);
    if (! ampEnvelope.isActive())
    {
        nativeMasterFx.reset();
        nativeAmbientEngine.reset();
        nativeTextureEngine.reset();
    }

    outputPeak.store(mainOutput.getMagnitude(0, numSamples), std::memory_order_relaxed);
    renderedSeconds += (double) numSamples / currentSampleRate;
    captureEndpointSeconds.store(renderedSeconds, std::memory_order_release);
}

juce::AudioProcessorValueTreeState::ParameterLayout QuietMachineAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(makeFloat("tempoBpm", "Tempo", 10.0f, 120.0f, 0.1f, 30.0f));
    layout.add(makeFloat("glideSeconds", "Glide", 0.0f, 8.0f, 0.01f, 2.0f));
    layout.add(makeChoice("oscillatorWave", "Wave", { "Saw", "Square", "Triangle", "Sine" }, 0));
    layout.add(makeFloat("masterPitchSemitones", "Master Pitch", -12.0f, 12.0f, 0.1f, 0.0f));
    layout.add(makeFloat("lowpassHz", "Lowpass", 80.0f, 18000.0f, 1.0f, 2000.0f, 2000.0f));
    layout.add(makeFloat("lowpassQ", "Lowpass Resonance", 0.1f, 20.0f, 0.01f, 0.5f));
    layout.add(makeFloat("filterSweepDepth", "Filter Sweep", 0.0f, 1.0f, 0.001f, 0.2f));
    layout.add(makeFloat("highpassHz", "Highpass", 20.0f, 2000.0f, 1.0f, 20.0f, 180.0f));
    layout.add(makeFloat("tapeSpeed", "Tape Speed", 0.25f, 2.0f, 0.001f, 1.0f));
    layout.add(makeFloat("tapeAttack", "Tape Attack", 0.0f, 1.0f, 0.001f, 0.3f));
    layout.add(makeFloat("wowRateHz", "Wow Rate", 0.05f, 10.0f, 0.001f, 1.5f));
    layout.add(makeFloat("wowDepth", "Wow Depth", 0.0f, 1.0f, 0.001f, 0.3f));
    layout.add(makeFloat("warpDepth", "Tape Warp", 0.0f, 1.0f, 0.001f, 0.4f));
    layout.add(makeFloat("tapeWear", "Tape Wear", 0.0f, 1.0f, 0.001f, 0.1f));
    layout.add(makeFloat("tapeDrive", "Tape Drive", 0.0f, 1.0f, 0.001f, 0.3f));
    layout.add(makeFloat("reverbMix", "Reverb", 0.0f, 1.0f, 0.001f, 0.7f));
    layout.add(makeBool("delayEnabled", "Delay", true));
    layout.add(makeBool("freezeEnabled", "Freeze", false));
    layout.add(makeFloat("masterGain", "Master", 0.0f, 1.0f, 0.001f, 0.7f));
    layout.add(makeFloat("ampAttack", "Amp Attack", 0.005f, 10.0f, 0.001f, 0.25f, 0.4f));
    layout.add(makeFloat("ampDecay", "Amp Decay", 0.01f, 10.0f, 0.001f, 1.5f, 1.0f));
    layout.add(makeFloat("ampSustain", "Amp Sustain", 0.0f, 1.0f, 0.001f, 0.85f));
    layout.add(makeFloat("ampRelease", "Amp Release", 0.01f, 20.0f, 0.001f, 3.5f, 2.0f));

    const std::array<float, 12> sourceDefaults { 0.6f, 0.5f, 0.4f, 0.3f, 0.5f, 0.4f,
                                                  0.5f, 0.6f, 0.5f, 0.7f, 0.0f, 0.0f };
    for (size_t i = 0; i < qm::sourceNames.size(); ++i)
    {
        const auto source = juce::String(qm::sourceNames[i]);
        layout.add(makeFloat((source + "Gain").toRawUTF8(), (source + " Level").toRawUTF8(),
                             0.0f, 1.0f, 0.001f, sourceDefaults[i]));
    }
    for (const auto* sourceName : qm::sourceNames)
    {
        const auto source = juce::String(sourceName);
        layout.add(makeBool((source + "Mute").toRawUTF8(), (source + " Mute").toRawUTF8(), false));
    }

    layout.add(makeBool("generatorEnabled", "Generator", false));
    layout.add(makeBool("hostTempoSync", "Use Host Tempo", true));
    layout.add(makeBool("bypass", "Bypass", false));
    layout.add(makeBool("midiOutEnabled", "MIDI Output", false));
    layout.add(makeBool("midiRecordEnabled", "Record Generated MIDI", false));
    layout.add(makeBool("midiChordMode", "MIDI Chord Shape", false));
    layout.add(makeBool("ampLegato", "Overall ADSR Legato", true));
    return layout;
}

void QuietMachineAudioProcessor::initialiseStructuredState()
{
    const auto ensureChild = [this](const juce::Identifier& type)
    {
        auto child = parameters.state.getChildWithName(type);
        if (! child.isValid())
        {
            child = juce::ValueTree(type);
            parameters.state.addChild(child, -1, nullptr);
        }
        return child;
    };

    auto generator = ensureChild("Generator");
    if (! generator.hasProperty("seedText"))
        generator.setProperty("seedText", "QUIET01", nullptr);
    if (! generator.hasProperty("seedStatus"))
        generator.setProperty("seedStatus", "FREE", nullptr);
    if (! generator.hasProperty("autoNotes"))
        generator.setProperty("autoNotes", "0,7,14,19", nullptr);


    auto tapeLoops = ensureChild("TapeLoops");
    constexpr std::array<double, 5> intervals { 17.3, 21.1, 23.5, 29.8, 37.0 };
    if (tapeLoops.getNumChildren() == 0)
        for (int loop = 0; loop < 5; ++loop)
        {
            juce::ValueTree item("Loop");
            item.setProperty("index", loop, nullptr);
            item.setProperty("intervalSeconds", intervals[(size_t) loop], nullptr);
            tapeLoops.addChild(item, -1, nullptr);
        }

    ensureChild("Runtime");
    ensureChild("Recording");
    ensureChild("LegacyStaff");
    ensureChild("UiState");
}

void QuietMachineAudioProcessor::setParameterFromUi(const juce::String& id, float actualValue)
{
    if (auto* parameter = parameters.getParameter(id))
    {
        const auto normalised = parameter->convertTo0to1(actualValue);
        if (std::abs(parameter->getValue() - normalised) > 0.000001f)
            parameter->setValueNotifyingHost(normalised);
    }
}

void QuietMachineAudioProcessor::beginParameterGesture(const juce::String& id)
{
    if (auto* parameter = parameters.getParameter(id))
        parameter->beginChangeGesture();
}

void QuietMachineAudioProcessor::endParameterGesture(const juce::String& id)
{
    if (auto* parameter = parameters.getParameter(id))
        parameter->endChangeGesture();
}

juce::var QuietMachineAudioProcessor::getParameterStateForUi() const
{
    auto result = std::make_unique<juce::DynamicObject>();
    for (const auto* id : qm::parameterIds)
        result->setProperty(id, get(parameters, id));
    return juce::var(result.release());
}

void QuietMachineAudioProcessor::setPatchStateFromUi(const juce::String& json)
{
    {
        const juce::ScopedLock lock(patchStateLock);
        patchStateJson = json;
    }
    rebuildGeneratorSnapshot(json);
}

juce::String QuietMachineAudioProcessor::getPatchStateForUi() const
{
    const juce::ScopedLock lock(patchStateLock);
    return patchStateJson;
}

void QuietMachineAudioProcessor::rebuildGeneratorSnapshot(const juce::String& json)
{
    GeneratorSnapshot next;
    next.revision = nextSnapshotRevision.fetch_add(1, std::memory_order_relaxed);

    const auto parsed = juce::JSON::parse(json);
    if (const auto* object = parsed.getDynamicObject())
    {
        const auto property = [object](const char* name) { return object->getProperty(name); };

        if (const auto* notes = property("autoNotes").getArray())
        {
            next.autoNoteCount = juce::jmin(16, notes->size());
            for (int index = 0; index < next.autoNoteCount; ++index)
                next.autoNotes[(size_t) index] = juce::jlimit(0, 35, (int) notes->getReference(index));
        }

        if (const auto* notes = property("midiChordNotes").getArray())
        {
            next.midiChordNoteCount = juce::jmin(16, notes->size());
            for (int index = 0; index < next.midiChordNoteCount; ++index)
                next.midiChordNotes[(size_t) index] = juce::jlimit(
                    0, 35, (int) notes->getReference(index));
        }
        else
        {
            next.midiChordNotes = next.autoNotes;
            next.midiChordNoteCount = next.autoNoteCount;
        }

        if (const auto* voices = property("keyVoices").getArray())
        {
            const auto voiceCount = juce::jmin(qm::keyVoiceCount, voices->size());
            for (int index = 0; index < voiceCount; ++index)
                if (const auto* voice = voices->getReference(index).getDynamicObject())
                {
                    auto& destination = next.keyVoices[(size_t) index];
                    if (voice->hasProperty("detuneCents"))
                        destination.detuneCents = juce::jlimit(
                            -50.0f, 50.0f, (float) voice->getProperty("detuneCents"));
                    if (voice->hasProperty("pan"))
                        destination.pan = juce::jlimit(
                            -1.0f, 1.0f, (float) voice->getProperty("pan"));
                    if (voice->hasProperty("oscillatorCount"))
                        destination.oscillatorCount = juce::jlimit(
                            1, 4, (int) voice->getProperty("oscillatorCount"));
                    if (voice->hasProperty("oscillatorSpreadCents"))
                        destination.oscillatorSpreadCents = juce::jlimit(
                            0.0f, 30.0f,
                            (float) voice->getProperty("oscillatorSpreadCents"));
                }
        }


    }

    {
        const juce::SpinLock::ScopedLockType producerLock(generatorSnapshotProducerLock);
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        generatorSnapshotFifo.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0)
        {
            generatorSnapshotStorage[(size_t) start1] = next;
            generatorSnapshotFifo.finishedWrite(1);
        }
        else
        {
            droppedGeneratorSnapshots.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

float QuietMachineAudioProcessor::getEffectiveTempo() const
{
    if (get(parameters, "hostTempoSync") > 0.5f)
        if (const auto* hostPlayHead = getPlayHead())
            if (const auto position = hostPlayHead->getPosition())
                if (const auto bpm = position->getBpm())
                    return (float) *bpm;
    return get(parameters, "tempoBpm");
}


void QuietMachineAudioProcessor::stopGeneratedNotes(juce::MidiBuffer& midi, int sampleOffset)
{
    for (int index = 0; index < generatedNoteCount; ++index)
        midi.addEvent(juce::MidiMessage::noteOff(16, generatedMidiNotes[(size_t) index]), sampleOffset);
    generatedNoteCount = 0;
}

void QuietMachineAudioProcessor::drainGeneratorSnapshots()
{
    const auto ready = generatorSnapshotFifo.getNumReady();
    if (ready <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    generatorSnapshotFifo.prepareToRead(ready, start1, size1, start2, size2);
    for (int index = 0; index < size1; ++index)
        activeGeneratorSnapshot = generatorSnapshotStorage[(size_t) (start1 + index)];
    for (int index = 0; index < size2; ++index)
        activeGeneratorSnapshot = generatorSnapshotStorage[(size_t) (start2 + index)];
    generatorSnapshotFifo.finishedRead(size1 + size2);

}

void QuietMachineAudioProcessor::clearPerformanceMidiMappings() noexcept
{
    performanceChordNoteCounts.fill(0);
    performanceRootActive.fill(false);
    performanceExpandedNoteRefCounts.fill(0);
    performanceSourceKeyByNote.fill(-1);
    for (auto& notes : performanceChordNotes)
        notes.fill(0);
    for (auto& sourceKeys : performanceChordSourceKeys)
        sourceKeys.fill(-1);
}

void QuietMachineAudioProcessor::clearCaptureInput() noexcept
{
    captureInputNotes.fill(0);
    heldInputChord.store(0, std::memory_order_release);
}

void QuietMachineAudioProcessor::trackCaptureInput(const juce::MidiMessage& message, bool fromUi) noexcept
{
    const int channel = message.getChannel();
    if (! fromUi && channel == 0) return;
    const auto source = uint32_t { 1 } << (fromUi ? 16 : channel - 1);
    if (message.isNoteOnOrOff())
    {
        auto& sources = captureInputNotes[(size_t) message.getNoteNumber()];
        if (message.isNoteOn()) sources |= source;
        else sources &= ~source;
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        for (auto& sources : captureInputNotes) sources &= ~source;
}

void QuietMachineAudioProcessor::buildPerformanceMidi(const juce::MidiBuffer& input,
                                                       juce::MidiBuffer& output,
                                                       bool chordMode,
                                                       int numSamples)
{
    output.clear();
    performanceSourceEventCount = 0;

    const auto queueSourceEvent = [this](int note, int sourceKey, int sampleOffset)
    {
        if (performanceSourceEventCount >= maxPerformanceSourceEventsPerBlock)
            return;
        performanceSourceEvents[(size_t) performanceSourceEventCount++] = {
            juce::jlimit(0, 127, note),
            juce::jlimit(-1, qm::keyVoiceCount - 1, sourceKey),
            sampleOffset
        };
    };

    if (chordMode != midiChordModeWasEnabled)
    {
        output.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        clearPerformanceMidiMappings();
    }

    for (const auto metadata : input)
    {
        auto message = metadata.getMessage();
        if (message.getChannel() > 0)
            message.setChannel(1);
        const auto sampleOffset = juce::jlimit(0, juce::jmax(0, numSamples - 1),
                                               metadata.samplePosition);

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            output.addEvent(message, sampleOffset);
            clearPerformanceMidiMappings();
            continue;
        }

        if (message.isNoteOn())
        {
            const auto root = (size_t) juce::jlimit(0, 127, message.getNoteNumber());
            if (performanceRootActive[root])
                continue;

            auto& storedNotes = performanceChordNotes[root];
            auto& storedSourceKeys = performanceChordSourceKeys[root];
            auto& storedCount = performanceChordNoteCounts[root];
            storedCount = 0;
            storedSourceKeys.fill(-1);

            const auto& shapeNotes = activeGeneratorSnapshot.midiChordNotes;
            const auto shapeNoteCount = activeGeneratorSnapshot.midiChordNoteCount;

            if (chordMode && shapeNoteCount > 0)
            {
                auto shapeRoot = shapeNotes[0];
                for (int index = 1; index < shapeNoteCount; ++index)
                    shapeRoot = juce::jmin(shapeRoot, shapeNotes[(size_t) index]);

                for (int index = 0; index < shapeNoteCount
                                    && storedCount < (int) storedNotes.size(); ++index)
                {
                    const auto expanded = (int) root
                                        + shapeNotes[(size_t) index]
                                        - shapeRoot;
                    if (! juce::isPositiveAndBelow(expanded, 128))
                        continue;
                    bool duplicate = false;
                    for (int stored = 0; stored < storedCount; ++stored)
                        duplicate = duplicate || storedNotes[(size_t) stored] == expanded;
                    if (! duplicate)
                    {
                        storedNotes[(size_t) storedCount] = expanded;
                        storedSourceKeys[(size_t) storedCount] = juce::jlimit(
                            0, qm::keyVoiceCount - 1, shapeNotes[(size_t) index]);
                        ++storedCount;
                    }
                }
            }

            if (storedCount == 0)
            {
                storedNotes[(size_t) storedCount] = (int) root;
                storedSourceKeys[(size_t) storedCount] = -1;
                ++storedCount;
            }

            performanceRootActive[root] = true;
            const auto velocity = message.getVelocity();
            for (int index = 0; index < storedCount; ++index)
            {
                auto& references = performanceExpandedNoteRefCounts[
                    (size_t) storedNotes[(size_t) index]];
                if (references++ == 0)
                {
                    performanceSourceKeyByNote[(size_t) storedNotes[(size_t) index]]
                        = storedSourceKeys[(size_t) index];
                    queueSourceEvent(storedNotes[(size_t) index],
                                     storedSourceKeys[(size_t) index], sampleOffset);
                    output.addEvent(juce::MidiMessage::noteOn(
                        1, storedNotes[(size_t) index], velocity), sampleOffset);
                }
            }
            continue;
        }

        if (message.isNoteOff())
        {
            const auto root = (size_t) juce::jlimit(0, 127, message.getNoteNumber());
            if (performanceRootActive[root])
            {
                const auto velocity = message.getVelocity();
                performanceRootActive[root] = false;
                for (int index = 0; index < performanceChordNoteCounts[root]; ++index)
                {
                    const auto note = performanceChordNotes[root][(size_t) index];
                    auto& references = performanceExpandedNoteRefCounts[(size_t) note];
                    if (references > 0 && --references == 0)
                    {
                        performanceSourceKeyByNote[(size_t) note] = -1;
                        queueSourceEvent(note, -1, sampleOffset);
                        output.addEvent(juce::MidiMessage::noteOff(1, note, velocity), sampleOffset);
                    }
                    else if (references > 0)
                    {
                        auto replacementSourceKey = -1;
                        for (size_t candidateRoot = 0;
                             candidateRoot < performanceRootActive.size()
                             && replacementSourceKey < 0; ++candidateRoot)
                        {
                            if (! performanceRootActive[candidateRoot])
                                continue;
                            for (int candidate = 0;
                                 candidate < performanceChordNoteCounts[candidateRoot]; ++candidate)
                                if (performanceChordNotes[candidateRoot][(size_t) candidate] == note)
                                {
                                    replacementSourceKey = performanceChordSourceKeys[candidateRoot]
                                        [(size_t) candidate];
                                    break;
                                }
                        }
                        performanceSourceKeyByNote[(size_t) note] = replacementSourceKey;
                        queueSourceEvent(note, replacementSourceKey, sampleOffset);
                    }
                }
                performanceChordNoteCounts[root] = 0;
            }
            continue;
        }

        output.addEvent(message, sampleOffset);
    }
}

void QuietMachineAudioProcessor::renderGenerator(juce::MidiBuffer& midi,
                                                 const juce::MidiBuffer& performanceEvents,
                                                 int numSamples)
{
    const auto holdRequested = get(parameters, "generatorEnabled") > 0.5f;
    const auto& snapshot = activeGeneratorSnapshot;
    const auto updateHold = [&](int sampleOffset)
    {
        const auto running = holdRequested && performanceHeldNoteCount == 0;
        if (! running)
        {
            if (generatorWasRunning)
                stopGeneratedNotes(midi, sampleOffset);
            generatorWasRunning = false;
            return;
        }

        bool shapeChanged = generatedNoteCount != snapshot.autoNoteCount;
        for (int index = 0; ! shapeChanged && index < generatedNoteCount; ++index)
            shapeChanged = generatedMidiNotes[(size_t) index]
                         != 48 + snapshot.autoNotes[(size_t) index];

        if (generatorWasRunning && ! shapeChanged)
            return;

        stopGeneratedNotes(midi, sampleOffset);
        for (int index = 0; index < snapshot.autoNoteCount; ++index)
        {
            const auto note = 48 + snapshot.autoNotes[(size_t) index];
            generatedMidiNotes[(size_t) generatedNoteCount++] = note;
            midi.addEvent(juce::MidiMessage::noteOn(16, note, (juce::uint8) 88), sampleOffset);
        }
        generatorWasRunning = true;
    };

    // No clock, random movement, step program, or periodic re-triggering.
    // Group simultaneous performance events before deciding who owns the gate.
    auto event = performanceEvents.cbegin();
    if (event == performanceEvents.cend() || (*event).samplePosition > 0)
        updateHold(0);
    while (event != performanceEvents.cend())
    {
        const auto sampleOffset = juce::jlimit(0, juce::jmax(0, numSamples - 1),
                                                (*event).samplePosition);
        do
        {
            updatePerformanceHeldNotes((*event).getMessage());
            ++event;
        }
        while (event != performanceEvents.cend()
               && (*event).samplePosition == sampleOffset);
        updateHold(sampleOffset);
    }
}

void QuietMachineAudioProcessor::appendActiveGeneratedNotes(juce::MidiBuffer& midi,
                                                             int sampleOffset,
                                                             bool noteOn) const
{
    for (int index = 0; index < generatedNoteCount; ++index)
    {
        const auto note = generatedMidiNotes[(size_t) index];
        const auto message = noteOn
            ? juce::MidiMessage::noteOn(16, note, (juce::uint8) 88)
            : juce::MidiMessage::noteOff(16, note);
        midi.addEvent(message, sampleOffset);
    }
}

void QuietMachineAudioProcessor::queueUiMidiEvent(uint8_t status, uint8_t data1, uint8_t data2)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    uiMidiFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 <= 0)
    {
        if ((status & 0xf0u) == 0x80u || ((status & 0xf0u) == 0x90u && data2 == 0))
            panicRequested.store(true, std::memory_order_release);
        return;
    }

    uiMidiStorage[(size_t) start1] = { status, data1, data2 };
    uiMidiFifo.finishedWrite(1);
}

void QuietMachineAudioProcessor::drainUiMidiEvents(juce::MidiBuffer& midi, int numSamples)
{
    const auto ready = uiMidiFifo.getNumReady();
    if (ready <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    uiMidiFifo.prepareToRead(ready, start1, size1, start2, size2);
    const auto appendRange = [&midi, numSamples, this](int start, int size)
    {
        for (int index = 0; index < size; ++index)
        {
            const auto& event = uiMidiStorage[(size_t) (start + index)];
            auto message = juce::MidiMessage((int) event.status, (int) event.data1,
                                              (int) event.data2);
            trackCaptureInput(message, true);
            if (message.getChannel() > 0)
                message.setChannel(1);
            midi.addEvent(message, juce::jmax(0, juce::jmin(numSamples - 1, 0)));
        }
    };
    appendRange(start1, size1);
    appendRange(start2, size2);
    uiMidiFifo.finishedRead(size1 + size2);
}

void QuietMachineAudioProcessor::updatePerformanceHeldNotes(const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        auto& count = performanceHeldNoteCounts[(size_t) message.getNoteNumber()];
        if (count == 0)
        {
            count = 1;
            ++performanceHeldNoteCount;
        }
    }
    else if (message.isNoteOff())
    {
        auto& count = performanceHeldNoteCounts[(size_t) message.getNoteNumber()];
        if (count > 0)
        {
            count = 0;
            --performanceHeldNoteCount;
        }
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        performanceHeldNoteCounts.fill(0);
        performanceHeldNoteCount = 0;
    }
}

void QuietMachineAudioProcessor::captureGeneratedEvent(const juce::MidiMessage& message,
                                                        int sampleOffset,
                                                        float bpm)
{
    if (message.getRawDataSize() < 2 || message.getRawDataSize() > 3)
        return;

    const auto* bytes = message.getRawData();
    CapturedMidiEvent event;
    event.timeSeconds = renderedSeconds + (double) sampleOffset / currentSampleRate;
    event.bpm = juce::jlimit(1.0f, 960.0f, bpm);
    event.status = bytes[0];
    event.data1 = bytes[1];
    event.data2 = message.getRawDataSize() == 3 ? bytes[2] : 0;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    midiCaptureFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        midiCaptureStorage[(size_t) start1] = event;
        midiCaptureFifo.finishedWrite(1);
    }
    else
    {
        droppedMidiCaptureEvents.fetch_add(1, std::memory_order_relaxed);
    }
}

void QuietMachineAudioProcessor::captureActiveGeneratedNotes(bool noteOn,
                                                              int sampleOffset,
                                                              float bpm)
{
    for (int index = 0; index < generatedNoteCount; ++index)
    {
        const auto note = generatedMidiNotes[(size_t) index];
        captureGeneratedEvent(noteOn
                                  ? juce::MidiMessage::noteOn(16, note, (juce::uint8) 88)
                                  : juce::MidiMessage::noteOff(16, note),
                              sampleOffset, bpm);
    }
}

void QuietMachineAudioProcessor::drainMidiCaptureToCache()
{
    const auto ready = midiCaptureFifo.getNumReady();
    if (ready <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    midiCaptureFifo.prepareToRead(ready, start1, size1, start2, size2);
    for (int index = 0; index < size1; ++index)
        midiCaptureCache.push_back(midiCaptureStorage[(size_t) (start1 + index)]);
    for (int index = 0; index < size2; ++index)
        midiCaptureCache.push_back(midiCaptureStorage[(size_t) (start2 + index)]);
    midiCaptureFifo.finishedRead(size1 + size2);
}

juce::String QuietMachineAudioProcessor::getMidiCaptureAsBase64()
{
    const juce::ScopedLock consumerLock(midiCaptureConsumerLock);
    drainMidiCaptureToCache();

    constexpr int ticksPerQuarterNote = 960;
    juce::MidiMessageSequence sequence;
    auto trackName = juce::MidiMessage::textMetaEvent(3, "Quiet Machine Native Held MIDI");
    trackName.setTimeStamp(0.0);
    sequence.addEvent(trackName);

    const auto defaultTempo = juce::jlimit(1.0f, 960.0f,
                                           lastRenderedTempo.load(std::memory_order_relaxed));
    auto previousTempo = midiCaptureCache.empty()
        ? defaultTempo : juce::jlimit(1.0f, 960.0f, midiCaptureCache.front().bpm);
    auto tempoMessage = juce::MidiMessage::tempoMetaEvent(
        juce::roundToInt(60000000.0 / previousTempo));
    tempoMessage.setTimeStamp(0.0);
    sequence.addEvent(tempoMessage);

    const auto originSeconds = midiCaptureCache.empty() ? 0.0 : midiCaptureCache.front().timeSeconds;
    auto previousSeconds = originSeconds;
    double tick = 0.0;
    std::array<int, 128> activeNoteCounts {};

    for (const auto& event : midiCaptureCache)
    {
        const auto eventSeconds = juce::jmax(previousSeconds, event.timeSeconds);
        tick += (eventSeconds - previousSeconds) * previousTempo
              * (double) ticksPerQuarterNote / 60.0;

        const auto eventTempo = juce::jlimit(1.0f, 960.0f, event.bpm);
        if (std::abs(eventTempo - previousTempo) > 0.01f)
        {
            auto change = juce::MidiMessage::tempoMetaEvent(
                juce::roundToInt(60000000.0 / eventTempo));
            change.setTimeStamp(tick);
            sequence.addEvent(change);
            previousTempo = eventTempo;
        }

        juce::MidiMessage message((int) event.status, (int) event.data1,
                                  (int) event.data2, tick);
        sequence.addEvent(message);
        if (message.isNoteOn())
            ++activeNoteCounts[(size_t) message.getNoteNumber()];
        else if (message.isNoteOff())
        {
            auto& count = activeNoteCounts[(size_t) message.getNoteNumber()];
            count = juce::jmax(0, count - 1);
        }
        previousSeconds = eventSeconds;
    }

    const auto endpoint = captureEndpointSeconds.load(std::memory_order_acquire);
    const auto endTick = tick + juce::jmax(0.0, endpoint - previousSeconds)
                              * previousTempo * (double) ticksPerQuarterNote / 60.0;
    for (int note = 0; note < (int) activeNoteCounts.size(); ++note)
        if (activeNoteCounts[(size_t) note] > 0)
        {
            auto noteOff = juce::MidiMessage::noteOff(16, note);
            noteOff.setTimeStamp(juce::jmax(tick + 1.0, endTick));
            sequence.addEvent(noteOff);
        }

    const auto dropped = droppedMidiCaptureEvents.load(std::memory_order_relaxed);
    if (dropped > 0)
    {
        auto warning = juce::MidiMessage::textMetaEvent(
            1, juce::String("Quiet Machine capture FIFO dropped ")
                   + juce::String(dropped) + " events");
        warning.setTimeStamp(0.0);
        sequence.addEvent(warning);
    }

    sequence.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(ticksPerQuarterNote);
    file.addTrack(sequence);

    juce::MemoryOutputStream output;
    if (! file.writeTo(output, 1))
        return {};
    return output.getMemoryBlock().toBase64Encoding();
}

void QuietMachineAudioProcessor::clearMidiCapture()
{
    const juce::ScopedLock consumerLock(midiCaptureConsumerLock);
    midiCaptureCache.clear();

    const auto ready = midiCaptureFifo.getNumReady();
    if (ready > 0)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        midiCaptureFifo.prepareToRead(ready, start1, size1, start2, size2);
        midiCaptureFifo.finishedRead(size1 + size2);
    }
    droppedMidiCaptureEvents.store(0, std::memory_order_relaxed);
    midiCaptureRestartRequested.store(true, std::memory_order_release);
}

void QuietMachineAudioProcessor::noteOnFromUi(int midiNote, float velocity)
{
    const auto note = (uint8_t) juce::jlimit(0, 127, midiNote);
    const auto midiVelocity = (uint8_t) juce::jlimit(
        1, 127, juce::roundToInt(juce::jlimit(0.0f, 1.0f, velocity) * 127.0f));
    queueUiMidiEvent(0x90, note, midiVelocity);
}

void QuietMachineAudioProcessor::noteOffFromUi(int midiNote)
{
    queueUiMidiEvent(0x80, (uint8_t) juce::jlimit(0, 127, midiNote), 0);
}

void QuietMachineAudioProcessor::panicFromUi()
{
    panicRequested.store(true, std::memory_order_release);
}

void QuietMachineAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    for (int index = 0; index < state.getNumChildren(); ++index)
    {
        auto child = state.getChild(index);
        const auto id = child.getProperty("id").toString();
        if (id == "generatorEnabled" || id == "midiRecordEnabled")
            child.setProperty("value", 0.0f, nullptr);
    }
    {
        const juce::ScopedLock lock(patchStateLock);
        state.setProperty("browserPatchJson", patchStateJson, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}

void QuietMachineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            const auto json = restored.getProperty("browserPatchJson").toString();
            parameters.replaceState(restored);
            if (auto* generator = parameters.getParameter("generatorEnabled"))
                generator->setValue(0.0f);
            if (auto* record = parameters.getParameter("midiRecordEnabled"))
                record->setValue(0.0f);
            initialiseStructuredState();
            {
                const juce::ScopedLock lock(patchStateLock);
                patchStateJson = json;
            }
            rebuildGeneratorSnapshot(json);
            engineResetRequested.store(true, std::memory_order_release);
        }
}

juce::AudioProcessorEditor* QuietMachineAudioProcessor::createEditor()
{
    return new QuietMachineAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuietMachineAudioProcessor();
}
