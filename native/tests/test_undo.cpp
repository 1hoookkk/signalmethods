#include "harness.hpp"

#include "editor_state.hpp"

#include <cmath>
#include <variant>
#include <vector>

namespace {

using Lane = EditorState::Lane;
using Resonant = trench::core::native::Resonant;

const Resonant& pole(const EditorState& state, std::size_t section) {
  return std::get<Resonant>(state.section(section).pole);
}

}  // namespace

TRENCH_TEST(boot_is_empty_and_toggling_places_a_fresh_pole) {
  EditorState state;
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      CHECK(!state.sectionEnabledAt(corner, index));
      CHECK(!state.zeroPresentAt(corner, index));
    }
  }
  const auto cascade = state.cascade();
  for (const auto& section : cascade) {
    CHECK(section[1] == 0.0 && section[2] == 0.0 && section[3] == 0.0 && section[4] == 0.0);
  }
  CHECK(!state.canUndo());
  state.toggleSection(2);
  CHECK(state.sectionEnabled(2));
  CHECK(pole(state, 2).hz == 1'000.0);
  CHECK(pole(state, 2).bw_hz == 100.0);
  CHECK(!state.rootPresent(2, Lane::kZero));
  CHECK(state.canUndo());
  state.undo();
  CHECK(!state.sectionEnabled(2));
  CHECK(!state.canUndo());
  CHECK(state.canRedo());
  state.redo();
  CHECK(state.sectionEnabled(2));
}

TRENCH_TEST(undo_restores_each_edit_and_groups_a_drag) {
  EditorState state;
  state.toggleSection(0);
  state.setRoot(0, Lane::kPole, 300.0, 40.0);
  const auto before_drag = state.document();
  state.beginUndoGroup();
  for (int step = 1; step <= 5; ++step) {
    state.setRoot(0, Lane::kPole, 300.0 + 20.0 * step, 40.0 + step);
  }
  state.endUndoGroup();
  CHECK(pole(state, 0).hz == 400.0);
  state.undo();
  CHECK(state.document() == before_drag);
  CHECK(pole(state, 0).hz == 300.0);
  state.undo();
  CHECK(pole(state, 0).hz == 1'000.0);
  state.undo();
  CHECK(!state.sectionEnabled(0));
  CHECK(!state.canUndo());
  state.redo();
  state.redo();
  state.redo();
  CHECK(pole(state, 0).hz == 400.0);
  CHECK(!state.canRedo());

  state.setRoot(0, Lane::kPole, 500.0, 50.0);
  CHECK(!state.canRedo());
  state.loadPoles({{120.0, 20.0}, {900.0, 80.0}});
  CHECK(state.sectionEnabled(1) && !state.sectionEnabled(2));
  state.undo();
  CHECK(pole(state, 0).hz == 500.0);
  CHECK(!state.sectionEnabled(1));
}

TRENCH_TEST(reset_is_a_blank_document_and_undoable) {
  EditorState state;
  state.loadPoles({{120.0, 20.0}, {900.0, 80.0}, {2'500.0, 200.0}});
  state.selectSection(1);
  state.addZeroAt(1'100.0, 90.0);
  const auto authored = state.document();
  state.setDocument(EditorState::blank());
  CHECK(state.document() == EditorState::blank());
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    CHECK(!state.sectionEnabled(index));
  }
  state.undo();
  CHECK(state.document() == authored);
  CHECK(state.rootPresent(1, Lane::kZero));
}

TRENCH_TEST(zero_habits_add_a_skirt_and_level_trims) {
  EditorState state;
  state.loadPoles({{250.0, 90.0}, {1'000.0, 50.0}, {2'400.0, 400.0}, {4'000.0, 100.0}});
  const auto habits = state.applyZeroHabits();
  CHECK(habits.skirts == 1);
  CHECK(habits.trims == 2);
  CHECK(state.rootPresent(0, Lane::kZero));
  const auto& skirt = std::get<Resonant>(state.section(0).zero);
  CHECK_NEAR(skirt.hz, 250.0 / std::exp2(0.46), 1e-9);
  CHECK(skirt.bw_hz == 90.0);
  CHECK(state.rootPresent(1, Lane::kZero));
  const auto& trim = std::get<Resonant>(state.section(1).zero);
  CHECK(trim.hz == 1'000.0);
  CHECK_NEAR(trim.bw_hz, 50.0 * 20.0 / 2.0, 1e-9);
  CHECK(!state.rootPresent(2, Lane::kZero));
  CHECK(state.rootPresent(3, Lane::kZero));
  CHECK_NEAR(std::get<Resonant>(state.section(3).zero).bw_hz, 100.0 * 40.0 / 2.0, 1e-9);
  const auto again = state.applyZeroHabits();
  CHECK(again.skirts == 0 && again.trims == 0);
  state.undo();
  for (std::size_t index = 0; index < 4; ++index) CHECK(!state.rootPresent(index, Lane::kZero));

  EditorState trench;
  trench.loadPoles({{1'000.0, 200.0}, {1'500.0, 200.0}, {2'000.0, 200.0},
                    {3'000.0, 200.0}, {4'000.0, 200.0}, {220.0, 20.0}});
  const auto sixth = trench.applyZeroHabits();
  CHECK(sixth.skirts == 0 && sixth.trims == 0);
  CHECK(!trench.rootPresent(5, Lane::kZero));
}
