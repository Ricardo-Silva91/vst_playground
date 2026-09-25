#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

static constexpr float kW = 700.f;
static constexpr float kH = 600.f;

static constexpr float kKnobR = 24.f;
static constexpr float kArcR  = kKnobR + 6.f;

static const juce::Colour kAccent { 0xff9ccc65 };

// One strip per module, top to bottom in signal order.
static constexpr int   kNumRows    = 4;
static constexpr float kRowY[kNumRows] = { 150.f, 266.f, 382.f, 498.f };
static constexpr float kPanelH     = 104.f;
static constexpr float kPanelX     = 24.f;
static constexpr float kKnobAreaX  = 170.f;
static constexpr float kKnobAreaR  = 664.f;
static constexpr float kKnobStep   = 98.f;

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
    { "dustRate",    "RATE",    0, 0, 5 },
    { "dustBits",    "BITS",    0, 1, 5 },
    { "dustLowpass", "LOW-PASS",0, 2, 5 },
    { "dustDrive",   "DRIVE",   0, 3, 5 },
    { "dustMix",     "MIX",     0, 4, 5 },

    { "chewRate",    "RATE",    1, 0, 3 },
    { "chewDepth",   "DEPTH",   1, 1, 3 },
    { "chewSmooth",  "SMOOTH",  1, 2, 3 },

    { "murkRoom",    "ROOM",    2, 0, 3 },
    { "murkDamp",    "DAMP",    2, 1, 3 },
    { "murkMix",     "MIX",     2, 2, 3 },

    { "vinylAge",    "AGE",     3, 0, 3 },
    { "vinylAmount", "AMOUNT",  3, 1, 3 },
    { "vinylSeed",   "SEED",    3, 2, 3 },
};
static constexpr int kNumKnobs = (int)(sizeof(kKnobDefs) / sizeof(kKnobDefs[0]));

class LizardSuiteEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit LizardSuiteEditor (LizardSuiteProcessor&);
    ~LizardSuiteEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override {}

private:
    void timerCallback() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void drawHeader        (juce::Graphics&);
    void drawModuleStrips  (juce::Graphics&);
    void drawAllKnobs      (juce::Graphics&);

    juce::Point<float> knobCenter  (int index) const;
    int                knobHitTest (juce::Point<float>) const;

    float        getNorm      (int knobIndex) const;
    void         setNorm      (int knobIndex, float norm);
    juce::String getValueText (int knobIndex) const;

    LizardSuiteProcessor& proc;

    juce::Font rajdhaniBold;
    juce::Font shareTechMono;
    std::unique_ptr<juce::Drawable> logoDrawable;

    float cachedNorm[kNumKnobs] = {};

    int   draggingKnob = -1;
    float dragStartY   = 0.f;
    float dragStartVal = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LizardSuiteEditor)
};
