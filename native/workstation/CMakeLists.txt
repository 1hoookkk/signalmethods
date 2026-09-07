set(TRENCH_HEADSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/app")
set(TRENCH_HEADSPACE_UI "${CMAKE_CURRENT_SOURCE_DIR}/Source/ui")
set(TRENCH_HEADSPACE_DSP "${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp")
set(TRENCH_HEADSPACE_SOURCES
  "${TRENCH_HEADSPACE_DIR}/Quad.h" "${TRENCH_HEADSPACE_DIR}/Quad.cpp"
  "${TRENCH_HEADSPACE_DIR}/Library.h" "${TRENCH_HEADSPACE_DIR}/Library.cpp"
  "${TRENCH_HEADSPACE_DIR}/Audio.h" "${TRENCH_HEADSPACE_DIR}/Audio.cpp"
  "${TRENCH_HEADSPACE_DIR}/Session.h" "${TRENCH_HEADSPACE_DIR}/Session.cpp"
  "${TRENCH_HEADSPACE_DIR}/Bridge.h" "${TRENCH_HEADSPACE_DIR}/Bridge.cpp"
  "${TRENCH_HEADSPACE_UI}/Look.h" "${TRENCH_HEADSPACE_UI}/Look.cpp"
  "${TRENCH_HEADSPACE_UI}/Plot.h" "${TRENCH_HEADSPACE_UI}/Plot.cpp"
  "${TRENCH_HEADSPACE_UI}/Palette.h" "${TRENCH_HEADSPACE_UI}/Palette.cpp"
  "${TRENCH_HEADSPACE_UI}/Mother.h" "${TRENCH_HEADSPACE_UI}/Mother.cpp"
  "${TRENCH_HEADSPACE_UI}/Stage.h" "${TRENCH_HEADSPACE_UI}/Stage.cpp"
  "${TRENCH_HEADSPACE_UI}/Body.h" "${TRENCH_HEADSPACE_UI}/Body.cpp"
  "${TRENCH_HEADSPACE_UI}/Engine.h" "${TRENCH_HEADSPACE_UI}/Engine.cpp"
  "${TRENCH_HEADSPACE_UI}/Keyboard.h" "${TRENCH_HEADSPACE_UI}/Keyboard.cpp"
  "${TRENCH_HEADSPACE_UI}/PinMenu.h" "${TRENCH_HEADSPACE_UI}/PinMenu.cpp"
  "${TRENCH_HEADSPACE_UI}/Screen.h" "${TRENCH_HEADSPACE_UI}/Screen.cpp"
  "${TRENCH_HEADSPACE_UI}/Spectrogram.h" "${TRENCH_HEADSPACE_UI}/Spectrogram.cpp"
  "${TRENCH_HEADSPACE_DSP}/Peevers.h" "${TRENCH_HEADSPACE_DSP}/Peevers.cpp"
  "${TRENCH_HEADSPACE_DSP}/Formants.h" "${TRENCH_HEADSPACE_DSP}/Formants.cpp"
  "${TRENCH_HEADSPACE_DSP}/VectorFit.h" "${TRENCH_HEADSPACE_DSP}/VectorFit.cpp")

option(TRENCH_HEADSPACE_LTO "Enable link-time optimization for HEADSPACE and its acceptance tests" OFF)

set(TRENCH_HEADSPACE_DEFINITIONS ${TRENCH_COMMON_DEFINITIONS})
list(REMOVE_ITEM TRENCH_HEADSPACE_DEFINITIONS "JUCE_WEB_BROWSER=0")
set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${TRENCH_WORKSPACE_DIR}/out/nuget" CACHE PATH "Where the Microsoft.Web.WebView2 package sits")
set(TRENCH_JUCE_INTEROP_JS "${juce_SOURCE_DIR}/modules/juce_gui_extra/native/typescript/webview-interop/dist/index.js")

add_library(TRENCH_Headspace_Shared INTERFACE)
target_sources(TRENCH_Headspace_Shared INTERFACE ${TRENCH_HEADSPACE_SOURCES})
target_include_directories(TRENCH_Headspace_Shared INTERFACE "${TRENCH_HEADSPACE_DIR}" "${TRENCH_HEADSPACE_DIR}/..")
target_compile_features(TRENCH_Headspace_Shared INTERFACE cxx_std_20)
target_compile_definitions(TRENCH_Headspace_Shared INTERFACE ${TRENCH_HEADSPACE_DEFINITIONS})
target_link_libraries(TRENCH_Headspace_Shared INTERFACE
  trench_native_core
  juce::juce_audio_utils
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)
if(TRENCH_HEADSPACE_LTO)
  target_link_libraries(TRENCH_Headspace_Shared INTERFACE juce::juce_recommended_lto_flags)
endif()

juce_add_gui_app(TRENCH_Headspace_App
  PRODUCT_NAME "HEADSPACE"
  COMPANY_NAME "Signal Methods"
  BUNDLE_ID "com.signalmethods.headspace"
  VERSION 0.1.0
  NEEDS_WEBVIEW2 TRUE)
target_sources(TRENCH_Headspace_App PRIVATE
  "${TRENCH_HEADSPACE_DIR}/Main.cpp"
  "${TRENCH_HEADSPACE_UI}/Web.h" "${TRENCH_HEADSPACE_UI}/Web.cpp")
target_compile_definitions(TRENCH_Headspace_App PRIVATE
  JUCE_WEB_BROWSER=1
  JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
  TRENCH_JUCE_INTEROP_JS="${TRENCH_JUCE_INTEROP_JS}")
target_link_libraries(TRENCH_Headspace_App PRIVATE TRENCH_Headspace_Shared)

juce_add_console_app(TRENCH_QuadTests PRODUCT_NAME "TRENCH Quad Tests")
target_sources(TRENCH_QuadTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests/QuadTests.cpp")
target_compile_definitions(TRENCH_QuadTests PRIVATE JUCE_WEB_BROWSER=0)
target_link_libraries(TRENCH_QuadTests PRIVATE TRENCH_Headspace_Shared)
add_test(NAME trench_quad COMMAND TRENCH_QuadTests WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(trench_quad PROPERTIES ENVIRONMENT "TRENCH_HEADLESS=1" TIMEOUT 300)
