#include "PluginEditor.h"
#include <BinaryData.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void drawScanLines(juce::Graphics& g, juce::Rectangle<int> bounds,
                          float opacity, int step = 2)
{
    g.setColour(juce::Colours::white.withAlpha(opacity));
    for (int y = bounds.getY(); y < bounds.getBottom(); y += step)
        g.drawHorizontalLine(y, (float)bounds.getX(), (float)bounds.getRight());
}

static juce::Font monoFont(float size)
{
    return juce::Font(juce::FontOptions()
        .withName("Courier New")
        .withHeight(size));
}

static juce::Font rajdhaniFont(float size, bool bold = true)
{
    // Rajdhani may not be available on the runner — fall back to a sans
    return juce::Font(juce::FontOptions()
        .withName(bold ? "Arial Bold" : "Arial")
        .withHeight(size));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

ThroughTheWallAudioProcessorEditor::ThroughTheWallAudioProcessorEditor(
    ThroughTheWallAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(kEditorW, kEditorH);

    // Load logo from BinaryData
    logoImage = juce::ImageCache::getFromMemory(
        BinaryData::logo_transparent_png,
        BinaryData::logo_transparent_pngSize);

    // ── Knob setup (face panel) ───────────────────────────────────────────
    auto setupKnob = [&](juce::Slider& k) {
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(k);
    };
    setupKnob(thicknessKnob);
    setupKnob(bleedKnob);
    setupKnob(rattleKnob);
    setupKnob(distanceKnob);

    thicknessAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "thickness", thicknessKnob);
    bleedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "bleed", bleedKnob);
    rattleAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "rattle", rattleKnob);
    distanceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "distance", distanceKnob);

    // ── Slider setup (content panel rows) ────────────────────────────────
    auto setupSlider = [&](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setAlpha(0.0f);   // invisible — we draw them manually, sliders handle input
        addAndMakeVisible(s);
    };
    setupSlider(thicknessSlider);
    setupSlider(bleedSlider);
    setupSlider(rattleSlider);
    setupSlider(distanceSlider);

    thicknessSliderAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "thickness", thicknessSlider);
    bleedSliderAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "bleed", bleedSlider);
    rattleSliderAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "rattle", rattleSlider);
    distanceSliderAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts, "distance", distanceSlider);

    // Repaint when any parameter changes
    startTimerHz(30);
}

ThroughTheWallAudioProcessorEditor::~ThroughTheWallAudioProcessorEditor()
{
    stopTimer();
}

