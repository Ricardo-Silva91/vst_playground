#include "PluginEditor.h"
#include "PluginProcessor.h"

// ── Colour palette ─────────────────────────────────────────────────────────────
static const juce::Colour kBg         { 0xFF1A1A2E };
static const juce::Colour kPanel      { 0xFF16213E };
static const juce::Colour kAccent     { 0xFFE94560 };
static const juce::Colour kAccentDim  { 0xFF9B2335 };
static const juce::Colour kText       { 0xFFE0E0E0 };
static const juce::Colour kTextDim    { 0xFF888899 };
static const juce::Colour kKnobFill   { 0xFF0F3460 };
static const juce::Colour kKnobTrack  { 0xFFE94560 };

// ── Custom LookAndFeel ─────────────────────────────────────────────────────────
class DSLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DSLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId,             kAccent);
        setColour (juce::Slider::rotarySliderFillColourId,  kKnobTrack);
        setColour (juce::Slider::rotarySliderOutlineColourId, kKnobFill);
        setColour (juce::Slider::backgroundColourId,        kKnobFill);
        setColour (juce::Label::textColourId,               kText);
        setColour (juce::TextButton::buttonColourId,        kPanel);
        setColour (juce::TextButton::buttonOnColourId,      kAccent);
        setColour (juce::TextButton::textColourOffId,       kTextDim);
        setColour (juce::TextButton::textColourOnId,        kText);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        float radius = (float)juce::jmin (width, height) * 0.4f;
        float cx = x + (float)width  * 0.5f;
        float cy = y + (float)height * 0.5f;

        // Track background
        juce::Path arcBg;
        arcBg.addCentredArc (cx, cy, radius, radius, 0.f, startAngle, endAngle, true);
        g.setColour (kKnobFill.brighter (0.1f));
        g.strokePath (arcBg, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // Value arc
        float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path arc;
        arc.addCentredArc (cx, cy, radius, radius, 0.f, startAngle, angle, true);
        g.setColour (kKnobTrack);
        g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Body
        g.setColour (kKnobFill);
        g.fillEllipse (cx - radius * 0.7f, cy - radius * 0.7f,
                       radius * 1.4f, radius * 1.4f);

        // Pointer
        juce::Path pointer;
        float pointerLen = radius * 0.55f;
        pointer.startNewSubPath (0, -pointerLen * 0.3f);
        pointer.lineTo (0, -pointerLen);
        g.setColour (kAccent);
        g.strokePath (pointer,
                      juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (angle).translated (cx, cy));

        // Centre dot
        g.setColour (kAccent.withAlpha (0.6f));
        g.fillEllipse (cx - 2.f, cy - 2.f, 4.f, 4.f);
    }
};

static DSLookAndFeel gLAF;

// ── Constructor ───────────────────────────────────────────────────────────────
DrumSmashEditor::DrumSmashEditor (DrumSmashProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&gLAF);
    setSize (880, 620);

    buildPresetButtons();
    buildKnobs();

    selectedPreset = proc.getCurrentPresetIndex();
    if (selectedPreset >= 0 && selectedPreset < (int)presetButtons.size())
        presetButtons[selectedPreset]->setToggleState (true, juce::dontSendNotification);

    startTimerHz (10);
}

DrumSmashEditor::~DrumSmashEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

// ── Build UI ──────────────────────────────────────────────────────────────────
void DrumSmashEditor::buildPresetButtons()
{
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto* btn = presetButtons.add (std::make_unique<juce::TextButton> (kPresets[i].name));
        btn->setClickingTogglesState (true);
        btn->setRadioGroupId (1);
        btn->onClick = [this, i] { selectPreset (i); };
        addAndMakeVisible (btn);
    }
}

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
static constexpr int kNumKnobs = (int)(sizeof(kKnobDefs)/sizeof(kKnobDefs[0]));

void DrumSmashEditor::buildKnobs()
{
    knobs.clear();
    for (int i = 0; i < kNumKnobs; ++i)
    {
        KnobRow row;
        row.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag,
                                                      juce::Slider::NoTextBox);
        row.label  = std::make_unique<juce::Label>  ("", kKnobDefs[i].label);
        row.label->setFont (juce::FontOptions (10.f));
        row.label->setJustificationType (juce::Justification::centred);
        row.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (proc.apvts, kKnobDefs[i].paramId, *row.slider);

        addAndMakeVisible (*row.slider);
        addAndMakeVisible (*row.label);
        knobs.push_back (std::move (row));
    }
}

