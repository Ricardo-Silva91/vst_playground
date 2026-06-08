cmake_minimum_required(VERSION 3.15)
project(vst_playground_tests VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

enable_testing()

# ── Catch2 ────────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

# ── JUCE ──────────────────────────────────────────────────────────────────────
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" JUCE_build)

# ── Common compile definitions ────────────────────────────────────────────────
set(JUCE_TEST_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STANDALONE_APPLICATION=0
)

# ── Helper: create one test executable per plugin ─────────────────────────────
#
#   add_plugin_test(
#       <target-name>           e.g. reverse_reverb_test
#       <plugin-dir>            e.g. reverse_reverb
#       [EXTRA_DEFS def1 def2 ...]
#   )
#
function(add_plugin_test TARGET PLUGIN_DIR)
    cmake_parse_arguments(APT "" "" "EXTRA_DEFS" ${ARGN})

    set(PLUGIN_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../${PLUGIN_DIR}")

    add_executable(${TARGET}
        "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.cpp"
        "${PLUGIN_PATH}/Source/PluginProcessor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs/${PLUGIN_DIR}_editor_stub.cpp"
    )

    target_include_directories(${TARGET}
        PRIVATE
            "${PLUGIN_PATH}/Source"
            "${CMAKE_CURRENT_SOURCE_DIR}/../shared"
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_compile_definitions(${TARGET}
        PRIVATE
            ${JUCE_TEST_DEFS}
            ${APT_EXTRA_DEFS}
    )

    target_link_libraries(${TARGET}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            Catch2::Catch2
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    catch_discover_tests(${TARGET})
endfunction()

# ── Plugin test targets ────────────────────────────────────────────────────────
add_plugin_test(reverse_reverb_test   reverse_reverb)
add_plugin_test(pitch_wobble_test     pitch_wobble)
add_plugin_test(drum_smash_test       drum_smash)
add_plugin_test(break_scientist_test  break_scientist)
add_plugin_test(choir_box_test        choir_box)
add_plugin_test(through_the_wall_test through_the_wall)

# through_the_wall uses JucePlugin_Name macro (normally set by juce_add_plugin).
# Define it separately to avoid CMake quoting issues with spaces in the value.
target_compile_definitions(through_the_wall_test
    PRIVATE "JucePlugin_Name=\"Through The Wall\""
)
