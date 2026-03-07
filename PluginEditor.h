#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ReverseReverbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    ReverseReverbAudioProcessorEditor(ReverseReverbAudioProcessor&);
    ~ReverseReverbAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ReverseReverbAudioProcessor& audioProcessor;

    // Knobs
    juce::Slider roomSizeSlider;
    juce::Slider wetMixSlider;
    juce::Slider windowSizeSlider;

    // Labels
    juce::Label roomSizeLabel;
    juce::Label wetMixLabel;
    juce::Label windowSizeLabel;
    juce::Label titleLabel;

    // Attachments keep knobs synced with AudioParameters automatically
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> roomSizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> windowSizeAttachment;

    void setupSlider(juce::Slider& slider, juce::Label& label,
                     const juce::String& labelText, juce::AudioParameterFloat* param);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseReverbAudioProcessorEditor)
};
