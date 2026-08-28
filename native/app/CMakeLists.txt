find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_library(trench_native_ui STATIC
  armadillo_view.cpp
  armadillo_view.hpp
  bisect_room.cpp
  bisect_room.hpp
  body_document.cpp
  body_document.hpp
  chassis_bar.cpp
  chassis_bar.hpp
  fit_controller.cpp
  fit_controller.hpp
  fit_room.cpp
  fit_room.hpp
  lane_strip.cpp
  lane_strip.hpp
  main_window.cpp
  main_window.hpp
  morph_strip.cpp
  morph_strip.hpp
  posture_list.cpp
  posture_list.hpp
  response_plot.cpp
  response_plot.hpp
  section_color.hpp
  section_readout.cpp
  section_readout.hpp
  user_postures.cpp
  user_postures.hpp
)
target_link_libraries(trench_native_ui PUBLIC trench_native_core trench_native_audio_boundary Qt6::Widgets)
target_include_directories(trench_native_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(trench_native_ui PRIVATE TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")

qt_add_executable(trench_native main.cpp)
set_target_properties(trench_native PROPERTIES OUTPUT_NAME trench_native)
target_link_libraries(trench_native PRIVATE trench_native_ui)
target_compile_definitions(trench_native PRIVATE TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")

message(STATUS "Resolved Qt ${Qt6_VERSION}")
