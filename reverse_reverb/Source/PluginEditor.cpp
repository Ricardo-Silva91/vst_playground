#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
static const juce::Colour cChassis  { 0xff141414 };
static const juce::Colour cMetal    { 0xff2e2e2e };
static const juce::Colour cEdge     { 0xff0a0a0a };
static const juce::Colour cAmber    { 0xffe8820a };
static const juce::Colour cBlue     { 0xff3aace8 };
static const juce::Colour cTextDim  { 0xff7a746c };
static const juce::Colour cSilk     { 0xffa09890 };

// Layout — single panel, 3 knobs centred with generous spacing
static constexpr int   kW         = 400;
static constexpr int   kH         = 310;
static constexpr float kKnobR     = 28.0f;   // larger knobs since they're the only control
static constexpr float kKnobSpX   = 110.0f;  // wide spacing so nothing crowds
static constexpr float kKnobY     = 155.0f;  // vertically centred with room for name above
static constexpr int   kNumKnobs  = 3;

//==============================================================================
ReverseReverbAudioProcessorEditor::ReverseReverbAudioProcessorEditor(ReverseReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(kW, kH);

    rajdhaniBold  = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::RajdhaniBold_ttf,
                                                BinaryData::RajdhaniBold_ttfSize)));
    shareTechMono = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::ShareTechMonoRegular_ttf,
                                                BinaryData::ShareTechMonoRegular_ttfSize)));
    logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_transparent_png,
                                                BinaryData::logo_transparent_pngSize);

    // The PNG has a black background — convert dark pixels to transparent so the
    // wireframe lines render cleanly against the panel without blurry blending.
    if (logoImage.isValid())
    {
        logoImage = logoImage.createCopy();
        for (int y = 0; y < logoImage.getHeight(); ++y)
        {
            for (int x = 0; x < logoImage.getWidth(); ++x)
            {
                juce::Colour c = logoImage.getPixelAt(x, y);
                // Brightness: pixels darker than threshold become fully transparent
                float brightness = c.getBrightness();
                if (brightness < 0.15f)
                    logoImage.setPixelAt(x, y, juce::Colours::transparentBlack);
                else
                    logoImage.setPixelAt(x, y, c.withAlpha(brightness));
            }
        }
    }
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
// Layout
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
// Params
//==============================================================================
float ReverseReverbAudioProcessorEditor::normRoom() const
{
    auto& p = *audioProcessor.roomSize;
    return (p.get() - p.range.start) / (p.range.end - p.range.start);
}
float ReverseReverbAudioProcessorEditor::normWet() const
{
    auto& p = *audioProcessor.wetMix;
    return (p.get() - p.range.start) / (p.range.end - p.range.start);
}
float ReverseReverbAudioProcessorEditor::normWindow() const
{
    auto& p = *audioProcessor.windowSizeMs;
    return (p.get() - p.range.start) / (p.range.end - p.range.start);
}
void ReverseReverbAudioProcessorEditor::setNorm(int idx, float norm)
{
    norm = juce::jlimit(0.0f, 1.0f, norm);
    auto set = [&](juce::AudioParameterFloat* p) {
        *p = p->range.start + norm * (p->range.end - p->range.start);
    };
    if      (idx == 0) set(audioProcessor.roomSize);
    else if (idx == 1) set(audioProcessor.wetMix);
    else if (idx == 2) set(audioProcessor.windowSizeMs);
}

//==============================================================================
// Mouse
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
    juce::AudioParameterFloat* param = (k == 0) ? audioProcessor.roomSize
                                     : (k == 1) ? audioProcessor.wetMix
                                                : audioProcessor.windowSizeMs;
    auto* box = new juce::AlertWindow("Enter value", param->getName(64),
                                       juce::MessageBoxIconType::NoIcon);
    box->addTextEditor("val", juce::String(param->get()));
    box->addButton("OK", 1);
    box->addButton("Cancel", 0);
    box->enterModalState(true, juce::ModalCallbackFunction::create([box, param](int result) {
        if (result == 1)
            *param = juce::jlimit(param->range.start, param->range.end,
                                  box->getTextEditorContents("val").getFloatValue());
    }), true);
}

//==============================================================================
// Paint
//==============================================================================
void ReverseReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawChassis(g);
    drawPlugin(g);
    drawScrews(g);
    drawScanLines(g, getLocalBounds().toFloat(), 0.012f);
}

void ReverseReverbAudioProcessorEditor::resized() {}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawChassis(juce::Graphics& g)
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

