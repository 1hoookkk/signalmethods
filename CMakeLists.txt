cmake_minimum_required(VERSION 3.30)

project(TrenchNative VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(native/core)
add_subdirectory(native/audio)
add_subdirectory(native/app)
add_subdirectory(plugin)
