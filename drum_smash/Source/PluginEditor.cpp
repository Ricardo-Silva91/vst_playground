#include "PluginEditor.h"
#include <BinaryData.h>

// ── Colours ───────────────────────────────────────────────────────────────────
static const juce::Colour cMetal   { 0xff2e2e2e };
static const juce::Colour cEdge    { 0xff0a0a0a };
static const juce::Colour cAccent  { 0xffe83a2a };
static const juce::Colour cAmber   { 0xffe8820a };
static const juce::Colour cTextDim { 0xff7a746c };
static const juce::Colour cSilk    { 0xffa09890 };

// ── Layout ────────────────────────────────────────────────────────────────────
static constexpr int   kW     = 580;
static constexpr int   kH     = 340;
static constexpr float kKnobR = 32.f;

// 5 knobs in a single row, centred
static constexpr int   kNumKnobs   = 5;
static constexpr float kKnobSpX    = 96.f;
static constexpr float kKnobY      = 210.f;   // centre Y of knobs

// ── Knob definitions ──────────────────────────────────────────────────────────
struct KnobDef { const char* paramId; const char* label; const char* unit; };
static const KnobDef kKnobs[kNumKnobs] = {
    { "drive",         "DRIVE",  ""   },
    { "bitDepth",      "CRUSH",  "bit"},
    { "noiseAmount",   "NOISE",  ""   },
    { "compThreshold", "COMP",   "dB" },
    { "reverbRoom",    "VERB",   ""   },
};

// ── Param helpers ─────────────────────────────────────────────────────────────
float DrumSmashEditor::getNorm (int i) const
{
    auto* p = proc.apvts.getParameter (kKnobs[i].paramId);
    return p ? p->getValue() : 0.f;
}

void DrumSmashEditor::setNorm (int i, float norm)
{
    auto* p = proc.apvts.getParameter (kKnobs[i].paramId);
    if (p) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
}

juce::String DrumSmashEditor::getValueText (int i) const
{
    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(
        proc.apvts.getParameter (kKnobs[i].paramId));
    if (!fp) return {};
    float v = fp->get();
    juce::String u = kKnobs[i].unit;
    if (u == "bit") return juce::String ((int) v);
    if (u == "dB" ) return juce::String (v, 1);
    return juce::String (v, 2);
}

// ── Layout helpers ────────────────────────────────────────────────────────────
juce::Point<float> DrumSmashEditor::knobCenter (int idx) const
{
    float totalW = kKnobSpX * (kNumKnobs - 1);
    float startX = ((float)kW - totalW) * 0.5f;
    return { startX + idx * kKnobSpX, kKnobY };
}

int DrumSmashEditor::knobHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumKnobs; ++i)
        if (pos.getDistanceFrom (knobCenter(i)) <= kKnobR + 8.f) return i;
    return -1;
}

// ── Constructor ───────────────────────────────────────────────────────────────
DrumSmashEditor::DrumSmashEditor (DrumSmashProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (kW, kH);

    rajdhaniBold = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::RajdhaniBold_ttf, BinaryData::RajdhaniBold_ttfSize)));
    shareTechMono = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize)));
    logoDrawable = juce::Drawable::createFromImageData (
        BinaryData::logo_transparent_svg, BinaryData::logo_transparent_svgSize);

    for (int i = 0; i < kNumKnobs; ++i)
        cachedNorm[i] = getNorm (i);

    startTimerHz (30);
}

DrumSmashEditor::~DrumSmashEditor() { stopTimer(); }

void DrumSmashEditor::timerCallback()
{
    bool changed = false;
    for (int i = 0; i < kNumKnobs; ++i)
    {
        float v = getNorm (i);
        if (v != cachedNorm[i]) { cachedNorm[i] = v; changed = true; }
    }
    if (changed) repaint();
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void DrumSmashEditor::mouseDown (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k >= 0) { draggingKnob = k; dragStartY = e.position.y; dragStartVal = getNorm (k); }
}

void DrumSmashEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 140.f;
    if (e.mods.isShiftDown()) delta *= 0.1f;
    setNorm (draggingKnob, dragStartVal + delta);
    repaint();
}

void DrumSmashEditor::mouseUp (const juce::MouseEvent&) { draggingKnob = -1; }

void DrumSmashEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;
    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(
        proc.apvts.getParameter (kKnobs[k].paramId));
    if (!fp) return;

    auto* box = new juce::AlertWindow ("Enter value", fp->getName (64),
                                        juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("val", juce::String (fp->get()));
    box->addButton ("OK", 1);
    box->addButton ("Cancel", 0);
    box->enterModalState (true,
        juce::ModalCallbackFunction::create ([box, fp](int r) {
            if (r == 1)
                *fp = juce::jlimit (fp->range.start, fp->range.end,
                                    box->getTextEditorContents ("val").getFloatValue());
        }), true);
}

void DrumSmashEditor::mouseWheelMove (const juce::MouseEvent& e,
                                       const juce::MouseWheelDetails& w)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;
    float step = e.mods.isShiftDown() ? 0.003f : 0.02f;
    setNorm (k, getNorm (k) + w.deltaY * step);
    repaint();
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void DrumSmashEditor::paint (juce::Graphics& g)
{
    drawChassis   (g);
    drawPlugin    (g);
    drawScrews    (g);
    drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

// ── Chassis ───────────────────────────────────────────────────────────────────
void DrumSmashEditor::drawChassis (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (cMetal); g.fillRect (b);
    g.setColour (juce::Colour(0xff333333)); g.drawRect (b, 1.f);
    g.setColour (juce::Colour(0xff444444));
    g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 2.f);
    g.setColour (cEdge);
    g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.f);
}

void DrumSmashEditor::drawScanLines (juce::Graphics& g,
                                      juce::Rectangle<float> area, float opacity)
{
    g.setColour (juce::Colours::white.withAlpha (opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.f)
        g.drawHorizontalLine ((int)y, area.getX(), area.getRight());
}

void DrumSmashEditor::drawScrews (juce::Graphics& g)
{
    const float inset = 8.f, d = 12.f, r = d * 0.5f;
    juce::Point<float> corners[4] = {
        { inset+r, inset+r }, { kW-inset-r, inset+r },
        { inset+r, kH-inset-r }, { kW-inset-r, kH-inset-r }
    };
    for (auto& c : corners)
    {
        juce::ColourGradient grad (juce::Colour(0xff3a3a3a), c.x-r*0.4f, c.y-r*0.35f,
                                    juce::Colour(0xff111111), c.x+r, c.y+r, true);
        g.setGradientFill (grad); g.fillEllipse (c.x-r, c.y-r, d, d);
        g.setColour (juce::Colours::black.withAlpha(0.8f));
        g.drawEllipse (c.x-r, c.y-r, d, d, 1.f);
        g.setColour (juce::Colours::white.withAlpha(0.08f));
        g.drawEllipse (c.x-r+1, c.y-r+1, d-2, d-2, 0.8f);
        float s = r-2.f;
        g.setColour (juce::Colours::black.withAlpha(0.7f));
        g.drawLine (c.x-s, c.y, c.x+s, c.y, 1.5f);
        g.drawLine (c.x, c.y-s, c.x, c.y+s, 1.5f);
    }
}

// ── Plugin ────────────────────────────────────────────────────────────────────
void DrumSmashEditor::drawPlugin (juce::Graphics& g)
{
    float W = (float)kW;

    // Module ID
    g.setFont (shareTechMono.withHeight(8.f));
    g.setColour (cTextDim);
    g.drawText ("04 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    // Plugin name — engraved
    float nameY = 32.f;
    g.setFont (rajdhaniBold.withHeight(24.f));
    g.setColour (juce::Colours::black.withAlpha(0.6f));
    g.drawText ("DRUM SMASH", 0, (int)nameY+1, (int)W, 30, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha(0.07f));
    g.drawText ("DRUM SMASH", 0, (int)nameY-1, (int)W, 30, juce::Justification::centred);
    g.setColour (cSilk);
    g.drawText ("DRUM SMASH", 0, (int)nameY,   (int)W, 30, juce::Justification::centred);

    // Accent line under name
    g.setColour (cAccent.withAlpha(0.4f));
    g.drawLine (W*0.3f, nameY+33.f, W*0.7f, nameY+33.f, 1.f);

    // Five knobs
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        drawKnob (g, c.x, c.y, getNorm(i), kKnobs[i].label, getValueText(i));
    }

    // Category badge bottom-left area
    float badgeY = kKnobY + kKnobR + 52.f;
    float dotX   = W * 0.5f - 34.f, dotR = 3.f;
    g.setColour (cAccent.withAlpha(0.5f));
    g.fillEllipse (dotX-dotR-2, badgeY-dotR-2, (dotR+2)*2, (dotR+2)*2);
    g.setColour (cAccent);
    g.fillEllipse (dotX-dotR, badgeY-dotR, dotR*2, dotR*2);
    g.setFont (shareTechMono.withHeight(8.f));
    g.setColour (cTextDim);
    g.drawText ("DRUM PROC", (int)(dotX+5), (int)(badgeY-5), 60, 10,
                juce::Justification::centredLeft);

    // Logo — bottom-right
    if (logoDrawable)
    {
        const float sz = 64.f, margin = 14.f;
        juce::Rectangle<float> bounds (W - sz - margin,
                                        kH - sz - margin,
                                        sz, sz);
        logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.4f);
    }


}

