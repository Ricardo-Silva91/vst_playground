#include "PluginEditor.h"
#include <BinaryData.h>

// ── Colours (drum_smash spec) ─────────────────────────────────────────────────
static const juce::Colour cChassis  { 0xff141414 };
static const juce::Colour cMetal    { 0xff2e2e2e };
static const juce::Colour cEdge     { 0xff0a0a0a };
static const juce::Colour cPanel    { 0xff1c1c1c };
static const juce::Colour cAccent   { 0xffe83a2a };  // LED red
static const juce::Colour cAmber    { 0xffe8820a };  // parameter labels
static const juce::Colour cTextDim  { 0xff7a746c };
static const juce::Colour cSilk     { 0xffa09890 };
static const juce::Colour cText     { 0xffd4cfc8 };

// ── Dimensions (spec: 680 x 360) ──────────────────────────────────────────────
static constexpr int   kW           = 680;
static constexpr int   kH           = 360;
static constexpr int   kFaceW       = 160;
static constexpr float kFaceKnobR   = 18.0f;

// ── Parameter table ───────────────────────────────────────────────────────────
// 21 params, grouped into 8 sections
struct ParamInfo { const char* id; const char* label; const char* unit; };

static const ParamInfo kParams[] = {
    // CRUSHER (0-1)
    { "bitDepth",      "BIT DEPTH",    "bit" },
    { "sampleRateDiv", "SAMPLE RATE",  "%"   },
    // SATURATION (2-3)
    { "drive",         "DRIVE",        ""    },
    { "outputGain",    "OUTPUT GAIN",  "x"   },
    // CHARACTER (4-5)
    { "noiseAmount",   "NOISE",        ""    },
    { "crackleRate",   "CRACKLE",      ""    },
    // FILTERS (6-7)
    { "lpfCutoff",     "LPF FREQ",     "Hz"  },
    { "hpfCutoff",     "HPF FREQ",     "Hz"  },
    // COMPRESSOR (8-11)
    { "compThreshold", "THRESHOLD",    "dB"  },
    { "compRatio",     "RATIO",        ":1"  },
    { "compAttack",    "ATTACK",       "ms"  },
    { "compRelease",   "RELEASE",      "ms"  },
    // REVERB (12-13)  — spec has 3 but we have 3: room, wet, damping
    { "reverbRoom",    "ROOM SIZE",    ""    },
    { "reverbWet",     "REVERB MIX",   ""    },
    { "reverbDamping", "DAMPING",      ""    },
    // MODULATION (15-17)
    { "pitchSemitones","PITCH SHIFT",  "st"  },
    { "wowRate",       "WOW RATE",     "Hz"  },
    { "wowDepth",      "WOW DEPTH",    "c"   },
    // SPATIAL (18-20)
    { "stereoWidth",   "STEREO WIDTH", ""    },
    { "compMakeup",    "MAKEUP",       "dB"  },
    { "transientBoost","TRANSIENT",    ""    },
};
static constexpr int kNumParams = 21;

struct SectionInfo { const char* name; int start; int count; };
static const SectionInfo kSections[] = {
    { "CRUSHER",    0,  2 },
    { "SATURATION", 2,  2 },
    { "CHARACTER",  4,  2 },
    { "FILTERS",    6,  2 },
    { "COMPRESSOR", 8,  4 },
    { "REVERB",     12, 3 },
    { "MODULATION", 15, 3 },
    { "SPATIAL",    18, 3 },
};
static constexpr int kNumSections = 8;

// Face panel knobs: 5 primary controls
struct FaceKnobInfo { int paramIdx; const char* label; };
static const FaceKnobInfo kFaceKnobs[] = {
    { 2,  "DRIVE"  },
    { 0,  "CRUSH"  },
    { 4,  "NOISE"  },
    { 8,  "COMP"   },
    { 12, "VERB"   },
};
static constexpr int kNumFaceKnobs = 5;

// ── Constructor ───────────────────────────────────────────────────────────────
DrumSmashEditor::DrumSmashEditor (DrumSmashProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (kW, kH);

    rajdhaniBold  = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::RajdhaniBold_ttf,
            BinaryData::RajdhaniBold_ttfSize)));
    shareTechMono = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::ShareTechMonoRegular_ttf,
            BinaryData::ShareTechMonoRegular_ttfSize)));
    logoDrawable = juce::Drawable::createFromImageData (
        BinaryData::logo_transparent_svg,
        BinaryData::logo_transparent_svgSize);

    selectedPreset = proc.getCurrentPresetIndex();
    for (int i = 0; i < kNumParams; ++i)
        cachedNorm[i] = getNorm (i);

    startTimerHz (30);
}

