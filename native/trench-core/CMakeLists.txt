add_library(trench_native_core STATIC
  src/measure.cpp
  src/packed_body.cpp
  src/response.cpp
  src/native_body.cpp
  src/rbj.cpp
  src/p2k/container.cpp
  src/p2k/corner.cpp
  src/p2k/fit.cpp
  src/p2k/gain.cpp
  src/p2k/objective.cpp
  src/p2k/polish.cpp
  src/p2k/push.cpp
  src/p2k/role.cpp
  src/p2k/morph.cpp
  src/p2k/formants.cpp
  src/p2k/rows_fit.cpp
  src/audition.cpp
  src/transpose.cpp
  src/p2k/section_param.cpp
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

add_executable(trench_audition_morph tools/audition_morph.cpp)
target_link_libraries(trench_audition_morph PRIVATE trench_native_core)

add_executable(trench_interior_envelope tools/interior_envelope.cpp)
target_link_libraries(trench_interior_envelope PRIVATE trench_native_core)
target_compile_definitions(trench_interior_envelope PRIVATE
  TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
)

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
    tests/p2k_space_test.cpp
    tests/p2k_role_test.cpp
    tests/p2k_section_param_test.cpp
    tests/p2k_morph_test.cpp
    tests/native_body_test.cpp
    tests/rbj_test.cpp
    tests/audition_test.cpp
    tests/transpose_test.cpp
    tests/p2k_formants_test.cpp
    tests/p2k_rows_fit_test.cpp
  )
  target_link_libraries(trench_native_core_tests PRIVATE trench_native_core GTest::gtest_main
    nlohmann_json::nlohmann_json)
  target_compile_definitions(trench_native_core_tests PRIVATE
    TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
  )
  include(GoogleTest)
  gtest_discover_tests(trench_native_core_tests)
endif()
