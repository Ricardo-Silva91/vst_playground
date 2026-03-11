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

    void drawChassis   (juce::Graphics&);
    void drawScrews    (juce::Graphics&);
    void drawScanLines (juce::Graphics&, juce::Rectangle<float>, float opacity);
    void drawPlugin    (juce::Graphics&);
    void drawKnob      (juce::Graphics&, float cx, float cy, float norm,
                        const juce::String& label, const juce::String& val);
    void drawPresetBar (juce::Graphics&);

    juce::Point<float> knobCenter   (int idx) const;
    int                knobHitTest  (juce::Point<float>) const;
    int                presetHitTest(juce::Point<float>) const;
    juce::Rectangle<float> presetBtnRect(int i) const;

    float        getNorm     (int knobIdx) const;
    void         setNorm     (int knobIdx, float norm);
    juce::String getValueText(int knobIdx) const;

    DrumSmashProcessor& proc;

    juce::Font rajdhaniBold;
    juce::Font shareTechMono;
    std::unique_ptr<juce::Drawable> logoDrawable;

    float cachedNorm[5] = {};
    int   selectedPreset = 0;

    int   draggingKnob = -1;
    float dragStartY   = 0.f;
    float dragStartVal = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSmashEditor)
};