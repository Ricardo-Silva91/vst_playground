#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr float kW = 700.f;
static constexpr float kH = 530.f;

// Knob geometry
static constexpr float kKnobR = 28.f;
static constexpr float kArcR  = kKnobR + 7.f;

// Accent colour — muted seafoam
static const juce::Colour kAccent { 0xff4ecdc4 };

// Row Y centres — extra 20px margin after each section so the value text
// below each knob doesn't crowd the next section's header label.
static constexpr float kRow1Y = 185.f;
static constexpr float kRow2Y = 338.f;  // was 318
static constexpr float kRow3Y = 462.f;  // was 432

// Knob descriptor
struct KnobDef
{
    const char* paramId;
    const char* label;
    int         row;       // 1, 2, or 3
    int         posInRow;  // 0-based
    int         rowCount;  // total knobs in this row
};

static const KnobDef kKnobDefs[] =
{
    // Row 1 — pitch / voice shape (4 knobs)
    { "upSemitones",   "UP SEMI",   1, 0, 4 },
    { "downSemitones", "DOWN SEMI", 1, 1, 4 },
    { "voices",        "VOICES",    1, 2, 4 },
    { "detune",        "DETUNE",    1, 3, 4 },

    // Row 2 — voice levels (3 knobs)
    { "dryLevel",      "DRY",       2, 0, 3 },
    { "upLevel",       "UP LVL",    2, 1, 3 },
    { "downLevel",     "DOWN LVL",  2, 2, 3 },

    // Row 3 — distortion + output (4 knobs)
    { "saturation",    "SAT",       3, 0, 4 },
    { "crush",         "CRUSH",     3, 1, 4 },
    { "distMix",       "DIST MIX",  3, 2, 4 },
    { "masterOut",     "OUTPUT",    3, 3, 4 },
};
static constexpr int kNumKnobs = (int)(sizeof(kKnobDefs) / sizeof(kKnobDefs[0]));

// ── Editor ────────────────────────────────────────────────────────────────────
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

    void drawChassis         (juce::Graphics&);
    void drawScrews          (juce::Graphics&);
    void drawScanLines       (juce::Graphics&);
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