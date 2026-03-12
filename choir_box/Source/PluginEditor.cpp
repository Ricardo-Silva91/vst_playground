#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

// ── Constructor / Destructor ──────────────────────────────────────────────────
ChoirBoxEditor::ChoirBoxEditor (ChoirBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize ((int)kW, (int)kH);

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

ChoirBoxEditor::~ChoirBoxEditor()
{
    stopTimer();
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void ChoirBoxEditor::timerCallback()
{
    bool changed = false;
    for (int i = 0; i < kNumKnobs; ++i)
    {
        float v = getNorm (i);
        if (v != cachedNorm[i]) { cachedNorm[i] = v; changed = true; }
    }
    if (changed) repaint();
}

// ── Layout ────────────────────────────────────────────────────────────────────
juce::Point<float> ChoirBoxEditor::knobCenter (int index) const
{
    const auto& k = kKnobDefs[index];

    float rowY = (k.row == 1) ? kRow1Y
               : (k.row == 2) ? kRow2Y
                               : kRow3Y;

    // Evenly distribute knobs across width with generous margins
    const float margin = 80.f;
    const float span   = kW - 2.f * margin;
    float x;

    if (k.rowCount == 1)
        x = kW * 0.5f;
    else
        x = margin + (float)k.posInRow * span / (float)(k.rowCount - 1);

    return { x, rowY };
}

int ChoirBoxEditor::knobHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        if (pos.getDistanceFrom (c) < kArcR + 8.f)
            return i;
    }
    return -1;
}

// ── Param helpers ─────────────────────────────────────────────────────────────
float ChoirBoxEditor::getNorm (int i) const
{
    auto* p = proc.apvts.getParameter (kKnobDefs[i].paramId);
    return p ? p->getValue() : 0.f;
}

void ChoirBoxEditor::setNorm (int i, float norm)
{
    auto* p = proc.apvts.getParameter (kKnobDefs[i].paramId);
    if (p) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
}

juce::String ChoirBoxEditor::getValueText (int i) const
{
    auto* p = proc.apvts.getParameter (kKnobDefs[i].paramId);
    if (!p) return "";

    // Show typed value with appropriate precision / unit
    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(p);
    if (!fp) return "";

    float v = fp->get();
    const juce::String id = kKnobDefs[i].paramId;

    if (id == "upSemitones" || id == "downSemitones")
        return (v >= 0 ? "+" : "") + juce::String ((int)std::round (v)) + " st";
    if (id == "voices")
        return juce::String ((int)std::round (v));
    if (id == "detune")
        return juce::String ((int)std::round (v)) + " ct";
    if (id == "masterOut")
        return juce::String (v, 2) + "x";

    // 0-1 params — show as percentage
    return juce::String ((int)std::round (v * 100.f)) + "%";
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void ChoirBoxEditor::mouseDown (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k >= 0)
    {
        draggingKnob = k;
        dragStartY   = e.position.y;
        dragStartVal = getNorm (k);
    }
}

void ChoirBoxEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 150.f;
    if (e.mods.isShiftDown()) delta *= 0.1f;
    setNorm (draggingKnob, dragStartVal + delta);
    repaint();
}

void ChoirBoxEditor::mouseUp (const juce::MouseEvent&)
{
    draggingKnob = -1;
}

void ChoirBoxEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;

    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(
        proc.apvts.getParameter (kKnobDefs[k].paramId));
    if (!fp) return;

    auto* box = new juce::AlertWindow ("Enter value",
                                        fp->getName (64),
                                        juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("val", juce::String (fp->get()));
    box->addButton ("OK",     1);
    box->addButton ("Cancel", 0);
    box->enterModalState (true,
        juce::ModalCallbackFunction::create ([box, fp](int result)
        {
            if (result == 1)
                *fp = juce::jlimit (fp->range.start, fp->range.end,
                                    box->getTextEditorContents ("val").getFloatValue());
        }), true);
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void ChoirBoxEditor::paint (juce::Graphics& g)
{
    drawChassis       (g);
    drawSectionPanels (g);
    drawHeader        (g);
    drawSectionLabels (g);
    drawAllKnobs      (g);
    drawScrews        (g);
    drawScanLines     (g);

    // Logo — bottom right
    if (logoDrawable)
    {
        const float sz = 56.f, margin = 14.f;
        juce::Rectangle<float> bounds (kW - sz - margin, kH - sz - margin, sz, sz);
        logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.35f);
    }
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
void ChoirBoxEditor::drawChassis (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Main background
    g.setColour (juce::Colour (0xff2e2e2e));
    g.fillRect (b);

    // Outer border
    g.setColour (juce::Colour (0xff333333));
    g.drawRect (b, 1.f);

    // Top highlight edge
    g.setColour (juce::Colour (0xff444444));
    g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 2.f);

    // Bottom shadow edge
    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.f);
}

