cmake_minimum_required(VERSION 3.25)

project(TRENCH_WorkstationProject VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use the pinned JUCE checkout already used by the shipping build when it is
# present. A normal FetchContent fallback keeps this clean workstation
# checkout reproducible on another machine without copying the old checkout.
# 8.0.15 (Tyson 2026-08-15): stays on the 8.x line but takes the Windows
# DPI/host-scaling fixes 8.0.13 lacked. The old 628b cache dir held 8.0.13,
# so the default pin points at an 8.0.15 cache slot; absent, FetchContent
# pulls the 8.0.15 tag.
set(TRENCH_JUCE_DIR "$ENV{USERPROFILE}/.cache/CPM/juce/901" CACHE PATH
    "Pinned JUCE 9.0.1 source directory")
if(EXISTS "${TRENCH_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${TRENCH_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# The key detector uses RTNeural's allocation-free forward pass. Prefer the
# local vendored checkout, with a pinned FetchContent fallback for clean clones.
# Model parsing happens once in the processor constructor, never on audio.
set(RTNEURAL_STL ON CACHE BOOL "Use RTNeural STL backend" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/RTNeural/CMakeLists.txt")
    add_subdirectory(libs/RTNeural EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        RTNeuralSource
        GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
        GIT_TAG 1fb1f075a5d66e85bfc8f488c3f3626840cb3a1d
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(RTNeuralSource)
endif()
target_compile_definitions(RTNeural PUBLIC RTNEURAL_NO_DEBUG=1)

set(TRENCH_WORKSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

# GOD MODE: the operator's live layout desk (Ctrl+Shift+G in the editor). OFF in
# every shipping build - with the option off the feature is not compiled at all,
# so the VST3 contains none of it. Turn it on for a face-tuning session with:
#     cmake -S plugin -B build -DTRENCH_GOD_MODE=ON
#     cmake --build build --target TRENCH_VST3 --config Release
# The same option adds the TRENCH_GodModeTest round-trip proof target.
option(TRENCH_GOD_MODE "Compile the in-editor layout desk (never ship this ON)" OFF)

set(TRENCH_COMMON_DEFINITIONS
    TRENCH_GOD_MODE=$<BOOL:${TRENCH_GOD_MODE}>
    TRENCH_TABLE_STITCH_PANEL=0    # product face: no TABLES dev overlay (dev workstation can flip to 1).
    TRENCH_TABLE_STITCH_ROOT="${TRENCH_WORKSPACE_DIR}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0   # no "Made with JUCE" overlay (requires a paid/appropriate JUCE license at release)
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JucePlugin_Name="TRENCH"
    JucePlugin_Desc="TRENCH filter processor"
    JucePlugin_Manufacturer="Trench"
    JucePlugin_ManufacturerCode=0x54726563
    JucePlugin_PluginCode=0x54303031
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_EditorRequiresKeyboardFocus=0
    JucePlugin_VersionString="1.0.0"
    JucePlugin_Version=0x10000
    JucePlugin_VST3Category=Fx)

file(GLOB TRENCH_PRESET_BODY_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/bodies/*.body240")

# X3 runtime preset cartridges (RUNTIME roster slots). Four verbatim xStream
# banks each; the loader picks the one nearest the host rate. Baked, not
# scanned, so the roster is identical on every machine.
#
# Named X3F_<stem>.json here, NOT .x3preset.json: JUCE mangles a filename into
# a BinaryData symbol, and the roster looks one up as base + "_json".
# "X3F_phaser_2.x3preset.json" would become "X3F_phaser_2_x3preset_json" and
# never resolve. The loader keys off the format string inside the file
# (isRuntimePresetJson), not the extension, so the shorter name is safe.
file(GLOB TRENCH_RUNTIME_PRESET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/bodies/X3F_*.json")

juce_add_binary_data(TRENCHAssets
    SOURCES
        assets/df2_panel_beige.png
        assets/trench_roller_strip.png
        assets/trench_knob_strip.png
        assets/trench_glass.png
        assets/trench_display_grid.png
        assets/thin_wheel_strip.png
        assets/key_model_rtneural.json
        assets/bodies/identity.body240
        ${TRENCH_PRESET_BODY_FILES}
        ${TRENCH_RUNTIME_PRESET_FILES})

set(TRENCH_PLUGIN_SOURCES
    source/PluginProcessor.h
    source/PluginProcessor.cpp
    source/PluginEditor.h
    source/PluginEditor.cpp
    source/TrenchBodyRoster.h
    source/UiLayout.h
    source/parameters/TrenchParameters.h
    source/parameters/TrenchParameters.cpp
    presets/PresetRoster.inc
    source/dsp/KeyDetector.h
    source/dsp/KeyDetector.cpp
    source/dsp/Movement.h
    source/dsp/SlamStage.h
    source/dsp/TrenchCleanBody.h
    source/dsp/TrenchDspBridge.h
    source/ui/DecalsLayer.h
    source/ui/FaceplateView.h
    source/ui/GodMode.h
    source/ui/GraphDisplay.h
    source/ui/KeySnapBox.h
    source/ui/LabelsLayer.h
    source/ui/SectionRail.h
    source/ui/MixKnob.h
    source/ui/ParamInteraction.h
    source/ui/SelectorLookAndFeel.h
    source/ui/Onboarding.h
    source/ui/Theme.h
    source/ui/TypeSelectorView.h
    source/ui/ValueReadout.h
    source/ui/WheelControl.h)

add_library(TRENCHPluginCommon STATIC ${TRENCH_PLUGIN_SOURCES})
target_compile_features(TRENCHPluginCommon PUBLIC cxx_std_20)
target_include_directories(TRENCHPluginCommon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCHPluginCommon PUBLIC ${TRENCH_COMMON_DEFINITIONS})
target_link_libraries(TRENCHPluginCommon
    PUBLIC
        TRENCHAssets
        juce_audio_utils
        juce_audio_processors
        juce_audio_formats
        juce_dsp
        juce_gui_basics
        juce_gui_extra
        RTNeural
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

set(TRENCH_CARGO_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo-target")
set(TRENCH_RUST_LIB_DIR "${TRENCH_CARGO_TARGET_DIR}/release")
if(WIN32)
    set(TRENCH_CORE_LIB "${TRENCH_RUST_LIB_DIR}/trench_core.lib")
else()
    set(TRENCH_CORE_LIB "${TRENCH_RUST_LIB_DIR}/libtrench_core.a")
endif()

find_program(CARGO_EXECUTABLE cargo REQUIRED)
file(GLOB_RECURSE TRENCH_RUST_SOURCES CONFIGURE_DEPENDS
    "${TRENCH_WORKSPACE_DIR}/trench-core/src/*.rs")
set(TRENCH_RUST_MANIFESTS
    "${TRENCH_WORKSPACE_DIR}/Cargo.toml"
    "${TRENCH_WORKSPACE_DIR}/Cargo.lock"
    "${TRENCH_WORKSPACE_DIR}/trench-core/Cargo.toml")

add_custom_command(
    OUTPUT "${TRENCH_CORE_LIB}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "CARGO_TARGET_DIR=${TRENCH_CARGO_TARGET_DIR}"
        "${CARGO_EXECUTABLE}" rustc -p trench-core --release --lib --crate-type staticlib
    WORKING_DIRECTORY "${TRENCH_WORKSPACE_DIR}"
    DEPENDS ${TRENCH_RUST_SOURCES} ${TRENCH_RUST_MANIFESTS}
    COMMENT "Building the shipping trench-core static library"
    VERBATIM
    USES_TERMINAL)
add_custom_target(trench_core_build DEPENDS "${TRENCH_CORE_LIB}")

set(TRENCH_WINDOWS_LIBS ntdll userenv ws2_32 bcrypt)

juce_add_plugin(TRENCH
    COMPANY_NAME "Trench"
    BUNDLE_ID "com.trench.field"
    PLUGIN_MANUFACTURER_CODE Trec
    PLUGIN_CODE T001
    FORMATS VST3 Standalone
    PRODUCT_NAME "TRENCH"
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_link_libraries(TRENCH PRIVATE TRENCHPluginCommon "${TRENCH_CORE_LIB}" ${TRENCH_WINDOWS_LIBS})
add_dependencies(TRENCH trench_core_build)
if(TARGET TRENCH_VST3)
    add_dependencies(TRENCH_VST3 trench_core_build)
endif()

# The JUCE authoring workstation is gone (Tyson verdict 2026-08-07): the
# standalone Workstation is tools/workstation (Python + Qt + the trench-core
# FFI), sharing batch_compiler's one planning path. Only the plugin and its
# proof tools build here. ArmadilloCoords.h stays: it is the coordinate law,
# proved by TRENCH_ArmadilloTests.

# The ARMAdillo coordinate law, proved on its own. No JUCE, no runtime, no
# audio - the map must be right before any UI is built on it.
add_executable(TRENCH_ArmadilloTests tools/ArmadilloTests.cpp)
target_include_directories(TRENCH_ArmadilloTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/native")
target_compile_definitions(TRENCH_ArmadilloTests PRIVATE
    ARMADILLO_COORDS_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/native/ArmadilloCoords.h")
set_target_properties(TRENCH_ArmadilloTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)


# Render the real faceplate to a PNG (judge the UI without opening a DAW). Not shipped.
juce_add_console_app(TRENCH_FaceShot PRODUCT_NAME "TRENCH FaceShot")
target_sources(TRENCH_FaceShot PRIVATE tools/FaceShot.cpp)
target_include_directories(TRENCH_FaceShot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_FaceShot PRIVATE ${TRENCH_COMMON_DEFINITIONS}
    JUCE_MODAL_LOOPS_PERMITTED=1)   # this tool pumps the message loop; the plugin never does
target_link_libraries(TRENCH_FaceShot PRIVATE
    TRENCHPluginCommon
    "${TRENCH_CORE_LIB}"
    ${TRENCH_WINDOWS_LIBS}
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_dependencies(TRENCH_FaceShot trench_core_build)

# The runtime proof: engine lifetime under an in-flight callback, host blocks
# larger and smaller than the prepared size, old-session parameter migration,
# and the AUTO KEY worker's bounded start/stop. Headless, no DAW.
juce_add_console_app(TRENCH_RenderNull PRODUCT_NAME "TRENCH Render Null")
target_sources(TRENCH_RenderNull PRIVATE tools/RenderNull.cpp)
target_include_directories(TRENCH_RenderNull PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_RenderNull PRIVATE ${TRENCH_COMMON_DEFINITIONS})
target_link_libraries(TRENCH_RenderNull PRIVATE
    TRENCHPluginCommon
    "${TRENCH_CORE_LIB}"
    ${TRENCH_WINDOWS_LIBS}
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
add_dependencies(TRENCH_RenderNull trench_core_build)

juce_add_console_app(TRENCH_LifecycleTests PRODUCT_NAME "TRENCH Lifecycle Tests")
target_sources(TRENCH_LifecycleTests PRIVATE tools/LifecycleTests.cpp)
target_include_directories(TRENCH_LifecycleTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(TRENCH_LifecycleTests PRIVATE ${TRENCH_COMMON_DEFINITIONS})
target_link_libraries(TRENCH_LifecycleTests PRIVATE
    TRENCHPluginCommon
    "${TRENCH_CORE_LIB}"
    ${TRENCH_WINDOWS_LIBS}
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_dependencies(TRENCH_LifecycleTests trench_core_build)

# GOD MODE's round trip, proved with no window: move an element, save, reload,
# and check the rect (and a colour token) came back byte-for-byte. Only exists
# when the option is on - the shipping configuration never sees this target.
if(TRENCH_GOD_MODE)
    juce_add_console_app(TRENCH_GodModeTest PRODUCT_NAME "TRENCH God Mode Test")
    target_sources(TRENCH_GodModeTest PRIVATE tools/GodModeTest.cpp)
    target_include_directories(TRENCH_GodModeTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
    target_compile_definitions(TRENCH_GodModeTest PRIVATE ${TRENCH_COMMON_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1)   # this tool pumps the message loop; the plugin never does
    target_link_libraries(TRENCH_GodModeTest PRIVATE
        TRENCHPluginCommon
        "${TRENCH_CORE_LIB}"
        ${TRENCH_WINDOWS_LIBS}
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_dependencies(TRENCH_GodModeTest trench_core_build)
endif()

# Offline proof executable: compares the JUCE FFT frontend and RTNeural
# probabilities with Python references for the same held-out audio window.
juce_add_console_app(TRENCH_KeyModelParity PRODUCT_NAME "TRENCH Key Model Parity")
target_compile_features(TRENCH_KeyModelParity PRIVATE cxx_std_20)
target_sources(TRENCH_KeyModelParity PRIVATE
    tools/KeyModelParity.cpp
    source/dsp/KeyDetector.cpp
    source/dsp/KeyDetector.h)
target_include_directories(TRENCH_KeyModelParity PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_link_libraries(TRENCH_KeyModelParity PRIVATE
    RTNeural
    juce_audio_basics
    juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
