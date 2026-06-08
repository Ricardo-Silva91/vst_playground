#include "PluginEditor.h"

ReverseReverbAudioProcessorEditor::ReverseReverbAudioProcessorEditor(ReverseReverbAudioProcessor& p)
    : juce::AudioProcessorEditor(p), audioProcessor(p)
{
    setSize(300, 200);
}

ReverseReverbAudioProcessorEditor::~ReverseReverbAudioProcessorEditor() {}

void ReverseReverbAudioProcessorEditor::paint(juce::Graphics& g)         { g.fillAll(juce::Colours::black); }
void ReverseReverbAudioProcessorEditor::resized()                         {}
void ReverseReverbAudioProcessorEditor::timerCallback()                   {}
void ReverseReverbAudioProcessorEditor::mouseDown(const juce::MouseEvent&)         {}
void ReverseReverbAudioProcessorEditor::mouseDrag(const juce::MouseEvent&)         {}
void ReverseReverbAudioProcessorEditor::mouseUp(const juce::MouseEvent&)           {}
void ReverseReverbAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent&)  {}
void ReverseReverbAudioProcessorEditor::drawPlugin(juce::Graphics&)       {}
void ReverseReverbAudioProcessorEditor::drawKnob(juce::Graphics&, float, float, float,
                                                  const juce::String&, const juce::String&) {}
juce::Point<float> ReverseReverbAudioProcessorEditor::knobCenter(int) const  { return {}; }
int                ReverseReverbAudioProcessorEditor::knobHitTest(juce::Point<float>) const { return -1; }
float              ReverseReverbAudioProcessorEditor::normRoom()   const { return 0.f; }
float              ReverseReverbAudioProcessorEditor::normWet()    const { return 0.f; }
float              ReverseReverbAudioProcessorEditor::normWindow() const { return 0.f; }
void               ReverseReverbAudioProcessorEditor::setNorm(int, float) {}
