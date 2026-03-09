#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

struct KnobState
{
    juce::String paramId;
    juce::String label;
    juce::String unit;
    float        value      = 0.0f;
    float        dragStart  = 0.0f;
    int          dragStartY = 0;
    bool         isDragging = false;
};

class PitchWobbleEditor : public juce::AudioProcessorEditor,
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PitchWobbleEditor (PitchWobbleProcessor&);
    ~PitchWobbleEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    void mouseDown      (const juce::MouseEvent&) override;
    void mouseDrag      (const juce::MouseEvent&) override;
    void mouseUp        (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails&) override;

    void parameterChanged (const juce::String& paramId, float) override;

private:
    PitchWobbleProcessor& proc;

    static constexpr int   W      = 480;
    static constexpr int   H      = 280;
    static constexpr float KNOB_D = 80.0f;

    const juce::Colour accent  { 0xffE8820A };
    const juce::Colour chassis { 0xff1e1e2e };
    const juce::Colour silk    { 0xffA09890 };
    const juce::Colour textDim { 0xff7A746C };

    std::array<KnobState, 3> knobs;

    // Invisible sliders — exist solely so FL Studio can find parameters
    // for automation. Alpha = 0, mouse interception disabled.
    juce::Slider ghostDepth, ghostRate, ghostSmooth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachDepth, attachRate, attachSmooth;

    juce::Rectangle<float> knobBounds (int index) const;
    int  hitTestKnob (juce::Point<int>) const;
    void setNorm     (int index, float norm);

    void drawBackground (juce::Graphics&) const;
    void drawHeader     (juce::Graphics&) const;
    void drawKnob       (juce::Graphics&, int index) const;
    void drawScrew      (juce::Graphics&, float x, float y) const;

    juce::String formatValue (int index) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchWobbleEditor)
};