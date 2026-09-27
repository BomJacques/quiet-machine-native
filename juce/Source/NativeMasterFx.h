#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

#include <cstdint>
#include <vector>

namespace qm
{
class NativeMasterFx final
{
public:
    NativeMasterFx() = default;

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer,
                 juce::AudioProcessorValueTreeState& parameters);

private:
    struct BlockParameters
    {
        float lowpassHz = 2000.0f;
        float lowpassQ = 0.5f;
        float highpassHz = 20.0f;
        float sweepDepth = 0.2f;
        float wowRateHz = 1.5f;
        float wowDepth = 0.3f;
        float warpDepth = 0.4f;
        float tapeWear = 0.1f;
        float reverbMix = 0.7f;
        float masterGain = 0.7f;
        bool delayEnabled = true;
        bool freezeEnabled = false;
    };

    struct ParameterPointers
    {
        const juce::AudioProcessorValueTreeState* owner = nullptr;
        const std::atomic<float>* lowpassHz = nullptr;
        const std::atomic<float>* lowpassQ = nullptr;
        const std::atomic<float>* highpassHz = nullptr;
        const std::atomic<float>* filterSweepDepth = nullptr;
        const std::atomic<float>* wowRateHz = nullptr;
        const std::atomic<float>* wowDepth = nullptr;
        const std::atomic<float>* warpDepth = nullptr;
        const std::atomic<float>* tapeWear = nullptr;
        const std::atomic<float>* reverbMix = nullptr;
        const std::atomic<float>* delayEnabled = nullptr;
        const std::atomic<float>* masterGain = nullptr;
        const std::atomic<float>* freezeEnabled = nullptr;
    };
    ParameterPointers parameterPointers;
    void cacheParameterPointers(const juce::AudioProcessorValueTreeState& parameters) noexcept;
    static float readParameter(const std::atomic<float>* parameter) noexcept;
    static float readCircular(const std::vector<float>& buffer, double position) noexcept;
    static int wrapIndex(int index, int size) noexcept;
    static double wrapPosition(double position, int size) noexcept;
    static float smoothStep(float value) noexcept;

    BlockParameters readBlockParameters() const noexcept;
    void beginFreeze() noexcept;
    void updateFreezeEvolution() noexcept;
    void scheduleNextDropout(float wear) noexcept;
    float renderDropoutGain(float wear) noexcept;
    float nextRandomFloat() noexcept;

    std::vector<float> delayLeft;
    std::vector<float> delayRight;
    std::vector<float> wowLeft;
    std::vector<float> wowRight;
    std::vector<float> freezeLeft;
    std::vector<float> freezeRight;

    juce::dsp::StateVariableTPTFilter<float> lowpass;
    juce::dsp::StateVariableTPTFilter<float> highpass;
    juce::dsp::StateVariableTPTFilter<float> airHighpass;
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayMixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freezeMixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowpassHzSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowpassQSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highpassHzSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wowDepthSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> warpDepthSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wearDepthSmoother;

    double currentSampleRate = 44100.0;
    int maximumPreparedBlockSize = 0;
    int delayWriteIndex = 0;
    int wowWriteIndex = 0;
    int freezeWriteIndex = 0;
    int delaySamplesLeft = 1;
    int delaySamplesRight = 1;

    bool prepared = false;
    bool delayWasEnabled = true;
    bool freezeWasEnabled = false;
    double freezeStartPosition = 0.0;
    double freezeReadPhase = 0.0;
    double freezeLengthSamples = 1.0;
    double freezeTargetLengthSamples = 1.0;
    double samplesUntilFreezeEvolution = 1.0;

    double sweepPhase = 0.0;
    double wowPhase = 0.0;
    double warpState = 0.0;
    double warpTarget = 0.0;
    double samplesUntilWarpTarget = 1.0;
    float wearNoiseState = 0.0f;
    float appliedLowpassHz = -1.0f;
    float appliedLowpassQ = -1.0f;
    float appliedHighpassHz = -1.0f;

    bool dropoutActive = false;
    double dropoutElapsedSamples = 0.0;
    double dropoutDurationSamples = 1.0;
    double samplesUntilDropout = 1.0;
    float dropoutDepth = 0.0f;

    uint32_t randomState = 0x514d4d46u;
};
}