DrumSmashEditor::~DrumSmashEditor() { stopTimer(); }

void DrumSmashEditor::timerCallback()
{
    bool changed = false;
    for (int i = 0; i < kNumParams; ++i)
    {
        float v = getNorm (i);
        if (v != cachedNorm[i]) { cachedNorm[i] = v; changed = true; }
    }
    int curr = proc.getCurrentPresetIndex();
    if (curr != selectedPreset) { selectedPreset = curr; changed = true; }
    if (changed) repaint();
}

// ── Param helpers ─────────────────────────────────────────────────────────────
float DrumSmashEditor::getNorm (int i) const
{
    auto* p = proc.apvts.getParameter (kParams[i].id);
    return p ? p->getValue() : 0.f;
}
void DrumSmashEditor::setNorm (int i, float norm)
{
    auto* p = proc.apvts.getParameter (kParams[i].id);
    if (p) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
}
juce::String DrumSmashEditor::getValueText (int i) const
{
    auto* p = proc.apvts.getParameter (kParams[i].id);
    if (!p) return "";
    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(p);
    if (fp)
    {
        float v = fp->get();
        juce::String unit = kParams[i].unit;
        if (unit == "bit") return juce::String ((int) v) + " bit";
        if (unit == "%")   return juce::String ((int)(100.f / v)) + "%";
        if (unit == "Hz")  return juce::String ((int) v) + " Hz";
        if (unit == "ms")  return juce::String (v, 1) + " ms";
        if (unit == "dB")  return juce::String (v, 1) + " dB";
        if (unit == "st")  return juce::String (v, 1) + " st";
        if (unit == ":1")  return juce::String (v, 1) + ":1";
        return juce::String (v, 2);
    }
    return p->getCurrentValueAsText();
}

// ── Layout helpers ────────────────────────────────────────────────────────────
juce::Rectangle<float> DrumSmashEditor::facePanel() const
{
    return { 0, 0, (float)kFaceW, (float)kH };
}
juce::Rectangle<float> DrumSmashEditor::contentPanel() const
{
    return { (float)kFaceW + 3, 0, (float)(kW - kFaceW - 3), (float)kH };
}
juce::Rectangle<float> DrumSmashEditor::presetBarRect() const
{
    auto cp = contentPanel();
    return { cp.getX(), cp.getY(), cp.getWidth(), 32.f };
}
juce::Rectangle<float> DrumSmashEditor::sectionsRect() const
{
    auto cp = contentPanel();
    return { cp.getX(), cp.getY() + 32.f, cp.getWidth(), cp.getHeight() - 32.f };
}

juce::Point<float> DrumSmashEditor::faceKnobCenter (int idx) const
{
    // 3 top, 2 bottom — centred in face panel
    float faceW = (float)kFaceW;
    if (idx < 3)
    {
        float spacing = faceW / 3.f;
        return { spacing * 0.5f + idx * spacing, 200.f };
    }
    else
    {
        float spacing = faceW / 2.f;
        return { spacing * 0.5f + (idx - 3) * spacing, 248.f };
    }
}

int DrumSmashEditor::faceKnobHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumFaceKnobs; ++i)
        if (pos.getDistanceFrom (faceKnobCenter (i)) <= kFaceKnobR + 8.f)
            return i;
    return -1;
}

juce::Rectangle<float> DrumSmashEditor::sliderRowRect (int paramIdx) const
{
    auto sr = sectionsRect();
    // Total rows = 21 params + 8 section headers
    // Each section header = 20px, each row = 38px (spec) but we scale to fit
    float totalH = sr.getHeight();
    float headerH = 20.f;
    float rowH = (totalH - kNumSections * headerH) / (float)kNumParams;

    // Find which section this param is in, count rows above
    float y = sr.getY();
    for (int s = 0; s < kNumSections; ++s)
    {
        y += headerH;
        for (int r = 0; r < kSections[s].count; ++r)
        {
            int pi = kSections[s].start + r;
            if (pi == paramIdx)
                return { sr.getX(), y, sr.getWidth() - 16.f, rowH };
            y += rowH;
        }
    }
    return {};
}

int DrumSmashEditor::sliderHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumParams; ++i)
    {
        auto row = sliderRowRect (i);
        if (row.contains (pos)) return i;
    }
    return -1;
}

