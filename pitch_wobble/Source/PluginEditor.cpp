#include "PluginEditor.h"
#include "SharedEditorUtils.h"

static const juce::Colour cAmber   { 0xffe8820a };
static const juce::Colour cTextDim { 0xff7a746c };
static const juce::Colour cSilk    { 0xffa09890 };

static constexpr int   kW        = 480;
static constexpr int   kH        = 280;
static constexpr float kKnobR    = 32.0f;
static constexpr float kKnobSpX  = 130.0f;
static constexpr float kKnobY    = 150.0f;
static constexpr int   kNumKnobs = 3;

static const SharedEditorUtils::KnobStyle kKnobStyle {
    cAmber,
    juce::Colour (0xff4a4a4a), juce::Colour (0xff2a2a2a), juce::Colour (0xff111111),
    kKnobR
};

//==============================================================================
PitchWobbleEditor::PitchWobbleEditor (PitchWobbleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (kW, kH);
    rajdhaniBold  = SharedEditorUtils::loadRajdhaniBold();
    shareTechMono = SharedEditorUtils::loadShareTechMono();
    logoDrawable  = SharedEditorUtils::loadLogo();

    auto setupGhost = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setInterceptsMouseClicks (false, false);
        s.setAlpha (0.0f);
    };
    setupGhost (ghostDepth);
    setupGhost (ghostRate);
    setupGhost (ghostSmooth);
    addAndMakeVisible (ghostDepth);
    addAndMakeVisible (ghostRate);
    addAndMakeVisible (ghostSmooth);

    attachDepth  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (proc.apvts, "depth",  ghostDepth);
    attachRate   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (proc.apvts, "rate",   ghostRate);
    attachSmooth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (proc.apvts, "smooth", ghostSmooth);

    startTimerHz (30);
}

PitchWobbleEditor::~PitchWobbleEditor() { stopTimer(); }

//==============================================================================
void PitchWobbleEditor::timerCallback()
{
    float d = normDepth(), r = normRate(), s = normSmooth();
    if (d != cachedDepth || r != cachedRate || s != cachedSmooth)
    {
        cachedDepth = d; cachedRate = r; cachedSmooth = s;
        repaint();
    }
}

//==============================================================================
juce::Point<float> PitchWobbleEditor::knobCenter (int index) const
{
    float totalW = kKnobSpX * (kNumKnobs - 1);
    float startX = ((float)kW - totalW) * 0.5f;
    return { startX + index * kKnobSpX, kKnobY };
}

int PitchWobbleEditor::knobHitTest (juce::Point<float> pos) const
{
    for (int i = 0; i < kNumKnobs; ++i)
        if (pos.getDistanceFrom (knobCenter (i)) <= kKnobR + 8.0f)
            return i;
    return -1;
}

void PitchWobbleEditor::resized()
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        juce::Rectangle<int> r ((int)(c.x - kKnobR), (int)(c.y - kKnobR),
                                 (int)(kKnobR * 2),   (int)(kKnobR * 2));
        if (i == 0) ghostDepth .setBounds (r);
        if (i == 1) ghostRate  .setBounds (r);
        if (i == 2) ghostSmooth.setBounds (r);
    }
}

//==============================================================================
float PitchWobbleEditor::normDepth() const
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter ("depth"));
    return p ? p->getValue() : 0.0f;
}
float PitchWobbleEditor::normRate() const
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter ("rate"));
    return p ? p->getValue() : 0.0f;
}
float PitchWobbleEditor::normSmooth() const
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter ("smooth"));
    return p ? p->getValue() : 0.0f;
}

void PitchWobbleEditor::setNorm (int idx, float norm)
{
    norm = juce::jlimit (0.0f, 1.0f, norm);
    const char* ids[] = { "depth", "rate", "smooth" };
    if (idx < 0 || idx >= kNumKnobs) return;
    if (auto* p = proc.apvts.getParameter (ids[idx]))
        p->setValueNotifyingHost (norm);
}

juce::String PitchWobbleEditor::formatValue (int idx) const
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*> (
        proc.apvts.getParameter (idx == 0 ? "depth" : idx == 1 ? "rate" : "smooth"));
    if (!p) return {};
    float v = p->convertFrom0to1 (p->getValue());
    if (idx == 0) return juce::String (v, 1) + " ct";
    if (idx == 1) return juce::String (v, 2) + " Hz";
    return juce::String (v, 2);
}

