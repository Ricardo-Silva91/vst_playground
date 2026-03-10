#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class DrumSmashEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit DrumSmashEditor (DrumSmashProcessor&);
    ~DrumSmashEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override {}

private:
    void timerCallback() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&,
                           const juce::MouseWheelDetails&) override;

    // ── Drawing helpers ───────────────────────────────────────────────────
    void drawChassis   (juce::Graphics&);
    void drawScrews    (juce::Graphics&);
    void drawScanLines (juce::Graphics&, juce::Rectangle<float>, float opacity);
    void drawFacePanel (juce::Graphics&);
    void drawContent   (juce::Graphics&);
    void drawFaceKnob  (juce::Graphics&, int idx);
    void drawSection   (juce::Graphics&, int sectionIdx, juce::Rectangle<float> area);
    void drawSliderRow (juce::Graphics&, int paramIdx, juce::Rectangle<float> row,
                        bool shaded);
    void drawPresetBar (juce::Graphics&);
    void drawLogoAndBadge (juce::Graphics&);

    // ── Layout ────────────────────────────────────────────────────────────
    juce::Rectangle<float> facePanel()    const;
    juce::Rectangle<float> contentPanel() const;
    juce::Rectangle<float> presetBarRect() const;
    juce::Rectangle<float> sectionsRect() const;

    juce::Point<float> faceKnobCenter (int idx) const;
    int  faceKnobHitTest (juce::Point<float>) const;
    int  sliderHitTest   (juce::Point<float>) const;
    int  presetHitTest   (juce::Point<float>) const;
    juce::Rectangle<float> sliderRowRect (int paramIdx) const;
    juce::Rectangle<float> sliderThumb   (int paramIdx) const;
    juce::Rectangle<float> presetRect    (int i) const;

    // ── Param helpers ─────────────────────────────────────────────────────
    float getNorm (int paramIdx) const;
    void  setNorm (int paramIdx, float norm);
    juce::String getValueText (int paramIdx) const;

    DrumSmashProcessor& proc;

    juce::Font rajdhaniBold;
    juce::Font shareTechMono;
    std::unique_ptr<juce::Drawable> logoDrawable;

    // Cached normalised values for repaint-on-change
    float cachedNorm[21] = {};

    // Drag state — face knobs
    int   dragFaceKnob   = -1;
    float dragStartY     = 0.f;
    float dragStartVal   = 0.f;

    // Drag state — content sliders
    int   dragSlider     = -1;
    float dragSliderStartX   = 0.f;
    float dragSliderStartVal = 0.f;

    int selectedPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSmashEditor)
};