juce::Rectangle<float> DrumSmashEditor::presetRect (int i) const
{
    auto bar = presetBarRect();
    float w = bar.getWidth() / (float)kNumPresets;
    return { bar.getX() + i * w, bar.getY(), w, bar.getHeight() };
}

int DrumSmashEditor::presetHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumPresets; ++i)
        if (presetRect(i).contains (pos)) return i;
    return -1;
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void DrumSmashEditor::mouseDown (const juce::MouseEvent& e)
{
    // Preset bar
    int pr = presetHitTest (e.position);
    if (pr >= 0)
    {
        selectedPreset = pr;
        proc.setCurrentProgram (pr);
        repaint();
        return;
    }

    // Face knobs
    int fk = faceKnobHitTest (e.position);
    if (fk >= 0)
    {
        dragFaceKnob  = fk;
        dragStartY    = e.position.y;
        dragStartVal  = getNorm (kFaceKnobs[fk].paramIdx);
        return;
    }

    // Content sliders
    int sl = sliderHitTest (e.position);
    if (sl >= 0)
    {
        dragSlider       = sl;
        dragSliderStartX   = e.position.x;
        dragSliderStartVal = getNorm (sl);
    }
}

void DrumSmashEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFaceKnob >= 0)
    {
        float delta = (dragStartY - e.position.y) / 150.f;
        if (e.mods.isShiftDown()) delta *= 0.1f;
        setNorm (kFaceKnobs[dragFaceKnob].paramIdx, dragStartVal + delta);
        repaint();
    }
    else if (dragSlider >= 0)
    {
        auto row = sliderRowRect (dragSlider);
        float trackW = row.getWidth() - 120.f; // label + value space
        float trackX = row.getX() + 100.f;
        float norm   = (e.position.x - trackX) / trackW;
        if (e.mods.isShiftDown())
            norm = dragSliderStartVal + (e.position.x - dragSliderStartX) / (trackW * 8.f);
        setNorm (dragSlider, norm);
        repaint();
    }
}

void DrumSmashEditor::mouseUp (const juce::MouseEvent&)
{
    dragFaceKnob = -1;
    dragSlider   = -1;
}

void DrumSmashEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int fk = faceKnobHitTest (e.position);
    int paramIdx = -1;
    if (fk >= 0) paramIdx = kFaceKnobs[fk].paramIdx;
    else          paramIdx = sliderHitTest (e.position);

    if (paramIdx < 0) return;

    auto* p  = proc.apvts.getParameter (kParams[paramIdx].id);
    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(p);
    if (!fp) return;

    auto* box = new juce::AlertWindow ("Enter value",
                                        fp->getName (64),
                                        juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("val", juce::String (fp->get()));
    box->addButton ("OK", 1);
    box->addButton ("Cancel", 0);
    box->enterModalState (true,
        juce::ModalCallbackFunction::create ([box, fp](int result) {
            if (result == 1)
                *fp = juce::jlimit (fp->range.start, fp->range.end,
                                    box->getTextEditorContents ("val").getFloatValue());
        }), true);
}

