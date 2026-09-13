add_library(trench_native_core STATIC
  src/body_from_audio.cpp
  src/measure.cpp
  src/fit_target.cpp
  src/packed_body.cpp
  src/response.cpp
  src/native_body.cpp
  src/native_fit.cpp
  src/p2k/container.cpp
  src/p2k/corner.cpp
  src/p2k/fit.cpp
  src/p2k/gain.cpp
  src/p2k/objective.cpp
  src/p2k/polish.cpp
  src/p2k/role.cpp
  src/p2k/morph.cpp
  src/p2k/formants.cpp
  src/p2k/postures.cpp
  src/p2k/templates.cpp
  src/p2k/rows_fit.cpp
  src/p2k/widths.cpp
  src/audition.cpp
  src/transpose.cpp
  src/p2k/section_param.cpp
  src/p2k/seed.cpp
  src/p2k/watched.cpp
  src/bisection.cpp
)

find_package(Ceres 2.2.0 EXACT CONFIG REQUIRED)
find_package(Eigen3 5.0.1 EXACT CONFIG REQUIRED)
find_path(POCKETFFT_INCLUDE_DIR pocketfft_hdronly.h REQUIRED)
target_link_libraries(trench_native_core PRIVATE Ceres::ceres Eigen3::Eigen)
target_include_directories(trench_native_core PRIVATE ${POCKETFFT_INCLUDE_DIR})

target_include_directories(trench_native_core PUBLIC include)
target_compile_features(trench_native_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(trench_native_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(trench_native_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(trench_core_c SHARED
  src/trench_core_c.cpp
)
target_link_libraries(trench_core_c PRIVATE trench_native_core)
target_include_directories(trench_core_c PUBLIC include)
target_compile_features(trench_core_c PUBLIC cxx_std_20)
target_compile_definitions(trench_core_c PRIVATE TRENCH_CORE_C_BUILD)
if(MSVC)
  target_compile_options(trench_core_c PRIVATE /W4 /permissive-)
endif()

if(NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  add_executable(trench_core_tests tests/packed_body_tests.cpp)
  target_link_libraries(trench_core_tests PRIVATE trench_native_core)
  add_test(NAME trench_core COMMAND trench_core_tests)
  add_executable(trench_core_from_audio_tests tests/body_from_audio_tests.cpp)
  target_link_libraries(trench_core_from_audio_tests PRIVATE trench_native_core)
  add_test(NAME trench_core_from_audio COMMAND trench_core_from_audio_tests)
  add_executable(trench_morph_parity tools/morph_parity.cpp)
  target_link_libraries(trench_morph_parity PRIVATE trench_native_core)
endif()
