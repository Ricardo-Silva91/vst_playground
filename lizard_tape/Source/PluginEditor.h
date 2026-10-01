#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class LizardTapeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LizardTapeAudioProcessorEditor (LizardTapeAudioProcessor&);
    ~LizardTapeAudioProcessorEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    void addKnob (Knob& k, const juce::String& paramID, const juce::String& text);
    LizardTapeAudioProcessor& processor;
    Knob drive, bias, wowDepth, wowRate, flutDepth, flutRate, tone, hiss, mix, output;
    std::array<Knob*, 10> knobs { &drive, &bias, &wowDepth, &wowRate, &flutDepth,
                                  &flutRate, &tone, &hiss, &mix, &output };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LizardTapeAudioProcessorEditor)
};
