#include "PluginEditor.h"

PitchWobbleEditor::PitchWobbleEditor(PitchWobbleProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p)
{
    setSize(300, 200);
}

PitchWobbleEditor::~PitchWobbleEditor() {}

void PitchWobbleEditor::paint(juce::Graphics& g)         { g.fillAll(juce::Colours::black); }
void PitchWobbleEditor::resized()                         {}
void PitchWobbleEditor::timerCallback()                   {}
void PitchWobbleEditor::mouseDown(const juce::MouseEvent&)         {}
void PitchWobbleEditor::mouseDrag(const juce::MouseEvent&)         {}
void PitchWobbleEditor::mouseUp(const juce::MouseEvent&)           {}
void PitchWobbleEditor::mouseDoubleClick(const juce::MouseEvent&)  {}
void PitchWobbleEditor::drawPlugin(juce::Graphics&)       {}
void PitchWobbleEditor::drawKnob(juce::Graphics&, float, float, float,
                                  const juce::String&, const juce::String&) {}
juce::Point<float> PitchWobbleEditor::knobCenter(int) const      { return {}; }
int                PitchWobbleEditor::knobHitTest(juce::Point<float>) const { return -1; }
float              PitchWobbleEditor::normDepth()  const { return 0.f; }
float              PitchWobbleEditor::normRate()   const { return 0.f; }
float              PitchWobbleEditor::normSmooth() const { return 0.f; }
void               PitchWobbleEditor::setNorm(int, float) {}
juce::String       PitchWobbleEditor::formatValue(int) const { return {}; }
