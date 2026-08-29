find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Gui Test)

add_executable(trench_native_tests
  harness.hpp
  test_main.cpp
  test_audition.cpp
  test_smoke.cpp
  test_cascade.cpp
  test_document.cpp
  test_zplane.cpp
)
target_link_libraries(trench_native_tests PRIVATE trench_native_ui Qt6::Test)

set(TRENCH_NATIVE_TEST_CASES
  absent_zero_exports_flat
  audition_button_reports_device_state
  audition_device_opens_streams_and_closes
  cascade_parity_graph_ears_export
  document_load_restores_editable_state_not_overlay
  document_rejects_malformed
  document_roundtrip_is_lossless_and_deterministic
  import_classification_by_extension
  legacy_export_size_and_legality
  off_section_exports_identity
  pole_material_rejects_negative_rows
  positive_response_table_is_not_pole_material
  praat_formant_table_opens_as_median_poles
  s6_zero_export_law_is_the_existing_one
  window_smoke_at_minimum_and_normal_size
  zplane_circle_and_labels_fit_compact_pane
  zplane_drag_is_bounded_and_mirrors
  zplane_frequency_bandwidth_roundtrip
  zplane_rejects_outside_disc_and_lower_half
)

set(TRENCH_NATIVE_TEST_NAMES)
foreach(case IN LISTS TRENCH_NATIVE_TEST_CASES)
  add_test(NAME native.${case} COMMAND trench_native_tests ${case})
  list(APPEND TRENCH_NATIVE_TEST_NAMES native.${case})
endforeach()
add_test(NAME native.ctest_registry
  COMMAND trench_native_tests --registry ${TRENCH_NATIVE_TEST_CASES})
list(APPEND TRENCH_NATIVE_TEST_NAMES native.ctest_registry)
set_tests_properties(${TRENCH_NATIVE_TEST_NAMES}
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;TRENCH_SMOKE_DIR=${CMAKE_BINARY_DIR}/smoke" TIMEOUT 120)
set_tests_properties(native.audition_button_reports_device_state
  native.audition_device_opens_streams_and_closes
  PROPERTIES SKIP_RETURN_CODE 77)

if(WIN32 AND TARGET Qt6::QOffscreenIntegrationPlugin)
  add_custom_command(TARGET trench_native_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:trench_native_tests>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
      "$<TARGET_FILE_DIR:trench_native_tests>/platforms/qoffscreen.dll"
  )
endif()
