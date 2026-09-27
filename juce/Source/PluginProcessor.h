#pragma once

#include <JuceHeader.h>
#include "NativeAmbientEngine.h"
#include "NativeMasterFx.h"
#include "NativeTextureEngine.h"
#include "SourceOutput.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace qm
{
inline constexpr auto parameterIds = std::to_array<const char*>({
    "tempoBpm", "glideSeconds", "oscillatorWave",
    "masterPitchSemitones", "lowpassHz", "lowpassQ", "filterSweepDepth", "highpassHz",
    "tapeSpeed", "tapeAttack", "wowRateHz", "wowDepth", "warpDepth", "tapeWear",
    "tapeDrive", "reverbMix", "delayEnabled", "freezeEnabled", "masterGain",
    "ampAttack", "ampDecay", "ampSustain", "ampRelease",
    "tape1Gain", "tape2Gain", "tape3Gain", "tape4Gain", "tape5Gain",
    "swellGain", "rumbleGain", "pianoGain", "droneGain", "subGain", "staticGain", "hissGain",
    "tape1Mute", "tape2Mute", "tape3Mute", "tape4Mute", "tape5Mute",
    "swellMute", "rumbleMute", "pianoMute", "droneMute", "subMute", "staticMute", "hissMute",
    "generatorEnabled",
    "hostTempoSync", "bypass", "midiOutEnabled", "midiRecordEnabled", "midiChordMode", "ampLegato"
});

inline constexpr auto sourceNames = std::to_array<const char*>({
    "tape1", "tape2", "tape3", "tape4", "tape5", "swell", "rumble",
    "piano", "drone", "sub", "static", "hiss"
});
}

class QuietMachineAudioProcessor final : public juce::AudioProcessor
{
public:
    QuietMachineAudioProcessor();
    ~QuietMachineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Ambient Directive"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setParameterFromUi(const juce::String& id, float actualValue);
    void beginParameterGesture(const juce::String& id);
    void endParameterGesture(const juce::String& id);
    juce::var getParameterStateForUi() const;
    void setPatchStateFromUi(const juce::String& json);
    juce::String getPatchStateForUi() const;
    void noteOnFromUi(int midiNote, float velocity);
    void noteOffFromUi(int midiNote);
    void panicFromUi();
    juce::String getMidiCaptureAsBase64();
    void clearMidiCapture();
    float getOutputPeak() const noexcept { return outputPeak.load(std::memory_order_relaxed); }
    // Bits 0..35 are raw input notes C3..B5; bit 63 signals notes outside that range.
    uint64_t getHeldInputChord() const noexcept
    {
        if (engineResetRequested.load(std::memory_order_acquire)
            || panicRequested.load(std::memory_order_acquire))
            return 0;
        return heldInputChord.load(std::memory_order_acquire);
    }

    juce::AudioProcessorValueTreeState parameters;

private:
    static BusesProperties createBusesProperties();

    struct GeneratorSnapshot
    {
        std::array<int, 16> autoNotes { 0, 7, 14, 19 };
        int autoNoteCount = 4;
        std::array<int, 16> midiChordNotes { 0, 7, 14, 19 };
        int midiChordNoteCount = 4;
        std::array<qm::KeyVoiceProfile, qm::keyVoiceCount> keyVoices {};
        uint64_t revision = 1;
    };

    struct QueuedUiMidiEvent
    {
        uint8_t status = 0;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
    };

    struct CapturedMidiEvent
    {
        double timeSeconds = 0.0;
        float bpm = 30.0f;
        uint8_t status = 0;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
    };

    void initialiseStructuredState();
    void rebuildGeneratorSnapshot(const juce::String& json);
    void renderGenerator(juce::MidiBuffer&, const juce::MidiBuffer& performanceEvents,
                         int numSamples);
    void buildPerformanceMidi(const juce::MidiBuffer& input, juce::MidiBuffer& output,
                              bool chordMode, int numSamples);
    void clearPerformanceMidiMappings() noexcept;
    void trackCaptureInput(const juce::MidiMessage&, bool fromUi) noexcept;
    void clearCaptureInput() noexcept;
    void stopGeneratedNotes(juce::MidiBuffer&, int sampleOffset);
    void appendActiveGeneratedNotes(juce::MidiBuffer&, int sampleOffset, bool noteOn) const;
    void queueUiMidiEvent(uint8_t status, uint8_t data1, uint8_t data2);
    void drainUiMidiEvents(juce::MidiBuffer&, int numSamples);
    void updatePerformanceHeldNotes(const juce::MidiMessage&);
    void captureGeneratedEvent(const juce::MidiMessage&, int sampleOffset, float bpm);
    void captureActiveGeneratedNotes(bool noteOn, int sampleOffset, float bpm);
    void drainMidiCaptureToCache();
    void drainGeneratorSnapshots();
    float getEffectiveTempo() const;

