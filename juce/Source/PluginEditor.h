#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class QuietMachineAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit QuietMachineAudioProcessorEditor(QuietMachineAudioProcessor&);
    ~QuietMachineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    static juce::String helpText(int section);

private:
    class Interface;
    std::unique_ptr<Interface> interface;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuietMachineAudioProcessorEditor)
};