//==============================================================================
void PitchWobbleEditor::mouseDown (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k >= 0)
    {
        draggingKnob = k;
        dragStartY   = e.position.y;
        dragStartVal = (k == 0) ? normDepth() : (k == 1) ? normRate() : normSmooth();
    }
}

void PitchWobbleEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 140.0f;
    setNorm (draggingKnob, dragStartVal + delta);
    repaint();
}

void PitchWobbleEditor::mouseUp (const juce::MouseEvent&) { draggingKnob = -1; }

void PitchWobbleEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int k = knobHitTest (e.position);
    if (k < 0) return;

    const char* ids[] = { "depth", "rate", "smooth" };
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (
                      proc.apvts.getParameter (ids[k]));
    if (!param) return;

    float current = param->convertFrom0to1 (param->getValue());
    auto* box = new juce::AlertWindow ("Enter value",
                                       param->getName (64),
                                       juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("val", juce::String (current));
    box->addButton ("OK", 1);
    box->addButton ("Cancel", 0);
    box->enterModalState (true,
        juce::ModalCallbackFunction::create ([box, param] (int result)
        {
            if (result == 1)
            {
                float v = box->getTextEditorContents ("val").getFloatValue();
                param->setValueNotifyingHost (param->convertTo0to1 (
                    juce::jlimit (param->getNormalisableRange().start,
                                  param->getNormalisableRange().end, v)));
            }
        }), true);
}

//==============================================================================
void PitchWobbleEditor::paint (juce::Graphics& g)
{
    SharedEditorUtils::drawChassis   (g, getLocalBounds().toFloat());
    drawPlugin    (g);
    SharedEditorUtils::drawScrews    (g, kW, kH);
    SharedEditorUtils::drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

//==============================================================================
void PitchWobbleEditor::drawPlugin (juce::Graphics& g)
{
    float W = (float)kW;

    g.setFont (shareTechMono.withHeight (8.0f));
    g.setColour (cTextDim);
    g.drawText ("02 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    float nameY = 30.0f;
    g.setFont (rajdhaniBold.withHeight (22.0f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("PITCH WOBBLE", 0, (int)nameY + 1, (int)W, 28, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("PITCH WOBBLE", 0, (int)nameY - 1, (int)W, 28, juce::Justification::centred);
    g.setColour (cSilk);
    g.drawText ("PITCH WOBBLE", 0, (int)nameY, (int)W, 28, juce::Justification::centred);

    g.setColour (cAmber.withAlpha (0.4f));
    g.drawLine (W * 0.25f, nameY + 31.0f, W * 0.75f, nameY + 31.0f, 1.0f);

    const char* labels[] = { "DEPTH", "RATE", "SMOOTH" };
    float norms[] = { normDepth(), normRate(), normSmooth() };

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        drawKnob (g, c.x, c.y, norms[i], labels[i], formatValue (i));
    }

    float badgeY = (float)kH - 22.0f;
    float dotR   = 3.0f;
    float dotX   = W * 0.5f - 36.0f;

    g.setColour (cAmber.withAlpha (0.35f));
    g.fillEllipse (dotX - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    g.setColour (cAmber);
    g.fillEllipse (dotX - dotR, badgeY - dotR, dotR * 2, dotR * 2);

    g.setFont (shareTechMono.withHeight (8.0f));
    g.setColour (cTextDim);
    g.drawText ("MODULATION", (int)(dotX + 6), (int)(badgeY - 5), 70, 10,
                juce::Justification::centredLeft);

    if (logoDrawable != nullptr)
    {
        const int logoSize = 80, margin = 14;
        juce::Rectangle<float> bounds (W - logoSize - margin, kH - logoSize - margin,
                                        logoSize, logoSize);
        logoDrawable->drawWithin (g, bounds, juce::RectanglePlacement::centred, 0.4f);
    }
}

//==============================================================================
void PitchWobbleEditor::drawKnob (juce::Graphics& g,
                                   float cx, float cy, float value,
                                   const juce::String& label,
                                   const juce::String& valueText)
{
    SharedEditorUtils::drawKnob (g, cx, cy, value, label, valueText, kKnobStyle, shareTechMono);
}

//==============================================================================
juce::AudioProcessorEditor* PitchWobbleProcessor::createEditor()
{
    return new PitchWobbleEditor (*this);
}
