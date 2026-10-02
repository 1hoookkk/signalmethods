set(HEADSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(HEADSPACE_IMGUI "${HEADSPACE_ROOT}/vendor/imgui")

add_library(TRENCH_Headspace_Shared STATIC
  Source/slice/Model.cpp
  Source/slice/Sources.cpp
  Source/slice/Span.cpp
  Source/slice/Audio.cpp)
target_include_directories(TRENCH_Headspace_Shared PUBLIC Source/slice)
target_link_libraries(TRENCH_Headspace_Shared PUBLIC trench_native_core)

add_library(TRENCH_Headspace_ImGui STATIC
  "${HEADSPACE_IMGUI}/imgui.cpp"
  "${HEADSPACE_IMGUI}/imgui_draw.cpp"
  "${HEADSPACE_IMGUI}/imgui_tables.cpp"
  "${HEADSPACE_IMGUI}/imgui_widgets.cpp"
  "${HEADSPACE_IMGUI}/backends/imgui_impl_win32.cpp"
  "${HEADSPACE_IMGUI}/backends/imgui_impl_dx11.cpp")
target_include_directories(TRENCH_Headspace_ImGui PUBLIC
  "${HEADSPACE_IMGUI}" "${HEADSPACE_IMGUI}/backends")
target_link_libraries(TRENCH_Headspace_ImGui PUBLIC d3d11 dxgi d3dcompiler dwmapi)

add_executable(TRENCH_Headspace_App WIN32 Source/app/Main.cpp)
set_target_properties(TRENCH_Headspace_App PROPERTIES OUTPUT_NAME HEADSPACE)
target_include_directories(TRENCH_Headspace_App PRIVATE "${HEADSPACE_ROOT}/tools")
target_compile_definitions(TRENCH_Headspace_App PRIVATE
  NOMINMAX UNICODE _UNICODE HEADSPACE_ROOT="${HEADSPACE_ROOT}")
target_link_libraries(TRENCH_Headspace_App PRIVATE
  TRENCH_Headspace_Shared TRENCH_Headspace_ImGui comdlg32 ole32 shell32 winmm)

add_executable(TRENCH_QuadTests Tests/SliceTests.cpp)
target_link_libraries(TRENCH_QuadTests PRIVATE TRENCH_Headspace_Shared)
target_compile_definitions(TRENCH_QuadTests PRIVATE HEADSPACE_ROOT="${HEADSPACE_ROOT}")
add_test(NAME trench_headspace_slice COMMAND TRENCH_QuadTests)
set_tests_properties(trench_headspace_slice PROPERTIES TIMEOUT 180)
