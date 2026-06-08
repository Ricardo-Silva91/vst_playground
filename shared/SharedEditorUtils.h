#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace SharedEditorUtils
{
    // Font / asset loading — each plugin embeds the same files under the same BinaryData names
    juce::Font                      loadRajdhaniBold();
    juce::Font                      loadShareTechMono();
    std::unique_ptr<juce::Drawable> loadLogo();

    // ── Drawing helpers ───────────────────────────────────────────────────────
    void drawScanLines (juce::Graphics& g, juce::Rectangle<float> area, float opacity);

    // inset and d (diameter) are parameterised so choir_box can pass its slightly
    // different screw geometry (inset=9, d=11) while all others use the defaults.
    void drawScrews (juce::Graphics& g, float W, float H,
                     float inset = 8.f, float d = 12.f);

    void drawChassis (juce::Graphics& g, juce::Rectangle<float> bounds);

    // ── Knob style ────────────────────────────────────────────────────────────
    struct KnobStyle
    {
        juce::Colour accent;
        juce::Colour bodyHigh, bodyMid, bodyLow;
        float knobR     = 32.f;
        bool  glowArc   = false;   // true → extra wide glow pass behind active arc
        float arcWidth  = 5.0f;    // core active-arc stroke width
        float glowWidth = 8.0f;    // glow pass width (only used when glowArc=true)
    };

    void drawKnob (juce::Graphics& g,
                   float cx, float cy, float value,
                   const juce::String& label,
                   const juce::String& valueText,
                   const KnobStyle& style,
                   const juce::Font& shareTechMono);
}
