#include "PluginEditor.h"

PitchWobbleEditor::PitchWobbleEditor(PitchWobbleProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    auto makeKnob = [&](juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff7ec8e3));
        s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible(s);

        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(13.0f));
        l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(l);
    };

    makeKnob(depthSlider,  depthLabel,  "Depth (cents)");
    makeKnob(rateSlider,   rateLabel,   "Rate (Hz)");
    makeKnob(smoothSlider, smoothLabel, "Smoothness");

    depthAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "depth",  depthSlider);
    rateAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "rate",   rateSlider);
    smoothAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "smooth", smoothSlider);

    setSize(360, 200);
}

PitchWobbleEditor::~PitchWobbleEditor() {}

void PitchWobbleEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e2e));

    g.setColour(juce::Colour(0xff7ec8e3));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Pitch Wobble", getLocalBounds().removeFromTop(36), juce::Justification::centred);
}

void PitchWobbleEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(36); // title

    int knobW = area.getWidth() / 3;
    int labelH = 20;
    int knobH = area.getHeight() - labelH;

    auto depthArea  = area.removeFromLeft(knobW);
    auto rateArea   = area.removeFromLeft(knobW);
    auto smoothArea = area;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> a)
    {
        l.setBounds(a.removeFromBottom(labelH));
        s.setBounds(a);
    };

    placeKnob(depthSlider,  depthLabel,  depthArea);
    placeKnob(rateSlider,   rateLabel,   rateArea);
    placeKnob(smoothSlider, smoothLabel, smoothArea);
}

juce::AudioProcessorEditor* PitchWobbleProcessor::createEditor()
{
    return new PitchWobbleEditor(*this);
}