// ── selectPreset ──────────────────────────────────────────────────────────────
void DrumSmashEditor::selectPreset (int index)
{
    selectedPreset = index;
    proc.setCurrentProgram (index);
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

    // Title bar
    bounds.removeFromTop (48);

    // Preset strip (left column)
    auto presetCol = bounds.removeFromLeft (160);
    presetCol.removeFromTop (8);
    int btnH = 50;
    int gap   = 4;
    for (auto* btn : presetButtons)
    {
        btn->setBounds (presetCol.removeFromTop (btnH).reduced (8, 2));
        presetCol.removeFromTop (gap);
    }

    // Knob grid (rest of space)
    bounds.removeFromLeft (8);
    bounds.removeFromTop (8);

    // Section labels areas
    // Groups: Crusher(2), Saturation(2), Character(2), Filters(2),
    //         Comp(5), Reverb(3), Modulation(3), Spatial(2)
    // = 21 knobs in 7 rows of 3 (fill 3 columns)
    const int cols    = 7;
    const int rows    = 3;
    int kW = bounds.getWidth()  / cols;
    int kH = bounds.getHeight() / rows;

    for (int i = 0; i < kNumKnobs; ++i)
    {
        int col = i / rows;
        int row = i % rows;
        juce::Rectangle<int> cell (bounds.getX() + col * kW,
                                    bounds.getY() + row * kH,
                                    kW, kH);
        auto labelR  = cell.removeFromBottom (18);
        knobs[(size_t)i].slider->setBounds (cell.reduced (6));
        knobs[(size_t)i].label ->setBounds (labelR);
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void DrumSmashEditor::paint (juce::Graphics& g)
{
    // Background
    g.fillAll (kBg);

    // Title bar
    g.setColour (kPanel);
    g.fillRect (0, 0, getWidth(), 48);

    g.setColour (kAccent);
    g.fillRect (0, 46, getWidth(), 2);

    // Title
    g.setFont (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), 24.f,
                                   juce::Font::bold));
    g.setColour (kText);
    g.drawText ("DRUM SMASH", 170, 0, getWidth() - 170, 48, juce::Justification::centredLeft);

    // Subtitle
    g.setFont (juce::FontOptions (11.f));
    g.setColour (kTextDim);
    g.drawText ("Drum Effects Processor", 170, 28, 300, 18, juce::Justification::left);

    // Preset panel background
    g.setColour (kPanel);
    g.fillRect (0, 48, 160, getHeight() - 48);
    g.setColour (kAccent.withAlpha (0.3f));
    g.drawLine (160.f, 48.f, 160.f, (float)getHeight(), 1.f);

    // Section dividers in knob area
    const int cols = 7;
    float kStart = 168.f;
    float kW     = (getWidth() - (int)kStart) / (float)cols;
    static const char* sectionLabels[] = {
        "CRUSHER", "SATURATION", "CHARACTER", "FILTERS",
        "COMPRESSOR", "REVERB", "MODULATION"
    };
    static const juce::Colour sectionColours[] = {
        juce::Colour(0xFF9B59B6), juce::Colour(0xFFE74C3C),
        juce::Colour(0xFFE67E22), juce::Colour(0xFF27AE60),
        juce::Colour(0xFF2980B9), juce::Colour(0xFF16A085),
        juce::Colour(0xFFD35400)
    };

    for (int c = 0; c < cols; ++c)
    {
        float x = kStart + c * kW;
        g.setColour (sectionColours[c].withAlpha (0.15f));
        g.fillRect (juce::Rectangle<float> (x + 2, 56, kW - 4, (float)getHeight() - 64));
        g.setColour (sectionColours[c]);
        g.fillRect (juce::Rectangle<float> (x + 2, 56, kW - 4, 3));
        g.setFont (juce::FontOptions (8.5f));
        g.setColour (sectionColours[c].brighter (0.3f));
        g.drawText (sectionLabels[c], (int)x, 60, (int)kW, 14, juce::Justification::centred);
    }

    // "PRESETS" label at top of preset column
    g.setFont (juce::FontOptions (10.f));
    g.setColour (kAccent);
    g.drawText ("PRESETS", 0, 50, 160, 20, juce::Justification::centred);
}

// ── createEditor ─────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new DrumSmashEditor (*this);
}
