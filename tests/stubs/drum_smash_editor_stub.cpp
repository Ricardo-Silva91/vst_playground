#include "PluginEditor.h"

DrumSmashEditor::DrumSmashEditor(DrumSmashProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p)
{
    setSize(300, 200);
}

DrumSmashEditor::~DrumSmashEditor() {}

void DrumSmashEditor::paint(juce::Graphics& g)         { g.fillAll(juce::Colours::black); }
void DrumSmashEditor::timerCallback()                   {}
void DrumSmashEditor::mouseDown(const juce::MouseEvent&)         {}
void DrumSmashEditor::mouseDrag(const juce::MouseEvent&)         {}
void DrumSmashEditor::mouseUp(const juce::MouseEvent&)           {}
void DrumSmashEditor::mouseDoubleClick(const juce::MouseEvent&)  {}
void DrumSmashEditor::mouseWheelMove(const juce::MouseEvent&,
                                      const juce::MouseWheelDetails&) {}
void DrumSmashEditor::drawPlugin(juce::Graphics&)       {}
void DrumSmashEditor::drawKnob(juce::Graphics&, float, float, float,
                                const juce::String&, const juce::String&) {}
juce::Point<float> DrumSmashEditor::knobCenter(int) const      { return {}; }
int                DrumSmashEditor::knobHitTest(juce::Point<float>) const { return -1; }
float              DrumSmashEditor::getNorm(int) const     { return 0.f; }
void               DrumSmashEditor::setNorm(int, float)    {}
juce::String       DrumSmashEditor::getValueText(int) const { return {}; }

juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new DrumSmashEditor(*this);
}
