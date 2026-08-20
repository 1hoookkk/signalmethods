find_package(Ceres 2.2.0 EXACT CONFIG REQUIRED)
find_package(Eigen3 5.0.1 EXACT CONFIG REQUIRED)
find_package(GTest 1.18.0 EXACT CONFIG REQUIRED)
find_path(POCKETFFT_INCLUDE_DIR pocketfft_hdronly.h REQUIRED)

message(STATUS "Resolved Ceres ${Ceres_VERSION}")
message(STATUS "Resolved Eigen ${Eigen3_VERSION}")
message(STATUS "Resolved GoogleTest ${GTest_VERSION}")

if(BUILD_TESTING)
  add_executable(trench_native_foundation_tests foundation_tests.cpp)
  target_include_directories(trench_native_foundation_tests PRIVATE ${POCKETFFT_INCLUDE_DIR})
  target_link_libraries(trench_native_foundation_tests PRIVATE
    Ceres::ceres
    Eigen3::Eigen
    GTest::gtest_main
  )
  include(GoogleTest)
  gtest_discover_tests(trench_native_foundation_tests)
endif()
