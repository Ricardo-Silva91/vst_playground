#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
static float normToAngle (float norm)
{
    // -145° (min) to +145° (max), measured clockwise from 12 o'clock
    return juce::degreesToRadians (-145.0f + norm * 290.0f);
}

//==============================================================================
PitchWobbleEditor::PitchWobbleEditor (PitchWobbleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    knobs[0] = { "depth",  "DEPTH",  "ct", 0.0f };
    knobs[1] = { "rate",   "RATE",   "Hz", 0.0f };
    knobs[2] = { "smooth", "SMOOTH", "",   0.0f };

    for (auto& k : knobs)
    {
        if (auto* param = proc.apvts.getParameter (k.paramId))
            k.value = param->getValue();
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
void PitchWobbleEditor::parameterChanged (const juce::String& paramId, float)
{
    juce::MessageManager::callAsync ([this, paramId]
    {
        for (auto& k : knobs)
            if (k.paramId == paramId)
                if (auto* param = proc.apvts.getParameter (paramId))
                    k.value = param->getValue();
        repaint();
    });
}

//==============================================================================
// Layout
//==============================================================================

juce::Rectangle<float> PitchWobbleEditor::knobBounds (int i) const
{
    // Three knobs evenly spaced across full width, vertically centred
    // with a slight downward offset to leave room for header
    float slotW = (float)W / 3.0f;
    float cx    = slotW * i + slotW / 2.0f;
    float cy    = (float)H / 2.0f + 20.0f;   // nudged down for header
    return { cx - KNOB_D / 2.0f, cy - KNOB_D / 2.0f, KNOB_D, KNOB_D };
}

int PitchWobbleEditor::hitTestKnob (juce::Point<int> pos) const
{
    for (int i = 0; i < 3; ++i)
        if (knobBounds (i).expanded (8.0f).contains (pos.toFloat()))
            return i;
    return -1;
}

void PitchWobbleEditor::setNorm (int index, float norm)
{
    norm = juce::jlimit (0.0f, 1.0f, norm);
    knobs[index].value = norm;
    if (auto* param = proc.apvts.getParameter (knobs[index].paramId))
        param->setValueNotifyingHost (norm);
    repaint();
}

//==============================================================================
// Mouse
//==============================================================================

void PitchWobbleEditor::mouseDown (const juce::MouseEvent& e)
{
    int idx = hitTestKnob (e.getPosition());
    if (idx < 0) return;
    knobs[idx].isDragging = true;
    knobs[idx].dragStart  = knobs[idx].value;
    knobs[idx].dragStartY = e.getPosition().y;
}

void PitchWobbleEditor::mouseDrag (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
    {
        if (!knobs[i].isDragging) continue;
        float delta = (knobs[i].dragStartY - e.getPosition().y) / 200.0f;
        setNorm (i, knobs[i].dragStart + delta);
    }
}

void PitchWobbleEditor::mouseUp (const juce::MouseEvent&)
{
    for (auto& k : knobs) k.isDragging = false;
}

void PitchWobbleEditor::mouseWheelMove (const juce::MouseEvent& e,
                                         const juce::MouseWheelDetails& w)
{
    int idx = hitTestKnob (e.getPosition());
    if (idx < 0) return;
    setNorm (idx, knobs[idx].value + w.deltaY * 0.05f);
}

//==============================================================================
// Paint
//==============================================================================

void PitchWobbleEditor::paint (juce::Graphics& g)
{
    drawBackground (g);
    drawHeader     (g);
    for (int i = 0; i < 3; ++i)
        drawKnob (g, i);

    // Corner screws
    drawScrew (g, 14.0f,       14.0f);
    drawScrew (g, W - 14.0f,   14.0f);
    drawScrew (g, 14.0f,       H - 14.0f);
    drawScrew (g, W - 14.0f,   H - 14.0f);

    // Category badge bottom-centre
    {
        float badgeCX = W / 2.0f;
        float badgeY  = H - 22.0f;
        float ledR    = 3.0f;
        float ledX    = badgeCX - 38.0f;

        // LED glow
        juce::ColourGradient glow (accent.withAlpha (0.35f), ledX, badgeY,
                                   juce::Colours::transparentBlack, ledX + 10, badgeY + 10, true);
        g.setGradientFill (glow);
        g.fillEllipse (ledX - 5, badgeY - 5, 16.0f, 16.0f);

        // LED dot
        g.setColour (accent);
        g.fillEllipse (ledX - ledR, badgeY - ledR, ledR * 2, ledR * 2);

        // Label
        g.setColour (textDim);
        g.setFont (8.0f);
        g.drawText ("SPATIAL", (int)(ledX + 6), (int)(badgeY - 6),
                    80, 12, juce::Justification::left, false);
    }
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawBackground (juce::Graphics& g) const
{
    // Main fill
    g.setColour (chassis);
    g.fillRect (getLocalBounds());

    // Subtle inner vignette — darken edges
    {
        juce::ColourGradient vignette (juce::Colours::transparentBlack, W / 2.0f, H / 2.0f,
                                       juce::Colour (0x55000000), 0.0f, 0.0f, true);
        g.setGradientFill (vignette);
        g.fillRect (getLocalBounds());
    }

    // Outer border
    g.setColour (juce::Colour (0xff2a2a3e));
    g.drawRect (getLocalBounds(), 1);

    // Thin accent line under header
    g.setColour (accent.withAlpha (0.35f));
    g.fillRect (40.0f, 52.0f, (float)W - 80.0f, 1.0f);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawHeader (juce::Graphics& g) const
{
    // Module ID top-left
    g.setColour (textDim);
    g.setFont (8.0f);
    g.drawText ("01 / 04", 14, 12, 60, 10, juce::Justification::left, false);

    // Plugin title centred
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("PITCH WOBBLE", 0, 22, W, 24, juce::Justification::centred, false);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawKnob (juce::Graphics& g, int index) const
{
    auto  b    = knobBounds (index);
    float cx   = b.getCentreX();
    float cy   = b.getCentreY();
    float r    = KNOB_D / 2.0f;
    float norm = knobs[index].value;

    // ── arc track ────────────────────────────────────────────────────────────
    float arcR      = r + 8.0f;
    float startDeg  = -145.0f;
    float endDeg    = 145.0f;
    float valueDeg  = startDeg + norm * (endDeg - startDeg);

    // background track
    {
        juce::Path track;
        track.addCentredArc (cx, cy, arcR, arcR, 0.0f,
                             juce::degreesToRadians (startDeg),
                             juce::degreesToRadians (endDeg), true);
        g.setColour (juce::Colour (0xff2a2a3e));
        g.strokePath (track, juce::PathStrokeType (3.5f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // filled arc
    {
        juce::Path filled;
        filled.addCentredArc (cx, cy, arcR, arcR, 0.0f,
                              juce::degreesToRadians (startDeg),
                              juce::degreesToRadians (valueDeg), true);
        // glow pass
        g.setColour (accent.withAlpha (0.3f));
        g.strokePath (filled, juce::PathStrokeType (6.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // sharp pass
        g.setColour (accent);
        g.strokePath (filled, juce::PathStrokeType (3.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ── knob body ─────────────────────────────────────────────────────────────
    {
        // Outer shadow
        juce::ColourGradient shadow (juce::Colour (0xff0a0a14), cx, cy + r,
                                     juce::Colours::transparentBlack, cx, cy - r, false);
        g.setGradientFill (shadow);
        g.fillEllipse (b.expanded (3.0f));

        // Body
        juce::ColourGradient body (juce::Colour (0xff3a3a4e), cx - r * 0.3f, cy - r * 0.4f,
                                   juce::Colour (0xff111120), cx + r * 0.2f, cy + r * 0.5f, false);
        g.setGradientFill (body);
        g.fillEllipse (b);

        // Rim
        g.setColour (juce::Colour (0xff444455));
        g.drawEllipse (b, 1.0f);

        // Inner highlight arc (top-left)
        juce::Path highlight;
        highlight.addCentredArc (cx, cy, r - 3.0f, r - 3.0f, 0.0f,
                                 juce::degreesToRadians (-150.0f),
                                 juce::degreesToRadians (-30.0f), true);
        g.setColour (juce::Colour (0x22ffffff));
        g.strokePath (highlight, juce::PathStrokeType (1.5f));
    }

    // ── pointer dot ──────────────────────────────────────────────────────────
    {
        float angle = normToAngle (norm);
        float dotDist = r - 10.0f;
        float dx = cx + std::sin (angle) * dotDist;
        float dy = cy - std::cos (angle) * dotDist;

        // Glow
        juce::ColourGradient dotGlow (accent.withAlpha (0.6f), dx, dy,
                                      juce::Colours::transparentBlack, dx + 6, dy + 6, true);
        g.setGradientFill (dotGlow);
        g.fillEllipse (dx - 5, dy - 5, 10.0f, 10.0f);

        // Dot
        g.setColour (accent);
        g.fillEllipse (dx - 2.5f, dy - 2.5f, 5.0f, 5.0f);
    }

    // ── label above knob ─────────────────────────────────────────────────────
    g.setColour (silk);
    g.setFont (9.0f);
    g.drawText (knobs[index].label,
                (int)(cx - 40), (int)(b.getY() - arcR - 16),
                80, 12, juce::Justification::centred, false);

    // ── value below knob ─────────────────────────────────────────────────────
    g.setColour (silk);
    g.setFont (9.0f);
    g.drawText (formatValue (index),
                (int)(cx - 40), (int)(b.getBottom() + arcR + 4),
                80, 12, juce::Justification::centred, false);
}

//──────────────────────────────────────────────────────────────────────────────
void PitchWobbleEditor::drawScrew (juce::Graphics& g, float x, float y) const
{
    float r = 5.0f;
    juce::ColourGradient grad (juce::Colour (0xff3a3a4e), x - r, y - r,
                               juce::Colour (0xff111120), x + r, y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (x - r, y - r, r * 2, r * 2);
    g.setColour (juce::Colour (0xff555566));
    g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.75f);
    // Phillips cross
    g.setColour (juce::Colour (0x88000000));
    g.fillRect (x - 3.0f, y - 0.6f, 6.0f, 1.2f);
    g.fillRect (x - 0.6f, y - 3.0f, 1.2f, 6.0f);
}

//──────────────────────────────────────────────────────────────────────────────
juce::String PitchWobbleEditor::formatValue (int index) const
{
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (
                      proc.apvts.getParameter (knobs[index].paramId));
    if (!param) return {};
    float v = param->convertFrom0to1 (knobs[index].value);

    if (knobs[index].paramId == "depth")  return juce::String (v, 1) + " ct";
    if (knobs[index].paramId == "rate")   return juce::String (v, 2) + " Hz";
    if (knobs[index].paramId == "smooth") return juce::String (v, 2);
    return juce::String (v, 2);
}

//==============================================================================
juce::AudioProcessorEditor* PitchWobbleProcessor::createEditor()
{
    return new PitchWobbleEditor (*this);
}