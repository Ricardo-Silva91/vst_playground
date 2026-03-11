#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// The generic editor is used to avoid the FL Studio / JUCE 8 / Apple Silicon
// font system crash. All parameters are exposed natively via the APVTS.
// A custom UI can be added later following the reverse_reverb pattern.
class BreakScientistEditor : public juce::AudioProcessorEditor
{
public:
    explicit BreakScientistEditor (BreakScientistProcessor& p)
        : AudioProcessorEditor (&p)
    {
        setSize (400, 300);
    }

    ~BreakScientistEditor() override {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1c1c1c));
    }

    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreakScientistEditor)
};