void DrumSmashEditor::mouseWheelMove (const juce::MouseEvent& e,
                                       const juce::MouseWheelDetails& w)
{
    int fk = faceKnobHitTest (e.position);
    if (fk >= 0)
    {
        float step = e.mods.isShiftDown() ? 0.003f : 0.02f;
        setNorm (kFaceKnobs[fk].paramIdx,
                 getNorm (kFaceKnobs[fk].paramIdx) + w.deltaY * step);
        repaint();
        return;
    }
    int sl = sliderHitTest (e.position);
    if (sl >= 0)
    {
        float step = e.mods.isShiftDown() ? 0.003f : 0.02f;
        setNorm (sl, getNorm (sl) + w.deltaY * step);
        repaint();
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void DrumSmashEditor::paint (juce::Graphics& g)
{
    drawChassis   (g);
    drawFacePanel (g);
    drawContent   (g);
    drawScrews    (g);
    drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

// ── Chassis ───────────────────────────────────────────────────────────────────
void DrumSmashEditor::drawChassis (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (cChassis);
    g.fillRect (b);
    g.setColour (juce::Colour (0xff333333));
    g.drawRect (b, 1.f);
    g.setColour (juce::Colour (0xff444444));
    g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 2.f);
    g.setColour (cEdge);
    g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.f);
}

void DrumSmashEditor::drawScanLines (juce::Graphics& g,
                                      juce::Rectangle<float> area,
                                      float opacity)
{
    g.setColour (juce::Colours::white.withAlpha (opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.f)
        g.drawHorizontalLine ((int)y, area.getX(), area.getRight());
}

void DrumSmashEditor::drawScrews (juce::Graphics& g)
{
    const float inset = 8.f, d = 12.f, r = d * 0.5f;
    juce::Point<float> corners[4] = {
        { inset + r,         inset + r         },
        { kW - inset - r,    inset + r         },
        { inset + r,         kH - inset - r    },
        { kW - inset - r,    kH - inset - r    }
    };
    for (auto& c : corners)
    {
        juce::ColourGradient grad (juce::Colour (0xff3a3a3a), c.x - r*0.4f, c.y - r*0.35f,
                                    juce::Colour (0xff111111), c.x + r,      c.y + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, d, d);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - r, c.y - r, d, d, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - r + 1, c.y - r + 1, d - 2, d - 2, 0.8f);
        float s = r - 2.f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (c.x - s, c.y, c.x + s, c.y, 1.5f);
        g.drawLine (c.x, c.y - s, c.x, c.y + s, 1.5f);
    }
}

// ── Face panel ────────────────────────────────────────────────────────────────
void DrumSmashEditor::drawFacePanel (juce::Graphics& g)
{
    auto fp = facePanel();

    // Panel background with brushed lines
    g.setColour (cMetal);
    g.fillRect (fp);
    g.setColour (juce::Colours::white.withAlpha (0.008f));
    for (float y = fp.getY(); y < fp.getBottom(); y += 2.f)
        g.drawHorizontalLine ((int)y, fp.getX(), fp.getRight());

    // Divider
    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRect (fp.getRight(), fp.getY(), 2.f, fp.getHeight());
    g.setColour (juce::Colour (0xff3d3d3d));
    g.fillRect (fp.getRight() + 2.f, fp.getY(), 1.f, fp.getHeight());

    float fW = fp.getWidth();

    // Module ID
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (cTextDim);
    g.drawText ("04 / 04", 14, 18, 80, 12, juce::Justification::centredLeft);

    // Plugin name
    float nameY = 36.f;
    g.setFont (rajdhaniBold.withHeight (22.f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("DRUM",  0, (int)nameY + 1, (int)fW, 26, juce::Justification::centred);
    g.drawText ("SMASH", 0, (int)nameY + 25, (int)fW, 26, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("DRUM",  0, (int)nameY - 1, (int)fW, 26, juce::Justification::centred);
    g.drawText ("SMASH", 0, (int)nameY + 23, (int)fW, 26, juce::Justification::centred);
    g.setColour (cSilk);
    g.drawText ("DRUM",  0, (int)nameY,    (int)fW, 26, juce::Justification::centred);
    g.drawText ("SMASH", 0, (int)nameY + 24, (int)fW, 26, juce::Justification::centred);

    // Accent line under name
    g.setColour (cAccent.withAlpha (0.4f));
    g.drawLine (fW * 0.2f, nameY + 53.f, fW * 0.8f, nameY + 53.f, 1.f);

    // Face knobs
    for (int i = 0; i < kNumFaceKnobs; ++i)
        drawFaceKnob (g, i);

    // Category badge at bottom
    float badgeY  = (float)kH - 22.f;
    float dotX    = fW * 0.5f - 34.f;
    float dotR    = 3.f;
    g.setColour (cAccent.withAlpha (0.5f));
    g.fillEllipse (dotX - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    g.setColour (cAccent);
    g.fillEllipse (dotX - dotR, badgeY - dotR, dotR * 2, dotR * 2);
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (cTextDim);
    g.drawText ("DRUM PROC", (int)(dotX + 5), (int)(badgeY - 5), 60, 10,
                juce::Justification::centredLeft);

    // Logo bottom-right of face panel
    if (logoDrawable)
    {
        const float logoSize = 40.f;
        const float margin   = 10.f;
        juce::Rectangle<float> bounds (fW - logoSize - margin,
                                        kH - logoSize - margin,
                                        logoSize, logoSize);
        logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.35f);
    }
}

void DrumSmashEditor::drawFaceKnob (juce::Graphics& g, int idx)
{
    auto c    = faceKnobCenter (idx);
    int  pi   = kFaceKnobs[idx].paramIdx;
    float val = getNorm (pi);
    float r   = kFaceKnobR;

    float arcR    = r + 5.f;
    float startA  = juce::MathConstants<float>::pi * 1.2f;
    float endA    = juce::MathConstants<float>::pi * 2.8f;
    float valueA  = startA + val * (endA - startA);

    // Inactive arc
    juce::Path inactiveArc;
    inactiveArc.addArc (c.x - arcR, c.y - arcR, arcR*2, arcR*2, valueA, endA, true);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.strokePath (inactiveArc, juce::PathStrokeType (4.f));

    // Active arc
    juce::Path activeArc;
    activeArc.addArc (c.x - arcR, c.y - arcR, arcR*2, arcR*2, startA, valueA, true);
    g.setColour (cAccent.withAlpha (0.8f));
    g.strokePath (activeArc, juce::PathStrokeType (4.f));

    // Body — red-brown tint per spec
    juce::ColourGradient bodyGrad (juce::Colour (0xff3a1a14), c.x - r*0.35f, c.y - r*0.3f,
                                    juce::Colour (0xff060504), c.x + r, c.y + r, true);
    bodyGrad.addColour (0.45, juce::Colour (0xff200e0a));
    g.setGradientFill (bodyGrad);
    g.fillEllipse (c.x - r, c.y - r, r*2, r*2);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (c.x - r, c.y - r, r*2, r*2, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawEllipse (c.x - r + 1, c.y - r + 1, r*2 - 2, r*2 - 2, 0.7f);

    // Pointer
    float angle = startA + val * (endA - startA) - juce::MathConstants<float>::halfPi;
    float px1 = c.x + std::cos(angle) * r * 0.25f;
    float py1 = c.y + std::sin(angle) * r * 0.25f;
    float px2 = c.x + std::cos(angle) * r * 0.78f;
    float py2 = c.y + std::sin(angle) * r * 0.78f;
    g.setColour (cAccent.withAlpha (0.7f));
    g.drawLine (px1, py1, px2, py2, 2.5f);
    g.setColour (cAccent);
    g.drawLine (px1, py1, px2, py2, 1.5f);

    // Label below knob
    g.setFont (shareTechMono.withHeight (7.f));
    g.setColour (cSilk);
    g.drawText (kFaceKnobs[idx].label,
                (int)(c.x - 22), (int)(c.y + arcR + 5), 44, 10,
                juce::Justification::centred);
}

// ── Content panel ─────────────────────────────────────────────────────────────
void DrumSmashEditor::drawContent (juce::Graphics& g)
{
    auto cp = contentPanel();

    // Background
    g.setColour (cPanel);
    g.fillRect (cp);

    // VU meter strip (decorative) — right edge
    float vuX = cp.getRight() - 14.f;
    float vuY = cp.getY() + cp.getHeight() * 0.25f;
    float segH = 3.f, segGap = 2.f;
    const juce::Colour vuColours[] = {
        cAccent, cAccent,
        cAmber, cAmber,
        juce::Colour(0xff4ecf6a), juce::Colour(0xff4ecf6a),
        juce::Colour(0xff4ecf6a), juce::Colour(0xff4ecf6a)
    };
    for (int i = 0; i < 8; ++i)
    {
        float sy = vuY + i * (segH + segGap);
        g.setColour (vuColours[i].withAlpha (0.5f));
        g.fillRect (vuX, sy, 6.f, segH);
    }

    drawPresetBar (g);

    // Draw sections
    auto sr = sectionsRect();
    float totalH  = sr.getHeight();
    float headerH = 20.f;
    float rowH    = (totalH - kNumSections * headerH) / (float)kNumParams;
    float y       = sr.getY();

    for (int s = 0; s < kNumSections; ++s)
    {
        // Section header
        juce::Rectangle<float> hdr (sr.getX(), y, sr.getWidth() - 16.f, headerH);

        g.setColour (cChassis);
        g.fillRect (hdr);
        // Left accent bar
        g.setColour (cAccent);
        g.fillRect (hdr.getX(), hdr.getY(), 3.f, hdr.getHeight());
        // LED dot
        float dotX = hdr.getX() + 10.f;
        float dotY = hdr.getCentreY();
        g.setColour (cAccent.withAlpha (0.6f));
        g.fillEllipse (dotX - 3, dotY - 3, 6, 6);
        // Section name
        g.setFont (shareTechMono.withHeight (8.f));
        g.setColour (cAccent);
        g.drawText (kSections[s].name,
                    (int)(hdr.getX() + 20), (int)hdr.getY(),
                    (int)hdr.getWidth() - 20, (int)headerH,
                    juce::Justification::centredLeft);
        // Bottom border
        g.setColour (juce::Colour (0xff1a1a1a));
        g.drawLine (hdr.getX(), hdr.getBottom(), hdr.getRight(), hdr.getBottom(), 1.f);
        y += headerH;

        // Param rows
        for (int r = 0; r < kSections[s].count; ++r)
        {
            int pi = kSections[s].start + r;
            juce::Rectangle<float> row (sr.getX(), y, sr.getWidth() - 16.f, rowH);
            drawSliderRow (g, pi, row, (r % 2 == 1));
            y += rowH;
        }
    }
}

void DrumSmashEditor::drawPresetBar (juce::Graphics& g)
{
    auto bar = presetBarRect();
    g.setColour (cChassis);
    g.fillRect (bar);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawLine (bar.getX(), bar.getBottom(), bar.getRight(), bar.getBottom(), 2.f);

    // PRESET label
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (cTextDim);
    g.drawText ("PRESET", (int)bar.getX() + 8, (int)bar.getY(),
                50, (int)bar.getHeight(), juce::Justification::centredLeft);

    // Preset name buttons
    float btnW = (bar.getWidth() - 60.f) / (float)kNumPresets;
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto r = presetRect (i);
        // Adjust to leave space for "PRESET" label
        juce::Rectangle<float> btn (bar.getX() + 58.f + i * btnW,
                                     bar.getY() + 3.f,
                                     btnW - 2.f,
                                     bar.getHeight() - 6.f);
        bool sel = (i == selectedPreset);
        g.setColour (sel ? cAccent.withAlpha (0.8f) : juce::Colour (0xff2a2a2a));
        g.fillRoundedRectangle (btn, 2.f);
        g.setColour (sel ? juce::Colours::white : cTextDim);
        g.setFont (shareTechMono.withHeight (7.f));
        g.drawText (kPresets[i].name, btn.toNearestInt(),
                    juce::Justification::centred, false);
        if (!sel)
        {
            g.setColour (juce::Colour (0xff3a3a3a));
            g.drawRoundedRectangle (btn, 2.f, 0.5f);
        }
    }
}

void DrumSmashEditor::drawSliderRow (juce::Graphics& g, int pi,
                                      juce::Rectangle<float> row, bool shaded)
{
    // Row background
    g.setColour (shaded ? juce::Colour (0xff242424) : cPanel);
    g.fillRect (row);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawLine (row.getX(), row.getBottom(), row.getRight(), row.getBottom(), 1.f);

    float norm = getNorm (pi);

    // Label
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (cAmber);
    g.drawText (kParams[pi].label,
                (int)row.getX() + 8, (int)row.getY(),
                96, (int)row.getHeight(),
                juce::Justification::centredLeft);

    // Slider track
    float trackX  = row.getX() + 108.f;
    float trackW  = row.getWidth() - 108.f - 52.f;
    float trackCY = row.getCentreY();
    float trackH  = 4.f;

    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRect (trackX, trackCY - trackH*0.5f, trackW, trackH);
    g.setColour (juce::Colour (0xff222222));
    g.drawRect (trackX, trackCY - trackH*0.5f, trackW, trackH, 1.f);

    // Fill
    g.setColour (cAccent);
    g.fillRect (trackX, trackCY - trackH*0.5f, trackW * norm, trackH);

    // Thumb
    float thumbX = trackX + trackW * norm;
    float thumbW = 10.f, thumbH = 16.f;
    juce::Rectangle<float> thumb (thumbX - thumbW*0.5f,
                                    trackCY - thumbH*0.5f,
                                    thumbW, thumbH);
    g.setColour (juce::Colour (0xff3d3d3d));
    g.fillRoundedRectangle (thumb, 2.f);
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawLine (thumb.getX(), thumb.getY(), thumb.getRight(), thumb.getY(), 1.f);
    g.setColour (juce::Colour (0xff0d0d0d));
    g.drawLine (thumb.getX(), thumb.getBottom(), thumb.getRight(), thumb.getBottom(), 1.f);

    // Value readout
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (cSilk);
    g.drawText (getValueText (pi),
                (int)(trackX + trackW + 4), (int)row.getY(),
                48, (int)row.getHeight(),
                juce::Justification::centredLeft);
}

// ── createEditor ─────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new DrumSmashEditor (*this);
}