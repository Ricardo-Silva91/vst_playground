#include "PluginEditor.h"

ReverseReverbAudioProcessorEditor::ReverseReverbAudioProcessorEditor(ReverseReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(400, 280);

    // Title
    titleLabel.setText("Reverse Reverb", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    // Setup sliders — these directly control the AudioParameterFloat values
    setupSlider(roomSizeSlider,   roomSizeLabel,   "Room Size",    audioProcessor.roomSize);
    setupSlider(wetMixSlider,     wetMixLabel,     "Wet Mix",      audioProcessor.wetMix);
    setupSlider(windowSizeSlider, windowSizeLabel, "Window (ms)",  audioProcessor.windowSizeMs);

    // Start timer to keep UI synced with parameter state
    startTimerHz(30);
}

ReverseReverbAudioProcessorEditor::~ReverseReverbAudioProcessorEditor()
{
    stopTimer();
}

void ReverseReverbAudioProcessorEditor::setupSlider(juce::Slider& slider,
                                                     juce::Label& label,
                                                     const juce::String& labelText,
                                                     juce::AudioParameterFloat* param)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setRange(param->range.start, param->range.end, 0.01);
    slider.setValue(param->get(), juce::dontSendNotification);
    slider.setColour(juce::Slider::thumbColourId,       juce::Colour(0xff8888ff));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff5555cc));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // When knob moves, update the parameter
    slider.onValueChange = [&slider, param]()
    {
        *param = static_cast<float>(slider.getValue());
    };

    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(label);
}

void ReverseReverbAudioProcessorEditor::timerCallback()
{
    // Keep knob positions in sync if parameters are automated by FL Studio
    roomSizeSlider  .setValue(audioProcessor.roomSize->get(),     juce::dontSendNotification);
    wetMixSlider    .setValue(audioProcessor.wetMix->get(),       juce::dontSendNotification);
    windowSizeSlider.setValue(audioProcessor.windowSizeMs->get(), juce::dontSendNotification);
}

void ReverseReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Dark gradient background
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Subtle panel behind knobs
    g.setColour(juce::Colour(0xff16213e));
    g.fillRoundedRectangle(20, 60, getWidth() - 40, getHeight() - 80, 12.0f);

    // Accent line under title
    g.setColour(juce::Colour(0xff8888ff));
    g.fillRect(60, 48, getWidth() - 120, 2);
}

void ReverseReverbAudioProcessorEditor::resized()
{
    titleLabel.setBounds(0, 10, getWidth(), 35);

    int knobSize  = 100;
    int knobY     = 80;
    int labelH    = 20;
    int spacing   = getWidth() / 3;

    roomSizeSlider  .setBounds(spacing * 0 + 10, knobY,           knobSize, knobSize);
    roomSizeLabel   .setBounds(spacing * 0 + 10, knobY + knobSize, knobSize, labelH);

    wetMixSlider    .setBounds(spacing * 1 + 10, knobY,           knobSize, knobSize);
    wetMixLabel     .setBounds(spacing * 1 + 10, knobY + knobSize, knobSize, labelH);

    windowSizeSlider.setBounds(spacing * 2 + 10, knobY,           knobSize, knobSize);
    windowSizeLabel .setBounds(spacing * 2 + 10, knobY + knobSize, knobSize, labelH);
}
