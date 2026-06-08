#include "PluginEditor.h"
#include "SharedEditorUtils.h"

static const juce::Colour cGreen   { 0xff4ecf6a };
static const juce::Colour cTextDim { 0xff7a746c };
static const juce::Colour cSilk    { 0xffa09890 };

static constexpr int   kW        = 540;
static constexpr int   kH        = 300;
static constexpr float kKnobR    = 32.0f;
static constexpr float kKnobSpX  = 120.0f;
static constexpr float kKnobY    = 158.0f;
static constexpr int   kNumKnobs = 4;

static const SharedEditorUtils::KnobStyle kKnobStyle {
    cGreen,
    juce::Colour (0xff4a4a4a), juce::Colour (0xff2a2a2a), juce::Colour (0xff111111),
    kKnobR
};

//==============================================================================
ThroughTheWallAudioProcessorEditor::ThroughTheWallAudioProcessorEditor(
    ThroughTheWallAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(kW, kH);
    rajdhaniBold  = SharedEditorUtils::loadRajdhaniBold();
    shareTechMono = SharedEditorUtils::loadShareTechMono();
    logoDrawable  = SharedEditorUtils::loadLogo();
    startTimerHz(30);
}

ThroughTheWallAudioProcessorEditor::~ThroughTheWallAudioProcessorEditor() { stopTimer(); }

void ThroughTheWallAudioProcessorEditor::timerCallback()
{
    float t = normThickness(), b = normBleed(),
          r = normRattle(),    d = normDistance();
    if (t != thicknessVal || b != bleedVal ||
        r != rattleVal    || d != distanceVal)
    {
        thicknessVal = t; bleedVal = b;
        rattleVal    = r; distanceVal = d;
        repaint();
    }
}

//==============================================================================
juce::Point<float> ThroughTheWallAudioProcessorEditor::knobCenter(int index) const
{
    float totalW = kKnobSpX * (kNumKnobs - 1);
    float startX = ((float)kW - totalW) * 0.5f;
    return { startX + index * kKnobSpX, kKnobY };
}

int ThroughTheWallAudioProcessorEditor::knobHitTest(juce::Point<float> pos) const
{
    for (int i = 0; i < kNumKnobs; ++i)
        if (pos.getDistanceFrom(knobCenter(i)) <= kKnobR + 8.0f)
            return i;
    return -1;
}

//==============================================================================
float ThroughTheWallAudioProcessorEditor::normThickness() const
{
    auto* p = audioProcessor.apvts.getParameter("thickness");
    return p ? p->getValue() : 0.0f;
}
float ThroughTheWallAudioProcessorEditor::normBleed() const
{
    auto* p = audioProcessor.apvts.getParameter("bleed");
    return p ? p->getValue() : 0.0f;
}
float ThroughTheWallAudioProcessorEditor::normRattle() const
{
    auto* p = audioProcessor.apvts.getParameter("rattle");
    return p ? p->getValue() : 0.0f;
}
float ThroughTheWallAudioProcessorEditor::normDistance() const
{
    auto* p = audioProcessor.apvts.getParameter("distance");
    return p ? p->getValue() : 0.0f;
}

void ThroughTheWallAudioProcessorEditor::setNorm(int idx, float norm)
{
    norm = juce::jlimit(0.0f, 1.0f, norm);
    const char* ids[] = { "thickness", "bleed", "rattle", "distance" };
    if (idx >= 0 && idx < kNumKnobs)
        if (auto* p = audioProcessor.apvts.getParameter(ids[idx]))
            p->setValueNotifyingHost(norm);
}

//==============================================================================
void ThroughTheWallAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    int k = knobHitTest(e.position);
    if (k >= 0)
    {
        draggingKnob = k;
        dragStartY   = e.position.y;
        float norms[] = { normThickness(), normBleed(), normRattle(), normDistance() };
        dragStartVal = norms[k];
    }
}

void ThroughTheWallAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 140.0f;
    setNorm(draggingKnob, dragStartVal + delta);
    repaint();
}

