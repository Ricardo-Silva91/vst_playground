#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

// ── Constructor ───────────────────────────────────────────────────────────────
BreakScientistEditor::BreakScientistEditor (BreakScientistProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize ((int)W, (int)H);

    // Load fonts from BinaryData — never from system (FL Studio crash risk)
    rajdhaniBold = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::RajdhaniBold_ttf, BinaryData::RajdhaniBold_ttfSize)));

    shareTechMono = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize)));

    logoDrawable = juce::Drawable::createFromImageData (
        BinaryData::logo_transparent_svg, BinaryData::logo_transparent_svgSize);

    for (int i = 0; i < kTotalKnobs; ++i)
        cachedNorm[i] = getNorm (i);

    startTimerHz (30);
}

BreakScientistEditor::~BreakScientistEditor()
{
    stopTimer();
}

// ── Timer — repaint only on change ───────────────────────────────────────────
void BreakScientistEditor::timerCallback()
{
    bool changed = false;
    for (int i = 0; i < kTotalKnobs; ++i)
    {
        float v = getNorm (i);
        if (v != cachedNorm[i]) { cachedNorm[i] = v; changed = true; }
    }
    if (changed) repaint();
}

// ── Param helpers ─────────────────────────────────────────────────────────────
float BreakScientistEditor::getNorm (int i) const
{
    auto* p = proc.apvts.getParameter (knobInfo (i).paramId);
    return p ? p->getValue() : 0.f;
}

void BreakScientistEditor::setNorm (int i, float norm)
{
    auto* p = proc.apvts.getParameter (knobInfo (i).paramId);
    if (p) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
}

juce::String BreakScientistEditor::getValueText (int i) const
{
    auto* p = proc.apvts.getParameter (knobInfo (i).paramId);
    if (!p) return "-";
    return p->getText (p->getValue(), 5);
}

// ── Layout ────────────────────────────────────────────────────────────────────
// Two groups side-by-side, each 3 knobs in a row.
// Group rect: x=30 (timing), x=295 (character), y=80, w=235, h=160
static constexpr float kGroupW      = 235.f;
static constexpr float kGroupH      = 175.f;
static constexpr float kGroupY      = 72.f;
static constexpr float kGroupLX     = 30.f;   // timing group left edge
static constexpr float kGroupRX     = 295.f;  // character group left edge
static constexpr float kKnobR       = 28.f;
static constexpr float kKnobSpX     = 78.f;   // spacing between knob centres

juce::Point<float> BreakScientistEditor::knobCenter (int globalIndex) const
{
    const bool  isChar = (globalIndex >= kKnobsPerGroup);
    const int   local  = globalIndex % kKnobsPerGroup;
    const float groupX = isChar ? kGroupRX : kGroupLX;

    // 3 knobs evenly spaced within group
    const float startX = groupX + (kGroupW - kKnobSpX * 2.f) * 0.5f;
    const float cx     = startX + (float)local * kKnobSpX;
    const float cy     = kGroupY + kGroupH * 0.52f;
    return { cx, cy };
}

int BreakScientistEditor::knobHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kTotalKnobs; ++i)
    {
        auto c = knobCenter (i);
        if (pos.getDistanceFrom (c) <= kKnobR + 10.f)
            return i;
    }
    return -1;
}

// ── Mouse handling ────────────────────────────────────────────────────────────
void BreakScientistEditor::mouseDown (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k >= 0)
    {
        draggingKnob = k;
        dragStartY   = e.position.y;
        dragStartVal = getNorm (k);
    }
}

void BreakScientistEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 140.f;
    if (e.mods.isShiftDown()) delta *= 0.1f;
    setNorm (draggingKnob, dragStartVal + delta);
    repaint();
}

void BreakScientistEditor::mouseUp (const juce::MouseEvent&)
{
    draggingKnob = -1;
}

void BreakScientistEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;

    auto* fp = dynamic_cast<juce::AudioParameterFloat*> (
        proc.apvts.getParameter (knobInfo (k).paramId));
    if (!fp) return;

    auto* box = new juce::AlertWindow ("Enter value", fp->getName (64),
                                        juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("val", juce::String (fp->get()));
    box->addButton ("OK",     1);
    box->addButton ("Cancel", 0);
    box->enterModalState (true,
        juce::ModalCallbackFunction::create ([box, fp] (int result) {
            if (result == 1)
                *fp = juce::jlimit (fp->range.start, fp->range.end,
                                    box->getTextEditorContents ("val").getFloatValue());
        }), true);
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void BreakScientistEditor::paint (juce::Graphics& g)
{
    drawChassis (g);
    drawScrews  (g);

    // ── Plugin name — engraved effect ────────────────────────────────────────
    const float nameY = 18.f;
    g.setFont (rajdhaniBold.withHeight (22.f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("BREAK SCIENTIST", 0, (int)nameY + 1, (int)W, 28, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("BREAK SCIENTIST", 0, (int)nameY - 1, (int)W, 28, juce::Justification::centred);
    g.setColour (juce::Colour (0xffa09890));
    g.drawText ("BREAK SCIENTIST", 0, (int)nameY,     (int)W, 28, juce::Justification::centred);

    // Accent line under title
    g.setColour (cAccent.withAlpha (0.45f));
    g.drawLine (W * 0.28f, nameY + 31.f, W * 0.72f, nameY + 31.f, 1.f);

    // Module ID badge bottom-left
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (juce::Colour (0x887a746c));
    g.drawText ("BS-001", 14, (int)H - 18, 60, 12, juce::Justification::centredLeft);

    // Divider line between the two groups
    const float divX = W * 0.5f;
    g.setColour (juce::Colour (0xff222222));
    g.drawLine (divX, kGroupY - 8.f, divX, kGroupY + kGroupH + 8.f, 1.f);
    g.setColour (cAccent.withAlpha (0.08f));
    g.drawLine (divX + 1.f, kGroupY - 8.f, divX + 1.f, kGroupY + kGroupH + 8.f, 1.f);

    // Draw groups
    juce::Rectangle<float> timingArea (kGroupLX, kGroupY, kGroupW, kGroupH);
    juce::Rectangle<float> charArea   (kGroupRX, kGroupY, kGroupW, kGroupH);
    drawGroup (g, timingArea, "TIMING",    kTimingKnobs, 0);
    drawGroup (g, charArea,   "CHARACTER", kCharKnobs,   kKnobsPerGroup);

    drawLogo (g);

    // Scan lines last — subtle texture over everything
    drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

// ── Draw helpers ──────────────────────────────────────────────────────────────
void BreakScientistEditor::drawChassis (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Outer chassis — darkest layer
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (b);

    // Panel face
    g.setColour (juce::Colour (0xff2e2e2e));
    g.fillRect (b.reduced (4.f));

    // Top highlight
    g.setColour (juce::Colour (0xff444444));
    g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 2.f);

    // Bottom shadow
    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.f);

    // Border
    g.setColour (juce::Colour (0xff333333));
    g.drawRect (b, 1.f);

    // Subtle inner vignette gradient
    juce::ColourGradient vignette (juce::Colours::black.withAlpha (0.25f),
                                    W * 0.5f, H * 0.5f,
                                    juce::Colours::black.withAlpha (0.0f),
                                    0.f, 0.f, true);
    g.setGradientFill (vignette);
    g.fillRect (b);
}

void BreakScientistEditor::drawScrews (juce::Graphics& g)
{
    const float inset = 8.f, d = 12.f, r = d * 0.5f;
    juce::Point<float> corners[4] = {
        { inset + r,     inset + r     },
        { W - inset - r, inset + r     },
        { inset + r,     H - inset - r },
        { W - inset - r, H - inset - r }
    };
    for (auto& c : corners)
    {
        juce::ColourGradient grad (juce::Colour (0xff3a3a3a), c.x - r * 0.4f, c.y - r * 0.35f,
                                    juce::Colour (0xff111111), c.x + r, c.y + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, d, d);

        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - r, c.y - r, d, d, 1.f);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - r + 1.f, c.y - r + 1.f, d - 2.f, d - 2.f, 0.8f);

        // Cross slot
        const float s = r - 2.f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (c.x - s, c.y,     c.x + s, c.y,     1.5f);
        g.drawLine (c.x,     c.y - s, c.x,     c.y + s, 1.5f);
    }
}

void BreakScientistEditor::drawScanLines (juce::Graphics& g,
                                           juce::Rectangle<float> area,
                                           float opacity)
{
    g.setColour (juce::Colours::white.withAlpha (opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.f)
        g.drawHorizontalLine ((int)y, area.getX(), area.getRight());
}

void BreakScientistEditor::drawGroup (juce::Graphics& g,
                                       juce::Rectangle<float> area,
                                       const char* title,
                                       const KnobInfo* knobs,
                                       int startIdx)
{
    // Group background panel — slightly inset
    auto panel = area.reduced (6.f, 4.f);
    g.setColour (juce::Colour (0xff1c1c1c));
    g.fillRoundedRectangle (panel, 4.f);
    g.setColour (cAccent.withAlpha (0.18f));
    g.drawRoundedRectangle (panel, 4.f, 1.f);

    // Group title
    g.setFont (shareTechMono.withHeight (8.5f));
    g.setColour (cAccent.withAlpha (0.7f));
    g.drawText (title, (int)panel.getX(), (int)panel.getY() + 5, (int)panel.getWidth(), 12,
                juce::Justification::centred);

    // Title underline
    const float lineY = panel.getY() + 19.f;
    g.setColour (cAccent.withAlpha (0.25f));
    g.drawLine (panel.getX() + 16.f, lineY, panel.getRight() - 16.f, lineY, 1.f);

    // Draw each knob
    for (int i = 0; i < kKnobsPerGroup; ++i)
    {
        const int   globalIdx = startIdx + i;
        auto        centre    = knobCenter (globalIdx);
        const float norm      = cachedNorm[globalIdx];
        const juce::String label (knobs[i].label);
        const juce::String val = getValueText (globalIdx);
        drawKnob (g, centre.x, centre.y, norm, label, val);
    }
}

void BreakScientistEditor::drawKnob (juce::Graphics& g,
                                      float cx, float cy, float norm,
                                      const juce::String& label,
                                      const juce::String& val)
{
    const float r      = kKnobR;
    const float arcR   = r + 7.f;
    const float startA = juce::MathConstants<float>::pi * 1.2f;
    const float endA   = juce::MathConstants<float>::pi * 2.8f;
    const float valueA = startA + norm * (endA - startA);

    // ── Inactive arc track ────────────────────────────────────────────────────
    juce::Path arc;
    arc.addArc (cx - arcR, cy - arcR, arcR * 2.f, arcR * 2.f, valueA, endA, true);
    g.setColour (juce::Colour (0xff1a1a1a).withAlpha (0.9f));
    g.strokePath (arc, juce::PathStrokeType (4.5f));

    // ── Active arc — purple accent ────────────────────────────────────────────
    arc.clear();
    arc.addArc (cx - arcR, cy - arcR, arcR * 2.f, arcR * 2.f, startA, valueA, true);

    // Glow pass (slightly wider, lower alpha)
    g.setColour (cAccent.withAlpha (0.25f));
    g.strokePath (arc, juce::PathStrokeType (8.f));

    // Core arc
    g.setColour (cAccent.withAlpha (0.75f));
    g.strokePath (arc, juce::PathStrokeType (4.5f));

    // ── Knob body — dark with purple-tinted gradient ──────────────────────────
    juce::ColourGradient body (juce::Colour (0xff2a1a3a), cx - r * 0.35f, cy - r * 0.3f,
                                juce::Colour (0xff080508), cx + r, cy + r, true);
    body.addColour (0.45, juce::Colour (0xff150d20));
    g.setGradientFill (body);
    g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);

    // Rim
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, 1.5f);

    // Inner highlight ring
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (cx - r + 1.f, cy - r + 1.f, r * 2.f - 2.f, r * 2.f - 2.f, 0.8f);

    // ── Pointer ───────────────────────────────────────────────────────────────
    const float angle = startA + norm * (endA - startA) - juce::MathConstants<float>::halfPi;
    const float px1   = cx + std::cos (angle) * r * 0.22f;
    const float py1   = cy + std::sin (angle) * r * 0.22f;
    const float px2   = cx + std::cos (angle) * r * 0.78f;
    const float py2   = cy + std::sin (angle) * r * 0.78f;

    g.setColour (cAccent.withAlpha (0.5f));
    g.drawLine (px1, py1, px2, py2, 3.5f);
    g.setColour (cAccent);
    g.drawLine (px1, py1, px2, py2, 2.f);

    // ── Labels ────────────────────────────────────────────────────────────────
    // Amber label above
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (juce::Colour (0xffe8820a));
    g.drawText (label, (int)(cx - 36.f), (int)(cy - arcR - 15.f),
                72, 11, juce::Justification::centred);

    // Silk value below
    g.setFont (shareTechMono.withHeight (8.5f));
    g.setColour (juce::Colour (0xffa09890));
    g.drawText (val, (int)(cx - 36.f), (int)(cy + arcR + 5.f),
                72, 11, juce::Justification::centred);
}

void BreakScientistEditor::drawLogo (juce::Graphics& g)
{
    if (!logoDrawable) return;
    const float sz     = 56.f;
    const float margin = 12.f;
    juce::Rectangle<float> bounds (W - sz - margin, H - sz - margin, sz, sz);
    logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.4f);
}