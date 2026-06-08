#include "PluginEditor.h"

static const juce::Colour cAmber   { 0xffe8820a };
static const juce::Colour cBlue    { 0xff3aace8 };
static const juce::Colour cTextDim { 0xff7a746c };
static const juce::Colour cSilk    { 0xffa09890 };

static constexpr int   kW        = 520;
static constexpr int   kH        = 380;
static constexpr float kKnobR    = 32.0f;
static constexpr float kKnobSpX  = 140.0f;
static constexpr float kKnobY    = 190.0f;
static constexpr int   kNumKnobs = 3;

static const SharedEditorUtils::KnobStyle kKnobStyle {
    cBlue,
    juce::Colour (0xff4a4a4a), juce::Colour (0xff2a2a2a), juce::Colour (0xff111111),
    kKnobR
};

//==============================================================================
ReverseReverbAudioProcessorEditor::ReverseReverbAudioProcessorEditor(ReverseReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(kW, kH);
    rajdhaniBold  = SharedEditorUtils::loadRajdhaniBold();
    shareTechMono = SharedEditorUtils::loadShareTechMono();
    logoDrawable  = SharedEditorUtils::loadLogo();
    startTimerHz(30);
}

ReverseReverbAudioProcessorEditor::~ReverseReverbAudioProcessorEditor() { stopTimer(); }

void ReverseReverbAudioProcessorEditor::timerCallback()
{
    float r = normRoom(), w = normWet(), win = normWindow();
    if (r != roomVal || w != wetVal || win != windowVal)
    {
        roomVal = r; wetVal = w; windowVal = win;
        repaint();
    }
}

//==============================================================================
juce::Point<float> ReverseReverbAudioProcessorEditor::knobCenter(int index) const
{
    float totalW = kKnobSpX * (kNumKnobs - 1);
    float startX = ((float)kW - totalW) * 0.5f;
    return { startX + index * kKnobSpX, kKnobY };
}

int ReverseReverbAudioProcessorEditor::knobHitTest(juce::Point<float> pos) const
{
    for (int i = 0; i < kNumKnobs; ++i)
        if (pos.getDistanceFrom(knobCenter(i)) <= kKnobR + 8.0f)
            return i;
    return -1;
}

//==============================================================================
float ReverseReverbAudioProcessorEditor::normRoom() const
{
    auto* p = audioProcessor.apvts.getParameter("roomSize");
    return p ? p->getValue() : 0.0f;
}
float ReverseReverbAudioProcessorEditor::normWet() const
{
    auto* p = audioProcessor.apvts.getParameter("wetMix");
    return p ? p->getValue() : 0.0f;
}
float ReverseReverbAudioProcessorEditor::normWindow() const
{
    auto* p = audioProcessor.apvts.getParameter("windowMs");
    return p ? p->getValue() : 0.0f;
}
void ReverseReverbAudioProcessorEditor::setNorm(int idx, float norm)
{
    norm = juce::jlimit(0.0f, 1.0f, norm);
    const char* ids[] = { "roomSize", "wetMix", "windowMs" };
    if (idx >= 0 && idx < kNumKnobs)
        if (auto* p = audioProcessor.apvts.getParameter(ids[idx]))
            p->setValueNotifyingHost(norm);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    int k = knobHitTest(e.position);
    if (k >= 0)
    {
        draggingKnob = k;
        dragStartY   = e.position.y;
        dragStartVal = (k == 0) ? normRoom() : (k == 1) ? normWet() : normWindow();
    }
}
void ReverseReverbAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 140.0f;
    setNorm(draggingKnob, dragStartVal + delta);
    repaint();
}
void ReverseReverbAudioProcessorEditor::mouseUp(const juce::MouseEvent&) { draggingKnob = -1; }

void ReverseReverbAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    int k = knobHitTest(e.position);
    if (k < 0) return;
    const char* ids[] = { "roomSize", "wetMix", "windowMs" };
    auto* param = dynamic_cast<juce::RangedAudioParameter*>(audioProcessor.apvts.getParameter(ids[k]));
    if (!param) return;

    float current = param->convertFrom0to1(param->getValue());
    auto* box = new juce::AlertWindow("Enter value", param->getName(64),
                                       juce::MessageBoxIconType::NoIcon);
    box->addTextEditor("val", juce::String(current));
    box->addButton("OK", 1);
    box->addButton("Cancel", 0);
    box->enterModalState(true, juce::ModalCallbackFunction::create([box, param](int result) {
        if (result == 1)
        {
            float v = box->getTextEditorContents("val").getFloatValue();
            param->setValueNotifyingHost(param->convertTo0to1(
                juce::jlimit(param->getNormalisableRange().start,
                             param->getNormalisableRange().end, v)));
        }
    }), true);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    SharedEditorUtils::drawChassis(g, getLocalBounds().toFloat());
    drawPlugin(g);
    SharedEditorUtils::drawScrews(g, kW, kH);
    SharedEditorUtils::drawScanLines(g, getLocalBounds().toFloat(), 0.012f);
}

void ReverseReverbAudioProcessorEditor::resized() {}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawPlugin(juce::Graphics& g)
{
    float W = (float)kW;

    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("01 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    float nameY = 32.0f;
    g.setFont(rajdhaniBold.withHeight(24.0f));
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText("REVERSE REVERB", 0, (int)nameY + 1, (int)W, 30, juce::Justification::centred);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawText("REVERSE REVERB", 0, (int)nameY - 1, (int)W, 30, juce::Justification::centred);
    g.setColour(cSilk);
    g.drawText("REVERSE REVERB", 0, (int)nameY, (int)W, 30, juce::Justification::centred);

    g.setColour(cBlue.withAlpha(0.4f));
    g.drawLine(W * 0.25f, nameY + 33.0f, W * 0.75f, nameY + 33.0f, 1.0f);

    auto* rs = audioProcessor.apvts.getParameter("roomSize");
    auto* wt = audioProcessor.apvts.getParameter("wetMix");
    auto* ws = dynamic_cast<juce::RangedAudioParameter*>(audioProcessor.apvts.getParameter("windowMs"));

    struct KnobData { const char* label; float norm; juce::String val; };
    KnobData knobs[3] = {
        { "ROOM SIZE", normRoom(),   juce::String(rs ? rs->convertFrom0to1(rs->getValue()) : 0.f, 2) },
        { "WET MIX",   normWet(),    juce::String(wt ? wt->convertFrom0to1(wt->getValue()) : 0.f, 2) },
        { "WINDOW MS", normWindow(), ws ? juce::String((int)ws->convertFrom0to1(ws->getValue())) + " ms" : "-" }
    };

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter(i);
        drawKnob(g, c.x, c.y, knobs[i].norm, knobs[i].label, knobs[i].val);
    }

    float badgeY = (float)kH - 22.0f;
    float dotR   = 3.0f;
    float dotX   = W * 0.5f - 32.0f;

    g.setColour(cBlue.withAlpha(0.4f));
    g.fillEllipse(dotX - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    g.setColour(cBlue);
    g.fillEllipse(dotX - dotR, badgeY - dotR, dotR * 2, dotR * 2);

    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("SPATIAL", (int)(dotX + 5), (int)(badgeY - 5), 50, 10,
               juce::Justification::centredLeft);

    if (logoDrawable != nullptr)
    {
        const int logoSize = 80, margin = 14;
        juce::Rectangle<float> bounds(W - logoSize - margin, kH - logoSize - margin,
                                       logoSize, logoSize);
        logoDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 0.4f);
    }
}

void ReverseReverbAudioProcessorEditor::drawKnob(juce::Graphics& g,
                                                  float cx, float cy, float value,
                                                  const juce::String& label,
                                                  const juce::String& valueText)
{
    SharedEditorUtils::drawKnob(g, cx, cy, value, label, valueText, kKnobStyle, shareTechMono);
}