    qm::NativeAmbientEngine nativeAmbientEngine;
    qm::NativeTextureEngine nativeTextureEngine;
    qm::NativeMasterFx nativeMasterFx;
    juce::ADSR ampEnvelope;
    std::array<juce::AudioBuffer<float>, qm::sourceCount> sourceBuffers;
    juce::AudioBuffer<float> ampEnvelopeBuffer;

    static constexpr int generatorSnapshotCapacity = 32;
    static constexpr int uiMidiCapacity = 256;
    static constexpr int midiCaptureCapacity = 65536;
    static constexpr int synthesisMidiReserveBytes = 65536;
    static constexpr int generatedMidiReserveBytes = 4096;
    static constexpr int maxIncomingMidiEventsPerBlock = 2048;
    static constexpr int maxPerformanceSourceEventsPerBlock =
        maxIncomingMidiEventsPerBlock * 16;

    std::array<GeneratorSnapshot, generatorSnapshotCapacity> generatorSnapshotStorage {};
    juce::AbstractFifo generatorSnapshotFifo { generatorSnapshotCapacity };
    juce::SpinLock generatorSnapshotProducerLock;
    GeneratorSnapshot activeGeneratorSnapshot;
    uint64_t appliedVoiceProfileRevision = 0;
    std::atomic<uint64_t> nextSnapshotRevision { 2 };
    std::atomic<uint32_t> droppedGeneratorSnapshots { 0 };

    std::array<QueuedUiMidiEvent, uiMidiCapacity> uiMidiStorage {};
    juce::AbstractFifo uiMidiFifo { uiMidiCapacity };
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> engineResetRequested { false };

    std::array<CapturedMidiEvent, midiCaptureCapacity> midiCaptureStorage {};
    juce::AbstractFifo midiCaptureFifo { midiCaptureCapacity };
    std::vector<CapturedMidiEvent> midiCaptureCache;
    juce::CriticalSection midiCaptureConsumerLock;
    std::atomic<uint32_t> droppedMidiCaptureEvents { 0 };
    std::atomic<bool> midiCaptureRestartRequested { false };

    juce::MidiBuffer synthesisMidi;
    juce::MidiBuffer deferredHostMidi;
    juce::MidiBuffer performanceMidi;
    juce::MidiBuffer generatedMidi;
    juce::MidiBuffer envelopeMidi;
    std::array<int, 16> generatedMidiNotes {};
    int generatedNoteCount = 0;
    std::array<int, 128> performanceHeldNoteCounts {};
    int performanceHeldNoteCount = 0;
    std::array<std::array<int, 16>, 128> performanceChordNotes {};
    std::array<std::array<int, 16>, 128> performanceChordSourceKeys {};
    std::array<int, 128> performanceChordNoteCounts {};
    std::array<bool, 128> performanceRootActive {};
    std::array<int, 128> performanceExpandedNoteRefCounts {};
    std::array<int, 128> performanceSourceKeyByNote {};
    std::array<qm::PerformanceSourceEvent, maxPerformanceSourceEventsPerBlock>
        performanceSourceEvents {};
    int performanceSourceEventCount = 0;
    std::array<std::array<int, 128>, 16> ampHeldNoteCounts {};
    int ampHeldNoteCount = 0;
    bool ampGeneratorResumePending = false;
    bool generatorWasRunning = false;
    bool midiOutWasEnabled = false;
    bool midiRecordWasEnabled = false;
    bool midiChordModeWasEnabled = false;
    bool bypassWasEnabled = false;
    bool hostTransportKnown = false;
    bool hostWasPlaying = false;
    double currentSampleRate = 44100.0;
    double renderedSeconds = 0.0;
    std::atomic<double> captureEndpointSeconds { 0.0 };
    std::atomic<float> lastRenderedTempo { 30.0f };
    std::atomic<float> outputPeak { 0.0f };
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    std::atomic<uint64_t> heldInputChord { 0 };
    // Per-pitch bits: host channels 1..16, then the editor keyboard.
    std::array<uint32_t, 128> captureInputNotes {};

    mutable juce::CriticalSection patchStateLock;
    juce::String patchStateJson;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuietMachineAudioProcessor)
};
