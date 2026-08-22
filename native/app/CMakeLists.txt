find_package(Qt6 6.11.2 EXACT REQUIRED COMPONENTS Test Widgets)

qt_standard_project_setup()

add_library(trench_native_ui STATIC
  body_document.cpp
  body_document.hpp
  chassis_bar.cpp
  chassis_bar.hpp
  fit_controller.cpp
  fit_controller.hpp
  fit_room.cpp
  fit_room.hpp
  main_window.cpp
  main_window.hpp
  morph_strip.cpp
  morph_strip.hpp
  response_plot.cpp
  response_plot.hpp
  section_color.hpp
  section_strip.cpp
  section_strip.hpp
  space_dock.cpp
  space_dock.hpp
)
target_link_libraries(trench_native_ui PUBLIC trench_native_core trench_native_audio_boundary Qt6::Widgets)
target_include_directories(trench_native_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(trench_native_ui PRIVATE TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")

qt_add_executable(trench_native main.cpp)
set_target_properties(trench_native PROPERTIES OUTPUT_NAME trench_native)
target_link_libraries(trench_native PRIVATE trench_native_ui)
target_compile_definitions(trench_native PRIVATE TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")

if(BUILD_TESTING)
  qt_add_executable(trench_native_qt_tests tests/main_window_test.cpp)
  target_link_libraries(trench_native_qt_tests PRIVATE trench_native_ui Qt6::Test)
  target_compile_definitions(trench_native_qt_tests PRIVATE TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
  add_test(NAME QtShell.CoreToViewSmoke COMMAND trench_native_qt_tests -platform offscreen)
  set_tests_properties(QtShell.CoreToViewSmoke PROPERTIES
    WORKING_DIRECTORY $<TARGET_FILE_DIR:trench_native_qt_tests>
  )
endif()

message(STATUS "Resolved Qt ${Qt6_VERSION}")
