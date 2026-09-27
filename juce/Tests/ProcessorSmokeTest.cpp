#include "../Source/PluginProcessor.h"
#include "../Source/FactoryPresets.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

void require(bool condition, const juce::String& message)
{
    if (! condition)
        throw std::runtime_error(message.toStdString());
}

double energy(const juce::AudioBuffer<float>& audio)
{
    double result = 0.0;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto value = audio.getSample(channel, sample);
            require(std::isfinite(value), "non-finite audio");
            result += (double) value * value;
        }
    return result;
}

struct Rig
{
    // Processor is deliberately heap allocated: its MIDI queues are large.
    std::unique_ptr<QuietMachineAudioProcessor> processor =
        std::make_unique<QuietMachineAudioProcessor>();
    juce::AudioBuffer<float> audio;
    juce::MidiBuffer midi;

    explicit Rig(bool stems = false, double rate = sampleRate, int preparedBlock = blockSize)
    {
        if (stems)
            require(processor->enableAllBuses(), "enable stem buses");
        processor->setRateAndBufferSizeDetails(rate, preparedBlock);
        processor->prepareToPlay(rate, preparedBlock);
        audio.setSize(processor->getTotalNumOutputChannels(), preparedBlock);
    }

    void set(const char* id, float value)
    {
        require(processor->parameters.getParameter(id) != nullptr,
                juce::String("missing parameter: ") + id);
        processor->setParameterFromUi(id, value);
    }

    void dry()
    {
        for (const auto* source : qm::sourceNames)
        {
            processor->setParameterFromUi(juce::String(source) + "Gain", 0.0f);
            processor->setParameterFromUi(juce::String(source) + "Mute", 0.0f);
        }
        set("subGain", 1.0f);
        set("oscillatorWave", 3.0f);
        set("glideSeconds", 0.0f);
        set("ampAttack", 0.005f);
        set("ampDecay", 0.01f);
        set("ampSustain", 1.0f);
        set("ampRelease", 0.01f);
        set("reverbMix", 0.0f);
        set("delayEnabled", 0.0f);
        set("wowDepth", 0.0f);
        set("warpDepth", 0.0f);
        set("tapeWear", 0.0f);
        set("tapeDrive", 0.0f);
        set("filterSweepDepth", 0.0f);
        set("lowpassHz", 18000.0f);
        set("highpassHz", 20.0f);
        set("masterGain", 1.0f);
        set("generatorEnabled", 0.0f);
    }

    void tick()
    {
        audio.clear();
        processor->processBlock(audio, midi);
    }

    void zeroTick()
    {
        juce::AudioBuffer<float> empty(audio.getNumChannels(), 0);
        processor->processBlock(empty, midi);
    }

    void blocks(int count)
    {
        for (int block = 0; block < count; ++block)
        {
            tick();
            midi.clear(); // Never feed generated MIDI back into the next block.
        }
    }

    double busEnergy(int bus)
    {
        return energy(processor->getBusBuffer(audio, false, bus));
    }

    void on(int note, int offset = 0)
    {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), offset);
    }

    void off(int note, int offset = 0)
    {
        midi.addEvent(juce::MidiMessage::noteOff(1, note), offset);
    }
};

juce::String patch()
{
    return R"json({"seedText":"NATIVE01","engineMode":"PROG","navMode":"WANDER",
        "autoNotes":[0,4,7],"midiChordNotes":[4,8,11],
        "seqSteps":[{"notes":[23],"duration":1}],
        "pianoSeq":[[35],[23],[11]],
        "keyVoices":[{"detuneCents":23,"pan":-0.4,"oscillatorCount":3,
                      "oscillatorSpreadCents":17}]})json";
}

void startup()
{
    Rig rig(true);
    rig.processor->setPatchStateFromUi(patch());
    // Exercise noise sources too: their silence must not depend on default gains.
    rig.set("staticGain", 1.0f);
    rig.set("hissGain", 1.0f);
    for (int block = 0; block < 12; ++block)
    {
        rig.tick();
        require(energy(rig.audio) < 1.0e-12, "startup audio without MIDI/transport");
        require(rig.midi.isEmpty(), "startup generated MIDI");
    }
}

