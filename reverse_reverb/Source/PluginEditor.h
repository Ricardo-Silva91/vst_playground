#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
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

    juce::Font  rajdhaniBold;
    juce::Font  shareTechMono;
    juce::Image logoImage;

    float roomVal   = 0.0f;
    float wetVal    = 0.0f;
    float windowVal = 0.0f;

    int   draggingKnob = -1;
    float dragStartY   = 0.0f;
    float dragStartVal = 0.0f;

    void mouseDown       (const juce::MouseEvent&) override;
    void mouseDrag       (const juce::MouseEvent&) override;
    void mouseUp         (const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void drawChassis  (juce::Graphics&);
    void drawScrews   (juce::Graphics&);
    void drawPlugin   (juce::Graphics&);
    void drawKnob     (juce::Graphics&, float cx, float cy, float value,
                       const juce::String& label, const juce::String& valueText);
    void drawScanLines(juce::Graphics&, juce::Rectangle<float> area, float opacity);

    juce::Point<float> knobCenter(int index) const;
    int                knobHitTest(juce::Point<float>) const;

    float normRoom()   const;
    float normWet()    const;
    float normWindow() const;
    void  setNorm(int knobIndex, float normVal);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseReverbAudioProcessorEditor)
};