void ThroughTheWallAudioProcessorEditor::mouseUp(const juce::MouseEvent&) { draggingKnob = -1; }

void ThroughTheWallAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    int k = knobHitTest(e.position);
    if (k < 0) return;

    const char* ids[]   = { "thickness", "bleed", "rattle", "distance" };
    const char* names[] = { "Wall Thickness", "Room Bleed", "Wall Rattle", "Distance" };
    auto* param = audioProcessor.apvts.getParameter(ids[k]);
    if (!param) return;

    auto* box = new juce::AlertWindow("Enter value", names[k],
                                       juce::MessageBoxIconType::NoIcon);
    box->addTextEditor("val", juce::String(param->getValue(), 2));
    box->addButton("OK", 1);
    box->addButton("Cancel", 0);
    box->enterModalState(true, juce::ModalCallbackFunction::create(
        [box, param](int result) {
            if (result == 1)
                param->setValueNotifyingHost(
                    juce::jlimit(0.0f, 1.0f,
                        box->getTextEditorContents("val").getFloatValue()));
        }), true);
}

//==============================================================================
void ThroughTheWallAudioProcessorEditor::paint(juce::Graphics& g)
{
    SharedEditorUtils::drawChassis   (g, getLocalBounds().toFloat());
    drawPlugin    (g);
    SharedEditorUtils::drawScrews    (g, kW, kH);
    SharedEditorUtils::drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

void ThroughTheWallAudioProcessorEditor::resized() {}

//==============================================================================
void ThroughTheWallAudioProcessorEditor::drawPlugin(juce::Graphics& g)
{
    float W = (float)kW;

    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("03 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    float nameY = 28.0f;
    g.setFont(rajdhaniBold.withHeight(22.0f));
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText("THROUGH THE WALL", 0, (int)nameY + 1, (int)W, 28, juce::Justification::centred);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawText("THROUGH THE WALL", 0, (int)nameY - 1, (int)W, 28, juce::Justification::centred);
    g.setColour(cSilk);
    g.drawText("THROUGH THE WALL", 0, (int)nameY, (int)W, 28, juce::Justification::centred);

    g.setColour(cGreen.withAlpha(0.4f));
    g.drawLine(W * 0.25f, nameY + 31.0f, W * 0.75f, nameY + 31.0f, 1.0f);

    struct KnobData { const char* label; float norm; juce::String val; };
    KnobData knobs[kNumKnobs] = {
        { "WALL THICKNESS", normThickness(), juce::String(normThickness(), 2) },
        { "ROOM BLEED",     normBleed(),     juce::String(normBleed(), 2)     },
        { "WALL RATTLE",    normRattle(),    juce::String(normRattle(), 2)    },
        { "DISTANCE",       normDistance(),  juce::String(normDistance(), 2)  },
    };

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter(i);
        drawKnob(g, c.x, c.y, knobs[i].norm, knobs[i].label, knobs[i].val);
    }

    float badgeY = (float)kH - 22.0f;
    float dotR   = 3.0f;
    float dotX   = W * 0.5f - 34.0f;

    g.setColour(cGreen.withAlpha(0.4f));
    g.fillEllipse(dotX - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    g.setColour(cGreen);
    g.fillEllipse(dotX - dotR, badgeY - dotR, dotR * 2, dotR * 2);

    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("PHYSICAL", (int)(dotX + 6), (int)(badgeY - 5), 56, 10,
               juce::Justification::centredLeft);

    if (logoDrawable != nullptr)
    {
        const int logoSize = 80, margin = 14;
        juce::Rectangle<float> bounds(W - logoSize - margin,
                                       (float)kH - logoSize - margin,
                                       (float)logoSize, (float)logoSize);
        logoDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 0.4f);
    }
}

//==============================================================================
void ThroughTheWallAudioProcessorEditor::drawKnob(juce::Graphics& g,
                                                   float cx, float cy, float value,
                                                   const juce::String& label,
                                                   const juce::String& valueText)
{
    SharedEditorUtils::drawKnob(g, cx, cy, value, label, valueText, kKnobStyle, shareTechMono);
}