void variableCallbacks()
{
    constexpr std::array<int, 5> sizes { 0, 64, 127, 512, 2048 };
    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
    {
        Rig rig(true, rate, 2048);
        rig.dry();
        require(std::abs(rig.processor->getTailLengthSeconds() - 20.0) < 1.0e-9,
                "reported tail must cover maximum 20-second release");
        const auto context = juce::String(" sr=") + juce::String(rate, 0);
        const auto render = [&rig](int size)
        {
            rig.audio.setSize(rig.audio.getNumChannels(), size, false, false, true);
            rig.tick();
            rig.midi.clear();
            return energy(rig.audio);
        };
        for (const auto size : sizes)
            require(render(size) < 1.0e-12, "variable-block startup audio" + context);
        for (const auto onsetSize : { 64, 127, 512, 2048 })
        {
            const auto label = context + " onsetBlock=" + juce::String(onsetSize);
            rig.on(60, onsetSize - 1); // Exercise the final valid event offset.
            double activeEnergy = render(onsetSize);
            int elapsed = 0, cursor = 0;
            while (elapsed < (int) (rate * 0.06))
            {
                const auto size = sizes[(size_t) cursor++ % sizes.size()];
                activeEnergy += render(size);
                elapsed += size;
            }
            require(activeEnergy > 1.0e-7, "variable-block note onset silent" + label);
            rig.off(60, onsetSize - 1);
            render(onsetSize);
            elapsed = 0;
            while (elapsed < (int) (rate * 0.08))
            {
                const auto size = sizes[(size_t) cursor++ % sizes.size()];
                render(size);
                elapsed += size;
            }
            // Check every size after release, not just a possibly empty final block.
            for (const auto size : sizes)
                require(render(size) < 1.0e-12, "variable-block release left audio" + label);
        }
    }
}

void expectNotes(const juce::MidiBuffer& midi, bool on, std::set<int> expected)
{
    const auto expectedCount = (int) expected.size();
    int count = 0;
    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        require(on ? message.isNoteOn() : message.isNoteOff(), "unexpected MIDI event type");
        require(message.getChannel() == 16 && event.samplePosition == 0,
                "generator MIDI channel/offset");
        require(expected.erase(message.getNoteNumber()) == 1, "unexpected/duplicate generated note");
        ++count;
    }
    require(count == expectedCount && expected.empty(), "missing generated chord notes");
}

void hold()
{
    Rig rig;
    rig.dry();
    for (const auto* id : { "engineMode", "navigationMode", "pianoSequencerEnabled",
                           "pianoRateHz", "followHostTransport" })
        require(rig.processor->parameters.getParameter(id) == nullptr,
                juce::String("obsolete parameter remains: ") + id);
    rig.processor->setPatchStateFromUi(patch());
    rig.set("tempoBpm", 120.0f);
    rig.set("midiOutEnabled", 1.0f);
    rig.set("generatorEnabled", 1.0f);
    rig.tick();
    expectNotes(rig.midi, true, { 48, 52, 55 });
    // Over three former 120 BPM beat boundaries, including a live tempo change.
    for (int block = 0; block < 160; ++block)
    {
        if (block == 100)
        {
            rig.set("tempoBpm", 60.0f);
        }
        rig.midi.clear();
        rig.tick();
        require(rig.midi.isEmpty(), "HOLD retriggered or changed with tempo/mode");
    }
    require(energy(rig.audio) > 1.0e-7, "HOLD stopped sounding");
    rig.set("generatorEnabled", 0.0f);
    rig.midi.clear();
    rig.tick();
    expectNotes(rig.midi, false, { 48, 52, 55 });
    rig.midi.clear();
    rig.blocks(24);
    require(energy(rig.audio) < 1.0e-12, "HOLD release left audio");
}

void zeroSampleHold()
{
    Rig rig;
    rig.dry();
    rig.processor->setPatchStateFromUi(R"({"autoNotes":[0,4,7]})");
    rig.set("midiOutEnabled", 1.0f);
    rig.set("generatorEnabled", 1.0f);
    for (int callback = 0; callback < 3; ++callback)
    {
        rig.zeroTick();
        require(rig.midi.isEmpty(), "zero-sample callback emitted the initial HOLD gate");
    }
    rig.tick();
    expectNotes(rig.midi, true, { 48, 52, 55 });
    double activeEnergy = energy(rig.audio);
    rig.midi.clear();
    rig.zeroTick();
    require(rig.midi.isEmpty(), "zero-sample callback retriggered an existing HOLD");
    // Master tape processing has a fixed 50 ms delay even with wowDepth=0.
    for (int block = 1; block < 12; ++block)
    {
        rig.tick();
        require(rig.midi.isEmpty(), "HOLD retriggered after the initial nonempty callback");
        activeEnergy += energy(rig.audio);
    }
    require(activeEnergy > 1.0e-9, "HOLD remained silent beyond master tape startup");
}

