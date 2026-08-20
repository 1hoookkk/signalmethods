add_library(trench_native_core STATIC
  src/packed_body.cpp
  src/response.cpp
)

target_include_directories(trench_native_core PUBLIC include)
target_compile_features(trench_native_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(trench_native_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(trench_native_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(trench_corpus_contribution tools/corpus_contribution.cpp)
target_link_libraries(trench_corpus_contribution PRIVATE trench_native_core)

if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)
  add_executable(trench_native_core_tests tests/core_tests.cpp)
  target_link_libraries(trench_native_core_tests PRIVATE trench_native_core GTest::gtest_main)
  target_compile_definitions(trench_native_core_tests PRIVATE
    TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
  )
  include(GoogleTest)
  gtest_discover_tests(trench_native_core_tests)
endif()
