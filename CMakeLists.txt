cmake_minimum_required (VERSION 3.22)

project (EasyLooper VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)

if (EXISTS "${CMAKE_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory (JUCE)
else()
    include (FetchContent)
    FetchContent_Declare (
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable (juce)
endif()

juce_add_plugin (EasyLooper
    COMPANY_NAME "Kamil Karpinski"
    BUNDLE_ID "com.kamikarpinski.easylooper"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Kmlp
    PLUGIN_CODE Elp1
    FORMATS AU VST3
    PRODUCT_NAME "Easy Looper"
)

target_sources (EasyLooper
    PRIVATE
        Source/Looper.cpp
        Source/Looper.h
        Source/MidiMapper.cpp
        Source/MidiMapper.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_include_directories (EasyLooper PRIVATE Source)

target_compile_definitions (EasyLooper
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries (EasyLooper
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_executable (EasyLooperTests
    Tests/LooperTests.cpp
    Source/Looper.cpp
    Source/MidiMapper.cpp
)

target_include_directories (EasyLooperTests PRIVATE Source)
target_compile_features (EasyLooperTests PRIVATE cxx_std_20)

enable_testing()
add_test (NAME EasyLooperTests COMMAND EasyLooperTests)