void zeroSamplePhysicalMidi()
{
    Rig deferred, direct;
    deferred.dry();
    direct.dry();
    deferred.blocks(4);
    direct.blocks(4);
    deferred.on(67);
    deferred.zeroTick();
    // The host's buffer does not survive callbacks; the processor must own a copy.
    deferred.midi.clear();
    deferred.zeroTick();
    direct.on(67);
    const auto compareBlock = [&]
    {
        deferred.tick();
        direct.tick();
        const auto deferredEnergy = energy(deferred.audio);
        energy(direct.audio);
        for (int channel = 0; channel < direct.audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
                require(std::abs(deferred.audio.getSample(channel, sample)
                                 - direct.audio.getSample(channel, sample)) < 1.0e-7f,
                        "deferred/direct MIDI waveforms differ on a real callback");
        deferred.midi.clear();
        direct.midi.clear();
        return deferredEnergy;
    };
    double activeEnergy = 0.0;
    // Compare from the first callback, including silent tape-delay startup.
    for (int block = 0; block < 12; ++block)
        activeEnergy += compareBlock();
    require(activeEnergy > 1.0e-9, "physical note-on lost in zero-sample callback");
    deferred.off(67);
    deferred.zeroTick();
    deferred.midi.clear();
    deferred.zeroTick();
    direct.off(67);
    for (int block = 0; block < 24; ++block)
        compareBlock();
    require(energy(deferred.audio) < 1.0e-12, "physical note-off lost in zero-sample callback");

    juce::MemoryBlock silentState;
    deferred.processor->getStateInformation(silentState);
    deferred.on(72);
    deferred.zeroTick();
    deferred.midi.clear();
    deferred.processor->setStateInformation(silentState.getData(), (int) silentState.getSize());
    // Span the fixed master tape delay: an erroneously retained note may not
    // become audible on the first callback after recall.
    for (int block = 0; block < 16; ++block)
    {
        deferred.blocks(1);
        require(energy(deferred.audio) < 1.0e-12,
                "state restore replayed a physical note deferred by a zero-sample callback");
    }
}

void heldMidiCapture()
{
    Rig rig;
    rig.dry();
    rig.processor->setPatchStateFromUi(R"({"autoNotes":[0,4,7]})");
    rig.set("tempoBpm", 120.0f);
    rig.processor->clearMidiCapture();
    rig.set("midiRecordEnabled", 1.0f);
    rig.set("generatorEnabled", 1.0f);
    const auto checkExport = [&rig](int renderedBlocks)
    {
        juce::MemoryBlock bytes;
        require(bytes.fromBase64Encoding(rig.processor->getMidiCaptureAsBase64())
                && ! bytes.isEmpty(), "held capture export empty");
        juce::MemoryInputStream stream(bytes, false);
        juce::MidiFile file;
        require(file.readFrom(stream), "held capture is not readable SMF");
        require(file.getTimeFormat() > 0, "held capture must use quarter-note timing");
        const auto tolerance = 2.0 * 60.0 / (120.0 * file.getTimeFormat());
        file.convertTimestampTicksToSeconds();
        std::array<double, 128> starts {}, ends {};
        std::array<int, 128> ons {}, offs {};
        std::set<int> notes;
        for (int track = 0; track < file.getNumTracks(); ++track)
        {
            const auto* sequence = file.getTrack(track);
            for (int event = 0; event < sequence->getNumEvents(); ++event)
            {
                const auto& message = sequence->getEventPointer(event)->message;
                if (! message.isNoteOn() && ! message.isNoteOff()) continue;
                require(message.getChannel() == 16, "captured HOLD channel changed");
                const auto note = (size_t) message.getNoteNumber();
                notes.insert((int) note);
                if (message.isNoteOn())
                {
                    ++ons[note];
                    starts[note] = message.getTimeStamp();
                }
                else
                {
                    ++offs[note];
                    ends[note] = message.getTimeStamp();
                }
            }
        }
        require(notes == std::set<int> { 48, 52, 55 }, "captured HOLD chord changed");
        const auto duration = (double) renderedBlocks * blockSize / sampleRate;
        for (const auto note : notes)
        {
            const auto i = (size_t) note;
            require(ons[i] == 1 && offs[i] == 1, "capture duplicated or omitted a held note event");
            require(std::abs(starts[i]) <= tolerance, "held capture did not begin at origin");
            require(std::abs((ends[i] - starts[i]) - duration) <= tolerance,
                    "export before stop truncated HOLD instead of sustaining to render endpoint");
        }
    };
    rig.blocks(94); // 1.002667 seconds, entirely offline.
    checkExport(94);
    rig.blocks(47);
    checkExport(141); // Export must not terminate or duplicate the recording.
    require(rig.processor->parameters.getRawParameterValue("generatorEnabled")->load() > 0.5f
            && energy(rig.audio) > 1.0e-9, "export stopped the live HOLD");
}

void pianoAfterIdle()
{
    Rig released(true), clean(true);
    for (auto* rig : { &released, &clean })
    {
        rig->dry();
        rig->set("subGain", 0.0f);
        rig->set("pianoGain", 1.0f);
        rig->blocks(4);
    }
    for (const auto note : { 48, 52, 55 }) released.on(note);
    double oldPianoEnergy = 0.0;
    for (int block = 0; block < 24; ++block)
    {
        released.blocks(1);
        oldPianoEnergy += released.busEnergy(8);
    }
    require(oldPianoEnergy > 1.0e-7, "piano tail regression did not establish an old voice");
    for (const auto note : { 48, 52, 55 }) released.off(note);
    released.blocks(4); // 10 ms master release, much shorter than the piano's decay.
    require(energy(released.audio) < 1.0e-12, "short piano release did not close master");
    // Longer than a piano voice's lifetime: the idle fast path must not freeze
    // hidden voices and resurrect them when the envelope next opens.
    released.blocks(660);
    require(energy(released.audio) < 1.0e-12, "piano idle not silent");
    for (const auto note : { 61, 65, 68 })
    {
        released.on(note);
        clean.on(note);
    }
    double referenceEnergy = 0.0, differenceEnergy = 0.0;
    for (int block = 0; block < 32; ++block)
    {
        released.blocks(1);
        clean.blocks(1);
        const auto actual = released.processor->getBusBuffer(released.audio, false, 8);
        const auto reference = clean.processor->getBusBuffer(clean.audio, false, 8);
        referenceEnergy += energy(reference);
        energy(actual);
        for (int channel = 0; channel < reference.getNumChannels(); ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const auto difference = (double) actual.getSample(channel, sample)
                                      - reference.getSample(channel, sample);
                differenceEnergy += difference * difference;
            }
    }
    require(referenceEnergy > 1.0e-7, "disjoint new piano chord silent");
    require(differenceEnergy <= referenceEnergy * 1.0e-10 + 1.0e-12,
            "released piano voices/state revived after idle instead of matching a clean start");
}

std::vector<float> captureDrone(Rig& rig)
{
    rig.blocks(24); // Allow previous voices' 40 ms smoothing to settle.
    std::vector<float> samples;
    samples.reserve(32 * blockSize);
    for (int block = 0; block < 32; ++block)
    {
        rig.blocks(1);
        auto stem = rig.processor->getBusBuffer(rig.audio, false, 9);
        samples.insert(samples.end(), stem.getReadPointer(0),
                       stem.getReadPointer(0) + blockSize);
    }
    return samples;
}

double magnitude(const std::vector<float>& samples, int midiNote)
{
    const auto hz = juce::MidiMessage::getMidiNoteInHertz(midiNote - 12);
    double real = 0.0, imaginary = 0.0;
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const auto phase = juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate;
        const auto window = 0.5 - 0.5 * std::cos(
            juce::MathConstants<double>::twoPi * (double) i / (double) (samples.size() - 1));
        real += samples[i] * window * std::cos(phase);
        imaginary += samples[i] * window * std::sin(phase);
    }
    return 2.0 * std::hypot(real, imaginary) / (double) samples.size();
}

