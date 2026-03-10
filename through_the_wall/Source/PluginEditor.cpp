#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
static const juce::Colour cChassis { 0xff141414 };
static const juce::Colour cMetal   { 0xff2e2e2e };
static const juce::Colour cEdge    { 0xff0a0a0a };
static const juce::Colour cGreen   { 0xff4ecf6a };   // accent — spec #4ECF6A
static const juce::Colour cAmber   { 0xffe8820a };
static const juce::Colour cTextDim { 0xff7a746c };
static const juce::Colour cSilk    { 0xffa09890 };

// Layout — 4 knobs, 540×300
static constexpr int   kW        = 540;
static constexpr int   kH        = 300;
static constexpr float kKnobR    = 32.0f;
static constexpr float kKnobSpX  = 120.0f;
static constexpr float kKnobY    = 158.0f;
static constexpr int   kNumKnobs = 4;

//==============================================================================
ThroughTheWallAudioProcessorEditor::ThroughTheWallAudioProcessorEditor(
    ThroughTheWallAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(kW, kH);

    rajdhaniBold = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::RajdhaniBold_ttf,
                                                BinaryData::RajdhaniBold_ttfSize)));
    shareTechMono = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::ShareTechMonoRegular_ttf,
                                                BinaryData::ShareTechMonoRegular_ttfSize)));
    logoDrawable = juce::Drawable::createFromImageData(
        BinaryData::logo_transparent_svg,
        BinaryData::logo_transparent_svgSize);

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
// Layout
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
// Params
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
// Mouse
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

void ThroughTheWallAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    draggingKnob = -1;
}

void ThroughTheWallAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    int k = knobHitTest(e.position);
    if (k < 0) return;

    const char* ids[]    = { "thickness", "bleed", "rattle", "distance" };
    const char* names[]  = { "Wall Thickness", "Room Bleed", "Wall Rattle", "Distance" };
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
// Paint
//==============================================================================
void ThroughTheWallAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawChassis(g);
    drawPlugin(g);
    drawScrews(g);
    drawScanLines(g, getLocalBounds().toFloat(), 0.012f);
}

void ThroughTheWallAudioProcessorEditor::resized() {}

//==============================================================================
void ThroughTheWallAudioProcessorEditor::drawChassis(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(cMetal);
    g.fillRect(b);
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(b, 1.0f);
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(b.getX(), b.getY(), b.getRight(), b.getY(), 2.0f);
    g.setColour(cEdge);
    g.drawLine(b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.0f);
}

void ThroughTheWallAudioProcessorEditor::drawScanLines(juce::Graphics& g,
                                                        juce::Rectangle<float> area,
                                                        float opacity)
{
    g.setColour(juce::Colours::white.withAlpha(opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.0f)
        g.drawHorizontalLine((int)y, area.getX(), area.getRight());
}

void ThroughTheWallAudioProcessorEditor::drawScrews(juce::Graphics& g)
{
    const float inset = 8.0f, d = 12.0f, r = d * 0.5f;
    float W = (float)kW, H = (float)kH;
    juce::Point<float> corners[4] = {
        { inset + r, inset + r }, { W - inset - r, inset + r },
        { inset + r, H - inset - r }, { W - inset - r, H - inset - r }
    };
    for (auto& c : corners)
    {
        juce::ColourGradient grad(juce::Colour(0xff3a3a3a), c.x - r*0.4f, c.y - r*0.35f,
                                  juce::Colour(0xff111111), c.x + r, c.y + r, true);
        g.setGradientFill(grad);
        g.fillEllipse(c.x - r, c.y - r, d, d);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawEllipse(c.x - r, c.y - r, d, d, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawEllipse(c.x - r + 1, c.y - r + 1, d - 2, d - 2, 0.8f);
        float s = r - 2.0f;
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawLine(c.x - s, c.y, c.x + s, c.y, 1.5f);
        g.drawLine(c.x, c.y - s, c.x, c.y + s, 1.5f);
    }
}

//==============================================================================
void ThroughTheWallAudioProcessorEditor::drawPlugin(juce::Graphics& g)
{
    float W = (float)kW;

    // Module ID top-left
    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("03 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    // Plugin name
    float nameY = 28.0f;
    g.setFont(rajdhaniBold.withHeight(22.0f));
    // Engraved shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText("THROUGH THE WALL", 0, (int)nameY + 1, (int)W, 28, juce::Justification::centred);
    // Highlight
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawText("THROUGH THE WALL", 0, (int)nameY - 1, (int)W, 28, juce::Justification::centred);
    // Main
    g.setColour(cSilk);
    g.drawText("THROUGH THE WALL", 0, (int)nameY, (int)W, 28, juce::Justification::centred);

    // Thin accent line under name
    g.setColour(cGreen.withAlpha(0.4f));
    g.drawLine(W * 0.25f, nameY + 31.0f, W * 0.75f, nameY + 31.0f, 1.0f);

    // Four knobs
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

    // Category badge bottom-centre
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

    // Logo — bottom-right, SVG at 40% opacity
    if (logoDrawable != nullptr)
    {
        const int logoSize = 80;
        const int margin   = 14;
        juce::Rectangle<float> bounds(
            W - logoSize - margin,
            (float)kH - logoSize - margin,
            (float)logoSize, (float)logoSize);
        logoDrawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 0.4f);
    }
}

//==============================================================================
void ThroughTheWallAudioProcessorEditor::drawKnob(juce::Graphics& g,
                                                   float cx, float cy,
                                                   float value,
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
    inactiveArc.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, valueAng, endAng, true);
    g.setColour(juce::Colour(0xff1a1a1a).withAlpha(0.8f));
    g.strokePath(inactiveArc, juce::PathStrokeType(5.0f));

    // Active arc (green)
    juce::Path activeArc;
    activeArc.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, startAng, valueAng, true);
    g.setColour(cGreen.withAlpha(0.7f));
    g.strokePath(activeArc, juce::PathStrokeType(5.0f));

    // Knob body
    juce::ColourGradient bodyGrad(juce::Colour(0xff4a4a4a), cx - r*0.35f, cy - r*0.3f,
                                  juce::Colour(0xff111111), cx + r, cy + r, true);
    bodyGrad.addColour(0.45, juce::Colour(0xff2a2a2a));
    g.setGradientFill(bodyGrad);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);

    // Shadow ring
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.5f);
    // Top highlight
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawEllipse(cx - r + 1, cy - r + 1, r * 2 - 2, r * 2 - 2, 0.8f);

    // Pointer (green)
    float angle = startAng + value * (endAng - startAng) - juce::MathConstants<float>::halfPi;
    float px1 = cx + std::cos(angle) * r * 0.25f;
    float py1 = cy + std::sin(angle) * r * 0.25f;
    float px2 = cx + std::cos(angle) * r * 0.78f;
    float py2 = cy + std::sin(angle) * r * 0.78f;

    g.setColour(cGreen.withAlpha(0.7f));
    g.drawLine(px1, py1, px2, py2, 3.5f);
    g.setColour(cGreen);
    g.drawLine(px1, py1, px2, py2, 2.0f);

    // Label above knob
    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cAmber);
    g.drawText(label, (int)(cx - 44), (int)(cy - arcR - 16), 88, 11,
               juce::Justification::centred);

    // Value readout below knob
    g.setFont(shareTechMono.withHeight(8.5f));
    g.setColour(cSilk);
    g.drawText(valueText, (int)(cx - 36), (int)(cy + arcR + 6), 72, 11,
               juce::Justification::centred);
}