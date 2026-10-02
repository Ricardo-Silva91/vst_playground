#include "PluginEditor.h"

using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;

LizardTapeAudioProcessorEditor::LizardTapeAudioProcessorEditor (LizardTapeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addKnob (drive,     "drive",        "Drive");
    addKnob (bias,      "bias",         "Bias");
    addKnob (wowDepth,  "wowDepth",     "Wow");
    addKnob (wowRate,   "wowRate",      "Wow Rate");
    addKnob (flutDepth, "flutterDepth", "Flutter");
    addKnob (flutRate,  "flutterRate",  "Flt Rate");
    addKnob (tone,      "tone",         "Tone");
    addKnob (hiss,      "hiss",         "Hiss");
    addKnob (mix,       "mix",          "Mix");
    addKnob (output,    "output",       "Output");
    setSize (620, 300);
}

void LizardTapeAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramID, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    addAndMakeVisible (k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (13.0f, juce::Font::bold));
    addAndMakeVisible (k.label);
    k.attach = std::make_unique<Attach> (processor.apvts, paramID, k.slider);
}

void LizardTapeAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient grad (juce::Colour (0xff20232a), 0, 0,
                               juce::Colour (0xff11131a), 0, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillAll();
    g.setColour (juce::Colour (0xff8bd450));
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("LIZARD  ·  TAPE", getLocalBounds().removeFromTop (34),
                juce::Justification::centred);
}

void LizardTapeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (30);
    const int cols = 5, rows = 2;
    const int cw = area.getWidth() / cols;
    const int rh = area.getHeight() / rows;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        const int r = i / cols, c = i % cols;
        juce::Rectangle<int> cell (area.getX() + c * cw, area.getY() + r * rh, cw, rh);
        auto& k = *knobs[(size_t) i];
        k.label.setBounds (cell.removeFromTop (16));
        k.slider.setBounds (cell.reduced (4));
    }
}