void chords()
{
    Rig rig(true);
    rig.dry();
    rig.set("subGain", 0.0f);
    rig.set("droneGain", 1.0f);
    rig.set("midiChordMode", 1.0f);
    rig.processor->setPatchStateFromUi(
        R"({"autoNotes":[0],"midiChordNotes":[4,8,11]})");
    rig.blocks(4);
    rig.on(62, 96);
    const auto transposed = captureDrone(rig);
    for (const auto note : { 62, 66, 69 })
        require(magnitude(transposed, note) > 0.003, "chord did not transpose from minimum shape note");
    require(magnitude(transposed, 60) < 0.001, "untransposed chord leaked");

    rig.on(69); // Shares note 69 with root 62.
    rig.blocks(8);
    rig.off(62);
    const auto overlap = captureDrone(rig);
    for (const auto note : { 69, 73, 76 })
        require(magnitude(overlap, note) > 0.003, "overlap release killed a still-owned note");
    require(magnitude(overlap, 62) < 0.001, "released root remained held");

    // Releasing must use the mapping at key-down, not this new chord shape.
    rig.processor->setPatchStateFromUi(R"({"midiChordNotes":[0,2,9]})");
    rig.blocks(1);
    rig.off(69);
    rig.blocks(32);
    require(energy(rig.audio) < 1.0e-12, "expanded chord cleanup left audio");
}

void envelope()
{
    Rig rig(true);
    rig.dry();
    for (const auto* source : qm::sourceNames)
        rig.processor->setParameterFromUi(juce::String(source) + "Gain", 1.0f);
    rig.set("ampAttack", 0.1f);
    rig.set("ampDecay", 0.1f);
    rig.set("ampSustain", 0.25f);
    rig.set("ampRelease", 0.1f);
    rig.blocks(4);
    rig.on(60);
    std::array<double, 13> early {}, peak {}, sustain {};
    for (int block = 0; block < 40; ++block)
    {
        rig.blocks(1);
        for (int bus = 0; bus < 13; ++bus)
        {
            const auto e = rig.busEnergy(bus);
            if (block < 2) early[(size_t) bus] += e / 2.0;
            if (block >= 7 && block < 11) peak[(size_t) bus] += e / 4.0;
            if (block >= 28) sustain[(size_t) bus] += e / 12.0;
        }
    }
    // Average noise over windows; avoid fragile single-sample/random-noise assertions.
    for (const auto bus : { 10, 11, 12 })
    {
        require(peak[(size_t) bus] > early[(size_t) bus] * 2.0, "master attack did not shape Sub/Static/Hiss");
        require(sustain[(size_t) bus] > 1.0e-9, "sustain unexpectedly silent");
        require(sustain[(size_t) bus] < peak[(size_t) bus] * 0.4, "decay/sustain did not reduce Sub/Static/Hiss");
    }
    rig.off(60);
    rig.blocks(1);
    for (const auto bus : { 0, 10, 11, 12 })
        require(rig.busEnergy(bus) > 1.0e-10, "note-off cut release immediately");
    rig.blocks(32);
    require(energy(rig.audio) < 1.0e-12, "master/stems escaped release");

    // Regression: release from an interrupted attack with zero sustain.
    rig.set("ampAttack", 1.0f);
    rig.set("ampSustain", 0.0f);
    rig.set("ampRelease", 0.4f);
    rig.on(64);
    rig.blocks(2);
    rig.off(64);
    rig.blocks(18);
    for (const auto bus : { 0, 10, 11, 12 })
        require(rig.busEnergy(bus) > 1.0e-12, "interrupted-attack release ended early");
    rig.blocks(24);
    require(energy(rig.audio) < 1.0e-12, "interrupted-attack release did not finish");
}

