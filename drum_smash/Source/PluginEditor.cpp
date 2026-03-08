#include "PluginEditor.h"
#include "PluginProcessor.h"

juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}