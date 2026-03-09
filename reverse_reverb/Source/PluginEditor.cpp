#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
static const juce::Colour cChassis   { 0xff141414 };
static const juce::Colour cPanel     { 0xff1c1c1c };
static const juce::Colour cMetal     { 0xff2e2e2e };
static const juce::Colour cEdge      { 0xff0a0a0a };
static const juce::Colour cAmber     { 0xffe8820a };
static const juce::Colour cBlue      { 0xff3aace8 };
static const juce::Colour cGreen     { 0xff4ecf6a };
static const juce::Colour cTextDim   { 0xff7a746c };
static const juce::Colour cSilk      { 0xffa09890 };

// Layout constants
static constexpr float kLeftW    = 160.0f; // left panel width
static constexpr float kDivider  = 2.0f;
static constexpr float kKnobR    = 18.0f;  // knob radius — smaller than original to avoid overlap
static constexpr float kKnobSpX  = 50.0f;  // centre-to-centre spacing — generous air between knobs
static constexpr float kKnobY    = 185.0f; // knob row Y — pushed lower to give name room to breathe
static constexpr int   kNumKnobs = 3;

//==============================================================================
ReverseReverbAudioProcessorEditor::ReverseReverbAudioProcessorEditor(ReverseReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(480, 300);  // taller editor so rows have generous vertical space

    rajdhaniBold  = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::RajdhaniBold_ttf,
                                                BinaryData::RajdhaniBold_ttfSize)));
    shareTechMono = juce::Font(juce::FontOptions(
        juce::Typeface::createSystemTypefaceFor(BinaryData::ShareTechMonoRegular_ttf,
                                                BinaryData::ShareTechMonoRegular_ttfSize)));
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
juce::Rectangle<float> ReverseReverbAudioProcessorEditor::leftPanel() const
{
    return { 0.0f, 0.0f, kLeftW, (float)getHeight() };
}

juce::Rectangle<float> ReverseReverbAudioProcessorEditor::rightPanel() const
{
    return { kLeftW + kDivider + 1.0f, 0.0f,
             (float)getWidth() - kLeftW - kDivider - 1.0f, (float)getHeight() };
}

juce::Point<float> ReverseReverbAudioProcessorEditor::knobCenter(int index) const
{
    float totalW = kKnobSpX * (kNumKnobs - 1);
    float startX = (kLeftW - totalW) * 0.5f;
    return { startX + index * kKnobSpX, kKnobY };
}

juce::Rectangle<float> ReverseReverbAudioProcessorEditor::sliderRow(int index) const
{
    auto rp = rightPanel();
    float topPad = 20.0f;   // generous top padding
    float botPad = 20.0f;   // generous bottom padding
    float available = rp.getHeight() - topPad - botPad;
    float rowH = available / 3.0f;  // equal thirds with room to breathe
    return { rp.getX(), rp.getY() + topPad + index * rowH, rp.getWidth(), rowH };
}

int ReverseReverbAudioProcessorEditor::knobHitTest(juce::Point<float> pos) const
{
    for (int i = 0; i < kNumKnobs; ++i)
        if (pos.getDistanceFrom(knobCenter(i)) <= kKnobR + 6.0f)
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
    float delta = (dragStartY - e.position.y) / 130.0f;
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
    drawLeftPanel(g);
    drawRightPanel(g);
    drawScrews(g);
    drawScanLines(g, getLocalBounds().toFloat(), 0.012f);
}

