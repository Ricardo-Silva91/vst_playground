#include "PluginEditor.h"
#include "SharedEditorUtils.h"
#include <cmath>

static const SharedEditorUtils::KnobStyle kKnobStyle {
    kAccent,
    juce::Colour (0xff1e3a38), juce::Colour (0xff0e2422), juce::Colour (0xff050f0e),
    kKnobR,
    true,
    3.5f,
    7.0f
};

// ── Constructor / Destructor ──────────────────────────────────────────────────
ChoirBoxEditor::ChoirBoxEditor (ChoirBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize ((int)kW, (int)kH);
    rajdhaniBold  = SharedEditorUtils::loadRajdhaniBold();
    shareTechMono = SharedEditorUtils::loadShareTechMono();
    logoDrawable  = SharedEditorUtils::loadLogo();

    for (int i = 0; i < kNumKnobs; ++i)
        cachedNorm[i] = getNorm (i);

    startTimerHz (30);
}

ChoirBoxEditor::~ChoirBoxEditor() { stopTimer(); }

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
    float rowY = (k.row == 1) ? kRow1Y : (k.row == 2) ? kRow2Y : kRow3Y;

    const float margin = 120.f;
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
        if (pos.getDistanceFrom (knobCenter (i)) < kArcR + 8.f)
            return i;
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
    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(
        proc.apvts.getParameter (kKnobDefs[i].paramId));
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
    return juce::String ((int)std::round (v * 100.f)) + "%";
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void ChoirBoxEditor::mouseDown (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;

    if (e.mods.isRightButtonDown())
    {
        auto* param = proc.apvts.getParameter (kKnobDefs[k].paramId);
        if (param) param->setValueNotifyingHost (param->getDefaultValue());
        repaint();
        return;
    }

    draggingKnob = k;
    dragStartY   = e.position.y;
    dragStartVal = getNorm (k);
}

void ChoirBoxEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 150.f;
    if (e.mods.isShiftDown()) delta *= 0.1f;
    setNorm (draggingKnob, dragStartVal + delta);
    repaint();
}

void ChoirBoxEditor::mouseUp (const juce::MouseEvent&) { draggingKnob = -1; }

void ChoirBoxEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;

    auto* fp = dynamic_cast<juce::AudioParameterFloat*>(
        proc.apvts.getParameter (kKnobDefs[k].paramId));
    if (!fp) return;

    auto* box = new juce::AlertWindow ("Enter value", fp->getName (64),
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
    SharedEditorUtils::drawChassis   (g, getLocalBounds().toFloat());
    drawHeader        (g);
    drawSectionLabels (g);
    drawAllKnobs      (g);
    SharedEditorUtils::drawScrews    (g, kW, kH, 9.f, 11.f);
    SharedEditorUtils::drawScanLines (g, getLocalBounds().toFloat(), 0.012f);

    if (logoDrawable)
    {
        const float sz = 56.f, margin = 14.f;
        juce::Rectangle<float> bounds (kW - sz - margin, kH - sz - margin, sz, sz);
        logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.35f);
    }
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
void ChoirBoxEditor::drawSectionPanels (juce::Graphics& g)
{
    auto drawPanel = [&](float centreY, int numKnobs)
    {
        const float panelH  = 115.f;
        const float margin  = 55.f;
        const float panelW  = kW - 2.f * margin;
        const float panelX  = margin;
        const float panelY  = centreY - panelH * 0.5f;
        const float radius  = 6.f;

        juce::Rectangle<float> panel (panelX, panelY, panelW, panelH);

        g.setColour (juce::Colour (0xff222222));
        g.fillRoundedRectangle (panel, radius);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine (panelX + radius, panelY + 1.f, panelX + panelW - radius, panelY + 1.f, 1.f);

        g.setColour (juce::Colour (0xff1a1a1a));
        g.drawRoundedRectangle (panel, radius, 1.f);

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
    const float nameY = 28.f;
    g.setFont (rajdhaniBold.withHeight (30.f));

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawText ("CHOIR BOX", 0, (int)nameY + 1, (int)kW, 36, juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("CHOIR BOX", 0, (int)nameY - 1, (int)kW, 36, juce::Justification::centred);

    g.setColour (juce::Colour (0xffa09890));
    g.drawText ("CHOIR BOX", 0, (int)nameY, (int)kW, 36, juce::Justification::centred);

    g.setColour (kAccent.withAlpha (0.5f));
    g.drawLine (kW * 0.32f, nameY + 40.f, kW * 0.68f, nameY + 40.f, 1.f);

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
        const float margin  = 120.f;
        const float panelY  = centreY - panelH * 0.5f;

        g.setColour (kAccent.withAlpha (0.6f));
        g.drawText (text, (int)margin, (int)panelY - 14, 200, 11,
                    juce::Justification::left);

        const float lineStart = margin - 20.f;
        const float lineEnd   = kW - margin + 20.f;
        const float tw = shareTechMono.withHeight (8.5f).getStringWidth (text) + 8.f;
        g.setColour (kAccent.withAlpha (0.2f));
        g.drawLine (margin + tw, panelY - 8.f, lineEnd, panelY - 8.f, 0.8f);
    };

    drawLabel ("PITCH & VOICES",   kRow1Y);
    drawLabel ("LEVELS",           kRow2Y);
    drawLabel ("DISTORTION & OUT", kRow3Y);
}

void ChoirBoxEditor::drawAllKnobs (juce::Graphics& g)
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        drawKnob (g, c.x, c.y, cachedNorm[i], kKnobDefs[i].label, getValueText (i));
    }
}

void ChoirBoxEditor::drawKnob (juce::Graphics& g, float cx, float cy,
                                float norm,
                                const juce::String& label,
                                const juce::String& val)
{
    SharedEditorUtils::drawKnob (g, cx, cy, norm, label, val, kKnobStyle, shareTechMono);
}
