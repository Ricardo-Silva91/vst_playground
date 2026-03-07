#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PitchWobbleEditor : public juce::AudioProcessorEditor
{
public:
    explicit PitchWobbleEditor(PitchWobbleProcessor&);
    ~PitchWobbleEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PitchWobbleProcessor& processorRef;

    juce::Slider depthSlider, rateSlider, smoothSlider;
    juce::Label  depthLabel,  rateLabel,  smoothLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach, rateAttach, smoothAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchWobbleEditor)
};