void ReverseReverbAudioProcessorEditor::resized() {}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawChassis(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(cChassis);
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
    const float inset = 7.0f, d = 11.0f, r = d * 0.5f;
    float W = (float)getWidth(), H = (float)getHeight();
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
void ReverseReverbAudioProcessorEditor::drawLeftPanel(juce::Graphics& g)
{
    auto lp = leftPanel();

    g.setColour(cMetal);
    g.fillRect(lp);
    drawScanLines(g, lp, 0.008f);

    // Divider
    float dx = lp.getRight();
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawLine(dx, 0, dx, (float)getHeight(), 2.0f);
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawLine(dx + 2, 0, dx + 2, (float)getHeight(), 1.0f);

    // Module ID
    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("01 / 04", 14, 20, 80, 12, juce::Justification::centredLeft);

    // Plugin name — two lines, engraved, with generous space above knobs
    float nameY = 45.0f;
    g.setFont(rajdhaniBold.withHeight(22.0f));

    // Shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText("REVERSE", lp.withY(nameY + 1.0f).withHeight(27.0f), juce::Justification::centred);
    g.drawText("REVERB",  lp.withY(nameY + 29.0f).withHeight(27.0f), juce::Justification::centred);
    // Highlight
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawText("REVERSE", lp.withY(nameY - 1.0f).withHeight(27.0f), juce::Justification::centred);
    g.drawText("REVERB",  lp.withY(nameY + 27.0f).withHeight(27.0f), juce::Justification::centred);
    // Main
    g.setColour(cSilk);
    g.drawText("REVERSE", lp.withY(nameY).withHeight(27.0f), juce::Justification::centred);
    g.drawText("REVERB",  lp.withY(nameY + 28.0f).withHeight(27.0f), juce::Justification::centred);

    // Knobs
    const char* labels[3] = { "ROOM", "WET", "WINDOW" };
    float vals[3] = { normRoom(), normWet(), normWindow() };
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto c = knobCenter(i);
        drawKnob(g, c.x, c.y, vals[i], labels[i]);
    }

    // Category badge — well clear of the bottom edge
    float badgeY = (float)getHeight() - 22.0f;
    float badgeCX = lp.getCentreX();
    float dotR = 3.0f;
    g.setColour(cBlue.withAlpha(0.4f));
    g.fillEllipse(badgeCX - 30.0f - dotR - 2, badgeY - dotR - 2, (dotR + 2) * 2, (dotR + 2) * 2);
    g.setColour(cBlue);
    g.fillEllipse(badgeCX - 30.0f - dotR, badgeY - dotR, dotR * 2, dotR * 2);
    g.setFont(shareTechMono.withHeight(8.0f));
    g.setColour(cTextDim);
    g.drawText("SPATIAL", (int)(badgeCX - 22), (int)(badgeY - 5), 50, 10,
               juce::Justification::centredLeft);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawKnob(juce::Graphics& g,
                                                   float cx, float cy,
                                                   float value,
                                                   const juce::String& label)
{
    float r = kKnobR;

    // Arc ring — sits outside the knob body with clear space
    float arcR     = r + 5.0f;
    float startAng = juce::MathConstants<float>::pi * 1.2f;
    float endAng   = juce::MathConstants<float>::pi * 2.8f;
    float valueAng = startAng + value * (endAng - startAng);

    juce::Path inactiveArc;
    inactiveArc.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, valueAng, endAng, true);
    g.setColour(juce::Colour(0xff222222).withAlpha(0.4f));
    g.strokePath(inactiveArc, juce::PathStrokeType(4.0f));

    juce::Path activeArc;
    activeArc.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, startAng, valueAng, true);
    g.setColour(cBlue.withAlpha(0.5f));
    g.strokePath(activeArc, juce::PathStrokeType(4.0f));

    // Knob body
    juce::ColourGradient bodyGrad(juce::Colour(0xff4a4a4a), cx - r*0.35f, cy - r*0.3f,
                                  juce::Colour(0xff111111), cx + r, cy + r, true);
    bodyGrad.addColour(0.45, juce::Colour(0xff2a2a2a));
    g.setGradientFill(bodyGrad);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);

    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawEllipse(cx - r + 1, cy - r + 1, r * 2 - 2, r * 2 - 2, 0.7f);

    // Pointer
    float angle = startAng + value * (endAng - startAng) - juce::MathConstants<float>::halfPi;
    float px1 = cx + std::cos(angle) * r * 0.25f;
    float py1 = cy + std::sin(angle) * r * 0.25f;
    float px2 = cx + std::cos(angle) * r * 0.78f;
    float py2 = cy + std::sin(angle) * r * 0.78f;

    g.setColour(cBlue.withAlpha(0.8f));
    g.drawLine(px1, py1, px2, py2, 3.0f);
    g.setColour(cBlue);
    g.drawLine(px1, py1, px2, py2, 1.8f);

    // Label — clear space below arc
    g.setFont(shareTechMono.withHeight(7.5f));
    g.setColour(cSilk);
    g.drawText(label, (int)(cx - 22), (int)(cy + arcR + 4), 44, 10,
               juce::Justification::centred);
}