void ThroughTheWallAudioProcessorEditor::timerCallback()
{
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::resized()
{
    // ── Knobs: 2×2 grid in face panel ────────────────────────────────────
    const int knobSize = 40;
    const int gap      = 8;
    const int gridW    = knobSize * 2 + gap;
    const int gridH    = knobSize * 2 + gap;
    const int gridX    = (kFaceW - gridW) / 2;
    const int gridY    = 130;

    thicknessKnob.setBounds(gridX,              gridY,              knobSize, knobSize);
    bleedKnob    .setBounds(gridX + knobSize + gap, gridY,          knobSize, knobSize);
    rattleKnob   .setBounds(gridX,              gridY + knobSize + gap, knobSize, knobSize);
    distanceKnob .setBounds(gridX + knobSize + gap, gridY + knobSize + gap, knobSize, knobSize);

    // ── Sliders: invisible hit-targets over each content row ─────────────
    const int contentX  = kFaceW + 3;  // past divider
    const int rowH      = 52;
    const int topPad    = 14;
    const int leftMargin  = 16;
    const int rightMargin = 28;
    const int sliderW   = kEditorW - contentX - leftMargin - rightMargin;
    const int trackY    = rowH / 2 + 4; // vertical centre of track within row

    auto rowBounds = [&](int idx) -> juce::Rectangle<int> {
        int y = topPad + idx * rowH;
        return { contentX + leftMargin, y, sliderW, rowH };
    };

    thicknessSlider.setBounds(rowBounds(0));
    bleedSlider    .setBounds(rowBounds(1));
    rattleSlider   .setBounds(rowBounds(2));
    distanceSlider .setBounds(rowBounds(3));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Paint
// ─────────────────────────────────────────────────────────────────────────────

float ThroughTheWallAudioProcessorEditor::getNormValue(const juce::String& paramId) const
{
    auto* param = audioProcessor.apvts.getParameter(paramId);
    return param ? param->getValue() : 0.0f;
}

void ThroughTheWallAudioProcessorEditor::paint(juce::Graphics& g)
{
    paintChassis(g);
    paintFacePanel(g);
    paintContentPanel(g);
    paintLogo(g);
}

// ── Chassis ──────────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintChassis(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Base chassis
    g.setColour(kChassis);
    g.fillAll();

    // Outer border
    g.setColour(juce::Colour(0xff333333));
    g.drawRect(bounds, 1);
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(0, 0, (float)getWidth());
    g.setColour(juce::Colour(0xff0a0a0a));
    g.drawHorizontalLine(getHeight() - 1, 0, (float)getWidth());

    // Full-editor scan lines
    drawScanLines(g, bounds, 0.012f, 2);

    // Corner screws
    paintScrew(g, 6,                      6);
    paintScrew(g, getWidth() - 6.0f,      6);
    paintScrew(g, 6,                      getHeight() - 6.0f);
    paintScrew(g, getWidth() - 6.0f,      getHeight() - 6.0f);
}

// ── Face panel ───────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintFacePanel(juce::Graphics& g)
{
    juce::Rectangle<int> face(0, 0, kFaceW, kEditorH);

    // Background
    g.setColour(kMetal);
    g.fillRect(face);

    // Brushed horizontal lines
    drawScanLines(g, face, 0.008f, 2);

    // Divider (right edge of face panel)
    g.setColour(juce::Colour(0xff0d0d0d));
    g.fillRect(kFaceW, 0, 2, kEditorH);
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawVerticalLine(kFaceW + 2, 0, (float)kEditorH);

    // ── Module ID ────────────────────────────────────────────────────────
    g.setFont(monoFont(8.0f));
    g.setColour(kTextDim);
    g.drawText("03 / 04", 10, 10, kFaceW - 20, 12, juce::Justification::left);

    // ── Plugin name ──────────────────────────────────────────────────────
    g.setFont(rajdhaniFont(20.0f));
    // Shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText("THROUGH",   0, 37, kFaceW, 22, juce::Justification::centred);
    g.drawText("THE WALL",  0, 57, kFaceW, 22, juce::Justification::centred);
    // Silk text
    g.setColour(kSilk);
    g.drawText("THROUGH",   0, 36, kFaceW, 22, juce::Justification::centred);
    g.drawText("THE WALL",  0, 56, kFaceW, 22, juce::Justification::centred);

    // ── Knobs ────────────────────────────────────────────────────────────
    paintKnob(g, thicknessKnob.getBounds().toFloat(), getNormValue("thickness"), "THICK");
    paintKnob(g, bleedKnob    .getBounds().toFloat(), getNormValue("bleed"),     "BLEED");
    paintKnob(g, rattleKnob   .getBounds().toFloat(), getNormValue("rattle"),    "RATTLE");
    paintKnob(g, distanceKnob .getBounds().toFloat(), getNormValue("distance"),  "DIST");

    // ── Category badge ───────────────────────────────────────────────────
    const int badgeY = kEditorH - 22;
    // LED dot
    g.setColour(kAccent);
    g.fillEllipse((float)(kFaceW / 2 - 26), (float)badgeY + 3.0f, 6.0f, 6.0f);
    // Glow
    g.setColour(kAccent.withAlpha(0.25f));
    g.fillEllipse((float)(kFaceW / 2 - 28), (float)badgeY + 1.0f, 10.0f, 10.0f);
    // Text
    g.setFont(monoFont(8.0f));
    g.setColour(kTextDim);
    g.drawText("PHYSICAL", kFaceW / 2 - 18, badgeY, 70, 12, juce::Justification::left);
}

// ── Single knob ──────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintKnob(juce::Graphics& g,
    juce::Rectangle<float> bounds, float normVal, const juce::String& label)
{
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float radius = bounds.getWidth() * 0.42f;

    const float startAngle = juce::MathConstants<float>::pi * 1.25f;
    const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
    float angle = startAngle + normVal * (endAngle - startAngle);

    // Arc background
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, radius + 5, radius + 5, 0, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff222222));
        g.strokePath(arc, juce::PathStrokeType(5.0f));
    }
    // Arc fill
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, radius + 5, radius + 5, 0, startAngle, angle, true);
        g.setColour(kAccent.withAlpha(0.25f));
        g.strokePath(arc, juce::PathStrokeType(5.0f));
    }

    // Knob body — green-tinted radial gradient
    {
        juce::ColourGradient grad(
            juce::Colour(0xff2a3a2a), cx - radius * 0.3f, cy - radius * 0.4f,
            juce::Colour(0xff060c06), cx + radius * 0.3f, cy + radius * 0.5f, true);
        g.setGradientFill(grad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
    }

    // Outer shadow ring
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawEllipse(cx - radius - 1, cy - radius - 1, (radius + 1) * 2, (radius + 1) * 2, 1.5f);

    // Rim
    g.setColour(juce::Colour(0xff1a2a1a));
    g.drawEllipse(cx - radius, cy - radius, radius * 2, radius * 2, 1.0f);

    // Top highlight
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawEllipse(cx - radius + 1, cy - radius + 1, (radius - 1) * 2, (radius - 1) * 2, 0.5f);

    // Pointer line
    float dotDist = radius * 0.65f;
    float px = cx + std::sin(angle) * dotDist;
    float py = cy - std::cos(angle) * dotDist;
    // Glow
    g.setColour(kAccent.withAlpha(0.4f));
    g.fillEllipse(px - 3.5f, py - 3.5f, 7.0f, 7.0f);
    // Dot
    g.setColour(kAccent);
    g.fillEllipse(px - 2.0f, py - 2.0f, 4.0f, 4.0f);

    // Label below knob
    g.setFont(monoFont(7.0f));
    g.setColour(kSilk);
    g.drawText(label,
        (int)(cx - 24), (int)(bounds.getBottom() + 3),
        48, 10, juce::Justification::centred);
}

