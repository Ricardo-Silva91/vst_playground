#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

struct KnobInfo
{
    const char* paramId;
    const char* label;
};

static const KnobInfo kTimingKnobs[] =
{
    { "swing",    "SWING"   },
    { "humanize", "HUMANIZ" },
    { "drag",     "DRAG"    },
};

static const KnobInfo kCharKnobs[] =
{
    { "sensitivity", "SENS"    },
    { "velocityvar", "VEL VAR" },
    { "wetmix",      "WET MIX" },
};

static constexpr int kKnobsPerGroup = 3;
static constexpr int kTotalKnobs    = 6;

class BreakScientistEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit BreakScientistEditor (BreakScientistProcessor&);
    ~BreakScientistEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override {}

private:
    void timerCallback() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void drawChassis (juce::Graphics&);
    void drawGroup   (juce::Graphics&, juce::Rectangle<float> area,
                      const char* title, const KnobInfo* knobs, int startIdx);
    void drawKnob    (juce::Graphics&, float cx, float cy, float norm,
                      const juce::String& label, const juce::String& val);
    void drawLogo    (juce::Graphics&);

    juce::Point<float> knobCenter  (int globalIndex) const;
    int                knobHitTest (juce::Point<float>) const;

    float        getNorm     (int knobIndex) const;
    void         setNorm     (int knobIndex, float norm);
    juce::String getValueText(int knobIndex) const;

    const KnobInfo& knobInfo (int globalIndex) const
    {
        if (globalIndex < kKnobsPerGroup) return kTimingKnobs[globalIndex];
        return kCharKnobs[globalIndex - kKnobsPerGroup];
    }

    BreakScientistProcessor& proc;

    juce::Font rajdhaniBold;
    juce::Font shareTechMono;
    std::unique_ptr<juce::Drawable> logoDrawable;

    float cachedNorm[kTotalKnobs] = {};

    int   draggingKnob = -1;
    float dragStartY   = 0.f;
    float dragStartVal = 0.f;

    static constexpr float W = 560.f;
    static constexpr float H = 300.f;

    const juce::Colour cAccent { 0xff9b59f5 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreakScientistEditor)
};
