#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ThroughTheWallAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit ThroughTheWallAudioProcessorEditor(ThroughTheWallAudioProcessor&);
    ~ThroughTheWallAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    ThroughTheWallAudioProcessor& audioProcessor;

    // ── Colours (spec) ────────────────────────────────────────────────────
    const juce::Colour kChassis { 0xff141414 };
    const juce::Colour kPanel   { 0xff1c1c1c };
    const juce::Colour kMetal   { 0xff2e2e2e };
    const juce::Colour kAccent  { 0xff4ecf6a };
    const juce::Colour kAmber   { 0xffe8820a };
    const juce::Colour kSilk    { 0xffa09890 };
    const juce::Colour kTextDim { 0xff7a746c };
    const juce::Colour kText    { 0xffd4cfc8 };

    static constexpr int kFaceW   = 160;
    static constexpr int kEditorW = 540;
    static constexpr int kEditorH = 300;

    // ── Logo ──────────────────────────────────────────────────────────────
    juce::Image logoImage;

    // ── Knobs (face panel 2x2) ────────────────────────────────────────────
    juce::Slider thicknessKnob, bleedKnob, rattleKnob, distanceKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        thicknessAttach, bleedAttach, rattleAttach, distanceAttach;

    // ── Sliders (content panel rows) ──────────────────────────────────────
    juce::Slider thicknessSlider, bleedSlider, rattleSlider, distanceSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        thicknessSliderAttach, bleedSliderAttach, rattleSliderAttach, distanceSliderAttach;

    // ── Paint helpers ─────────────────────────────────────────────────────
    void paintChassis     (juce::Graphics&);
    void paintFacePanel   (juce::Graphics&);
    void paintKnob        (juce::Graphics&, juce::Rectangle<float> bounds,
                           float normVal, const juce::String& label);
    void paintContentPanel(juce::Graphics&);
    void paintParamRow    (juce::Graphics&, juce::Rectangle<int> row,
                           const juce::String& label, float normVal, bool shaded);
    void paintVUStrip     (juce::Graphics&);
    void paintScrew       (juce::Graphics&, float cx, float cy);
    void paintLogo        (juce::Graphics&);

    float getNormValue(const juce::String& paramId) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThroughTheWallAudioProcessorEditor)
};