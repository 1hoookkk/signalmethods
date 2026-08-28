find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_library(trench_native_ui STATIC
  armadillo_editor.cpp
  armadillo_editor.hpp
  cascade_plot.cpp
  cascade_plot.hpp
  editor_state.cpp
  editor_state.hpp
  main_window.cpp
  main_window.hpp
)
target_link_libraries(trench_native_ui PUBLIC trench_native_core trench_native_audio_boundary Qt6::Widgets)
target_include_directories(trench_native_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(trench_native_ui PRIVATE TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")

qt_add_executable(trench_native main.cpp)
set_target_properties(trench_native PROPERTIES OUTPUT_NAME trench_native)
target_link_libraries(trench_native PRIVATE trench_native_ui)
target_compile_definitions(trench_native PRIVATE TRENCH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")

if(WIN32 AND TARGET Qt6::QWindowsIntegrationPlugin)
  add_custom_command(TARGET trench_native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:trench_native>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
      "$<TARGET_FILE_DIR:trench_native>/platforms/qwindows.dll"
  )
endif()

message(STATUS "Resolved Qt ${Qt6_VERSION}")
