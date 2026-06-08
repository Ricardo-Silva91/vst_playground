#include "SharedEditorUtils.h"
#include <BinaryData.h>
#include <cmath>

namespace SharedEditorUtils
{

juce::Font loadRajdhaniBold()
{
    return juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::RajdhaniBold_ttf, BinaryData::RajdhaniBold_ttfSize)));
}

juce::Font loadShareTechMono()
{
    return juce::Font (juce::FontOptions (
        juce::Typeface::createSystemTypefaceFor (
            BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize)));
}

std::unique_ptr<juce::Drawable> loadLogo()
{
    return juce::Drawable::createFromImageData (
        BinaryData::logo_transparent_svg, BinaryData::logo_transparent_svgSize);
}

void drawScanLines (juce::Graphics& g, juce::Rectangle<float> area, float opacity)
{
    g.setColour (juce::Colours::white.withAlpha (opacity));
    for (float y = area.getY(); y < area.getBottom(); y += 2.f)
        g.drawHorizontalLine ((int)y, area.getX(), area.getRight());
}

void drawScrews (juce::Graphics& g, float W, float H, float inset, float d)
{
    const float r = d * 0.5f;
    const juce::Point<float> corners[4] = {
        { inset + r,     inset + r     },
        { W - inset - r, inset + r     },
        { inset + r,     H - inset - r },
        { W - inset - r, H - inset - r }
    };
    for (auto& c : corners)
    {
        juce::ColourGradient grad (juce::Colour (0xff3a3a3a), c.x - r * 0.4f, c.y - r * 0.35f,
                                   juce::Colour (0xff111111), c.x + r, c.y + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, d, d);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - r, c.y - r, d, d, 1.f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - r + 1.f, c.y - r + 1.f, d - 2.f, d - 2.f, 0.8f);
        const float s = r - 2.f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (c.x - s, c.y, c.x + s, c.y, 1.5f);
        g.drawLine (c.x, c.y - s, c.x, c.y + s, 1.5f);
    }
}

void drawChassis (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour (juce::Colour (0xff2e2e2e));
    g.fillRect (bounds);
    g.setColour (juce::Colour (0xff333333));
    g.drawRect (bounds, 1.f);
    g.setColour (juce::Colour (0xff444444));
    g.drawLine (bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getY(), 2.f);
    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawLine (bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 2.f);
}

void drawKnob (juce::Graphics& g,
               float cx, float cy, float value,
               const juce::String& label,
               const juce::String& valueText,
               const KnobStyle& s,
               const juce::Font& shareTechMono)
{
    const float arcR   = s.knobR + 6.f;
    const float startA = juce::MathConstants<float>::pi * 1.2f;
    const float endA   = juce::MathConstants<float>::pi * 2.8f;
    const float valueA = startA + value * (endA - startA);

    // Inactive arc track
    {
        juce::Path arc;
        arc.addArc (cx - arcR, cy - arcR, arcR * 2.f, arcR * 2.f, valueA, endA, true);
        g.setColour (juce::Colour (0xff1a1a1a).withAlpha (0.8f));
        g.strokePath (arc, juce::PathStrokeType (s.arcWidth));
    }

    // Active arc (with optional glow pass)
    if (value > 0.001f)
    {
        juce::Path arc;
        arc.addArc (cx - arcR, cy - arcR, arcR * 2.f, arcR * 2.f, startA, valueA, true);
        if (s.glowArc)
        {
            g.setColour (s.accent.withAlpha (0.25f));
            g.strokePath (arc, juce::PathStrokeType (s.glowWidth));
        }
        g.setColour (s.accent.withAlpha (s.glowArc ? 0.75f : 0.7f));
        g.strokePath (arc, juce::PathStrokeType (s.arcWidth));
    }

    // Knob body
    juce::ColourGradient body (s.bodyHigh, cx - s.knobR * 0.35f, cy - s.knobR * 0.3f,
                                s.bodyLow, cx + s.knobR, cy + s.knobR, true);
    body.addColour (0.45, s.bodyMid);
    g.setGradientFill (body);
    g.fillEllipse (cx - s.knobR, cy - s.knobR, s.knobR * 2.f, s.knobR * 2.f);

    // Rim shadow
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (cx - s.knobR, cy - s.knobR, s.knobR * 2.f, s.knobR * 2.f, 1.5f);
    // Inner highlight ring
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawEllipse (cx - s.knobR + 1.f, cy - s.knobR + 1.f,
                   s.knobR * 2.f - 2.f, s.knobR * 2.f - 2.f, 0.8f);

    // Pointer
    const float angle = startA + value * (endA - startA) - juce::MathConstants<float>::halfPi;
    const float px1 = cx + std::cos (angle) * s.knobR * 0.25f;
    const float py1 = cy + std::sin (angle) * s.knobR * 0.25f;
    const float px2 = cx + std::cos (angle) * s.knobR * 0.78f;
    const float py2 = cy + std::sin (angle) * s.knobR * 0.78f;
    g.setColour (s.accent.withAlpha (0.7f));
    g.drawLine (px1, py1, px2, py2, 3.5f);
    g.setColour (s.accent);
    g.drawLine (px1, py1, px2, py2, 2.f);

    // Label above knob — amber
    g.setFont (shareTechMono.withHeight (8.f));
    g.setColour (juce::Colour (0xffe8820a));
    g.drawText (label, (int)(cx - 44.f), (int)(cy - arcR - 16.f), 88, 11,
                juce::Justification::centred);

    // Value readout below knob — silk
    g.setFont (shareTechMono.withHeight (8.5f));
    g.setColour (juce::Colour (0xffa09890));
    g.drawText (valueText, (int)(cx - 36.f), (int)(cy + arcR + 6.f), 72, 11,
                juce::Justification::centred);
}

} // namespace SharedEditorUtils