void ChoirBoxEditor::drawSectionPanels (juce::Graphics& g)
{
    // Draw a recessed panel behind each row of knobs
    auto drawPanel = [&](float centreY, int numKnobs)
    {
        const float panelH   = 115.f;
        const float margin   = 55.f;
        const float panelW   = kW - 2.f * margin;
        const float panelX   = margin;
        const float panelY   = centreY - panelH * 0.5f;
        const float radius   = 6.f;

        juce::Rectangle<float> panel (panelX, panelY, panelW, panelH);

        // Recessed fill
        g.setColour (juce::Colour (0xff222222));
        g.fillRoundedRectangle (panel, radius);

        // Inner shadow top
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine (panelX + radius, panelY + 1.f, panelX + panelW - radius, panelY + 1.f, 1.f);

        // Border
        g.setColour (juce::Colour (0xff1a1a1a));
        g.drawRoundedRectangle (panel, radius, 1.f);

        // Subtle top highlight
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawLine (panelX + radius, panelY, panelX + panelW - radius, panelY, 1.f);

        (void)numKnobs;
    };

    drawPanel (kRow1Y, 4);
    drawPanel (kRow2Y, 3);
    drawPanel (kRow3Y, 4);
}

void ChoirBoxEditor::drawHeader (juce::Graphics& g)
{
    // Plugin name — engraved effect (3 passes)
    const float nameY = 28.f;
    g.setFont (rajdhaniBold.withHeight (30.f));

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawText ("CHOIR BOX", 0, (int)nameY + 1, (int)kW, 36, juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("CHOIR BOX", 0, (int)nameY - 1, (int)kW, 36, juce::Justification::centred);

    g.setColour (juce::Colour (0xffa09890));
    g.drawText ("CHOIR BOX", 0, (int)nameY, (int)kW, 36, juce::Justification::centred);

    // Accent line below name
    g.setColour (kAccent.withAlpha (0.5f));
    g.drawLine (kW * 0.32f, nameY + 40.f, kW * 0.68f, nameY + 40.f, 1.f);

    // Module ID badge — bottom left of header area
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (juce::Colour (0xff7a746c));
    g.drawText ("CB-001  VOCAL HARMONIZER", 20, (int)nameY + 44, 260, 12,
                juce::Justification::left);
}

void ChoirBoxEditor::drawSectionLabels (juce::Graphics& g)
{
    g.setFont (shareTechMono.withHeight (8.5f));

    auto drawLabel = [&](const juce::String& text, float centreY)
    {
        const float panelH  = 115.f;
        const float margin  = 55.f;
        const float panelY  = centreY - panelH * 0.5f;

        // Small label tab sitting above the panel
        g.setColour (kAccent.withAlpha (0.6f));
        g.drawText (text, (int)margin, (int)panelY - 14, 200, 11,
                    juce::Justification::left);

        // Thin accent line to the right of label
        float textW = shareTechMono.withHeight (8.5f).getStringWidth (text) + 8.f;
        g.setColour (kAccent.withAlpha (0.2f));
        g.drawLine (margin + textW, panelY - 8.f, kW - margin, panelY - 8.f, 0.8f);
    };

    drawLabel ("PITCH & VOICES",  kRow1Y);
    drawLabel ("LEVELS",          kRow2Y);
    drawLabel ("DISTORTION & OUT", kRow3Y);
}

void ChoirBoxEditor::drawAllKnobs (juce::Graphics& g)
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        drawKnob (g, c.x, c.y, cachedNorm[i],
                  kKnobDefs[i].label, getValueText (i));
    }
}