void legatoEnvelope()
{
    const auto render = [](bool legato, bool chordMode)
    {
        Rig rig(true);
        rig.dry();
        rig.set("subGain", 0.0f);
        rig.set("hissGain", 1.0f);
        rig.set("ampAttack", 0.2f);
        rig.set("ampDecay", 0.1f);
        rig.set("ampSustain", 0.15f);
        rig.set("ampRelease", 0.1f);
        rig.set("ampLegato", legato ? 1.0f : 0.0f);
        rig.set("midiChordMode", chordMode ? 1.0f : 0.0f);
        rig.on(60); rig.blocks(60);
        rig.on(64);
        double total = 0;
        for (int i = 0; i < 18; ++i)
        {
            rig.blocks(1);
            total += rig.busEnergy(12);
        }
        rig.off(60); rig.off(64); rig.blocks(24);
        require(energy(rig.audio) < 1.0e-12, "legato option stranded release");
        juce::MemoryBlock saved;
        rig.processor->getStateInformation(saved);
        Rig restored;
        restored.processor->setStateInformation(saved.getData(), (int) saved.getSize());
        require((restored.processor->parameters.getRawParameterValue("ampLegato")->load() > 0.5f) == legato,
                "legato option failed state recall");
        return total;
    };
    for (const bool chordMode : { false, true })
    {
        const double continuous = render(true, chordMode);
        const double retriggered = render(false, chordMode);
        require(continuous > 1.0e-10 && retriggered > continuous * 4,
                "legato/retrigger did not differ on whole-output noise envelope");
    }
}

double delayTail(bool enabled)
{
    Rig rig;
    rig.dry();
    rig.set("ampRelease", 2.0f);
    rig.set("delayEnabled", enabled ? 1.0f : 0.0f);
    rig.blocks(4);
    rig.on(60);
    rig.off(60, 256);
    rig.blocks(1);
    rig.set("subGain", 0.0f);
    double tail = 0.0;
    for (int block = 0; block < 90; ++block)
    {
        rig.blocks(1);
        if (block >= 50) tail += energy(rig.audio);
    }
    return tail;
}

void delay()
{
    const auto wet = delayTail(true);
    const auto dry = delayTail(false);
    require(wet > 1.0e-9 && wet > dry * 100.0, "delay enabled/disabled tail not distinguished");
}

void panic()
{
    Rig rig(true);
    rig.dry();
    rig.set("staticGain", 1.0f);
    rig.set("hissGain", 1.0f);
    rig.set("ampRelease", 20.0f);
    rig.set("delayEnabled", 1.0f);
    rig.set("reverbMix", 0.7f);
    rig.on(60);
    rig.blocks(12);
    require(energy(rig.audio) > 1.0e-7, "panic precondition silent");
    rig.processor->panicFromUi();
    rig.blocks(1);
    require(energy(rig.audio) < 1.0e-12, "panic did not silence master and stems immediately");
    rig.blocks(8);
    require(energy(rig.audio) < 1.0e-12, "panic left a tail");
    rig.on(60);
    rig.blocks(8);
    require(energy(rig.audio) > 1.0e-7, "panic left stale MIDI ownership");
}

void state()
{
    Rig original;
    original.processor->setPatchStateFromUi(patch());
    original.set("ampAttack", 0.73f);
    original.set("delayEnabled", 0.0f);
    original.set("midiChordMode", 1.0f);
    original.set("generatorEnabled", 1.0f);
    original.set("midiRecordEnabled", 1.0f);
    juce::MemoryBlock bytes;
    original.processor->getStateInformation(bytes);
    Rig restored;
    restored.processor->setStateInformation(bytes.getData(), (int) bytes.getSize());
    const auto a = juce::JSON::parse(original.processor->getPatchStateForUi());
    const auto b = juce::JSON::parse(restored.processor->getPatchStateForUi());
    require(a.isObject() && b.isObject(), "state patch JSON invalid");
    for (const auto* key : { "autoNotes", "midiChordNotes", "keyVoices", "seedText" })
        require(juce::JSON::toString(a.getProperty(key, {})) ==
                juce::JSON::toString(b.getProperty(key, {})), juce::String("state lost ") + key);
    const auto values = restored.processor->getParameterStateForUi();
    require(std::abs((float) values.getProperty("ampAttack", 0.0f) - 0.73f) < 0.001f,
            "state lost ADSR");
    require((float) values.getProperty("delayEnabled", 1.0f) < 0.5f &&
            (float) values.getProperty("midiChordMode", 0.0f) > 0.5f, "state lost switches");
    require((float) values.getProperty("generatorEnabled", 1.0f) < 0.5f &&
            (float) values.getProperty("midiRecordEnabled", 1.0f) < 0.5f, "state restored transient transport");
    restored.blocks(8);
    require(energy(restored.audio) < 1.0e-12, "state restore auto-started audio");
}

