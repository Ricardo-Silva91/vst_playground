#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class PitchWobbleEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    PitchWobbleEditor (PitchWobbleProcessor&);
    ~PitchWobbleEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

private:
    PitchWobbleProcessor& proc;

    juce::Font rajdhaniBold;
    juce::Font shareTechMono;
    std::unique_ptr<juce::Drawable> logoDrawable;

    // Ghost sliders — invisible, exist solely for FL Studio automation
    juce::Slider ghostDepth, ghostRate, ghostSmooth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachDepth, attachRate, attachSmooth;

    // Cached normalised values for dirty-check in timer
    float cachedDepth  = -1.0f;
    float cachedRate   = -1.0f;
    float cachedSmooth = -1.0f;

    int   draggingKnob = -1;
    float dragStartY   = 0.0f;
    float dragStartVal = 0.0f;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // Drawing
    void drawChassis   (juce::Graphics&);
    void drawScrews    (juce::Graphics&);
    void drawPlugin    (juce::Graphics&);
    void drawScanLines (juce::Graphics&, juce::Rectangle<float>, float opacity);
    void drawKnob      (juce::Graphics&, float cx, float cy, float norm,
                        const juce::String& label, const juce::String& valueText);

    // Layout
    juce::Point<float> knobCenter  (int index) const;
    int                knobHitTest (juce::Point<float>) const;

    // Param helpers
    float normDepth()  const;
    float normRate()   const;
    float normSmooth() const;
    void  setNorm (int knobIndex, float normVal);
    juce::String formatValue (int knobIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchWobbleEditor)
};