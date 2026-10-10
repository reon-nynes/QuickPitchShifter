cmake_minimum_required(VERSION 3.24)
project(QuickPitchShifter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
# Local source overrides are supported through FETCHCONTENT_SOURCE_DIR_<NAME>.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850)
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG a670068d9aeb64913331d5cc29337b19a457a7df)
set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
    GIT_SUBMODULES_RECURSE ON)
FetchContent_MakeAvailable(JUCE signalsmith-stretch clap-juce-extensions)
# Keep third-party template warnings separate from warnings in project code.
foreach(dependency IN ITEMS signalsmith-stretch signalsmith-linear)
    get_target_property(dependency_includes ${dependency} INTERFACE_INCLUDE_DIRECTORIES)
    set_property(TARGET ${dependency} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${dependency_includes}")
endforeach()

set(PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AUv3 Standalone)
endif()

juce_add_plugin(QuickPitchShifter
    COMPANY_NAME "Nytera"
    BUNDLE_ID "com.nytera.QuickPitchShifter"
    PLUGIN_MANUFACTURER_CODE Nytr
    PLUGIN_CODE Qpsh
    PRODUCT_NAME "Quick Pitch Shifter"
    PLUGIN_NAME "Quick Pitch Shifter"
    DESCRIPTION "Live pitch shifting with Signalsmith Stretch"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx PitchShift
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS ${PLUGIN_FORMATS}
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(QuickPitchShifter PRIVATE
    src/PitchEngine.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)
target_compile_definitions(QuickPitchShifter PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(QuickPitchShifter
    PRIVATE signalsmith-stretch juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags
           juce::juce_recommended_lto_flags)

clap_juce_extensions_plugin(TARGET QuickPitchShifter
    CLAP_ID "com.nytera.QuickPitchShifter"
    CLAP_FEATURES audio-effect pitch-shifter mono stereo)

option(QPS_BUILD_TESTS "Build processor and DSP verification" OFF)
if(QPS_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(QuickPitchShifterTests PRODUCT_NAME "Quick Pitch Shifter Tests")
    target_sources(QuickPitchShifterTests PRIVATE tests/ProcessorTests.cpp)
    target_link_libraries(QuickPitchShifterTests PRIVATE QuickPitchShifter signalsmith-stretch juce::juce_audio_utils)
    add_test(NAME QuickPitchShifterTests COMMAND QuickPitchShifterTests)
    set_tests_properties(QuickPitchShifterTests PROPERTIES TIMEOUT 30)
endif()
