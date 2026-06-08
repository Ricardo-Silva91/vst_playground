#include <catch2/catch_all.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

// Catch2 v3: linking against Catch2::Catch2 (not Catch2WithMain) means we
// supply our own main so we can initialise JUCE before any tests run.
int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    return Catch::Session().run(argc, argv);
}
