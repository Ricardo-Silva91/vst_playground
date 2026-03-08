#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class DrumSmashEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit DrumSmashEditor (DrumSmashProcessor&);
    ~DrumSmashEditor() override;
    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    DrumSmashProcessor& proc;

    juce::OwnedArray<juce::TextButton> presetButtons;

    struct KnobRow
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    std::vector<KnobRow> knobs;

    int selectedPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSmashEditor)
};