void emptyPatchRecall()
{
    Rig restored(true), clean(true);
    require(restored.processor->getPatchStateForUi().isEmpty(), "initial patch must be empty for recall test");
    juce::MemoryBlock initialState;
    restored.processor->getStateInformation(initialState);
    restored.dry();
    restored.processor->setPatchStateFromUi(patch());
    restored.set("generatorEnabled", 1.0f);
    restored.set("midiOutEnabled", 1.0f);
    restored.tick();
    expectNotes(restored.midi, true, { 48, 52, 55 });
    restored.midi.clear();
    restored.processor->setStateInformation(initialState.getData(), (int) initialState.getSize());
    restored.blocks(1);
    require(restored.processor->getPatchStateForUi().isEmpty(), "empty patch JSON not restored");
    require(energy(restored.audio) < 1.0e-12, "empty patch recall retained active audio");
    restored.set("midiOutEnabled", 1.0f);
    restored.set("generatorEnabled", 1.0f);
    restored.tick();
    expectNotes(restored.midi, true, { 48, 55, 62, 67 });
    restored.midi.clear();

    // Compare isolated Drone against untouched defaults to cover the internal
    // MIDI chord and key-voice snapshot, not just the stored JSON string.
    for (auto* rig : { &restored, &clean })
    {
        rig->dry();
        rig->set("subGain", 0.0f);
        rig->set("droneGain", 1.0f);
        rig->set("midiChordMode", 1.0f);
        rig->blocks(24);
        rig->on(60);
    }
    const auto actual = captureDrone(restored);
    const auto reference = captureDrone(clean);
    double referenceEnergy = 0.0, differenceEnergy = 0.0;
    for (size_t sample = 0; sample < reference.size(); ++sample)
    {
        require(std::isfinite(actual[sample]) && std::isfinite(reference[sample]),
                "non-finite default voice recall");
        referenceEnergy += (double) reference[sample] * reference[sample];
        const auto difference = (double) actual[sample] - reference[sample];
        differenceEnergy += difference * difference;
    }
    require(referenceEnergy > 1.0e-7, "default voice recall reference silent");
    require(differenceEnergy <= referenceEnergy * 1.0e-10 + 1.0e-12,
            "empty JSON recall retained custom chord/key-voice snapshot");
}

void benchmark()
{
    for (const bool active : { false, true })
    {
        Rig rig;
        rig.processor->setPatchStateFromUi(R"({"autoNotes":[0,4,7,11]})");
        rig.set("generatorEnabled", active ? 1.0f : 0.0f);
        rig.blocks(16);
        constexpr int count = 192;
        const auto start = std::chrono::steady_clock::now();
        rig.blocks(count);
        const auto wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const auto audioSeconds = (double) count * blockSize / sampleRate;
        const auto outputEnergy = energy(rig.audio);
        require(active ? outputEnergy > 1.0e-9 : outputEnergy < 1.0e-12,
                "benchmark did not establish requested idle/active state");
        std::cout << "BENCH " << (active ? "active" : "idle")
                  << " sr=48000 block=512 blocks=" << count << " wall_s=" << wall
                  << " audio_s=" << audioSeconds << " wall/audio=" << wall / audioSeconds
                  << " (offline, no realtime deadline assertion)\n";
    }
}

void writeSnapshot(juce::AudioProcessorEditor& editor, const juce::File& file)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
    require(image.isValid(), "native snapshot failed");
    const auto first = image.getPixelAt(0, 0);
    bool varied = false;
    for (int y = 0; y < image.getHeight(); y += 7)
        for (int x = 0; x < image.getWidth(); x += 7)
            varied = varied || image.getPixelAt(x, y) != first;
    require(varied, "native snapshot is a single blank colour");
    require(file.getParentDirectory().createDirectory().wasOk(), "snapshot directory creation failed");
    {
        juce::FileOutputStream stream(file);
        require(stream.openedOk() && stream.setPosition(0) && stream.truncate().wasOk(),
                "snapshot output could not be opened/truncated");
        juce::PNGImageFormat png;
        require(png.writeImageToStream(image, stream), "PNG encoding failed");
        stream.flush();
        require(stream.getStatus().wasOk(), "PNG write failed");
    }
    const auto decoded = juce::ImageFileFormat::loadFrom(file);
    require(decoded.isValid() && decoded.getWidth() == image.getWidth() &&
            decoded.getHeight() == image.getHeight(), "PNG roundtrip failed");
    std::cout << "SNAPSHOT " << file.getFullPathName() << " "
              << image.getWidth() << 'x' << image.getHeight() << '\n';
}

