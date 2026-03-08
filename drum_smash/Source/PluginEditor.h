#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class DrumSmashEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit DrumSmashEditor (DrumSmashProcessor&);
    ~DrumSmashEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildPresetButtons();
    void buildKnobs();
    void selectPreset (int index);

    DrumSmashProcessor& proc;

    // Preset buttons
    juce::OwnedArray<juce::TextButton> presetButtons;

    // Knob + label helper struct
    struct KnobRow
    {
        std::unique_ptr<juce::Slider>    slider;
        std::unique_ptr<juce::Label>     label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    std::vector<KnobRow> knobs;

    int selectedPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSmashEditor)
};
