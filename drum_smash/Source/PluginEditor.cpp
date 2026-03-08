#include "PluginEditor.h"
#include "PluginProcessor.h"

// ── Colour palette ─────────────────────────────────────────────────────────────
static const juce::Colour kBg      { 0xFF1A1A2E };
static const juce::Colour kPanel   { 0xFF16213E };
static const juce::Colour kAccent  { 0xFFE94560 };
static const juce::Colour kText    { 0xFFE0E0E0 };
static const juce::Colour kTextDim { 0xFF888899 };
static const juce::Colour kKnobFill  { 0xFF0F3460 };
static const juce::Colour kKnobTrack { 0xFFE94560 };

// ── DSLookAndFeel ─────────────────────────────────────────────────────────────
// No font calls anywhere — only colour setup and geometry drawing
DSLookAndFeel::DSLookAndFeel()
{
    setColour (juce::Slider::thumbColourId,               kAccent);
    setColour (juce::Slider::rotarySliderFillColourId,    kKnobTrack);
    setColour (juce::Slider::rotarySliderOutlineColourId, kKnobFill);
    setColour (juce::Slider::backgroundColourId,          kKnobFill);
    setColour (juce::Label::textColourId,                 kText);
    setColour (juce::TextButton::buttonColourId,          kPanel);
    setColour (juce::TextButton::buttonOnColourId,        kAccent);
    setColour (juce::TextButton::textColourOffId,         kTextDim);
    setColour (juce::TextButton::textColourOnId,          kText);
}

void DSLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                       int width, int height,
                                       float sliderPos,
                                       float startAngle, float endAngle,
                                       juce::Slider&)
{
    float radius = (float) juce::jmin (width, height) * 0.4f;
    float cx = x + (float) width  * 0.5f;
    float cy = y + (float) height * 0.5f;

    // Track ring
    juce::Path arcBg;
    arcBg.addCentredArc (cx, cy, radius, radius, 0.f, startAngle, endAngle, true);
    g.setColour (kKnobFill.brighter (0.15f));
    g.strokePath (arcBg, juce::PathStrokeType (3.5f));

    // Value arc
    float angle = startAngle + sliderPos * (endAngle - startAngle);
    juce::Path arc;
    arc.addCentredArc (cx, cy, radius, radius, 0.f, startAngle, angle, true);
    g.setColour (kKnobTrack);
    g.strokePath (arc, juce::PathStrokeType (3.5f));

    // Knob body
    g.setColour (kKnobFill);
    g.fillEllipse (cx - radius * 0.7f, cy - radius * 0.7f,
                   radius * 1.4f, radius * 1.4f);

    // Pointer line
    juce::Path ptr;
    float pLen = radius * 0.55f;
    ptr.startNewSubPath (0.f, -pLen * 0.3f);
    ptr.lineTo          (0.f, -pLen);
    g.setColour (kAccent);
    g.strokePath (ptr, juce::PathStrokeType (2.5f),
                  juce::AffineTransform::rotation (angle).translated (cx, cy));

    // Centre dot
    g.setColour (kAccent.withAlpha (0.6f));
    g.fillEllipse (cx - 2.f, cy - 2.f, 4.f, 4.f);
}

// ── Knob definitions ──────────────────────────────────────────────────────────
struct KnobDef { const char* paramId; const char* label; };
static const KnobDef kKnobDefs[] =
{
    { "bitDepth",      "Bit Depth"   },
    { "sampleRateDiv", "SR Divide"   },
    { "drive",         "Drive"       },
    { "outputGain",    "Output Gain" },
    { "noiseAmount",   "Noise"       },
    { "crackleRate",   "Crackle"     },
    { "lpfCutoff",     "LPF"         },
    { "hpfCutoff",     "HPF"         },
    { "compThreshold", "Threshold"   },
    { "compRatio",     "Ratio"       },
    { "compAttack",    "Attack"      },
    { "compRelease",   "Release"     },
    { "compMakeup",    "Makeup"      },
    { "reverbRoom",    "Room"        },
    { "reverbWet",     "Rev Wet"     },
    { "reverbDamping", "Damping"     },
    { "pitchSemitones","Pitch"       },
    { "wowRate",       "Wow Rate"    },
    { "wowDepth",      "Wow Depth"   },
    { "stereoWidth",   "Width"       },
    { "transientBoost","Transient"   },
};
static constexpr int kNumKnobs = (int)(sizeof(kKnobDefs) / sizeof(kKnobDefs[0]));

