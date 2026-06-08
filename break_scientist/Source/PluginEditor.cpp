#include "PluginEditor.h"
#include "SharedEditorUtils.h"

static const SharedEditorUtils::KnobStyle kKnobStyle {
    juce::Colour (0xff9b59f5),
    juce::Colour (0xff2a1a3a), juce::Colour (0xff150d20), juce::Colour (0xff080508),
    kKnobR,
    true,
    4.5f,
    8.0f
};

// ── Constructor ───────────────────────────────────────────────────────────────
BreakScientistEditor::BreakScientistEditor (BreakScientistProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize ((int)W, (int)H);
    rajdhaniBold  = SharedEditorUtils::loadRajdhaniBold();
    shareTechMono = SharedEditorUtils::loadShareTechMono();
    logoDrawable  = SharedEditorUtils::loadLogo();

    for (int i = 0; i < kTotalKnobs; ++i)
        cachedNorm[i] = getNorm (i);

    startTimerHz (30);
}

BreakScientistEditor::~BreakScientistEditor() { stopTimer(); }

// ── Timer ─────────────────────────────────────────────────────────────────────
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
static constexpr float kGroupW  = 235.f;
static constexpr float kGroupH  = 175.f;
static constexpr float kGroupY  = 72.f;
static constexpr float kGroupLX = 30.f;
static constexpr float kGroupRX = 295.f;
static constexpr float kKnobSpX = 78.f;

juce::Point<float> BreakScientistEditor::knobCenter (int globalIndex) const
{
    const bool  isChar = (globalIndex >= kKnobsPerGroup);
    const int   local  = globalIndex % kKnobsPerGroup;
    const float groupX = isChar ? kGroupRX : kGroupLX;
    const float startX = groupX + (kGroupW - kKnobSpX * 2.f) * 0.5f;
    const float cx     = startX + (float)local * kKnobSpX;
    const float cy     = kGroupY + kGroupH * 0.52f;
    return { cx, cy };
}

int BreakScientistEditor::knobHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kTotalKnobs; ++i)
        if (pos.getDistanceFrom (knobCenter (i)) <= kKnobR + 10.f)
            return i;
    return -1;
}

// ── Mouse handling ────────────────────────────────────────────────────────────
void BreakScientistEditor::mouseDown (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k >= 0) { draggingKnob = k; dragStartY = e.position.y; dragStartVal = getNorm (k); }
}

void BreakScientistEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 140.f;
    if (e.mods.isShiftDown()) delta *= 0.1f;
    setNorm (draggingKnob, dragStartVal + delta);
    repaint();
}

void BreakScientistEditor::mouseUp (const juce::MouseEvent&) { draggingKnob = -1; }

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
    SharedEditorUtils::drawScrews (g, W, H);

    const float nameY = 18.f;
    g.setFont (rajdhaniBold.withHeight (22.f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("BREAK SCIENTIST", 0, (int)nameY + 1, (int)W, 28, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("BREAK SCIENTIST", 0, (int)nameY - 1, (int)W, 28, juce::Justification::centred);
    g.setColour (juce::Colour (0xffa09890));
    g.drawText ("BREAK SCIENTIST", 0, (int)nameY,     (int)W, 28, juce::Justification::centred);

    g.setColour (cAccent.withAlpha (0.45f));
    g.drawLine (W * 0.28f, nameY + 31.f, W * 0.72f, nameY + 31.f, 1.f);

    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (juce::Colour (0x887a746c));
    g.drawText ("BS-001", 14, (int)H - 18, 60, 12, juce::Justification::centredLeft);

    const float divX = W * 0.5f;
    g.setColour (juce::Colour (0xff222222));
    g.drawLine (divX, kGroupY - 8.f, divX, kGroupY + kGroupH + 8.f, 1.f);
    g.setColour (cAccent.withAlpha (0.08f));
    g.drawLine (divX + 1.f, kGroupY - 8.f, divX + 1.f, kGroupY + kGroupH + 8.f, 1.f);

    juce::Rectangle<float> timingArea (kGroupLX, kGroupY, kGroupW, kGroupH);
    juce::Rectangle<float> charArea   (kGroupRX, kGroupY, kGroupW, kGroupH);
    drawGroup (g, timingArea, "TIMING",    kTimingKnobs, 0);
    drawGroup (g, charArea,   "CHARACTER", kCharKnobs,   kKnobsPerGroup);

    drawLogo (g);
    SharedEditorUtils::drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

// ── Draw helpers ──────────────────────────────────────────────────────────────
void BreakScientistEditor::drawChassis (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff141414));
    g.fillRect (b);

    g.setColour (juce::Colour (0xff2e2e2e));
    g.fillRect (b.reduced (4.f));

    g.setColour (juce::Colour (0xff444444));
    g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 2.f);

    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.f);

    g.setColour (juce::Colour (0xff333333));
    g.drawRect (b, 1.f);

    juce::ColourGradient vignette (juce::Colours::black.withAlpha (0.25f),
                                    W * 0.5f, H * 0.5f,
                                    juce::Colours::black.withAlpha (0.0f),
                                    0.f, 0.f, true);
    g.setGradientFill (vignette);
    g.fillRect (b);
}

void BreakScientistEditor::drawGroup (juce::Graphics& g,
                                       juce::Rectangle<float> area,
                                       const char* title,
                                       const KnobInfo* knobs,
                                       int startIdx)
{
    auto panel = area.reduced (6.f, 4.f);

    g.setFont (shareTechMono.withHeight (8.5f));
    g.setColour (cAccent.withAlpha (0.7f));
    g.drawText (title, (int)panel.getX(), (int)panel.getY() + 5, (int)panel.getWidth(), 12,
                juce::Justification::centred);

    const float lineY = panel.getY() + 19.f;
    g.setColour (cAccent.withAlpha (0.25f));
    g.drawLine (panel.getX() + 16.f, lineY, panel.getRight() - 16.f, lineY, 1.f);

    for (int i = 0; i < kKnobsPerGroup; ++i)
    {
        const int   globalIdx = startIdx + i;
        auto        centre    = knobCenter (globalIdx);
        const float norm      = cachedNorm[globalIdx];
        drawKnob (g, centre.x, centre.y, norm,
                  juce::String (knobs[i].label), getValueText (globalIdx));
    }
}

void BreakScientistEditor::drawKnob (juce::Graphics& g,
                                      float cx, float cy, float norm,
                                      const juce::String& label,
                                      const juce::String& val)
{
    SharedEditorUtils::drawKnob (g, cx, cy, norm, label, val, kKnobStyle, shareTechMono);
}

void BreakScientistEditor::drawLogo (juce::Graphics& g)
{
    if (!logoDrawable) return;
    const float sz = 56.f, margin = 12.f;
    juce::Rectangle<float> bounds (W - sz - margin, H - sz - margin, sz, sz);
    logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.4f);
}
