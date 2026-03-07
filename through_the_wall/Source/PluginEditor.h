#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class WallKnob : public juce::Slider
{
public:
    WallKnob()
    {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }
};

class ThroughTheWallAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ThroughTheWallAudioProcessorEditor(ThroughTheWallAudioProcessor&);
    ~ThroughTheWallAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ThroughTheWallAudioProcessor& audioProcessor;

    WallKnob thicknessKnob, bleedKnob, rattleKnob, distanceKnob;
    juce::Label thicknessLabel, bleedLabel, rattleLabel, distanceLabel;
    juce::Label thicknessValue, bleedValue, rattleValue, distanceValue;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        thicknessAttach, bleedAttach, rattleAttach, distanceAttach;

    // Crack path for paint decoration
    juce::Path crackPath1, crackPath2;
    void generateCracks();

    juce::Colour bgColor     { 0xff2a2318 };
    juce::Colour wallColor   { 0xff3d3328 };
    juce::Colour plasterColor{ 0xff5c4f3a };
    juce::Colour accentColor { 0xffd4a85a };
    juce::Colour dimText     { 0xff8a7a62 };
    juce::Colour brightText  { 0xffe8d5b0 };

    void setupLabel(juce::Label& label, const juce::String& text);
    void setupValueLabel(juce::Label& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThroughTheWallAudioProcessorEditor)
};