juce::Component* findControl(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren())
        if (auto* found = findControl(*child, id)) return found;
    return nullptr;
}

template <typename Control>
Control& requireControl(juce::Component& root, const char* id)
{
    auto* control = dynamic_cast<Control*>(findControl(root, id));
    require(control != nullptr, juce::String("missing/wrong control type: ") + id);
    return *control;
}

int checkControlBindings(juce::Component& root, QuietMachineAudioProcessor& processor)
{
    int checked = 0;
    const auto id = root.getComponentID();
    const juce::StringArray uiOnly { "factoryPreset", "chordTarget", "keyboardMode",
        "chordKeyboard", "chordNoteNames", "captureChord", "helpToggle", "helpOverlay", "helpClose",
        "copyChord", "selectedKey" };
    if (id.isNotEmpty() && root.isVisible() && ! uiOnly.contains(id))
    {
        auto* parameter = processor.parameters.getRawParameterValue(id);
        require(parameter != nullptr, "control ID has no parameter: " + id);
        const auto actual = parameter->load();
        bool recognised = true;
        if (auto* slider = dynamic_cast<juce::Slider*>(&root))
            require(std::abs(slider->getValue() - actual) < 0.002, "slider binding mismatch: " + id);
        else if (auto* toggle = dynamic_cast<juce::Button*>(&root))
            require(toggle->getToggleState() == (actual > 0.5f), "toggle binding mismatch: " + id);
        else if (auto* choice = dynamic_cast<juce::ComboBox*>(&root))
            require(choice->getSelectedItemIndex() == juce::roundToInt(actual), "choice binding mismatch: " + id);
        else
            recognised = false;
        require(recognised, "unrecognised parameter control: " + id);
        ++checked;
    }
    for (auto* child : root.getChildren())
        checked += checkControlBindings(*child, processor);
    return checked;
}

#include "FeatureTests.h"

