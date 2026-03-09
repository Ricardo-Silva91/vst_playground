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

    // --- Fonts (loaded from embedded binary data) ---
    juce::Font rajdhaniBold;
    juce::Font shareTechMono;

    // --- Knob values (0.0 - 1.0 normalised) ---
    float roomVal   = 0.0f;
    float wetVal    = 0.0f;
    float windowVal = 0.0f;

    // --- Drag state for each knob (0=room, 1=wet, 2=window) ---
    int   draggingKnob  = -1;
    float dragStartY    = 0.0f;
    float dragStartVal  = 0.0f;

    // --- Mouse handling ---
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseDrag  (const juce::MouseEvent&) override;
    void mouseUp    (const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    // --- Drawing helpers ---
    void drawChassis      (juce::Graphics&);
    void drawScrews       (juce::Graphics&);
    void drawLeftPanel    (juce::Graphics&);
    void drawKnob         (juce::Graphics&, float cx, float cy, float value,
                           const juce::String& label);
    void drawRightPanel   (juce::Graphics&);
    void drawSliderRow    (juce::Graphics&, juce::Rectangle<float> row,
                           const juce::String& label, float value,
                           const juce::String& valueText, bool odd);
    void drawVUStrip      (juce::Graphics&);
    void drawScanLines    (juce::Graphics&, juce::Rectangle<float> area, float opacity);

    // --- Layout helpers ---
    juce::Rectangle<float> leftPanel()  const;
    juce::Rectangle<float> rightPanel() const;
    juce::Point<float>     knobCenter(int index) const;
    juce::Rectangle<float> sliderRow(int index)  const;
    int  knobHitTest(juce::Point<float> pos)     const;

    // --- Param accessors ---
    float normRoom()   const;
    float normWet()    const;
    float normWindow() const;
    void  setNorm(int knobIndex, float normVal);

    static constexpr float knobRadius   = 22.0f;
    static constexpr float knobSpacing  = 60.0f;
    static constexpr int   numKnobs     = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseReverbAudioProcessorEditor)
};