#include "PluginEditor.h"

ChoirBoxEditor::ChoirBoxEditor(ChoirBoxProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p)
{
    setSize(300, 200);
}

ChoirBoxEditor::~ChoirBoxEditor() {}

void ChoirBoxEditor::paint(juce::Graphics& g)         { g.fillAll(juce::Colours::black); }
void ChoirBoxEditor::timerCallback()                   {}
void ChoirBoxEditor::mouseDown(const juce::MouseEvent&)         {}
void ChoirBoxEditor::mouseDrag(const juce::MouseEvent&)         {}
void ChoirBoxEditor::mouseUp(const juce::MouseEvent&)           {}
void ChoirBoxEditor::mouseDoubleClick(const juce::MouseEvent&)  {}
void ChoirBoxEditor::drawHeader(juce::Graphics&)        {}
void ChoirBoxEditor::drawSectionPanels(juce::Graphics&) {}
void ChoirBoxEditor::drawSectionLabels(juce::Graphics&) {}
void ChoirBoxEditor::drawAllKnobs(juce::Graphics&)      {}
void ChoirBoxEditor::drawKnob(juce::Graphics&, float, float, float,
                               const juce::String&, const juce::String&) {}
juce::Point<float> ChoirBoxEditor::knobCenter(int) const      { return {}; }
int                ChoirBoxEditor::knobHitTest(juce::Point<float>) const { return -1; }
float              ChoirBoxEditor::getNorm(int) const     { return 0.f; }
void               ChoirBoxEditor::setNorm(int, float)    {}
juce::String       ChoirBoxEditor::getValueText(int) const { return {}; }