// ── Content panel ─────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintContentPanel(juce::Graphics& g)
{
    const int contentX = kFaceW + 3;
    const int contentW = kEditorW - contentX;

    // Background
    g.setColour(kPanel);
    g.fillRect(contentX, 0, contentW, kEditorH);

    // Param rows
    const int rowH     = 52;
    const int topPad   = 14;
    const juce::String labels[] = { "WALL THICKNESS", "ROOM BLEED", "WALL RATTLE", "DISTANCE" };
    const juce::String paramIds[] = { "thickness", "bleed", "rattle", "distance" };

    for (int i = 0; i < 4; ++i)
    {
        int y = topPad + i * rowH;
        juce::Rectangle<int> row(contentX, y, contentW, rowH);
        paintParamRow(g, row, labels[i], getNormValue(paramIds[i]), i % 2 == 1);
    }

    paintVUStrip(g);
}

// ── Single param row ─────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintParamRow(juce::Graphics& g,
    juce::Rectangle<int> row, const juce::String& label, float normVal, bool shaded)
{
    const int leftMargin  = 16;
    const int rightMargin = 28;

    // Row background
    g.setColour(shaded ? juce::Colour(0xff242424) : kPanel);
    g.fillRect(row);

    // Bottom border
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawHorizontalLine(row.getBottom() - 1, (float)row.getX(), (float)row.getRight());

    // Label
    g.setFont(monoFont(9.0f));
    g.setColour(kAmber);
    g.drawText(label,
        row.getX() + leftMargin, row.getY() + 8,
        row.getWidth() - leftMargin - rightMargin - 60, 12,
        juce::Justification::left);

    // Value readout (right-aligned)
    g.setFont(monoFont(9.0f));
    g.setColour(kSilk);
    g.drawText(juce::String(normVal, 2),
        row.getRight() - rightMargin - 38, row.getY() + 8,
        36, 12, juce::Justification::right);

    // Track
    const int trackH  = 4;
    const int trackY  = row.getY() + row.getHeight() / 2 + 6;
    const int trackX  = row.getX() + leftMargin;
    const int trackW  = row.getWidth() - leftMargin - rightMargin;

    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle((float)trackX, (float)trackY, (float)trackW, (float)trackH, 2.0f);
    g.setColour(juce::Colour(0xff222222));
    g.drawRoundedRectangle((float)trackX, (float)trackY, (float)trackW, (float)trackH, 2.0f, 1.0f);

    // Fill
    int fillW = (int)(trackW * normVal);
    if (fillW > 0)
    {
        g.setColour(kAccent);
        g.fillRoundedRectangle((float)trackX, (float)trackY, (float)fillW, (float)trackH, 2.0f);
    }

    // Thumb
    const int thumbW = 10;
    const int thumbH = 18;
    int thumbX = trackX + fillW - thumbW / 2;
    thumbX = juce::jlimit(trackX, trackX + trackW - thumbW, thumbX);
    int thumbY = trackY - (thumbH - trackH) / 2;

    juce::ColourGradient thumbGrad(juce::Colour(0xff4a4a4a), (float)thumbX, (float)thumbY,
                                   juce::Colour(0xff2a2a2a), (float)thumbX, (float)(thumbY + thumbH), false);
    g.setGradientFill(thumbGrad);
    g.fillRoundedRectangle((float)thumbX, (float)thumbY, (float)thumbW, (float)thumbH, 2.0f);

    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawHorizontalLine(thumbY, (float)thumbX, (float)(thumbX + thumbW));
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawHorizontalLine(thumbY + thumbH, (float)thumbX, (float)(thumbX + thumbW));
}