void ReverseReverbAudioProcessorEditor::drawScanLines(juce::Graphics& g,
                                                       juce::Rectangle<float> area,
                                                       float opacity)
{
    g.setColour(juce::Colours::white.withAlpha(opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.0f)
        g.drawHorizontalLine((int)y, area.getX(), area.getRight());
}

void ReverseReverbAudioProcessorEditor::drawScrews(juce::Graphics& g)
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
void ReverseReverbAudioProcessorEditor::drawPlugin(juce::Graphics& g)
{
    float W = (float)kW;

    // Module ID top-left
    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("01 / 04", 18, 20, 80, 12, juce::Justification::centredLeft);

    // Plugin name centred near top
    float nameY = 32.0f;
    g.setFont(rajdhaniBold.withHeight(24.0f));

    // Engraved shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText("REVERSE REVERB", 0, (int)nameY + 1, (int)W, 30, juce::Justification::centred);
    // Highlight
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawText("REVERSE REVERB", 0, (int)nameY - 1, (int)W, 30, juce::Justification::centred);
    // Main
    g.setColour(cSilk);
    g.drawText("REVERSE REVERB", 0, (int)nameY, (int)W, 30, juce::Justification::centred);

    // Thin accent line under name
    g.setColour(cBlue.withAlpha(0.4f));
    g.drawLine(W * 0.25f, nameY + 33.0f, W * 0.75f, nameY + 33.0f, 1.0f);

    // Three knobs
    auto& rs = *audioProcessor.roomSize;
    auto& wt = *audioProcessor.wetMix;
    auto& ws = *audioProcessor.windowSizeMs;

    struct KnobData { const char* label; float norm; juce::String val; };
    KnobData knobs[3] = {
        { "ROOM SIZE", normRoom(),   juce::String(rs.get(), 2) },
        { "WET MIX",   normWet(),    juce::String(wt.get(), 2) },
        { "WINDOW MS", normWindow(), juce::String((int)ws.get()) + " ms" }
    };

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter(i);
        drawKnob(g, c.x, c.y, knobs[i].norm, knobs[i].label, knobs[i].val);
    }

    // Category badge bottom-centre
    float badgeY = (float)kH - 22.0f;
    float dotR = 3.0f;
    float dotX = W * 0.5f - 32.0f;

    g.setColour(cBlue.withAlpha(0.4f));
    g.fillEllipse(dotX - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    g.setColour(cBlue);
    g.fillEllipse(dotX - dotR, badgeY - dotR, dotR * 2, dotR * 2);

    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("SPATIAL", (int)(dotX + 5), (int)(badgeY - 5), 50, 10,
               juce::Justification::centredLeft);

    // Logo — bottom-right corner
    // The PNG has a black background so we draw at reduced opacity so the
    // black areas sink into the dark metal panel and only the bright wireframe shows.
    if (logoImage.isValid())
    {
        const int logoSize = 90;
        const int margin   = 10;
        int lx = (int)W - logoSize - margin;
        int ly = kH     - logoSize - margin;

        g.setOpacity(0.85f);
        g.drawImage(logoImage, lx, ly, logoSize, logoSize,
                    0, 0, logoImage.getWidth(), logoImage.getHeight());
        g.setOpacity(1.0f);
    }
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawKnob(juce::Graphics& g,
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

    // Active arc
    juce::Path activeArc;
    activeArc.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, startAng, valueAng, true);
    g.setColour(cBlue.withAlpha(0.7f));
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

    // Pointer
    float angle = startAng + value * (endAng - startAng) - juce::MathConstants<float>::halfPi;
    float px1 = cx + std::cos(angle) * r * 0.25f;
    float py1 = cy + std::sin(angle) * r * 0.25f;
    float px2 = cx + std::cos(angle) * r * 0.78f;
    float py2 = cy + std::sin(angle) * r * 0.78f;

    g.setColour(cBlue.withAlpha(0.7f));
    g.drawLine(px1, py1, px2, py2, 3.5f);
    g.setColour(cBlue);
    g.drawLine(px1, py1, px2, py2, 2.0f);

    // Label above knob
    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cAmber);
    g.drawText(label, (int)(cx - 36), (int)(cy - arcR - 16), 72, 11,
               juce::Justification::centred);

    // Value readout below knob
    g.setFont(shareTechMono.withHeight(8.5f));
    g.setColour(cSilk);
    g.drawText(valueText, (int)(cx - 36), (int)(cy + arcR + 6), 72, 11,
               juce::Justification::centred);
}