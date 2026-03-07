#include "PluginEditor.h"

class WallLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour knobBg    { 0xff3a2f20 };
    juce::Colour knobRim   { 0xff6b5a3e };
    juce::Colour knobTrack { 0xffd4a85a };
    juce::Colour knobDot   { 0xfffcf0d8 };

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        float cx = x + width  * 0.5f;
        float cy = y + height * 0.5f;
        float radius = juce::jmin(width, height) * 0.42f;

        // Outer worn rim
        g.setColour(knobRim.darker(0.4f));
        g.fillEllipse(cx - radius - 3, cy - radius - 3, (radius + 3) * 2, (radius + 3) * 2);

        // Arc track (background)
        {
            juce::Path arcTrack;
            arcTrack.addCentredArc(cx, cy, radius + 1.5f, radius + 1.5f, 0,
                                   rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(knobRim.darker(0.6f));
            g.strokePath(arcTrack, juce::PathStrokeType(3.0f));
        }

        // Arc track (filled to value)
        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        {
            juce::Path arcFill;
            arcFill.addCentredArc(cx, cy, radius + 1.5f, radius + 1.5f, 0,
                                  rotaryStartAngle, angle, true);
            g.setColour(knobTrack);
            g.strokePath(arcFill, juce::PathStrokeType(3.0f));
        }

        // Knob body
        g.setColour(knobBg);
        g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

        // Subtle radial gradient feel — lighter top
        juce::ColourGradient grad(knobBg.brighter(0.15f), cx - radius * 0.3f, cy - radius * 0.4f,
                                  knobBg.darker(0.2f), cx + radius * 0.3f, cy + radius * 0.5f, true);
        g.setGradientFill(grad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

        // Rim line
        g.setColour(knobRim);
        g.drawEllipse(cx - radius, cy - radius, radius * 2, radius * 2, 1.2f);

        // Indicator dot
        float dotRadius = 3.0f;
        float dotDist = radius * 0.68f;
        float dotX = cx + std::sin(angle) * dotDist;
        float dotY = cy - std::cos(angle) * dotDist;
        g.setColour(knobDot);
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2, dotRadius * 2);
    }
};

static WallLookAndFeel wallLAF;

// ===================================================================

ThroughTheWallAudioProcessorEditor::ThroughTheWallAudioProcessorEditor(ThroughTheWallAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(480, 260);
    setLookAndFeel(&wallLAF);

    generateCracks();

    // Knobs
    addAndMakeVisible(thicknessKnob);
    addAndMakeVisible(bleedKnob);
    addAndMakeVisible(rattleKnob);
    addAndMakeVisible(distanceKnob);

    // Labels
    setupLabel(thicknessLabel, "WALL\nTHICKNESS");
    setupLabel(bleedLabel,     "ROOM\nBLEED");
    setupLabel(rattleLabel,    "WALL\nRATTLE");
    setupLabel(distanceLabel,  "DISTANCE");

    setupValueLabel(thicknessValue);
    setupValueLabel(bleedValue);
    setupValueLabel(rattleValue);
    setupValueLabel(distanceValue);

    // Value readouts via lambda
    auto setupValueReadout = [this](juce::Slider& knob, juce::Label& label) {
        knob.onValueChange = [&knob, &label]() {
            label.setText(juce::String(knob.getValue(), 2), juce::dontSendNotification);
        };
        label.setText(juce::String(knob.getValue(), 2), juce::dontSendNotification);
    };

    // Attachments
    thicknessAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "thickness", thicknessKnob);
    bleedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bleed", bleedKnob);
    rattleAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "rattle", rattleKnob);
    distanceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "distance", distanceKnob);

    setupValueReadout(thicknessKnob, thicknessValue);
    setupValueReadout(bleedKnob,     bleedValue);
    setupValueReadout(rattleKnob,    rattleValue);
    setupValueReadout(distanceKnob,  distanceValue);
}

