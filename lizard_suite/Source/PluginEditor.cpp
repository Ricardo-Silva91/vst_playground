#include "PluginEditor.h"
#include "SharedEditorUtils.h"
#include <cmath>

static const SharedEditorUtils::KnobStyle kKnobStyle {
    kAccent,
    juce::Colour (0xff2c3a22), juce::Colour (0xff18220f), juce::Colour (0xff080c05),
    kKnobR,
    true,
    3.5f,
    7.0f
};

struct ModuleDef { const char* name; const char* role; };
static const ModuleDef kModules[kNumRows] =
{
    { "DUST",  "SAMPLER" },
    { "CHEW",  "TAPE"    },
    { "MURK",  "ROOM"    },
    { "VINYL", "RECORD"  },
};

// ── Constructor / Destructor ──────────────────────────────────────────────────
LizardSuiteEditor::LizardSuiteEditor (LizardSuiteProcessor& p)
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

LizardSuiteEditor::~LizardSuiteEditor() { stopTimer(); }

// ── Timer ─────────────────────────────────────────────────────────────────────
void LizardSuiteEditor::timerCallback()
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
juce::Point<float> LizardSuiteEditor::knobCenter (int index) const
{
    const auto& k = kKnobDefs[index];
    const float areaCentre = (kKnobAreaX + kKnobAreaR) * 0.5f;
    const float x = areaCentre + ((float)k.posInRow - (float)(k.rowCount - 1) * 0.5f) * kKnobStep;
    return { x, kRowY[k.row] };
}

int LizardSuiteEditor::knobHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumKnobs; ++i)
        if (pos.getDistanceFrom (knobCenter (i)) < kArcR + 8.f)
            return i;
    return -1;
}

// ── Param helpers ─────────────────────────────────────────────────────────────
float LizardSuiteEditor::getNorm (int i) const
{
    auto* p = proc.apvts.getParameter (kKnobDefs[i].paramId);
    return p ? p->getValue() : 0.f;
}

void LizardSuiteEditor::setNorm (int i, float norm)
{
    auto* p = proc.apvts.getParameter (kKnobDefs[i].paramId);
    if (p) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
}

juce::String LizardSuiteEditor::getValueText (int i) const
{
    auto* rp = proc.apvts.getParameter (kKnobDefs[i].paramId);
    if (!rp) return "";

    const float v = rp->convertFrom0to1 (rp->getValue());
    const juce::String id = kKnobDefs[i].paramId;

    auto hz = [] (float f) {
        return f >= 1000.f ? juce::String (f / 1000.f, 1) + " kHz"
                           : juce::String ((int)std::round (f)) + " Hz";
    };

    if (id == "dustRate")    return hz (v);
    if (id == "dustBits")    return juce::String ((int)std::round (v)) + " bit";
    if (id == "dustLowpass") return v <= 0.f ? juce::String ("OFF") : hz (v);
    if (id == "dustDrive")   return juce::String (v, 2) + "x";
    if (id == "chewRate")    return juce::String (v, 1) + " /s";
    if (id == "chewSmooth")  return juce::String (v, 1) + " ms";
    if (id == "vinylSeed")   return "#" + juce::String ((int)std::round (v));
    return juce::String ((int)std::round (v * 100.f)) + "%";
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void LizardSuiteEditor::mouseDown (const juce::MouseEvent& e)
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

void LizardSuiteEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 150.f;
    if (e.mods.isShiftDown()) delta *= 0.1f;
    setNorm (draggingKnob, dragStartVal + delta);
    repaint();
}

void LizardSuiteEditor::mouseUp (const juce::MouseEvent&) { draggingKnob = -1; }

void LizardSuiteEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;

    auto* rp = proc.apvts.getParameter (kKnobDefs[k].paramId);
    if (!rp) return;

    auto* box = new juce::AlertWindow ("Enter value", rp->getName (64),
                                        juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("val", juce::String (rp->convertFrom0to1 (rp->getValue())));
    box->addButton ("OK",     1);
    box->addButton ("Cancel", 0);
    box->enterModalState (true,
        juce::ModalCallbackFunction::create ([box, rp](int result)
        {
            if (result == 1)
                rp->setValueNotifyingHost (rp->convertTo0to1 (
                    box->getTextEditorContents ("val").getFloatValue()));
        }), true);
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void LizardSuiteEditor::paint (juce::Graphics& g)
{
    SharedEditorUtils::drawChassis   (g, getLocalBounds().toFloat());
    drawHeader       (g);
    drawModuleStrips (g);
    drawAllKnobs     (g);
    SharedEditorUtils::drawScrews    (g, kW, kH);
    SharedEditorUtils::drawScanLines (g, getLocalBounds().toFloat(), 0.012f);

    if (logoDrawable)
    {
        const float sz = 56.f;
        juce::Rectangle<float> bounds (kW - sz - 34.f, 20.f, sz, sz);
        logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.35f);
    }
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
void LizardSuiteEditor::drawHeader (juce::Graphics& g)
{
    const float nameY = 22.f;
    g.setFont (rajdhaniBold.withHeight (30.f));

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawText ("LIZARD SUITE", 0, (int)nameY + 1, (int)kW, 36, juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("LIZARD SUITE", 0, (int)nameY - 1, (int)kW, 36, juce::Justification::centred);

    g.setColour (juce::Colour (0xffa09890));
    g.drawText ("LIZARD SUITE", 0, (int)nameY, (int)kW, 36, juce::Justification::centred);

    g.setColour (kAccent.withAlpha (0.5f));
    g.drawLine (kW * 0.32f, nameY + 40.f, kW * 0.68f, nameY + 40.f, 1.f);

    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (juce::Colour (0xff7a746c));
    g.drawText ("LZ-001  LO-FI CHAIN", 30, (int)nameY + 44, 200, 12,
                juce::Justification::left);
    g.drawText ("DUST > CHEW > MURK > VINYL", 0, (int)nameY + 44, (int)kW, 12,
                juce::Justification::centred);
}

void LizardSuiteEditor::drawModuleStrips (juce::Graphics& g)
{
    const float panelW = kW - 2.f * kPanelX;
    const float radius = 6.f;

    for (int r = 0; r < kNumRows; ++r)
    {
        const float panelY = kRowY[r] - kPanelH * 0.5f;
        juce::Rectangle<float> panel (kPanelX, panelY, panelW, kPanelH);

        g.setColour (juce::Colour (0xff242424));
        g.fillRoundedRectangle (panel, radius);
        g.setColour (juce::Colour (0xff1a1a1a));
        g.drawRoundedRectangle (panel, radius, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawLine (kPanelX + radius, panelY, kPanelX + panelW - radius, panelY, 1.f);

        // Module name block on the left
        const float blockX = kPanelX + 18.f;
        g.setFont (shareTechMono.withHeight (8.f));
        g.setColour (kAccent.withAlpha (0.45f));
        g.drawText ("0" + juce::String (r + 1), (int)blockX, (int)kRowY[r] - 30, 40, 11,
                    juce::Justification::left);

        g.setFont (rajdhaniBold.withHeight (24.f));
        g.setColour (kAccent.withAlpha (0.85f));
        g.drawText (kModules[r].name, (int)blockX, (int)kRowY[r] - 18, 120, 26,
                    juce::Justification::left);

        g.setFont (shareTechMono.withHeight (8.5f));
        g.setColour (juce::Colour (0xff7a746c));
        g.drawText (kModules[r].role, (int)blockX, (int)kRowY[r] + 10, 120, 11,
                    juce::Justification::left);

        // Divider between the name block and the knobs
        g.setColour (kAccent.withAlpha (0.15f));
        g.drawLine (kKnobAreaX - 22.f, panelY + 14.f, kKnobAreaX - 22.f, panelY + kPanelH - 14.f, 0.8f);

        // Signal-flow arrow into the next strip
        if (r + 1 < kNumRows)
        {
            const float ax = blockX + 4.f;
            const float ay = panelY + kPanelH + 2.f;
            juce::Path arrow;
            arrow.addTriangle (ax - 4.f, ay, ax + 4.f, ay, ax, ay + 7.f);
            g.setColour (kAccent.withAlpha (0.5f));
            g.fillPath (arrow);
        }
    }
}

void LizardSuiteEditor::drawAllKnobs (juce::Graphics& g)
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        SharedEditorUtils::drawKnob (g, c.x, c.y, cachedNorm[i], kKnobDefs[i].label,
                                     getValueText (i), kKnobStyle, shareTechMono);
    }
}
