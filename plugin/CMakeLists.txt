cmake_minimum_required(VERSION 3.30)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(TRENCHPlugin VERSION 0.1.0 LANGUAGES C CXX)
endif()

if(NOT TARGET trench_native_core)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../native/core"
                   "${CMAKE_CURRENT_BINARY_DIR}/native-core")
endif()

include(FetchContent)
if(WIN32 AND NOT DEFINED Python3_EXECUTABLE)
  find_program(TRENCH_PYTHON_COMMAND_FROM_PATH NAMES python python3 NO_CACHE)
  if(TRENCH_PYTHON_COMMAND_FROM_PATH)
    set(Python3_EXECUTABLE "${TRENCH_PYTHON_COMMAND_FROM_PATH}")
  endif()
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
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

set(TRENCH_WORKSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(TRENCH_COMMON_DEFINITIONS
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
  assets/trench_display_grid.png
  assets/display_bitmap4613.png
  assets/key_model_rtneural.json
  assets/svg/knob_body.svg
  assets/svg/knob_body_cream.svg
  assets/svg/knob_pointer.svg
  assets/svg/tick_ring.svg
  assets/bodies/identity.body240
  ${TRENCH_SHIPPING_PRESET_FILES})

set(TRENCH_PLUGIN_SOURCES
  source/PluginProcessor.h source/PluginProcessor.cpp
  source/PluginEditor.h source/PluginEditor.cpp
  source/TrenchBodyRoster.h source/UiLayout.h
  source/parameters/TrenchParameters.h source/parameters/TrenchParameters.cpp
  source/parameters/Curves.h source/parameters/CurveMap.h
  source/dsp/KeyDetector.h source/dsp/KeyDetector.cpp
  source/dsp/Movement.h source/dsp/SlamStage.h source/dsp/DeskDrive.h source/dsp/EnvFollower.h
  source/dsp/TrenchCleanBody.h source/dsp/TrenchDspBridge.h
  source/dsp/TrenchRuntimePreset.h source/dsp/FuncGenPatterns.h source/dsp/PreampLaw.h
  source/ui/FaceplateView.h source/ui/GraphDisplay.h source/ui/LabelsLayer.h source/ui/GlassWords.h source/ui/ModulationChip.h
  source/ui/ParamInteraction.h
  source/ui/SelectorLookAndFeel.h source/ui/Theme.h
  source/ui/TypeSelectorView.h source/ui/ValueReadout.h source/ui/WheelControl.h
  source/ui/Onboarding.h source/ui/DevPanel.h source/dsp/WheelLoop.h)

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

add_library(TRENCHPluginCommonDev STATIC ${TRENCH_PLUGIN_SOURCES})
target_compile_features(TRENCHPluginCommonDev PUBLIC cxx_std_20)
target_include_directories(TRENCHPluginCommonDev PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCHPluginCommonDev PUBLIC ${TRENCH_COMMON_DEFINITIONS} TRENCH_DEV_PANEL=1)
target_link_libraries(TRENCHPluginCommonDev PUBLIC
  TRENCHAssets trench_native_core RTNeural
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats
  juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_plugin(TRENCH_Dev
  COMPANY_NAME "Signal Methods"
  BUNDLE_ID "com.signalmethods.trench.dev"
  PLUGIN_MANUFACTURER_CODE Sgmd
  PLUGIN_CODE Tr0d
  FORMATS VST3
  PRODUCT_NAME "TRENCH Dev"
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_link_libraries(TRENCH_Dev PRIVATE TRENCHPluginCommonDev)

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

juce_add_console_app(TRENCH_Stimuli PRODUCT_NAME "TRENCH Stimuli")
target_sources(TRENCH_Stimuli PRIVATE tools/Stimuli.cpp)
target_include_directories(TRENCH_Stimuli PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_Stimuli PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(TRENCH_Stimuli PRIVATE
    TRENCHPluginCommon
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app(TRENCH_Render PRODUCT_NAME "TRENCH Render")
target_sources(TRENCH_Render PRIVATE tools/Render.cpp)
target_include_directories(TRENCH_Render PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_Render PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(TRENCH_Render PRIVATE
    TRENCHPluginCommon
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app(TRENCH_RenderDev PRODUCT_NAME "TRENCH Render Dev")
target_sources(TRENCH_RenderDev PRIVATE tools/Render.cpp)
target_include_directories(TRENCH_RenderDev PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_RenderDev PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_MODAL_LOOPS_PERMITTED=1 TRENCH_DEV_PANEL=1)
target_link_libraries(TRENCH_RenderDev PRIVATE
    TRENCHPluginCommonDev
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app(TRENCH_Tests PRODUCT_NAME "TRENCH Tests")
target_sources(TRENCH_Tests PRIVATE tests/PluginTests.cpp)
target_include_directories(TRENCH_Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_Tests PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(TRENCH_Tests PRIVATE
    TRENCHPluginCommon
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME trench_plugin COMMAND TRENCH_Tests WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(trench_plugin PROPERTIES ENVIRONMENT "TRENCH_HEADLESS=1")
juce_add_console_app(TRENCH_ReviewTests PRODUCT_NAME "TRENCH Review Tests")
target_sources(TRENCH_ReviewTests PRIVATE tests/ReviewTests.cpp)
target_include_directories(TRENCH_ReviewTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_ReviewTests PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(TRENCH_ReviewTests PRIVATE
    TRENCHPluginCommon
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME trench_plugin_review COMMAND TRENCH_ReviewTests WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(trench_plugin_review PROPERTIES ENVIRONMENT "TRENCH_HEADLESS=1" TIMEOUT 600)
add_test(NAME trench_gen_curves
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gen_curves.py"
    WORKING_DIRECTORY "${TRENCH_WORKSPACE_DIR}")
set_tests_properties(trench_gen_curves PROPERTIES ENVIRONMENT "MPLBACKEND=Agg")

juce_add_gui_app(TRENCH_Workstation
  COMPANY_NAME "Signal Methods"
  BUNDLE_ID "com.signalmethods.trench.workstation"
  PRODUCT_NAME "TRENCH Workstation")
set(TRENCH_WORKSTATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../native/workstation/Source")
target_sources(TRENCH_Workstation PRIVATE
  "${TRENCH_WORKSTATION_DIR}/Main.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Frame.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Delaunay.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Library.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Timeline.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Body.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Morph.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Stitch.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Sound.cpp"
  "${TRENCH_WORKSTATION_DIR}/render/Scene.cpp"
  "${TRENCH_WORKSTATION_DIR}/render/GLRenderer.cpp"
  "${TRENCH_WORKSTATION_DIR}/render/SoftwareRenderer.cpp"
  "${TRENCH_WORKSTATION_DIR}/render/Canvas.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/Layout.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/View3D.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/Workstation.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/WorkstationKeys.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/WorkstationActions.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/WorkstationInput.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/WorkstationScene.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/WorkstationPaint.cpp"
  "${TRENCH_WORKSTATION_DIR}/ui/Audio.cpp")
target_include_directories(TRENCH_Workstation PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../native/workstation/Source")
target_compile_features(TRENCH_Workstation PRIVATE cxx_std_20)
target_compile_definitions(TRENCH_Workstation PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_APPLICATION_NAME_STRING="TRENCH Workstation"
    JUCE_APPLICATION_VERSION_STRING="0.1.0")
target_link_libraries(TRENCH_Workstation PRIVATE
    trench_native_core
    juce::juce_gui_basics
    juce::juce_opengl
    juce::juce_dsp
    juce::juce_audio_devices
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app(TRENCH_WorkstationTests PRODUCT_NAME "TRENCH Workstation Tests")
target_sources(TRENCH_WorkstationTests PRIVATE
  "${TRENCH_WORKSTATION_DIR}/../Tests/MorphTests.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Frame.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Delaunay.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Library.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Body.cpp"
  "${TRENCH_WORKSTATION_DIR}/model/Morph.cpp")
target_include_directories(TRENCH_WorkstationTests PRIVATE "${TRENCH_WORKSTATION_DIR}")
target_compile_features(TRENCH_WorkstationTests PRIVATE cxx_std_20)
target_compile_definitions(TRENCH_WorkstationTests PRIVATE ${TRENCH_COMMON_DEFINITIONS})
target_link_libraries(TRENCH_WorkstationTests PRIVATE
    trench_native_core
    juce::juce_graphics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME trench_workstation COMMAND TRENCH_WorkstationTests WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(trench_workstation PROPERTIES ENVIRONMENT "TRENCH_HEADLESS=1" TIMEOUT 300)
