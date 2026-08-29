cmake_minimum_required(VERSION 3.30)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(TRENCHPlugin VERSION 0.1.0 LANGUAGES C CXX)
endif()

if(NOT TARGET trench_native_core)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../native/core"
                   "${CMAKE_CURRENT_BINARY_DIR}/native-core")
endif()

include(FetchContent)
if(NOT TARGET juce::juce_audio_processors)
  set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
  set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(juce
    URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/9.0.1
    URL_HASH SHA256=3D8561EE685592513EB546B10D10DC078AE857644E18AEB1E1B4C1F1758492EB
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(juce)
endif()

if(NOT TARGET RTNeural)
  set(RTNEURAL_STL ON CACHE BOOL "Use RTNeural STL backend" FORCE)
  FetchContent_Declare(RTNeuralSource
    GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
    GIT_TAG 1fb1f075a5d66e85bfc8f488c3f3626840cb3a1d
    GIT_SHALLOW FALSE)
  FetchContent_MakeAvailable(RTNeuralSource)
endif()
target_compile_definitions(RTNeural PUBLIC RTNEURAL_NO_DEBUG=1)

option(TRENCH_GOD_MODE "Compile the layout diagnostics" OFF)
set(TRENCH_WORKSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(TRENCH_COMMON_DEFINITIONS
  TRENCH_GOD_MODE=$<BOOL:${TRENCH_GOD_MODE}>
  TRENCH_TABLE_STITCH_PANEL=0
  TRENCH_TABLE_STITCH_ROOT="${TRENCH_WORKSPACE_DIR}"
  JUCE_WEB_BROWSER=0
  JUCE_DIRECT2D=1
  JUCE_USE_CURL=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

set(TRENCH_PRESET_ROSTER "${CMAKE_CURRENT_SOURCE_DIR}/presets/PresetRoster.inc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TRENCH_PRESET_ROSTER}")
file(STRINGS "${TRENCH_PRESET_ROSTER}" TRENCH_PRESET_ROSTER_LINES
     REGEX "^[ \t]*TRENCH_PRESET\\(")
set(TRENCH_SHIPPING_PRESET_FILES)
foreach(line IN LISTS TRENCH_PRESET_ROSTER_LINES)
  string(REGEX MATCH
    "TRENCH_PRESET\\(\"[^\"]+\",[ \t]*\"([^\"]+)\",[ \t]*\"([^\"]+)\"\\)"
    matched "${line}")
  if(NOT matched)
    message(FATAL_ERROR "Cannot parse shipping roster line: ${line}")
  endif()
  set(preset "${CMAKE_CURRENT_SOURCE_DIR}/presets/bodies/${CMAKE_MATCH_1}.body240")
  if(NOT EXISTS "${preset}")
    message(FATAL_ERROR "Shipping preset is missing: ${preset}")
  endif()
  list(APPEND TRENCH_SHIPPING_PRESET_FILES "${preset}")
endforeach()

juce_add_binary_data(TRENCHAssets SOURCES
  assets/df2_panel_beige.png
  assets/trench_roller_strip.png
  assets/trench_knob_strip.png
  assets/display_bitmap4613.png
  assets/key_model_rtneural.json
  assets/bodies/identity.body240
  ${TRENCH_SHIPPING_PRESET_FILES})

set(TRENCH_PLUGIN_SOURCES
  source/PluginProcessor.h source/PluginProcessor.cpp
  source/PluginEditor.h source/PluginEditor.cpp
  source/TrenchBodyRoster.h source/UiLayout.h
  source/parameters/TrenchParameters.h source/parameters/TrenchParameters.cpp
  source/parameters/Curves.h source/parameters/CurveMap.h
  source/dsp/KeyDetector.h source/dsp/KeyDetector.cpp
  source/dsp/Movement.h source/dsp/SlamStage.h
  source/dsp/TrenchCleanBody.h source/dsp/TrenchDspBridge.h
  source/dsp/TrenchRuntimePreset.h source/dsp/FuncGenPatterns.h
  source/ui/DecalsLayer.h source/ui/FaceplateView.h source/ui/GodMode.h
  source/ui/GraphDisplay.h source/ui/KeySnapBox.h source/ui/LabelsLayer.h
  source/ui/SectionRail.h source/ui/BayKnob.h source/ui/ParamInteraction.h
  source/ui/SelectorLookAndFeel.h source/ui/Onboarding.h source/ui/Theme.h
  source/ui/TypeSelectorView.h source/ui/ValueReadout.h source/ui/WheelControl.h)

add_library(TRENCHPluginCommon STATIC ${TRENCH_PLUGIN_SOURCES})
target_compile_features(TRENCHPluginCommon PUBLIC cxx_std_20)
target_include_directories(TRENCHPluginCommon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCHPluginCommon PUBLIC ${TRENCH_COMMON_DEFINITIONS})
target_link_libraries(TRENCHPluginCommon PUBLIC
  TRENCHAssets trench_native_core RTNeural
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats
  juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_plugin(TRENCH
  COMPANY_NAME "Signal Methods"
  BUNDLE_ID "com.signalmethods.trench"
  PLUGIN_MANUFACTURER_CODE Sgmd
  PLUGIN_CODE Tr01
  FORMATS VST3 Standalone
  PRODUCT_NAME "TRENCH"
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_link_libraries(TRENCH PRIVATE TRENCHPluginCommon)

juce_add_console_app(TRENCH_FaceShot PRODUCT_NAME "TRENCH FaceShot")
target_sources(TRENCH_FaceShot PRIVATE tools/FaceShot.cpp)
target_include_directories(TRENCH_FaceShot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_FaceShot PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(TRENCH_FaceShot PRIVATE
    TRENCHPluginCommon
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
