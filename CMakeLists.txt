cmake_minimum_required(VERSION 3.22)
# Must precede project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
# Universal: Live 12 runs natively on Apple Silicon, and an x86_64-only cmake would otherwise build Intel-only.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
project(EvoCtrl VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(EVOCTRL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND EVOCTRL_FORMATS AU)
endif()

juce_add_plugin(EvoCtrl
    COMPANY_NAME "Alexander Hills"
    BUNDLE_ID "com.alexanderhills.evoctrl"
    PLUGIN_MANUFACTURER_CODE Ahls
    PLUGIN_CODE EvCt
    FORMATS ${EVOCTRL_FORMATS}
    PRODUCT_NAME "EvoCtrl"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    VST3_CATEGORIES Instrument Tools
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(EvoCtrl PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EvolverLink.cpp
    Source/PatchLibrary.cpp)

target_compile_definitions(EvoCtrl PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(EvoCtrl
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

enable_testing()
add_executable(SysexTests tests/SysexTests.cpp)
add_test(NAME SysexTests COMMAND SysexTests)