void snapshot(const juce::String& path, int width, int height, bool helpOverlay)
{
    Rig rig;
    auto& processor = rig.processor;
    juce::MemoryBlock savedState;
    processor->getStateInformation(savedState);
    const auto savedAttack = processor->parameters.getRawParameterValue("ampAttack")->load();
    const auto savedPatch = processor->getPatchStateForUi();
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor->createEditor());
    require(editor != nullptr && editor->getWidth() > 0 && editor->getHeight() > 0,
            "editor has no drawable bounds");
    if (width > 0)
        editor->setSize(width, height);
    // Offscreen native component tree, no desktop peer/audio device/browser needed.
    editor->setVisible(true);
    auto& releaseControl = requireControl<juce::Slider>(*editor, "ampRelease");
    auto& gainControl = requireControl<juce::Slider>(*editor, "masterGain");
    auto& delayControl = requireControl<juce::ToggleButton>(*editor, "delayEnabled");
    auto& waveControl = requireControl<juce::ComboBox>(*editor, "oscillatorWave");
    require(checkControlBindings(*editor, *processor) == 49, "expected 49 native parameter bindings");
    const auto value = [&](const char* id)
    {
        return processor->parameters.getRawParameterValue(id)->load();
    };
    releaseControl.setValue(0.37, juce::sendNotificationSync);
    require(std::abs(value("ampRelease") - 0.37f) < 0.002f, "release slider did not update processor");
    delayControl.setToggleState(value("delayEnabled") < 0.5f, juce::sendNotificationSync);
    require((value("delayEnabled") > 0.5f) == delayControl.getToggleState(),
            "delay toggle did not update processor");
    waveControl.setSelectedItemIndex(3, juce::sendNotificationSync);
    require(std::abs(value("oscillatorWave") - 3.0f) < 0.01f, "wave choice did not update processor");
    processor->setParameterFromUi("masterGain", 0.31f);
    processor->setParameterFromUi("oscillatorWave", 1.0f);
    processor->setParameterFromUi("ampAttack", 0.73f);
    processor->setPatchStateFromUi(patch());
    processor->setParameterFromUi("generatorEnabled", 1.0f);
    juce::AudioBuffer<float> recallAudio(processor->getTotalNumOutputChannels(), blockSize);
    juce::MidiBuffer recallMidi;
    recallAudio.clear();
    processor->processBlock(recallAudio, recallMidi);
    MessageSteps steps;
    steps.add(50, [&]
    {
        require(std::abs(gainControl.getValue() - 0.31) < 0.002,
                "processor masterGain did not update slider after dispatch");
        require(waveControl.getSelectedItemIndex() == 1,
                "processor waveform did not update choice after dispatch");
        checkControlBindings(*editor, *processor);
        // Exercise the same public state entry point used by the host, with all
        // editor attachments alive and a running generator to be cleaned up.
        processor->setStateInformation(savedState.getData(), (int) savedState.getSize());
    });
    steps.add(150, [&]
    {
        const auto controlCount = checkControlBindings(*editor, *processor);
        require(controlCount == 49, "state recall lost native bindings");
        require(std::abs(processor->parameters.getRawParameterValue("ampAttack")->load()
                         - savedAttack) < 0.001f
                && processor->getPatchStateForUi() == savedPatch,
                "open-editor host state recall did not restore parameters/patch");
        recallMidi.clear();
        recallAudio.clear();
        processor->processBlock(recallAudio, recallMidi);
        require(energy(recallAudio) < 1.0e-12, "open-editor state recall retained active audio");
        require(processor->parameters.getRawParameterValue("generatorEnabled")->load() < 0.5f,
                "open-editor state recall retained HOLD");
        std::cout << "PASS host state recall with editor alive\n";
        std::cout << "PASS native control bindings (" << controlCount << " controls)\n";
    });
    appendEditorTests(steps, rig, *editor);
    appendPresetTests(steps, rig, *editor);
    steps.add(50, [&]
    {
        processor->setStateInformation(savedState.getData(), (int) savedState.getSize());
        rig.blocks(1);
        if (helpOverlay) clickButton(*editor, "helpToggle");
    });
    steps.add(150, [&]
    {
        require(checkControlBindings(*editor, *processor) == 49, "final parameter binding count changed");
        require(requireControl<juce::Component>(*editor, "helpOverlay").isVisible() == helpOverlay,
                "snapshot help visibility mismatch");
    });
    steps.run();
    if (path.isEmpty()) return;
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    writeSnapshot(*editor, file);
    if (auto* limits = editor->getConstrainer())
    {
        const auto minWidth = limits->getMinimumWidth();
        const auto minHeight = limits->getMinimumHeight();
        if (minWidth > 0 && minHeight > 0)
        {
            editor->setSize(minWidth, minHeight);
            writeSnapshot(*editor, file.getSiblingFile(file.getFileNameWithoutExtension() + "-min.png"));
        }
    }
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    try
    {
        bool bench = false, uiTests = false, helpOverlay = false;
        int width = 0, height = 0;
        juce::String snapshotPath;
        for (int i = 1; i < argc; ++i)
        {
            const juce::String option(argv[i]);
            if (option == "--benchmark") bench = true;
            else if (option == "--ui-tests") uiTests = true;
            else if (option == "--help-overlay") helpOverlay = true;
            else if (option == "--snapshot" && i + 1 < argc) snapshotPath = argv[++i];
            else if (option == "--size" && i + 1 < argc)
            {
                const juce::String size(argv[++i]);
                width = size.upToFirstOccurrenceOf("x", false, false).getIntValue();
                height = size.fromFirstOccurrenceOf("x", false, false).getIntValue();
                require(width >= 64 && height >= 64 && width <= 4096 && height <= 4096,
                        "--size expects WIDTHxHEIGHT (64..4096)");
            }
            else throw std::runtime_error("usage: QuietMachineSmoke [--benchmark] [--ui-tests] [--snapshot output.png [--size WIDTHxHEIGHT] [--help-overlay]]");
        }
        require(width == 0 || snapshotPath.isNotEmpty(), "--size requires --snapshot");
        require(! helpOverlay || snapshotPath.isNotEmpty(), "--help-overlay requires --snapshot");
        // Snapshot-only mode also works while DSP regression work is in progress.
        if (! bench && ! uiTests && snapshotPath.isEmpty())
        {
            int failures = 0;
            const auto run = [&failures](const char* name, void (*test)())
            {
                try
                {
                    test();
                    std::cout << "PASS " << name << '\n';
                }
                catch (const std::exception& error)
                {
                    ++failures;
                    std::cerr << "FAIL " << name << ": " << error.what() << '\n';
                }
            };
            run("startup silence", startup);
            run("variable sample rates/blocks and reported tail", variableCallbacks);
            run("static HOLD", hold);
            run("zero-sample HOLD gate", zeroSampleHold);
            run("zero-sample physical MIDI deferral", zeroSamplePhysicalMidi);
            run("held MIDI capture duration before stop", heldMidiCapture);
            run("piano tails after release/idle", pianoAfterIdle);
            run("MIDI chord transposition/overlap/cleanup", chords);
            run("whole-machine ADSR", envelope);
            run("whole-machine legato/retrigger and recall", legatoEnvelope);
            run("delay on/off", delay);
            run("panic", panic);
            run("state including key voices", state);
            run("empty patch recall restores default chord/voices", emptyPatchRecall);
            run("raw held input capture mask and cleanup", rawHeldInput);
            run("factory preset catalogue", factoryCatalogue);
            if (failures != 0) return 1;
        }
        if (bench) benchmark();
        if (uiTests || snapshotPath.isNotEmpty()) snapshot(snapshotPath, width, height, helpOverlay);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