//==============================================================================
void ReverseReverbAudioProcessorEditor::drawRightPanel(juce::Graphics& g)
{
    auto rp = rightPanel();
    g.setColour(cPanel);
    g.fillRect(rp);

    auto& rs = *audioProcessor.roomSize;
    auto& wt = *audioProcessor.wetMix;
    auto& ws = *audioProcessor.windowSizeMs;

    struct Row { juce::String label; float norm; juce::String val; };
    Row rows[3] = {
        { "ROOM SIZE", normRoom(),   juce::String(rs.get(), 2) },
        { "WET MIX",   normWet(),    juce::String(wt.get(), 2) },
        { "WINDOW MS", normWindow(), juce::String((int)ws.get()) + " ms" }
    };

    for (int i = 0; i < 3; ++i)
        drawSliderRow(g, sliderRow(i), rows[i].label, rows[i].norm, rows[i].val, i % 2 != 0);

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
    g.setColour(odd ? juce::Colour(0xff242424) : cPanel);
    g.fillRect(row);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawLine(row.getX(), row.getBottom(), row.getRight(), row.getBottom(), 1.0f);

    float leftM  = row.getX() + 16.0f;
    float rightM = row.getRight() - 26.0f;
    float usableW = rightM - leftM;

    // Label sits in upper portion of row with generous top margin
    float labelY = row.getY() + row.getHeight() * 0.22f;
    g.setFont(shareTechMono.withHeight(9.0f));
    g.setColour(cAmber);
    g.drawText(label, (int)leftM, (int)labelY, (int)(usableW * 0.65f), 12,
               juce::Justification::centredLeft);
    g.setColour(cSilk);
    g.drawText(valueText, (int)leftM, (int)labelY, (int)usableW, 12,
               juce::Justification::centredRight);

    // Slider track sits in lower portion with generous bottom margin
    float trackY  = row.getY() + row.getHeight() * 0.68f;
    float trackH  = 4.0f;

    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(leftM, trackY - trackH * 0.5f, usableW, trackH);
    g.setColour(juce::Colour(0xff222222));
    g.drawRect(leftM, trackY - trackH * 0.5f, usableW, trackH, 1.0f);

    float fillW = usableW * value;
    g.setColour(cBlue);
    g.fillRect(leftM, trackY - trackH * 0.5f, fillW, trackH);

    // Thumb — taller than the track so it's easy to grab
    float thumbW = 10.0f, thumbH = 20.0f;
    float thumbX = leftM + fillW - thumbW * 0.5f;
    float thumbY = trackY - thumbH * 0.5f;

    juce::ColourGradient tg(juce::Colour(0xff4a4a4a), thumbX, thumbY,
                            juce::Colour(0xff2a2a2a), thumbX, thumbY + thumbH, false);
    g.setGradientFill(tg);
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
    int   segs   = 8;
    float segH   = 4.0f;
    float segGap = 3.0f;
    float stripH = segs * (segH + segGap) - segGap;
    float stripW = 6.0f;
    float stripX = rp.getRight() - 12.0f - stripW;
    float stripY = rp.getCentreY() - stripH * 0.5f;

    for (int i = 0; i < segs; ++i)
    {
        float sy = stripY + (segs - 1 - i) * (segH + segGap);
        bool active = i < 4;
        if (active)
        {
            g.setColour(cGreen.withAlpha(0.35f));
            g.fillRect(stripX - 1, sy - 1, stripW + 2, segH + 2);
            g.setColour(cGreen.withAlpha(0.65f));
            g.fillRect(stripX, sy, stripW, segH);
            g.setColour(juce::Colour(0xff3aaf52).withAlpha(0.65f));
            g.drawRect(stripX, sy, stripW, segH, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff1a1a1a).withAlpha(0.6f));
            g.fillRect(stripX, sy, stripW, segH);
            g.setColour(juce::Colour(0xff222222).withAlpha(0.6f));
            g.drawRect(stripX, sy, stripW, segH, 1.0f);
        }
    }
}