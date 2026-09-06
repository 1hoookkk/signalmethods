set(TRENCH_HEADSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/app")
set(TRENCH_HEADSPACE_SOURCES
  "${TRENCH_HEADSPACE_DIR}/Strip.h" "${TRENCH_HEADSPACE_DIR}/Strip.cpp"
  "${TRENCH_HEADSPACE_DIR}/Library.h" "${TRENCH_HEADSPACE_DIR}/Library.cpp"
  "${TRENCH_HEADSPACE_DIR}/Audio.h" "${TRENCH_HEADSPACE_DIR}/Audio.cpp"
  "${TRENCH_HEADSPACE_DIR}/Session.h" "${TRENCH_HEADSPACE_DIR}/Session.cpp"
  "${TRENCH_HEADSPACE_DIR}/Screen.h" "${TRENCH_HEADSPACE_DIR}/Screen.cpp")

juce_add_gui_app(TRENCH_Headspace_App
  PRODUCT_NAME "HEADSPACE"
  COMPANY_NAME "Signal Methods"
  VERSION 0.1.0)
target_sources(TRENCH_Headspace_App PRIVATE "${TRENCH_HEADSPACE_DIR}/Main.cpp" ${TRENCH_HEADSPACE_SOURCES})
target_include_directories(TRENCH_Headspace_App PRIVATE "${TRENCH_HEADSPACE_DIR}")
target_compile_features(TRENCH_Headspace_App PRIVATE cxx_std_20)
target_compile_definitions(TRENCH_Headspace_App PRIVATE ${TRENCH_COMMON_DEFINITIONS})
target_link_libraries(TRENCH_Headspace_App PRIVATE
  trench_native_core
  juce::juce_audio_utils
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)

juce_add_console_app(TRENCH_StripTests PRODUCT_NAME "TRENCH Strip Tests")
target_sources(TRENCH_StripTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests/StripTests.cpp" ${TRENCH_HEADSPACE_SOURCES})
target_include_directories(TRENCH_StripTests PRIVATE "${TRENCH_HEADSPACE_DIR}")
target_compile_features(TRENCH_StripTests PRIVATE cxx_std_20)
target_compile_definitions(TRENCH_StripTests PRIVATE ${TRENCH_COMMON_DEFINITIONS})
target_link_libraries(TRENCH_StripTests PRIVATE
  trench_native_core
  juce::juce_audio_utils
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)
add_test(NAME trench_strip COMMAND TRENCH_StripTests WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(trench_strip PROPERTIES ENVIRONMENT "TRENCH_HEADLESS=1" TIMEOUT 300)
