cmake_minimum_required(VERSION 3.22)
project(EQHeatmap VERSION 0.1.0)

if (WIN32)
  set(VST3_COPY_DIR "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
elseif (APPLE)
  set(VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
  set(AU_COPY_DIR   "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
  if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
  endif()
endif()


# Point JUCE as needed, e.g. -DJUCE_DIR=...</JUCE>
include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.9   # pin to your version
)
FetchContent_MakeAvailable(juce)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (APPLE)
  set(EQHEATMAP_FORMATS VST3 AU)
else()
  set(EQHEATMAP_FORMATS VST3)
endif()

juce_add_plugin(EQHeatmap
    COMPANY_NAME "Zane Inc."
    BUNDLE_ID com.zaneinc.eqheatmap
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Zane
    PLUGIN_CODE EHmp
    FORMATS ${EQHEATMAP_FORMATS}
    PRODUCT_NAME "EQHeatmap")

juce_generate_juce_header(EQHeatmap)

target_sources(EQHeatmap PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Theme.cpp
    Source/Theme.h)

target_compile_definitions(EQHeatmap
    PRIVATE
      JUCE_VST3_CAN_REPLACE_VST2=0
      JUCE_DISPLAY_SPLASH_SCREEN=0
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0
      JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(EQHeatmap
    PRIVATE
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_opengl
)
