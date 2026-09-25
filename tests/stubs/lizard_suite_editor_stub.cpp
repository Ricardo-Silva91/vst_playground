#include "PluginEditor.h"

LizardSuiteEditor::LizardSuiteEditor(LizardSuiteProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p)
{
    setSize(300, 200);
}

LizardSuiteEditor::~LizardSuiteEditor() {}

void LizardSuiteEditor::paint(juce::Graphics& g)         { g.fillAll(juce::Colours::black); }
void LizardSuiteEditor::timerCallback()                   {}
void LizardSuiteEditor::mouseDown(const juce::MouseEvent&)         {}
void LizardSuiteEditor::mouseDrag(const juce::MouseEvent&)         {}
void LizardSuiteEditor::mouseUp(const juce::MouseEvent&)           {}
void LizardSuiteEditor::mouseDoubleClick(const juce::MouseEvent&)  {}
void LizardSuiteEditor::drawHeader(juce::Graphics&)       {}
void LizardSuiteEditor::drawModuleStrips(juce::Graphics&) {}
void LizardSuiteEditor::drawAllKnobs(juce::Graphics&)     {}
juce::Point<float> LizardSuiteEditor::knobCenter(int) const      { return {}; }
int                LizardSuiteEditor::knobHitTest(juce::Point<float>) const { return -1; }
float              LizardSuiteEditor::getNorm(int) const     { return 0.f; }
void               LizardSuiteEditor::setNorm(int, float)    {}
juce::String       LizardSuiteEditor::getValueText(int) const { return {}; }
