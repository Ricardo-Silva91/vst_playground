#include "PluginEditor.h"

LizardTapeAudioProcessorEditor::LizardTapeAudioProcessorEditor(LizardTapeAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(300, 200);
}

void LizardTapeAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::black); }
void LizardTapeAudioProcessorEditor::resized() {}
void LizardTapeAudioProcessorEditor::addKnob(Knob&, const juce::String&, const juce::String&) {}
