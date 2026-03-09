#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
// Helpers
//==============================================================================

static float normToAngle (float norm)
{
    // knob travels from -145° to +145° (290° sweep), 0 = fully left
    return juce::degreesToRadians (-145.0f + norm * 290.0f);
}

//==============================================================================
PitchWobbleEditor::PitchWobbleEditor (PitchWobbleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // Set up knob metadata
    knobs[0] = { "depth",  "DEPTH",  "ct",  0.0f };
    knobs[1] = { "rate",   "RATE",   "Hz",  0.0f };
    knobs[2] = { "smooth", "SMOOTH", "",    0.0f };

    // Pull current normalised values from apvts
    for (auto& k : knobs)
    {
        auto* param = proc.apvts.getParameter (k.paramId);
        if (param != nullptr)
            k.value = param->getValue();   // already normalised 0..1
        proc.apvts.addParameterListener (k.paramId, this);
    }

    setSize (W, H);
}

PitchWobbleEditor::~PitchWobbleEditor()
{
    for (auto& k : knobs)
        proc.apvts.removeParameterListener (k.paramId, this);
}

//==============================================================================
// Listener
//==============================================================================

void PitchWobbleEditor::parameterChanged (const juce::String& paramId, float /*newValue*/)
{
    // Re-read normalised value and repaint on message thread
    juce::MessageManager::callAsync ([this, paramId]
    {
        for (auto& k : knobs)
        {
            if (k.paramId == paramId)
            {
                auto* param = proc.apvts.getParameter (paramId);
                if (param) k.value = param->getValue();
            }
        }
        repaint();
    });
}

//==============================================================================
// Mouse
//==============================================================================

int PitchWobbleEditor::hitTestKnob (juce::Point<int> pos) const
{
    for (int i = 0; i < 3; ++i)
    {
        auto b = knobBounds (i);
        if (b.contains (pos.toFloat()))
            return i;
    }
    return -1;
}

juce::Rectangle<float> PitchWobbleEditor::knobBounds (int i) const
{
    // Three knobs centred in the face panel
    float totalWidth = 2.0f * KNOB_SPACING + KNOB_D;
    float startX = (FACE_W - totalWidth) / 2.0f;
    float cx = startX + i * KNOB_SPACING + KNOB_D / 2.0f;
    float cy = (float)KNOB_Y;
    return { cx - KNOB_D / 2.0f, cy - KNOB_D / 2.0f, (float)KNOB_D, (float)KNOB_D };
}

void PitchWobbleEditor::setKnobNormalisedValue (int index, float normVal)
{
    normVal = juce::jlimit (0.0f, 1.0f, normVal);
    knobs[index].value = normVal;

    auto* param = proc.apvts.getParameter (knobs[index].paramId);
    if (param)
        param->setValueNotifyingHost (normVal);

    repaint();
}

void PitchWobbleEditor::mouseDown (const juce::MouseEvent& e)
{
    int idx = hitTestKnob (e.getPosition());
    if (idx < 0) return;

    knobs[idx].isDragging  = true;
    knobs[idx].dragStart   = knobs[idx].value;
    knobs[idx].dragStartY  = e.getPosition().y;
}

void PitchWobbleEditor::mouseDrag (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
    {
        if (!knobs[i].isDragging) continue;

        float delta = (knobs[i].dragStartY - e.getPosition().y) / 200.0f;
        setKnobNormalisedValue (i, knobs[i].dragStart + delta);
    }
}

void PitchWobbleEditor::mouseUp (const juce::MouseEvent&)
{
    for (auto& k : knobs)
        k.isDragging = false;
}

void PitchWobbleEditor::mouseWheelMove (const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& w)
{
    int idx = hitTestKnob (e.getPosition());
    if (idx < 0) return;
    setKnobNormalisedValue (idx, knobs[idx].value + w.deltaY * 0.05f);
}

//==============================================================================
// Paint
//==============================================================================

