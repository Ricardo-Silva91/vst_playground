#include "PluginEditor.h"
#include "SharedEditorUtils.h"

static const juce::Colour cAccent  { 0xffe83a2a };
static const juce::Colour cTextDim { 0xff7a746c };
static const juce::Colour cSilk    { 0xffa09890 };

static constexpr int   kW        = 580;
static constexpr int   kH        = 340;
static constexpr float kKnobR    = 32.f;
static constexpr int   kNumKnobs = 5;
static constexpr float kKnobSpX  = 96.f;
static constexpr float kKnobY    = 210.f;

struct KnobDef { const char* paramId; const char* label; const char* unit; };
static const KnobDef kKnobs[kNumKnobs] = {
    { "drive",         "DRIVE",  ""   },
    { "bitDepth",      "CRUSH",  "bit"},
    { "noiseAmount",   "NOISE",  ""   },
    { "compThreshold", "COMP",   "dB" },
    { "reverbRoom",    "VERB",   ""   },
};

static const SharedEditorUtils::KnobStyle kKnobStyle {
    cAccent,
    juce::Colour (0xff3a1a14), juce::Colour (0xff200e0a), juce::Colour (0xff060504),
    kKnobR
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
    rajdhaniBold  = SharedEditorUtils::loadRajdhaniBold();
    shareTechMono = SharedEditorUtils::loadShareTechMono();
    logoDrawable  = SharedEditorUtils::loadLogo();

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
    SharedEditorUtils::drawChassis   (g, getLocalBounds().toFloat());
    drawPlugin    (g);
    SharedEditorUtils::drawScrews    (g, kW, kH);
    SharedEditorUtils::drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

// ── Plugin ────────────────────────────────────────────────────────────────────
void DrumSmashEditor::drawPlugin (juce::Graphics& g)
{
    float W = (float)kW;

    g.setFont (shareTechMono.withHeight(8.f));
    g.setColour (cTextDim);
    g.drawText ("04 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    float nameY = 32.f;
    g.setFont (rajdhaniBold.withHeight(24.f));
    g.setColour (juce::Colours::black.withAlpha(0.6f));
    g.drawText ("DRUM SMASH", 0, (int)nameY+1, (int)W, 30, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha(0.07f));
    g.drawText ("DRUM SMASH", 0, (int)nameY-1, (int)W, 30, juce::Justification::centred);
    g.setColour (cSilk);
    g.drawText ("DRUM SMASH", 0, (int)nameY,   (int)W, 30, juce::Justification::centred);

    g.setColour (cAccent.withAlpha(0.4f));
    g.drawLine (W*0.3f, nameY+33.f, W*0.7f, nameY+33.f, 1.f);

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        drawKnob (g, c.x, c.y, getNorm(i), kKnobs[i].label, getValueText(i));
    }

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

    if (logoDrawable)
    {
        const float sz = 64.f, margin = 14.f;
        juce::Rectangle<float> bounds (W - sz - margin, kH - sz - margin, sz, sz);
        logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.4f);
    }
}

// ── Knob ──────────────────────────────────────────────────────────────────────
void DrumSmashEditor::drawKnob (juce::Graphics& g,
                                  float cx, float cy, float norm,
                                  const juce::String& label,
                                  const juce::String& val)
{
    SharedEditorUtils::drawKnob (g, cx, cy, norm, label, val, kKnobStyle, shareTechMono);
}

// ── createEditor ──────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new DrumSmashEditor (*this);
}
