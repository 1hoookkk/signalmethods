add_library(trench_native_core STATIC
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
  src/p2k/zeros_fit.cpp
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
