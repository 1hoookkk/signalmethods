cmake_minimum_required(VERSION 3.30)

project(TrenchNative LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
add_subdirectory(native/trench-core)
add_subdirectory(native/foundation)
add_subdirectory(native/audio)
add_subdirectory(native/research)
add_subdirectory(native/app)