// ── Constructor ───────────────────────────────────────────────────────────────
DrumSmashEditor::DrumSmashEditor (DrumSmashProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (880, 620);

    // Preset buttons
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto* btn = presetButtons.add (std::make_unique<juce::TextButton> (kPresets[i].name));
        btn->setClickingTogglesState (true);
        btn->setRadioGroupId (1);
        btn->onClick = [this, i] {
            selectedPreset = i;
            proc.setCurrentProgram (i);
        };
        addAndMakeVisible (btn);
    }

    // Knobs — no setFont calls
    for (int i = 0; i < kNumKnobs; ++i)
    {
        KnobRow row;
        row.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag,
                                                      juce::Slider::NoTextBox);
        row.label  = std::make_unique<juce::Label>  ("", kKnobDefs[i].label);
        row.label->setJustificationType (juce::Justification::centred);
        row.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (proc.apvts, kKnobDefs[i].paramId, *row.slider);
        addAndMakeVisible (*row.slider);
        addAndMakeVisible (*row.label);
        knobs.push_back (std::move (row));
    }

    selectedPreset = proc.getCurrentPresetIndex();
    if (selectedPreset >= 0 && selectedPreset < presetButtons.size())
        presetButtons[selectedPreset]->setToggleState (true, juce::dontSendNotification);

    startTimerHz (10);
}

DrumSmashEditor::~DrumSmashEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void DrumSmashEditor::timerCallback()
{
    int curr = proc.getCurrentPresetIndex();
    if (curr != selectedPreset)
    {
        selectedPreset = curr;
        for (int i = 0; i < presetButtons.size(); ++i)
            presetButtons[i]->setToggleState (i == curr, juce::dontSendNotification);
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────
void DrumSmashEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (48); // title bar

    // Preset column
    auto presetCol = bounds.removeFromLeft (160);
    presetCol.removeFromTop (24);
    for (auto* btn : presetButtons)
        btn->setBounds (presetCol.removeFromTop (48).reduced (8, 2));

    // Knob grid
    bounds.removeFromLeft (8);
    bounds.removeFromTop  (24);

    const int cols = 7;
    const int rows = 3;
    int kW = bounds.getWidth()  / cols;
    int kH = bounds.getHeight() / rows;

    for (int i = 0; i < kNumKnobs; ++i)
    {
        int col = i / rows;
        int row = i % rows;
        juce::Rectangle<int> cell (bounds.getX() + col * kW,
                                    bounds.getY() + row * kH,
                                    kW, kH);
        knobs[(size_t)i].label ->setBounds (cell.removeFromBottom (16));
        knobs[(size_t)i].slider->setBounds (cell.reduced (4));
    }
}

// ── Paint — zero explicit font calls ─────────────────────────────────────────
void DrumSmashEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (kBg);

    // Title bar
    g.setColour (kPanel);
    g.fillRect (0, 0, getWidth(), 48);
    g.setColour (kAccent);
    g.fillRect (0, 46, getWidth(), 2);

    // Text drawn with JUCE default font — no g.setFont() calls at all
    g.setColour (kText);
    g.drawText ("DRUM SMASH", 170, 4, getWidth() - 180, 22,
                juce::Justification::centredLeft, false);

    g.setColour (kTextDim);
    g.drawText ("Drum Effects Processor", 170, 26, 300, 16,
                juce::Justification::centredLeft, false);

    // Preset panel background
    g.setColour (kPanel);
    g.fillRect (0, 48, 160, getHeight() - 48);
    g.setColour (kAccent.withAlpha (0.3f));
    g.drawLine (160.f, 48.f, 160.f, (float) getHeight(), 1.f);
    g.setColour (kAccent);
    g.drawText ("PRESETS", 0, 50, 160, 18, juce::Justification::centred, false);

    // Section colour bands + labels
    const int   cols   = 7;
    const float kStart = 168.f;
    const float kW     = (getWidth() - kStart) / (float) cols;

    static const char* sectionLabels[] = {
        "CRUSHER", "SATURATION", "CHARACTER", "FILTERS",
        "COMPRESSOR", "REVERB", "MODULATION"
    };
    static const juce::Colour sectionColours[] = {
        juce::Colour (0xFF9B59B6), juce::Colour (0xFFE74C3C),
        juce::Colour (0xFFE67E22), juce::Colour (0xFF27AE60),
        juce::Colour (0xFF2980B9), juce::Colour (0xFF16A085),
        juce::Colour (0xFFD35400)
    };

    for (int c = 0; c < cols; ++c)
    {
        float x = kStart + c * kW;
        g.setColour (sectionColours[c].withAlpha (0.12f));
        g.fillRect  (juce::Rectangle<float> (x + 2, 72, kW - 4, (float) getHeight() - 80));
        g.setColour (sectionColours[c]);
        g.fillRect  (juce::Rectangle<float> (x + 2, 72, kW - 4, 3));
        g.setColour (sectionColours[c].brighter (0.4f));
        g.drawText  (sectionLabels[c], (int) x, 54, (int) kW, 16,
                     juce::Justification::centred, false);
    }
}

// ── createEditor ─────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new DrumSmashEditor (*this);
}