// ── VU strip ─────────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintVUStrip(juce::Graphics& g)
{
    const int contentX = kFaceW + 3;
    const int contentW = kEditorW - contentX;
    const int segW     = 6;
    const int segH     = 3;
    const int segGap   = 2;
    const int numSegs  = 8;
    const int totalH   = numSegs * (segH + segGap) - segGap;
    const int stripX   = contentX + contentW - segW - 8;
    const int stripY   = (kEditorH - totalH) / 2;

    g.saveState();
    g.setOpacity(0.5f);

    for (int i = 0; i < numSegs; ++i)
    {
        int segY = stripY + i * (segH + segGap);
        bool active = i >= 3; // bottom 5 active (indices 3-7)

        if (active)
        {
            g.setColour(kAccent);
            g.fillRect(stripX, segY, segW, segH);
            // glow
            g.setColour(kAccent.withAlpha(0.3f));
            g.fillRect(stripX - 1, segY - 1, segW + 2, segH + 2);
        }
        else
        {
            g.setColour(juce::Colour(0xff1a1a1a));
            g.fillRect(stripX, segY, segW, segH);
            g.setColour(juce::Colour(0xff222222));
            g.drawRect(stripX, segY, segW, segH, 1);
        }
    }

    g.restoreState();
}

// ── Screw ─────────────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintScrew(juce::Graphics& g, float cx, float cy)
{
    const float r = 6.0f;
    juce::ColourGradient grad(juce::Colour(0xff3a3a3a), cx - r * 0.3f, cy - r * 0.4f,
                              juce::Colour(0xff111111), cx + r * 0.3f, cy + r * 0.4f, true);
    g.setGradientFill(grad);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);

    g.setColour(juce::Colours::black.withAlpha(0.7f));
    // Phillips slot H
    g.fillRect(cx - r * 0.5f, cy - 0.75f, r, 1.5f);
    // Phillips slot V
    g.fillRect(cx - 0.75f, cy - r * 0.5f, 1.5f, r);
}

// ── Logo ──────────────────────────────────────────────────────────────────────

void ThroughTheWallAudioProcessorEditor::paintLogo(juce::Graphics& g)
{
    if (!logoImage.isValid()) return;

    const int logoSize = 64;
    const int margin   = 8;
    const int x = getWidth()  - logoSize - margin;
    const int y = getHeight() - logoSize - margin;

    // Black circle background (matches logo bg)
    g.setColour(juce::Colours::black);
    g.fillEllipse((float)x, (float)y, (float)logoSize, (float)logoSize);

    // Dashed silk ring
    juce::Path ring;
    ring.addEllipse((float)x + 1, (float)y + 1, (float)logoSize - 2, (float)logoSize - 2);
    juce::PathStrokeType stroke(1.0f);
    float dashLengths[] = { 4.0f, 3.0f };
    stroke.createDashedStroke(ring, ring, dashLengths, 2);
    g.setColour(kSilk.withAlpha(0.5f));
    g.strokePath(ring, juce::PathStrokeType(1.0f));

    // Clip and draw logo
    g.saveState();
    juce::Path clip;
    clip.addEllipse((float)x, (float)y, (float)logoSize, (float)logoSize);
    g.reduceClipRegion(clip);
    g.drawImage(logoImage, x, y, logoSize, logoSize,
                0, 0, logoImage.getWidth(), logoImage.getHeight());
    g.restoreState();
}