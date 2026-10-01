cmake_minimum_required(VERSION 3.15)
project(lizard_tape VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)

juce_add_plugin(lizard_tape
    PLUGIN_MANUFACTURER_CODE LvsT
    PLUGIN_CODE Ltap
    FORMATS              VST3
    PRODUCT_NAME         "Lizard Tape"
    COMPANY_NAME         "lizard vst"
    IS_SYNTH             FALSE
    NEEDS_MIDI_INPUT     FALSE
    NEEDS_MIDI_OUTPUT    FALSE
    IS_MIDI_EFFECT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES      "Fx"
)

target_sources(lizard_tape PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(lizard_tape PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(lizard_tape
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
