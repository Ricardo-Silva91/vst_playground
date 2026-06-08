#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

static constexpr float kW = 700.f;
static constexpr float kH = 530.f;

static constexpr float kKnobR = 28.f;
static constexpr float kArcR  = kKnobR + 7.f;

static const juce::Colour kAccent { 0xff4ecdc4 };

static constexpr float kRow1Y = 185.f;
static constexpr float kRow2Y = 338.f;
static constexpr float kRow3Y = 462.f;

struct KnobDef
{
    const char* paramId;
    const char* label;
    int         row;
    int         posInRow;
    int         rowCount;
};

static const KnobDef kKnobDefs[] =
{
    { "upSemitones",   "UP SEMI",   1, 0, 4 },
    { "downSemitones", "DOWN SEMI", 1, 1, 4 },
    { "voices",        "VOICES",    1, 2, 4 },
    { "detune",        "DETUNE",    1, 3, 4 },

    { "dryLevel",      "DRY",       2, 0, 3 },
    { "upLevel",       "UP LVL",    2, 1, 3 },
    { "downLevel",     "DOWN LVL",  2, 2, 3 },

    { "saturation",    "SAT",       3, 0, 4 },
    { "crush",         "CRUSH",     3, 1, 4 },
    { "distMix",       "DIST MIX",  3, 2, 4 },
    { "masterOut",     "OUTPUT",    3, 3, 4 },
};
static constexpr int kNumKnobs = (int)(sizeof(kKnobDefs) / sizeof(kKnobDefs[0]));

class ChoirBoxEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit ChoirBoxEditor (ChoirBoxProcessor&);
    ~ChoirBoxEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override {}

private:
    void timerCallback() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void drawHeader          (juce::Graphics&);
    void drawSectionPanels   (juce::Graphics&);
    void drawSectionLabels   (juce::Graphics&);
    void drawAllKnobs        (juce::Graphics&);
    void drawKnob            (juce::Graphics&, float cx, float cy, float norm,
                              const juce::String& label, const juce::String& val);

    juce::Point<float> knobCenter  (int index) const;
    int                knobHitTest (juce::Point<float>) const;

    float        getNorm      (int knobIndex) const;
    void         setNorm      (int knobIndex, float norm);
    juce::String getValueText (int knobIndex) const;

    ChoirBoxProcessor& proc;

    juce::Font rajdhaniBold;
    juce::Font shareTechMono;
    std::unique_ptr<juce::Drawable> logoDrawable;

    float cachedNorm[kNumKnobs] = {};

    int   draggingKnob = -1;
    float dragStartY   = 0.f;
    float dragStartVal = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirBoxEditor)
};
