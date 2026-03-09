#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// All drawing is done in paint() using only g.fillRect / g.drawEllipse / g.drawText
// and Path objects. No juce::Slider, juce::Label, or juce::TextButton anywhere.
// This avoids the FL Studio / JUCE 8 / Apple Silicon font-cache crash.
//==============================================================================

struct KnobState
{
    juce::String  paramId;
    juce::String  label;
    juce::String  unit;
    float         value      = 0.0f;   // normalised 0..1
    float         dragStart  = 0.0f;
    int           dragStartY = 0;
    bool          isDragging = false;
};

class PitchWobbleEditor : public juce::AudioProcessorEditor,
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PitchWobbleEditor (PitchWobbleProcessor&);
    ~PitchWobbleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void parameterChanged (const juce::String& paramId, float newValue) override;

private:
    PitchWobbleProcessor& proc;

    // ── layout constants ─────────────────────────────────────────────────────
    static constexpr int   W            = 480;
    static constexpr int   H            = 280;
    static constexpr int   FACE_W       = 160;
    static constexpr int   KNOB_D       = 44;
    static constexpr int   KNOB_Y       = 170;   // centre Y of knob row
    static constexpr int   KNOB_SPACING = 60;
    static constexpr float SCREW_D      = 12.0f;
    static constexpr float SCREW_INSET  = 6.0f;

    // ── colours ───────────────────────────────────────────────────────────────
    const juce::Colour accent   { 0xffE8820A };
    const juce::Colour chassis  { 0xff141414 };
    const juce::Colour panel    { 0xff1C1C1C };
    const juce::Colour metal    { 0xff2E2E2E };
    const juce::Colour silk     { 0xffA09890 };
    const juce::Colour textDim  { 0xff7A746C };

    // ── knob state ────────────────────────────────────────────────────────────
    std::array<KnobState, 3> knobs;

    // ── helpers ───────────────────────────────────────────────────────────────
    juce::Rectangle<float> knobBounds (int index) const;
    int  hitTestKnob (juce::Point<int> pos) const;
    void setKnobNormalisedValue (int index, float normVal);
    float getNormalisedValue (int index) const;

    // drawing sub-routines
    void drawChassis       (juce::Graphics&) const;
    void drawScrews        (juce::Graphics&) const;
    void drawFacePanel     (juce::Graphics&) const;
    void drawKnob          (juce::Graphics&, int index) const;
    void drawRightPanel    (juce::Graphics&) const;
    void drawVuStrip       (juce::Graphics&) const;
    void drawParamRow      (juce::Graphics&, int index,
                            juce::Rectangle<float> row, bool alternate) const;
    void drawScrew         (juce::Graphics&, float x, float y) const;
    void drawSineDecoration(juce::Graphics&) const;

    juce::String formatValue (int index) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchWobbleEditor)
};