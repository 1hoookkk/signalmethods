find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_library(trench_native_ui STATIC
  body_io.cpp
  body_io.hpp
  cascade_plot.cpp
  cascade_plot.hpp
  editor_state.cpp
  editor_state.hpp
  gesture_dial.cpp
  gesture_dial.hpp
  main_window.cpp
  main_window.hpp
  morph_pad.cpp
  morph_pad.hpp
  path_meter.cpp
  path_meter.hpp
  pole_templates.cpp
  pole_templates.hpp
  row_table.cpp
  row_table.hpp
  skin.cpp
  skin.hpp
  word_dial.cpp
  word_dial.hpp
)
target_link_libraries(trench_native_ui PUBLIC trench_native_core trench_native_audio_boundary Qt6::Widgets)
target_include_directories(trench_native_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(trench_native_ui PUBLIC TRENCH_POLE_TEMPLATES="${CMAKE_CURRENT_SOURCE_DIR}/templates/pole_templates_by_type.json")
target_compile_definitions(trench_native_ui PUBLIC TRENCH_FRAMES_X3="${CMAKE_CURRENT_SOURCE_DIR}/templates/frames_x3.json" TRENCH_AUDITION_SLOT="${CMAKE_CURRENT_SOURCE_DIR}/../../plugin/patterns/audition_slot.body240")

qt_add_executable(trench_native main.cpp)
set_target_properties(trench_native PROPERTIES OUTPUT_NAME trench_native)
target_link_libraries(trench_native PRIVATE trench_native_ui)

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
