#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Choir Box uses GenericAudioProcessorEditor to avoid the FL Studio
// libQuickFontCachev2 crash on Apple Silicon + JUCE 8.
// createEditor() is defined in PluginProcessor.cpp and returns the generic editor.
// This header exists to satisfy the build system's expectation of an editor translation unit.