void PitchWobbleEditor::paint (juce::Graphics& g)
{
    drawChassis    (g);
    drawScrews     (g);
    drawFacePanel  (g);
    drawRightPanel (g);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawChassis (juce::Graphics& g) const
{
    auto bounds = getLocalBounds().toFloat();

    // Body
    g.setColour (chassis);
    g.fillRect (bounds);

    // Scanline overlay — every 2 px
    g.setColour (juce::Colour (0x03ffffff));
    for (float y = 0; y < H; y += 2.0f)
        g.fillRect (0.0f, y, (float)W, 1.0f);

    // Outer border
    g.setColour (juce::Colour (0xff333333));
    g.drawRect (bounds, 1.0f);
    g.setColour (juce::Colour (0xff444444));
    g.fillRect (0.0f, 0.0f, (float)W, 2.0f);   // top highlight
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (0.0f, (float)(H - 2), (float)W, 2.0f); // bottom shadow
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawScrew (juce::Graphics& g, float x, float y) const
{
    float r = SCREW_D / 2.0f;

    // Body gradient
    juce::ColourGradient grad (juce::Colour (0xff3A3A3A), x, y - r,
                               juce::Colour (0xff111111), x, y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (x - r, y - r, SCREW_D, SCREW_D);

    // Border
    g.setColour (juce::Colour (0xff222222));
    g.drawEllipse (x - r, y - r, SCREW_D, SCREW_D, 1.0f);

    // Phillips slot — horizontal
    g.setColour (juce::Colour (0xb3000000));
    g.fillRect (x - 4.0f, y - 0.75f, 8.0f, 1.5f);
    // Phillips slot — vertical
    g.fillRect (x - 0.75f, y - 4.0f, 1.5f, 8.0f);
}

void PitchWobbleEditor::drawScrews (juce::Graphics& g) const
{
    float i = SCREW_INSET + SCREW_D / 2.0f;
    drawScrew (g, i,         i);
    drawScrew (g, W - i,     i);
    drawScrew (g, i,         H - i);
    drawScrew (g, W - i,     H - i);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawFacePanel (juce::Graphics& g) const
{
    // Background
    g.setColour (metal);
    g.fillRect (0.0f, 0.0f, (float)FACE_W, (float)H);

    // Brushed lines — left panel only
    g.setColour (juce::Colour (0x02ffffff));
    for (float y = 0; y < H; y += 2.0f)
        g.fillRect (0.0f, y, (float)FACE_W, 1.0f);

    // Divider
    g.setColour (juce::Colour (0xff0D0D0D));
    g.fillRect ((float)FACE_W - 2.0f, 0.0f, 2.0f, (float)H);
    g.setColour (juce::Colour (0xff3D3D3D));
    g.fillRect ((float)FACE_W, 0.0f, 1.0f, (float)H);

    // Module ID
    g.setColour (textDim);
    g.setFont (8.0f);
    g.drawText ("02 / 04", 10, 10, FACE_W - 20, 12,
                juce::Justification::left, false);

    // Plugin name — two lines
    g.setColour (silk);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    int nameY = 30;
    g.drawText ("PITCH",  0, nameY,      FACE_W, 28, juce::Justification::centred, false);
    g.drawText ("WOBBLE", 0, nameY + 28, FACE_W, 28, juce::Justification::centred, false);

    // Three knobs
    for (int i = 0; i < 3; ++i)
        drawKnob (g, i);

    // Sine wave decoration
    drawSineDecoration (g);

    // Category badge
    {
        float badgeY = H - 28.0f;
        float ledX   = FACE_W / 2.0f - 28.0f;
        float ledY   = badgeY + 3.0f;

        // LED
        g.setColour (accent);
        g.fillEllipse (ledX, ledY, 6.0f, 6.0f);
        // LED glow
        juce::ColourGradient glow (accent.withAlpha (0.4f), ledX + 3, ledY + 3,
                                   juce::Colours::transparentBlack, ledX + 9, ledY + 9, true);
        g.setGradientFill (glow);
        g.fillEllipse (ledX - 3, ledY - 3, 12.0f, 12.0f);

        g.setColour (textDim);
        g.setFont (8.0f);
        g.drawText ("MODULATION", (int)(ledX + 10), (int)badgeY,
                    FACE_W, 12, juce::Justification::left, false);
    }
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawKnob (juce::Graphics& g, int index) const
{
    auto b   = knobBounds (index);
    float cx = b.getCentreX();
    float cy = b.getCentreY();
    float r  = KNOB_D / 2.0f;

    // Arc ring background (behind knob)
    {
        float arcR = r + 5.0f;
        juce::Path arcPath;
        arcPath.addCentredArc (cx, cy, arcR, arcR, 0.0f,
                               juce::degreesToRadians (-145.0f),
                               juce::degreesToRadians (145.0f), true);
        g.setColour (juce::Colour (0xff222222));
        g.strokePath (arcPath, juce::PathStrokeType (3.0f));

        // Filled arc up to current value
        float endAngle = normToAngle (knobs[index].value);
        juce::Path filledArc;
        filledArc.addCentredArc (cx, cy, arcR, arcR, 0.0f,
                                 juce::degreesToRadians (-145.0f),
                                 endAngle, true);
        g.setColour (accent.withAlpha (0.6f));
        g.strokePath (filledArc, juce::PathStrokeType (3.0f));
    }

    // Knob body — radial gradient (warm brown tint)
    {
        juce::ColourGradient bodyGrad (
            juce::Colour (0xff4A3A20), cx - r * 0.35f, cy - r * 0.30f,
            juce::Colour (0xff0D0A04), cx + r,          cy + r, true);
        bodyGrad.addColour (0.45, juce::Colour (0xff2A1E0A));
        g.setGradientFill (bodyGrad);
        g.fillEllipse (b);
    }

    // Outer shadow ring
    g.setColour (juce::Colour (0xcc000000));
    g.drawEllipse (b.expanded (1.5f), 2.0f);

    // Top highlight
    g.setColour (juce::Colour (0x1affffff));
    g.drawEllipse (b.reduced (1.0f), 1.0f);

    // Pointer line
    {
        float angle = normToAngle (knobs[index].value);
        float px = cx + std::sin (angle) * (r - 6.0f);
        float py = cy - std::cos (angle) * (r - 6.0f);
        float px2 = cx + std::sin (angle) * (r - 14.0f);
        float py2 = cy - std::cos (angle) * (r - 14.0f);

        // Glow pass
        g.setColour (accent.withAlpha (0.5f));
        juce::Path pointer;
        pointer.startNewSubPath (px2, py2);
        pointer.lineTo (px, py);
        g.strokePath (pointer, juce::PathStrokeType (4.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Sharp pass
        g.setColour (accent);
        g.strokePath (pointer, juce::PathStrokeType (2.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Label below knob
    g.setColour (silk);
    g.setFont (7.0f);
    g.drawText (knobs[index].label,
                (int)(cx - 24), (int)(b.getBottom() + 4),
                48, 10, juce::Justification::centred, false);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawSineDecoration (juce::Graphics& g) const
{
    float centreX = FACE_W / 2.0f;
    float baseY   = KNOB_Y + KNOB_D / 2.0f + 20.0f;
    float w       = 60.0f;
    float h       = 12.0f;

    juce::Path sine;
    bool started = false;
    for (float t = 0; t <= 1.0f; t += 0.02f)
    {
        float x = centreX - w / 2.0f + t * w;
        float y = baseY - std::sin (t * juce::MathConstants<float>::twoPi) * (h / 2.0f);
        if (!started) { sine.startNewSubPath (x, y); started = true; }
        else          { sine.lineTo (x, y); }
    }
    g.setColour (accent.withAlpha (0.2f));
    g.strokePath (sine, juce::PathStrokeType (1.5f));
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawRightPanel (juce::Graphics& g) const
{
    float rx = (float)FACE_W + 1.0f;
    float rw = (float)(W - FACE_W - 1);

    g.setColour (panel);
    g.fillRect (rx, 0.0f, rw, (float)H);

    drawVuStrip (g);

    // Three parameter rows
    float leftMargin  = rx + 16.0f;
    float rightMargin = 28.0f + 8.0f + 6.0f; // account for VU strip
    float rowW        = rw - 16.0f - rightMargin;
    float rowH        = 56.0f;
    float startY      = (H - 3.0f * rowH) / 2.0f;

    for (int i = 0; i < 3; ++i)
    {
        bool alt = (i % 2 == 1);
        juce::Rectangle<float> row (leftMargin, startY + i * rowH, rowW, rowH);
        drawParamRow (g, i, row, alt);
    }
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawVuStrip (juce::Graphics& g) const
{
    float stripX  = (float)(W - 8 - 6);
    float segH    = 3.0f;
    float segGap  = 2.0f;
    int   nSegs   = 8;
    float totalH  = nSegs * segH + (nSegs - 1) * segGap;
    float startY  = (H - totalH) / 2.0f;

    for (int i = 0; i < nSegs; ++i)
    {
        float sy   = startY + i * (segH + segGap);
        bool active = (i >= 2 && i <= 5);  // middle 4 active

        if (active)
        {
            g.setColour (accent.withAlpha (0.5f));
            g.fillRect (stripX, sy, 6.0f, segH);
        }
        else
        {
            g.setColour (juce::Colour (0xff1A1A1A));
            g.fillRect (stripX, sy, 6.0f, segH);
            g.setColour (juce::Colour (0xff222222));
            g.drawRect (stripX, sy, 6.0f, segH, 1.0f);
        }
    }
}

//──────────────────────────────────────────────────────────────────────────────
juce::String PitchWobbleEditor::formatValue (int index) const
{
    auto* param = proc.apvts.getParameter (knobs[index].paramId);
    if (param == nullptr) return {};

    auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
    if (ranged == nullptr) return {};

    float denorm = ranged->convertFrom0to1 (knobs[index].value);

    if (knobs[index].paramId == "depth")
        return juce::String (denorm, 1) + " ct";
    if (knobs[index].paramId == "rate")
        return juce::String (denorm, 2) + " Hz";
    if (knobs[index].paramId == "smooth")
        return juce::String (denorm, 2);

    return juce::String (denorm, 2);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawParamRow (juce::Graphics& g, int index,
                                      juce::Rectangle<float> row,
                                      bool alternate) const
{
    // Row background
    g.setColour (alternate ? juce::Colour (0xff242424) : juce::Colour (0xff1C1C1C));
    g.fillRect (row);

    // Bottom border
    g.setColour (juce::Colour (0xff2A2A2A));
    g.fillRect (row.getX(), row.getBottom() - 1.0f, row.getWidth(), 1.0f);

    float pad  = 8.0f;
    float lx   = row.getX() + pad;
    float ly   = row.getY() + 8.0f;

    // Label
    g.setColour (accent);
    g.setFont (9.0f);
    g.drawText (knobs[index].label, (int)lx, (int)ly, 80, 12,
                juce::Justification::left, false);

    // Value readout
    g.setColour (silk);
    g.drawText (formatValue (index),
                (int)(row.getX()), (int)ly,
                (int)(row.getWidth() - pad), 12,
                juce::Justification::right, false);

    // Slider track
    float trackY  = ly + 18.0f;
    float trackH  = 4.0f;
    float trackX  = lx;
    float trackW  = row.getWidth() - pad * 2.0f;

    g.setColour (juce::Colour (0xff1A1A1A));
    g.fillRect (trackX, trackY, trackW, trackH);
    g.setColour (juce::Colour (0xff222222));
    g.drawRect (trackX, trackY, trackW, trackH, 1.0f);

    // Filled portion
    float filled = knobs[index].value * trackW;
    g.setColour (accent);
    g.fillRect (trackX, trackY, filled, trackH);

    // Thumb
    float thumbW = 10.0f;
    float thumbH = 18.0f;
    float thumbX = trackX + filled - thumbW / 2.0f;
    float thumbY = trackY + trackH / 2.0f - thumbH / 2.0f;

    g.setColour (juce::Colour (0xff3D3D3D));
    g.fillRoundedRectangle (thumbX, thumbY, thumbW, thumbH, 2.0f);
    g.setColour (juce::Colour (0xff4A4A4A));
    g.fillRect (thumbX, thumbY, thumbW, 1.0f);          // top highlight
    g.setColour (juce::Colour (0xff0D0D0D));
    g.fillRect (thumbX, thumbY + thumbH - 1, thumbW, 1.0f); // bottom shadow
}

//==============================================================================
juce::AudioProcessorEditor* PitchWobbleProcessor::createEditor()
{
    return new PitchWobbleEditor (*this);
}