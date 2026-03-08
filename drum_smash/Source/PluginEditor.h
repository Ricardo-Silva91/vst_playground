#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class DSLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DSLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

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

    DSLookAndFeel laf;

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