ThroughTheWallAudioProcessorEditor::~ThroughTheWallAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void ThroughTheWallAudioProcessorEditor::setupLabel(juce::Label& label, const juce::String& text)
{
    addAndMakeVisible(label);
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font("Courier New", 9.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, dimText);
    label.setJustificationType(juce::Justification::centred);
}

void ThroughTheWallAudioProcessorEditor::setupValueLabel(juce::Label& label)
{
    addAndMakeVisible(label);
    label.setFont(juce::Font("Courier New", 9.0f, juce::Font::plain));
    label.setColour(juce::Label::textColourId, accentColor.withAlpha(0.8f));
    label.setJustificationType(juce::Justification::centred);
}

void ThroughTheWallAudioProcessorEditor::generateCracks()
{
    // Crack 1 — upper area
    crackPath1.startNewSubPath(60, 18);
    crackPath1.lineTo(72, 28);
    crackPath1.lineTo(68, 42);
    crackPath1.lineTo(80, 55);
    crackPath1.lineTo(74, 68);

    // Crack 2 — right side
    crackPath2.startNewSubPath(390, 30);
    crackPath2.lineTo(400, 45);
    crackPath2.lineTo(393, 58);
    crackPath2.lineTo(405, 72);
    crackPath2.lineTo(398, 90);
}

void ThroughTheWallAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Base wall background
    g.setColour(bgColor);
    g.fillAll();

    // Plaster texture panels
    g.setColour(wallColor);
    g.fillRoundedRectangle(bounds.reduced(8.0f), 4.0f);

    // Subtle horizontal mortar lines (brickwork feel)
    g.setColour(bgColor.withAlpha(0.4f));
    for (int y = 30; y < getHeight(); y += 28)
        g.drawHorizontalLine(y, 8.0f, getWidth() - 8.0f);

    // Slight vignette
    juce::ColourGradient vignette(juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
                                  bgColor.withAlpha(0.55f), 0, 0, true);
    g.setGradientFill(vignette);
    g.fillAll();

    // Crack decorations
    g.setColour(bgColor.withAlpha(0.6f));
    g.strokePath(crackPath1, juce::PathStrokeType(1.0f));
    g.strokePath(crackPath2, juce::PathStrokeType(1.0f));

    // Title
    g.setFont(juce::Font("Courier New", 14.0f, juce::Font::bold));
    g.setColour(brightText);
    g.drawText("THROUGH THE WALL", 0, 12, getWidth(), 20, juce::Justification::centred);

    // Subtitle
    g.setFont(juce::Font("Courier New", 8.5f, juce::Font::plain));
    g.setColour(dimText);
    g.drawText("transmission effect", 0, 29, getWidth(), 14, juce::Justification::centred);

    // Hairline separator
    g.setColour(plasterColor.withAlpha(0.5f));
    g.drawHorizontalLine(48, 20.0f, getWidth() - 20.0f);

    // Outer border
    g.setColour(plasterColor.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(8.0f), 4.0f, 1.0f);
}

void ThroughTheWallAudioProcessorEditor::resized()
{
    const int knobSize    = 80;
    const int labelH      = 28;
    const int valueH      = 14;
    const int topPad      = 54;
    const int totalKnobs  = 4;
    int totalWidth        = getWidth();
    int spacing           = totalWidth / totalKnobs;
    int knobY             = topPad + (getHeight() - topPad - knobSize - labelH - valueH) / 2;

    auto placeKnob = [&](int idx, juce::Slider& knob, juce::Label& lbl, juce::Label& val) {
        int cx = spacing * idx + spacing / 2;
        knob.setBounds(cx - knobSize / 2, knobY, knobSize, knobSize);
        lbl.setBounds(cx - 50, knobY + knobSize + 2, 100, labelH);
        val.setBounds(cx - 30, knobY + knobSize + labelH + 2, 60, valueH);
    };

    placeKnob(0, thicknessKnob, thicknessLabel, thicknessValue);
    placeKnob(1, bleedKnob,     bleedLabel,     bleedValue);
    placeKnob(2, rattleKnob,    rattleLabel,     rattleValue);
    placeKnob(3, distanceKnob,  distanceLabel,  distanceValue);
}
