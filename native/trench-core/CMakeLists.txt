add_library(trench_native_core STATIC
  src/measure.cpp
  src/packed_body.cpp
  src/response.cpp
  src/p2k/container.cpp
  src/p2k/corner.cpp
  src/p2k/fit.cpp
  src/p2k/gain.cpp
  src/p2k/objective.cpp
  src/p2k/polish.cpp
  src/p2k/push.cpp
  src/p2k/seed.cpp
  src/p2k/watched.cpp
)

find_package(Eigen3 5.0.1 EXACT CONFIG REQUIRED)
find_path(POCKETFFT_INCLUDE_DIR pocketfft_hdronly.h REQUIRED)
target_link_libraries(trench_native_core PRIVATE Eigen3::Eigen)
target_include_directories(trench_native_core PRIVATE ${POCKETFFT_INCLUDE_DIR})

target_include_directories(trench_native_core PUBLIC include)
target_compile_features(trench_native_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(trench_native_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(trench_native_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(trench_corpus_contribution tools/corpus_contribution.cpp)
target_link_libraries(trench_corpus_contribution PRIVATE trench_native_core)

add_executable(trench_fit_target tools/fit_target.cpp)
target_link_libraries(trench_fit_target PRIVATE trench_native_core)

if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  add_executable(trench_native_core_tests
    tests/core_tests.cpp
    tests/measure_test.cpp
    tests/p2k_parity_test.cpp
    tests/p2k_container_laws_test.cpp
    tests/p2k_fitter_semantics_test.cpp
    tests/p2k_push_test.cpp
    tests/p2k_refit_bank_test.cpp
  )
  target_link_libraries(trench_native_core_tests PRIVATE trench_native_core GTest::gtest_main
    nlohmann_json::nlohmann_json)
  target_compile_definitions(trench_native_core_tests PRIVATE
    TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
  )
  include(GoogleTest)
  gtest_discover_tests(trench_native_core_tests)
endif()
