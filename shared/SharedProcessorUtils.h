#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace SharedProcessorUtils
{
    // Save APVTS state to binary. Optionally embeds currentPreset (pass -1 to omit).
    inline void saveState (juce::AudioProcessor& proc,
                           juce::AudioProcessorValueTreeState& apvts,
                           juce::MemoryBlock& destData,
                           int currentPreset = -1)
    {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        if (currentPreset >= 0)
            xml->setAttribute ("currentPreset", currentPreset);
        proc.copyXmlToBinary (*xml, destData);
    }

    // Load APVTS state from binary. Optionally restores currentPreset via out-pointer.
    inline void loadState (juce::AudioProcessor& proc,
                           juce::AudioProcessorValueTreeState& apvts,
                           const void* data, int sizeInBytes,
                           int* outCurrentPreset = nullptr)
    {
        std::unique_ptr<juce::XmlElement> xmlState (proc.getXmlFromBinary (data, sizeInBytes));
        if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            if (outCurrentPreset)
                *outCurrentPreset = xmlState->getIntAttribute ("currentPreset", 0);
        }
    }

    // Apply a native-range value to an APVTS parameter (converts to 0-1 internally).
    inline void applyParam (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& id, float value)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
}
