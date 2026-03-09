#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
// Colour constants matching the spec
//==============================================================================
static const juce::Colour cChassis   { 0xff141414 };
static const juce::Colour cPanel     { 0xff1c1c1c };
static const juce::Colour cMetal     { 0xff2e2e2e };
static const juce::Colour cMetalHi   { 0xff3d3d3d };
static const juce::Colour cEdge      { 0xff0a0a0a };
static const juce::Colour cAmber     { 0xffe8820a };
static const juce::Colour cBlue      { 0xff3aace8 };
static const juce::Colour cGreen     { 0xff4ecf6a };
static const juce::Colour cText      { 0xffd4cfc8 };
static const juce::Colour cTextDim   { 0xff7a746c };
static const juce::Colour cSilk      { 0xffa09890 };

//==============================================================================
ReverseReverbAudioProcessorEditor::ReverseReverbAudioProcessorEditor(ReverseReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(480, 280);

    // Load embedded fonts
    rajdhaniBold  = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::RajdhaniBold_ttf,
                                                BinaryData::RajdhaniBold_ttfSize)));
    shareTechMono = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::ShareTechMonoRegular_ttf,
                                                BinaryData::ShareTechMonoRegular_ttfSize)));

    startTimerHz(30);
}

ReverseReverbAudioProcessorEditor::~ReverseReverbAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::timerCallback()
{
    // Sync display values from parameters (handles host automation)
    bool needsRepaint = false;
    float r = normRoom(), w = normWet(), win = normWindow();
    if (r != roomVal || w != wetVal || win != windowVal)
    {
        roomVal   = r;
        wetVal    = w;
        windowVal = win;
        needsRepaint = true;
    }
    if (needsRepaint) repaint();
}

//==============================================================================
// Layout helpers
//==============================================================================
juce::Rectangle<float> ReverseReverbAudioProcessorEditor::leftPanel() const
{
    return { 0.0f, 0.0f, 160.0f, (float)getHeight() };
}

juce::Rectangle<float> ReverseReverbAudioProcessorEditor::rightPanel() const
{
    return { 162.0f, 0.0f, (float)getWidth() - 162.0f, (float)getHeight() };
}

juce::Point<float> ReverseReverbAudioProcessorEditor::knobCenter(int index) const
{
    // Three knobs centered in the left panel at y=170
    float totalW  = knobSpacing * (numKnobs - 1);
    float startX  = (leftPanel().getWidth() - totalW) * 0.5f;
    float cx = startX + index * knobSpacing;
    float cy = 170.0f;
    return { cx, cy };
}

juce::Rectangle<float> ReverseReverbAudioProcessorEditor::sliderRow(int index) const
{
    auto rp = rightPanel();
    float topPad   = 16.0f;
    float botPad   = 16.0f;
    float available = rp.getHeight() - topPad - botPad;
    float rowH     = available / 3.0f;
    return { rp.getX(), rp.getY() + topPad + index * rowH, rp.getWidth(), rowH };
}

int ReverseReverbAudioProcessorEditor::knobHitTest(juce::Point<float> pos) const
{
    for (int i = 0; i < numKnobs; ++i)
    {
        auto c = knobCenter(i);
        if (pos.getDistanceFrom(c) <= knobRadius + 6.0f)
            return i;
    }
    return -1;
}

//==============================================================================
// Param helpers
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
    auto setParam = [&](juce::AudioParameterFloat* p) {
        *p = p->range.start + norm * (p->range.end - p->range.start);
    };
    if      (idx == 0) setParam(audioProcessor.roomSize);
    else if (idx == 1) setParam(audioProcessor.wetMix);
    else if (idx == 2) setParam(audioProcessor.windowSizeMs);
}

//==============================================================================
// Mouse
//==============================================================================
void ReverseReverbAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.position;
    int k = knobHitTest(pos);
    if (k >= 0)
    {
        draggingKnob = k;
        dragStartY   = pos.y;
        dragStartVal = (k == 0) ? normRoom() : (k == 1) ? normWet() : normWindow();
    }
}

void ReverseReverbAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingKnob < 0) return;
    float delta = (dragStartY - e.position.y) / 150.0f;
    setNorm(draggingKnob, dragStartVal + delta);
    repaint();
}

void ReverseReverbAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    draggingKnob = -1;
}

void ReverseReverbAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    int k = knobHitTest(e.position);
    if (k < 0) return;

    juce::AudioParameterFloat* param = (k == 0) ? audioProcessor.roomSize
                                     : (k == 1) ? audioProcessor.wetMix
                                                : audioProcessor.windowSizeMs;
    auto* box = new juce::AlertWindow("Enter value", param->getName(64), juce::MessageBoxIconType::NoIcon);
    box->addTextEditor("val", juce::String(param->get()));
    box->addButton("OK",     1);
    box->addButton("Cancel", 0);
    box->enterModalState(true, juce::ModalCallbackFunction::create([box, param](int result) {
        if (result == 1)
        {
            float v = box->getTextEditorContents("val").getFloatValue();
            *param = juce::jlimit(param->range.start, param->range.end, v);
        }
    }), true);
}

//==============================================================================
// Paint
//==============================================================================
void ReverseReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawChassis(g);
    drawLeftPanel(g);
    drawRightPanel(g);
    drawScrews(g);

    // Scan-line overlay over entire editor
    drawScanLines(g, getLocalBounds().toFloat(), 0.012f);
}

void ReverseReverbAudioProcessorEditor::resized() {}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawChassis(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Body
    g.setColour(cChassis);
    g.fillRect(b);

    // Outer border
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(b, 1.0f);
    // Top highlight
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(b.getX(), b.getY(), b.getRight(), b.getY(), 2.0f);
    // Bottom shadow
    g.setColour(cEdge);
    g.drawLine(b.getX(), b.getBottom(), b.getRight(), b.getBottom(), 2.0f);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawScanLines(juce::Graphics& g,
                                                       juce::Rectangle<float> area,
                                                       float opacity)
{
    g.setColour(juce::Colours::white.withAlpha(opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.0f)
        g.drawHorizontalLine((int)y, area.getX(), area.getRight());
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawScrews(juce::Graphics& g)
{
    const float inset = 6.0f, d = 12.0f, r = d * 0.5f;
    float W = (float)getWidth(), H = (float)getHeight();

    juce::Point<float> corners[4] = {
        { inset + r, inset + r },
        { W - inset - r, inset + r },
        { inset + r, H - inset - r },
        { W - inset - r, H - inset - r }
    };

    for (auto& c : corners)
    {
        // Body gradient
        juce::ColourGradient grad(juce::Colour(0xff3a3a3a), c.x - r * 0.4f, c.y - r * 0.35f,
                                  juce::Colour(0xff111111), c.x + r, c.y + r, true);
        g.setGradientFill(grad);
        g.fillEllipse(c.x - r, c.y - r, d, d);

        // Outer shadow
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawEllipse(c.x - r, c.y - r, d, d, 1.0f);

        // Inner highlight
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawEllipse(c.x - r + 1, c.y - r + 1, d - 2, d - 2, 1.0f);

        // Phillips slot lines
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        float s = r - 2.0f;
        g.drawLine(c.x - s, c.y, c.x + s, c.y, 1.5f);
        g.drawLine(c.x, c.y - s, c.x, c.y + s, 1.5f);
    }
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawLeftPanel(juce::Graphics& g)
{
    auto lp = leftPanel();

    // Metal fill
    g.setColour(cMetal);
    g.fillRect(lp);

    // Brushed-metal lines (left panel only)
    drawScanLines(g, lp, 0.008f);

    // Divider: 2px dark + 1px highlight
    float dx = lp.getRight();
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawLine(dx, 0, dx, (float)getHeight(), 2.0f);
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawLine(dx + 2, 0, dx + 2, (float)getHeight(), 1.0f);

    // Module identifier "01 / 04"
    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("01 / 04", lp.withTrimmedTop(10.0f).withTrimmedLeft(10.0f).withHeight(12.0f),
               juce::Justification::topLeft);

    // Plugin name — two lines
    float nameY = 38.0f;
    g.setFont(rajdhaniBold.withHeight(22.0f));

    // Engraved shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText("REVERSE", lp.withY(nameY + 1).withHeight(26.0f), juce::Justification::centred);
    g.drawText("REVERB",  lp.withY(nameY + 27).withHeight(26.0f), juce::Justification::centred);

    // Highlight above
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawText("REVERSE", lp.withY(nameY - 1).withHeight(26.0f), juce::Justification::centred);
    g.drawText("REVERB",  lp.withY(nameY - 1 + 27).withHeight(26.0f), juce::Justification::centred);

    // Main text
    g.setColour(cSilk);
    g.drawText("REVERSE", lp.withY(nameY).withHeight(26.0f), juce::Justification::centred);
    g.drawText("REVERB",  lp.withY(nameY + 27).withHeight(26.0f), juce::Justification::centred);

    // Knobs
    const char* labels[3] = { "ROOM", "WET", "WINDOW" };
    float vals[3] = { normRoom(), normWet(), normWindow() };
    for (int i = 0; i < numKnobs; ++i)
    {
        auto c = knobCenter(i);
        drawKnob(g, c.x, c.y, vals[i], labels[i]);
    }

    // Category badge at bottom
    float badgeY = lp.getBottom() - 24.0f;
    float badgeCX = lp.getCentreX();

    // LED dot
    float dotR = 3.0f;
    g.setColour(cBlue.withAlpha(0.7f)); // glow
    g.fillEllipse(badgeCX - 30.0f - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    g.setColour(cBlue);
    g.fillEllipse(badgeCX - 30.0f - dotR, badgeY - dotR, dotR * 2, dotR * 2);

    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("SPATIAL", (int)(badgeCX - 22), (int)(badgeY - 6), 50, 12,
               juce::Justification::centredLeft);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawKnob(juce::Graphics& g,
                                                   float cx, float cy,
                                                   float value,
                                                   const juce::String& label)
{
    float r = knobRadius;

    // Arc ring behind knob (conic-style approximation)
    {
        float arcR = r + 5.0f;
        float startAngle = juce::MathConstants<float>::pi * 1.2f;
        float endAngle   = juce::MathConstants<float>::pi * 2.8f;
        float valueAngle = startAngle + value * (endAngle - startAngle);

        // Inactive arc
        juce::Path arcPath;
        arcPath.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2,
                       valueAngle, endAngle, true);
        g.setColour(juce::Colour(0xff222222).withAlpha(0.25f));
        g.strokePath(arcPath, juce::PathStrokeType(5.0f));

        // Active arc
        juce::Path activeArc;
        activeArc.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2,
                         startAngle, valueAngle, true);
        g.setColour(cBlue.withAlpha(0.25f));
        g.strokePath(activeArc, juce::PathStrokeType(5.0f));
    }

    // Knob body gradient
    juce::ColourGradient bodyGrad(juce::Colour(0xff4a4a4a), cx - r * 0.35f, cy - r * 0.3f,
                                  juce::Colour(0xff111111), cx + r, cy + r, true);
    bodyGrad.addColour(0.45, juce::Colour(0xff2a2a2a));
    g.setGradientFill(bodyGrad);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);

    // Outer shadow ring
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.5f);

    // Top highlight
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawEllipse(cx - r + 1, cy - r + 1, r * 2 - 2, r * 2 - 2, 0.8f);

    // Pointer line
    float startAngle = juce::MathConstants<float>::pi * 1.2f;
    float endAngle   = juce::MathConstants<float>::pi * 2.8f;
    float angle      = startAngle + value * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;

    float innerR = r * 0.3f;
    float outerR = r * 0.8f;
    float px1 = cx + std::cos(angle) * innerR;
    float py1 = cy + std::sin(angle) * innerR;
    float px2 = cx + std::cos(angle) * outerR;
    float py2 = cy + std::sin(angle) * outerR;

    // Glow
    g.setColour(cBlue.withAlpha(0.8f));
    g.drawLine(px1, py1, px2, py2, 3.5f);
    // Core line
    g.setColour(cBlue);
    g.drawLine(px1, py1, px2, py2, 2.0f);

    // Label below knob
    g.setFont(shareTechMono.withHeight(7.0f));
    g.setColour(cSilk);
    g.drawText(label, (int)(cx - 22), (int)(cy + r + 4), 44, 10,
               juce::Justification::centred);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawRightPanel(juce::Graphics& g)
{
    auto rp = rightPanel();

    g.setColour(cPanel);
    g.fillRect(rp);

    // Three slider rows
    struct RowData { juce::String label; float value; juce::String valText; };

    auto& rs = *audioProcessor.roomSize;
    auto& wt = *audioProcessor.wetMix;
    auto& ws = *audioProcessor.windowSizeMs;

    RowData rows[3] = {
        { "ROOM SIZE", normRoom(),
          juce::String(rs.get(), 2) },
        { "WET MIX",   normWet(),
          juce::String(wt.get(), 2) },
        { "WINDOW MS", normWindow(),
          juce::String((int)ws.get()) + " ms" }
    };

    for (int i = 0; i < 3; ++i)
        drawSliderRow(g, sliderRow(i), rows[i].label, rows[i].value, rows[i].valText, i % 2 != 0);

    drawVUStrip(g);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawSliderRow(juce::Graphics& g,
                                                       juce::Rectangle<float> row,
                                                       const juce::String& label,
                                                       float value,
                                                       const juce::String& valueText,
                                                       bool odd)
{
    // Row background
    g.setColour(odd ? juce::Colour(0xff242424) : cPanel);
    g.fillRect(row);

    // Bottom border
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawLine(row.getX(), row.getBottom(), row.getRight(), row.getBottom(), 1.0f);

    float leftM  = row.getX() + 16.0f;
    float rightM = row.getRight() - 28.0f;
    float usableW = rightM - leftM;

    // Label
    g.setFont(shareTechMono.withHeight(9.0f));
    g.setColour(cAmber);
    g.drawText(label, (int)leftM, (int)(row.getY() + 8), (int)(usableW * 0.6f), 12,
               juce::Justification::centredLeft);

    // Value readout
    g.setColour(cSilk);
    g.drawText(valueText, (int)leftM, (int)(row.getY() + 8), (int)usableW, 12,
               juce::Justification::centredRight);

    // Slider track
    float trackY  = row.getCentreY() + 6.0f;
    float trackH  = 4.0f;
    float trackX  = leftM;
    float trackW  = usableW;

    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(trackX, trackY - trackH * 0.5f, trackW, trackH);
    g.setColour(juce::Colour(0xff222222));
    g.drawRect(trackX, trackY - trackH * 0.5f, trackW, trackH, 1.0f);

    // Slider fill
    float fillW = trackW * value;
    g.setColour(cBlue);
    g.fillRect(trackX, trackY - trackH * 0.5f, fillW, trackH);

    // Thumb
    float thumbW = 10.0f, thumbH = 18.0f;
    float thumbX = trackX + fillW - thumbW * 0.5f;
    float thumbY = trackY - thumbH * 0.5f;

    juce::ColourGradient thumbGrad(juce::Colour(0xff4a4a4a), thumbX, thumbY,
                                   juce::Colour(0xff2a2a2a), thumbX, thumbY + thumbH, false);
    g.setGradientFill(thumbGrad);
    g.fillRoundedRectangle(thumbX, thumbY, thumbW, thumbH, 2.0f);

    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawLine(thumbX, thumbY + 1, thumbX + thumbW, thumbY + 1, 1.0f);
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawLine(thumbX, thumbY + thumbH - 1, thumbX + thumbW, thumbY + thumbH - 1, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(thumbX, thumbY, thumbW, thumbH, 2.0f, 1.0f);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawVUStrip(juce::Graphics& g)
{
    auto rp = rightPanel();
    int   segments = 8;
    float segH     = 3.0f;
    float segGap   = 2.0f;
    float stripH   = segments * (segH + segGap) - segGap;
    float stripW   = 6.0f;
    float stripX   = rp.getRight() - 8.0f - stripW;
    float stripY   = rp.getCentreY() - stripH * 0.5f;

    for (int i = 0; i < segments; ++i)
    {
        float sy = stripY + (segments - 1 - i) * (segH + segGap);
        bool active = i < 4;

        if (active)
        {
            // Glow
            g.setColour(cGreen.withAlpha(0.6f * 0.5f));
            g.fillRect(stripX - 1, sy - 1, stripW + 2, segH + 2);

            g.setColour(cGreen.withAlpha(0.5f));
            g.fillRect(stripX, sy, stripW, segH);
            g.setColour(juce::Colour(0xff3aaf52).withAlpha(0.5f));
            g.drawRect(stripX, sy, stripW, segH, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff1a1a1a).withAlpha(0.5f));
            g.fillRect(stripX, sy, stripW, segH);
            g.setColour(juce::Colour(0xff222222).withAlpha(0.5f));
            g.drawRect(stripX, sy, stripW, segH, 1.0f);
        }
    }
}