void ChoirBoxEditor::drawKnob (juce::Graphics& g, float cx, float cy,
                                float norm,
                                const juce::String& label,
                                const juce::String& val)
{
    const float startA = juce::MathConstants<float>::pi * 1.25f;
    const float endA   = juce::MathConstants<float>::pi * 2.75f;
    const float valueA = startA + norm * (endA - startA);

    // ── Inactive arc ─────────────────────────────────────────────────────────
    {
        juce::Path arc;
        arc.addArc (cx - kArcR, cy - kArcR, kArcR * 2.f, kArcR * 2.f,
                    valueA, endA, true);
        g.setColour (juce::Colour (0xff111111).withAlpha (0.9f));
        g.strokePath (arc, juce::PathStrokeType (4.5f));
    }

    // ── Active arc ───────────────────────────────────────────────────────────
    if (norm > 0.001f)
    {
        juce::Path arc;
        arc.addArc (cx - kArcR, cy - kArcR, kArcR * 2.f, kArcR * 2.f,
                    startA, valueA, true);
        // Glow pass
        g.setColour (kAccent.withAlpha (0.25f));
        g.strokePath (arc, juce::PathStrokeType (7.f));
        // Main arc
        g.setColour (kAccent.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (3.5f));
    }

    // ── Knob body ─────────────────────────────────────────────────────────────
    // Teal-tinted dark gradient body
    juce::ColourGradient body (
        juce::Colour (0xff1e3a38), cx - kKnobR * 0.35f, cy - kKnobR * 0.3f,
        juce::Colour (0xff050f0e), cx + kKnobR,          cy + kKnobR, true);
    body.addColour (0.45, juce::Colour (0xff0e2422));
    g.setGradientFill (body);
    g.fillEllipse (cx - kKnobR, cy - kKnobR, kKnobR * 2.f, kKnobR * 2.f);

    // Rim shadow
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawEllipse (cx - kKnobR, cy - kKnobR, kKnobR * 2.f, kKnobR * 2.f, 1.5f);

    // Inner highlight ring
    g.setColour (kAccent.withAlpha (0.08f));
    g.drawEllipse (cx - kKnobR + 1.f, cy - kKnobR + 1.f,
                   kKnobR * 2.f - 2.f, kKnobR * 2.f - 2.f, 0.8f);

    // ── Pointer ───────────────────────────────────────────────────────────────
    float angle = startA + norm * (endA - startA) - juce::MathConstants<float>::halfPi;
    float px1 = cx + std::cos (angle) * kKnobR * 0.22f;
    float py1 = cy + std::sin (angle) * kKnobR * 0.22f;
    float px2 = cx + std::cos (angle) * kKnobR * 0.76f;
    float py2 = cy + std::sin (angle) * kKnobR * 0.76f;

    g.setColour (kAccent.withAlpha (0.5f));
    g.drawLine (px1, py1, px2, py2, 3.5f);
    g.setColour (kAccent);
    g.drawLine (px1, py1, px2, py2, 2.f);

    // ── Label above knob (amber) ──────────────────────────────────────────────
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (juce::Colour (0xffe8820a));
    g.drawText (label,
                (int)(cx - 40.f), (int)(cy - kArcR - 17.f),
                80, 11, juce::Justification::centred);

    // ── Value below knob (silk) ───────────────────────────────────────────────
    g.setFont (shareTechMono.withHeight (8.5f));
    g.setColour (juce::Colour (0xffa09890));
    g.drawText (val,
                (int)(cx - 40.f), (int)(cy + kArcR + 7.f),
                80, 11, juce::Justification::centred);
}

void ChoirBoxEditor::drawScrews (juce::Graphics& g)
{
    const float inset = 9.f, d = 11.f, r = d * 0.5f;
    juce::Point<float> corners[4] = {
        { inset + r, inset + r },
        { kW - inset - r, inset + r },
        { inset + r, kH - inset - r },
        { kW - inset - r, kH - inset - r }
    };

    for (auto& c : corners)
    {
        juce::ColourGradient grad (
            juce::Colour (0xff3a3a3a), c.x - r * 0.4f, c.y - r * 0.35f,
            juce::Colour (0xff111111), c.x + r, c.y + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, d, d);

        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - r, c.y - r, d, d, 1.f);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - r + 1.f, c.y - r + 1.f, d - 2.f, d - 2.f, 0.7f);

        float s = r - 2.f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (c.x - s, c.y, c.x + s, c.y, 1.5f);
        g.drawLine (c.x, c.y - s, c.x, c.y + s, 1.5f);
    }
}

void ChoirBoxEditor::drawScanLines (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (float y = b.getY(); y < b.getBottom(); y += 2.f)
        g.drawHorizontalLine ((int)y, b.getX(), b.getRight());
}