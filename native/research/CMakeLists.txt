find_package(Python3 3.13 EXACT REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 3.1.0 EXACT CONFIG REQUIRED)

message(STATUS "Resolved Python ${Python3_VERSION}")
message(STATUS "Resolved pybind11 ${pybind11_VERSION}")

pybind11_add_module(trench_native_research MODULE bindings.cpp)
target_link_libraries(trench_native_research PRIVATE trench_native_core)
set_target_properties(trench_native_research PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native/research"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native/research"
)
