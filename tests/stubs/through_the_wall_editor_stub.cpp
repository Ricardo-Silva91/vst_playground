#include "PluginEditor.h"

ThroughTheWallAudioProcessorEditor::ThroughTheWallAudioProcessorEditor(ThroughTheWallAudioProcessor& p)
    : juce::AudioProcessorEditor(p), audioProcessor(p)
{
    setSize(300, 200);
}

ThroughTheWallAudioProcessorEditor::~ThroughTheWallAudioProcessorEditor() {}

void ThroughTheWallAudioProcessorEditor::paint(juce::Graphics& g)         { g.fillAll(juce::Colours::black); }
void ThroughTheWallAudioProcessorEditor::resized()                         {}
void ThroughTheWallAudioProcessorEditor::timerCallback()                   {}
void ThroughTheWallAudioProcessorEditor::mouseDown(const juce::MouseEvent&)         {}
void ThroughTheWallAudioProcessorEditor::mouseDrag(const juce::MouseEvent&)         {}
void ThroughTheWallAudioProcessorEditor::mouseUp(const juce::MouseEvent&)           {}
void ThroughTheWallAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent&)  {}
void ThroughTheWallAudioProcessorEditor::drawPlugin(juce::Graphics&)       {}
void ThroughTheWallAudioProcessorEditor::drawKnob(juce::Graphics&, float, float, float,
                                                   const juce::String&, const juce::String&) {}
juce::Point<float> ThroughTheWallAudioProcessorEditor::knobCenter(int) const      { return {}; }
int                ThroughTheWallAudioProcessorEditor::knobHitTest(juce::Point<float>) const { return -1; }
float              ThroughTheWallAudioProcessorEditor::normThickness() const { return 0.f; }
float              ThroughTheWallAudioProcessorEditor::normBleed()     const { return 0.f; }
float              ThroughTheWallAudioProcessorEditor::normRattle()    const { return 0.f; }
float              ThroughTheWallAudioProcessorEditor::normDistance()  const { return 0.f; }
void               ThroughTheWallAudioProcessorEditor::setNorm(int, float) {}
