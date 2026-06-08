#include "PluginEditor.h"

BreakScientistEditor::BreakScientistEditor(BreakScientistProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p)
{
    setSize(300, 200);
}

BreakScientistEditor::~BreakScientistEditor() {}

void BreakScientistEditor::paint(juce::Graphics& g)         { g.fillAll(juce::Colours::black); }
void BreakScientistEditor::timerCallback()                   {}
void BreakScientistEditor::mouseDown(const juce::MouseEvent&)         {}
void BreakScientistEditor::mouseDrag(const juce::MouseEvent&)         {}
void BreakScientistEditor::mouseUp(const juce::MouseEvent&)           {}
void BreakScientistEditor::mouseDoubleClick(const juce::MouseEvent&)  {}
void BreakScientistEditor::drawChassis(juce::Graphics&)      {}
void BreakScientistEditor::drawGroup(juce::Graphics&, juce::Rectangle<float>,
                                      const char*, const KnobInfo*, int) {}
void BreakScientistEditor::drawKnob(juce::Graphics&, float, float, float,
                                     const juce::String&, const juce::String&) {}
void BreakScientistEditor::drawLogo(juce::Graphics&)         {}
juce::Point<float> BreakScientistEditor::knobCenter(int) const      { return {}; }
int                BreakScientistEditor::knobHitTest(juce::Point<float>) const { return -1; }
float              BreakScientistEditor::getNorm(int) const     { return 0.f; }
void               BreakScientistEditor::setNorm(int, float)    {}
juce::String       BreakScientistEditor::getValueText(int) const { return {}; }
