#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
// Colours — amber accent from spec, same chassis/metal/silk as reverse_reverb
static const juce::Colour cChassis { 0xff141414 };
static const juce::Colour cMetal   { 0xff2e2e2e };
static const juce::Colour cEdge    { 0xff0a0a0a };
static const juce::Colour cAmber   { 0xffe8820a };  // accent — replaces blue
static const juce::Colour cTextDim { 0xff7a746c };
static const juce::Colour cSilk    { 0xffa09890 };

// Layout
static constexpr int   kW        = 480;
static constexpr int   kH        = 280;
static constexpr float kKnobR    = 32.0f;
static constexpr float kKnobSpX  = 130.0f;
static constexpr float kKnobY    = 150.0f;
static constexpr int   kNumKnobs = 3;

//==============================================================================
PitchWobbleEditor::PitchWobbleEditor (PitchWobbleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (kW, kH);

    rajdhaniBold = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::RajdhaniBold_ttf,
            BinaryData::RajdhaniBold_ttfSize)));

    shareTechMono = juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::ShareTechMonoRegular_ttf,
            BinaryData::ShareTechMonoRegular_ttfSize)));

    logoDrawable = juce::Drawable::createFromImageData (
        BinaryData::logo_transparent_svg,
        BinaryData::logo_transparent_svgSize);

    // Ghost sliders for FL Studio automation — invisible, no mouse interception
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
// Layout
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
    // Position ghost sliders over each knob for right-click automation access
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
// Param helpers
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
// Mouse
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
// Paint
//==============================================================================
void PitchWobbleEditor::paint (juce::Graphics& g)
{
    drawChassis   (g);
    drawPlugin    (g);
    drawScrews    (g);
    drawScanLines (g, getLocalBounds().toFloat(), 0.012f);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawChassis (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (cMetal);
    g.fillRect (b);
    g.setColour (juce::Colour (0xff333333));
    g.drawRect (b, 1.0f);
    g.setColour (juce::Colour (0xff444444));
    g.drawLine (b.getX(), b.getY(), b.getRight(), b.getY(), 2.0f);
    g.setColour (cEdge);
    g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.0f);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawScanLines (juce::Graphics& g,
                                        juce::Rectangle<float> area,
                                        float opacity)
{
    g.setColour (juce::Colours::white.withAlpha (opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.0f)
        g.drawHorizontalLine ((int)y, area.getX(), area.getRight());
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawScrews (juce::Graphics& g)
{
    const float inset = 8.0f, d = 12.0f, r = d * 0.5f;
    float W = (float)kW, H = (float)kH;
    juce::Point<float> corners[4] = {
        { inset + r, inset + r }, { W - inset - r, inset + r },
        { inset + r, H - inset - r }, { W - inset - r, H - inset - r }
    };
    for (auto& c : corners)
    {
        juce::ColourGradient grad (juce::Colour (0xff3a3a3a), c.x - r*0.4f, c.y - r*0.35f,
                                   juce::Colour (0xff111111), c.x + r, c.y + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, d, d);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - r, c.y - r, d, d, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - r + 1, c.y - r + 1, d - 2, d - 2, 0.8f);
        float s = r - 2.0f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (c.x - s, c.y, c.x + s, c.y, 1.5f);
        g.drawLine (c.x, c.y - s, c.x, c.y + s, 1.5f);
    }
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawPlugin (juce::Graphics& g)
{
    float W = (float)kW;

    // Module ID top-left
    g.setFont (shareTechMono.withHeight (8.0f));
    g.setColour (cTextDim);
    g.drawText ("02 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    // Plugin name centred near top — same engraved treatment as reverse_reverb
    float nameY = 30.0f;
    g.setFont (rajdhaniBold.withHeight (22.0f));

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("PITCH WOBBLE", 0, (int)nameY + 1, (int)W, 28, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawText ("PITCH WOBBLE", 0, (int)nameY - 1, (int)W, 28, juce::Justification::centred);
    g.setColour (cSilk);
    g.drawText ("PITCH WOBBLE", 0, (int)nameY, (int)W, 28, juce::Justification::centred);

    // Thin amber accent line under name
    g.setColour (cAmber.withAlpha (0.4f));
    g.drawLine (W * 0.25f, nameY + 31.0f, W * 0.75f, nameY + 31.0f, 1.0f);

    // Three knobs
    const char* labels[] = { "DEPTH", "RATE", "SMOOTH" };
    float norms[] = { normDepth(), normRate(), normSmooth() };

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter (i);
        drawKnob (g, c.x, c.y, norms[i], labels[i], formatValue (i));
    }

    // Category badge bottom-centre
    float badgeY = (float)kH - 22.0f;
    float dotR   = 3.0f;
    float dotX   = W * 0.5f - 36.0f;

    // LED glow
    g.setColour (cAmber.withAlpha (0.35f));
    g.fillEllipse (dotX - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    // LED dot
    g.setColour (cAmber);
    g.fillEllipse (dotX - dotR, badgeY - dotR, dotR * 2, dotR * 2);

    g.setFont (shareTechMono.withHeight (8.0f));
    g.setColour (cTextDim);
    g.drawText ("MODULATION", (int)(dotX + 6), (int)(badgeY - 5), 70, 10,
                juce::Justification::centredLeft);

    // Logo — bottom-right, same size and placement as reverse_reverb
    if (logoDrawable != nullptr)
    {
        const int logoSize = 80;
        const int margin   = 14;
        juce::Rectangle<float> bounds (
            W - logoSize - margin,
            kH - logoSize - margin,
            logoSize, logoSize);
        logoDrawable->drawWithin (g, bounds,
                                  juce::RectanglePlacement::centred, 0.4f);
    }
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawKnob (juce::Graphics& g,
                                   float cx, float cy, float value,
                                   const juce::String& label,
                                   const juce::String& valueText)
{
    float r = kKnobR;

    float arcR     = r + 6.0f;
    float startAng = juce::MathConstants<float>::pi * 1.2f;
    float endAng   = juce::MathConstants<float>::pi * 2.8f;
    float valueAng = startAng + value * (endAng - startAng);

    // Inactive arc
    juce::Path inactiveArc;
    inactiveArc.addArc (cx - arcR, cy - arcR, arcR * 2, arcR * 2, valueAng, endAng, true);
    g.setColour (juce::Colour (0xff1a1a1a).withAlpha (0.8f));
    g.strokePath (inactiveArc, juce::PathStrokeType (5.0f));

    // Active arc — amber instead of blue
    juce::Path activeArc;
    activeArc.addArc (cx - arcR, cy - arcR, arcR * 2, arcR * 2, startAng, valueAng, true);
    g.setColour (cAmber.withAlpha (0.7f));
    g.strokePath (activeArc, juce::PathStrokeType (5.0f));

    // Knob body — same gradient recipe as reverse_reverb
    juce::ColourGradient bodyGrad (juce::Colour (0xff4a4a4a), cx - r*0.35f, cy - r*0.3f,
                                   juce::Colour (0xff111111), cx + r, cy + r, true);
    bodyGrad.addColour (0.45, juce::Colour (0xff2a2a2a));
    g.setGradientFill (bodyGrad);
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);

    // Shadow ring
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.5f);
    // Top highlight
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawEllipse (cx - r + 1, cy - r + 1, r * 2 - 2, r * 2 - 2, 0.8f);

    // Pointer — amber
    float angle = startAng + value * (endAng - startAng) - juce::MathConstants<float>::halfPi;
    float px1 = cx + std::cos (angle) * r * 0.25f;
    float py1 = cy + std::sin (angle) * r * 0.25f;
    float px2 = cx + std::cos (angle) * r * 0.78f;
    float py2 = cy + std::sin (angle) * r * 0.78f;

    g.setColour (cAmber.withAlpha (0.7f));
    g.drawLine (px1, py1, px2, py2, 3.5f);
    g.setColour (cAmber);
    g.drawLine (px1, py1, px2, py2, 2.0f);

    // Label above knob — amber (matches spec: parameter labels are amber)
    g.setFont (shareTechMono.withHeight (8.0f));
    g.setColour (cAmber);
    g.drawText (label, (int)(cx - 36), (int)(cy - arcR - 16), 72, 11,
                juce::Justification::centred);

    // Value readout below knob
    g.setFont (shareTechMono.withHeight (8.5f));
    g.setColour (cSilk);
    g.drawText (valueText, (int)(cx - 36), (int)(cy + arcR + 6), 72, 11,
                juce::Justification::centred);
}

//==============================================================================
juce::AudioProcessorEditor* PitchWobbleProcessor::createEditor()
{
    return new PitchWobbleEditor (*this);
}