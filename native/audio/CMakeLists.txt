include(FetchContent)

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(juce
  URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/9.0.1
  URL_HASH SHA256=3D8561EE685592513EB546B10D10DC078AE857644E18AEB1E1B4C1F1758492EB
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(juce)

add_library(trench_native_audio_boundary STATIC audio_boundary.cpp)
target_include_directories(trench_native_audio_boundary PUBLIC include)
target_link_libraries(trench_native_audio_boundary PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_audio_processors_headless
)
target_compile_definitions(trench_native_audio_boundary PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

if(BUILD_TESTING)
  find_package(GTest 1.18.0 EXACT CONFIG REQUIRED)
  add_executable(trench_native_audio_tests tests/audio_boundary_tests.cpp)
  target_link_libraries(trench_native_audio_tests PRIVATE
    trench_native_audio_boundary
    GTest::gtest_main
  )
  include(GoogleTest)
  gtest_discover_tests(trench_native_audio_tests)
endif()
