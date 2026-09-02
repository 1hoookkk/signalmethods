find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Gui Test)

add_executable(trench_native_tests
  harness.hpp
  test_main.cpp
  test_recipe.cpp
  test_gestures.cpp
  test_recipe_smoke.cpp
  test_rows.cpp
  test_helpers.cpp
)
target_link_libraries(trench_native_tests PRIVATE trench_native_ui Qt6::Test)

set(TRENCH_NATIVE_TEST_CASES
  copy_across_duplicates_the_partner_corner_and_undoes
  copy_across_pushes_this_corner_to_its_morph_partner
  derive_row_copies_and_raises_every_pole_radius_by_the_step
  cut_lowers_one_section_by_six_db_steps
  recipe_chrome_is_on_the_window
  dropped_file_opens_like_the_open_button
  solo_view_plays_the_selected_stage_alone
  ear_path_levels_by_average_power
  switching_s6_on_brings_the_cage
  packed_body_opens_as_an_editable_four_corner_clone
  pole_dial_words_survive_the_export
  zero_dial_words_survive_the_export_until_the_cage
  row_table_edits_reach_the_state
  corner_picker_moves_the_editing_corner
  row_table_reads_back_the_packed_words
  typed_frequency_lands_on_the_nearest_word
  offset_holds_the_zero_to_the_pole
  posture_throw_writes_the_morph_partner_transposed
  sharpen_throw_writes_the_q_partner_with_raised_radii
  cuts_and_real_roots_survive_save_and_open
  cut_survives_the_body240_round_trip
  skin_is_system_grey_and_aliased
  harmonic_helpers_land_root_times_n
  slot_six_is_always_a_unit_circle_notch
  path_meter_reports_the_interior_peak
  real_pole_word_makes_a_tilt_row
  console_owns_the_lower_pane
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
  PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts;TRENCH_SMOKE_DIR=${CMAKE_BINARY_DIR}/smoke" TIMEOUT 120)

if(WIN32 AND TARGET Qt6::QOffscreenIntegrationPlugin)
  add_custom_command(TARGET trench_native_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:trench_native_tests>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
      "$<TARGET_FILE_DIR:trench_native_tests>/platforms/qoffscreen.dll"
  )
endif()