// ── Knob ──────────────────────────────────────────────────────────────────────
void DrumSmashEditor::drawKnob (juce::Graphics& g,
                                  float cx, float cy, float norm,
                                  const juce::String& label,
                                  const juce::String& val)
{
    float r     = kKnobR;
    float arcR  = r + 6.f;
    float startA = juce::MathConstants<float>::pi * 1.2f;
    float endA   = juce::MathConstants<float>::pi * 2.8f;
    float valueA = startA + norm * (endA - startA);

    // Inactive arc
    juce::Path inactiveArc;
    inactiveArc.addArc (cx-arcR, cy-arcR, arcR*2, arcR*2, valueA, endA, true);
    g.setColour (juce::Colour(0xff1a1a1a).withAlpha(0.8f));
    g.strokePath (inactiveArc, juce::PathStrokeType(5.f));

    // Active arc
    juce::Path activeArc;
    activeArc.addArc (cx-arcR, cy-arcR, arcR*2, arcR*2, startA, valueA, true);
    g.setColour (cAccent.withAlpha(0.7f));
    g.strokePath (activeArc, juce::PathStrokeType(5.f));

    // Body — red-brown gradient (spec: #3A1A14 → #200E0A → #060504)
    juce::ColourGradient bodyGrad (juce::Colour(0xff3a1a14), cx-r*0.35f, cy-r*0.3f,
                                    juce::Colour(0xff060504), cx+r, cy+r, true);
    bodyGrad.addColour (0.45, juce::Colour(0xff200e0a));
    g.setGradientFill (bodyGrad);
    g.fillEllipse (cx-r, cy-r, r*2, r*2);
    g.setColour (juce::Colours::black.withAlpha(0.8f));
    g.drawEllipse (cx-r, cy-r, r*2, r*2, 1.5f);
    g.setColour (juce::Colours::white.withAlpha(0.1f));
    g.drawEllipse (cx-r+1, cy-r+1, r*2-2, r*2-2, 0.8f);

    // Pointer
    float angle = startA + norm*(endA-startA) - juce::MathConstants<float>::halfPi;
    float px1 = cx + std::cos(angle)*r*0.25f, py1 = cy + std::sin(angle)*r*0.25f;
    float px2 = cx + std::cos(angle)*r*0.78f, py2 = cy + std::sin(angle)*r*0.78f;
    g.setColour (cAccent.withAlpha(0.7f)); g.drawLine (px1,py1,px2,py2, 3.5f);
    g.setColour (cAccent);                 g.drawLine (px1,py1,px2,py2, 2.f);

    // Label above
    g.setFont (shareTechMono.withHeight(8.f));
    g.setColour (cAmber);
    g.drawText (label, (int)(cx-36), (int)(cy-arcR-16), 72, 11,
                juce::Justification::centred);

    // Value below
    g.setFont (shareTechMono.withHeight(8.5f));
    g.setColour (cSilk);
    g.drawText (val, (int)(cx-36), (int)(cy+arcR+6), 72, 11,
                juce::Justification::centred);
}

// ── createEditor ──────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new DrumSmashEditor (*this);
}