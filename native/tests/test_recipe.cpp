#include "harness.hpp"

#include "editor_state.hpp"
#include "ladder.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/native_body.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <variant>

namespace {

using Lane = EditorState::Lane;
using Resonant = trench::core::native::Resonant;

const Resonant& pole(const EditorState& state, std::size_t corner, std::size_t section) {
  return std::get<Resonant>(state.sectionAt(corner, section).pole);
}

double radius(double bw_hz) {
  return std::exp(-std::numbers::pi * bw_hz / EditorState::kDatumHz);
}

constexpr double kSixDb = 20.0 * 0.30102999566398120;

double dcGainDb(const EditorState& state, std::size_t corner) {
  trench::core::p2k::PackedCorner flat{};
  const auto& words = state.packed().words[corner];
  for (std::size_t stage = 0; stage < trench::core::p2k::kStageCount; ++stage) {
    for (std::size_t word = 0; word < trench::core::p2k::kWordCount; ++word) {
      flat[stage * trench::core::p2k::kWordCount + word] = words[stage][word];
    }
  }
  return trench::core::p2k::dc_gain_db(flat);
}

void seedCornerZero(EditorState& state) {
  state.setEditingCorner(0);
  state.toggleSection(1);
  state.setRoot(1, Lane::kPole, 960.0, 300.0);
  state.toggleSection(2);
  state.setRoot(2, Lane::kPole, 2'540.0, 340.0);
  state.selectSection(2);
  state.addZeroAt(4'700.0, 900.0);
}

}  // namespace

TRENCH_TEST(copy_across_duplicates_the_partner_corner_and_undoes) {
  EditorState state;
  seedCornerZero(state);
  const auto before = state.document();
  state.setEditingCorner(1);
  CHECK(!state.sectionEnabledAt(1, 1));
  state.copyCornerFrom(0);
  CHECK(state.document().corners[1] == before.corners[0]);
  CHECK(state.sectionEnabledAt(1, 2));
  CHECK(state.zeroPresentAt(1, 2));
  state.undo();
  CHECK(!state.sectionEnabledAt(1, 1));
  CHECK(state.document().corners[0] == before.corners[0]);
  state.copyCornerFrom(1);
  CHECK(state.document().corners[1] == before.corners[1]);
}

TRENCH_TEST(copy_across_pushes_this_corner_to_its_morph_partner) {
  EditorState state;
  seedCornerZero(state);
  const auto built = state.document().corners[0];
  CHECK(!state.sectionEnabledAt(1, 1));
  state.copyCornerTo(1);
  CHECK(state.document().corners[1] == built);
  CHECK(state.document().corners[0] == built);
  CHECK(state.editingCorner() == 0);
  state.undo();
  CHECK(!state.sectionEnabledAt(1, 1));
  CHECK(state.document().corners[0] == built);
}

TRENCH_TEST(derive_row_copies_and_raises_every_pole_radius_by_the_step) {
  EditorState state;
  seedCornerZero(state);
  state.setEditingCorner(2);
  state.beginUndoGroup();
  state.copyCornerFrom(0);
  state.sharpenPoles(0.01);
  state.endUndoGroup();
  for (std::size_t section : {1u, 2u}) {
    CHECK_NEAR(pole(state, 2, section).hz, pole(state, 0, section).hz, 1e-9);
    CHECK_NEAR(radius(pole(state, 2, section).bw_hz),
               radius(pole(state, 0, section).bw_hz) + 0.01, 1e-9);
  }
  CHECK(state.sectionAt(2, 2).zero == state.sectionAt(0, 2).zero);
  state.undo();
  CHECK(!state.sectionEnabledAt(2, 1));
  CHECK(!state.canUndo() || state.document().corners[0] == state.document().corners[0]);
}

TRENCH_TEST(cut_lowers_one_section_by_six_db_steps) {
  EditorState state;
  seedCornerZero(state);
  const auto flat = state.packed().words;
  const double unity = dcGainDb(state, 0);
  std::printf("dc %.4f dB, gain words %04X %04X\n", unity, flat[0][1][4], flat[0][2][4]);
  CHECK_NEAR(unity, 0.0, 0.05);
  CHECK(flat[0][1][4] == flat[0][2][4]);
  state.setCutAt(0, 2, 1);
  CHECK_NEAR(dcGainDb(state, 0) - unity, -kSixDb, 0.05);
  state.setCutAt(0, 1, 3);
  CHECK_NEAR(dcGainDb(state, 0) - unity, -4.0 * kSixDb, 0.05);
  std::printf("dc after cuts %.4f dB\n", dcGainDb(state, 0));
  CHECK(state.packed().words[4] == state.packed().words[0]);
  state.undo();
  state.undo();
  CHECK(state.packed().words == flat);
}

TRENCH_TEST(solo_view_plays_the_selected_stage_alone) {
  EditorState state;
  seedCornerZero(state);
  const auto& full = state.packed();
  const auto solo = state.soloView(2);
  const auto pole_only = state.soloView(2, true);
  for (std::size_t corner = 0; corner < trench::core::kCornerCount; ++corner) {
    for (std::size_t section = 0; section < trench::core::kSectionCount; ++section) {
      if (section == 2) {
        CHECK(solo.packed.words[corner][section] == full.words[corner][section]);
      } else {
        CHECK(solo.packed.words[corner][section] == trench::core::kIdentitySection);
        CHECK(pole_only.packed.words[corner][section] == trench::core::kIdentitySection);
      }
    }
    const auto& words = pole_only.packed.words[corner][2];
    CHECK(words[0] == trench::core::kIdentitySection[0]);
    CHECK(words[1] == trench::core::kIdentitySection[1]);
    CHECK(words[2] == full.words[corner][2][2]);
    CHECK(words[3] == full.words[corner][2][3]);
    CHECK(words[4] == full.words[corner][2][4]);
  }
  const auto db = [](const trench::audio::AuditionView& view, double hz) {
    const auto cascade = trench::audio::design_audition(view, EditorState::kDatumHz);
    return trench::core::cascade_response_db(cascade, hz, EditorState::kDatumHz);
  };
  CHECK(db(pole_only, 4'700.0) - db(solo, 4'700.0) > 3.0);
  CHECK_NEAR(db(state.soloView(4), 200.0) - db(state.soloView(4), 8'000.0), 0.0, 1e-9);
}

TRENCH_TEST(ear_path_levels_by_average_power) {
  EditorState state;
  voiceLadder(state, 0);
  const auto before = state.packed().words;
  const auto mean_db = [](trench::audio::AuditionView view, double trim) {
    view.trim_db = trim;
    const auto cascade = trench::audio::design_audition(view, EditorState::kDatumHz);
    double power = 0.0;
    std::size_t count = 0;
    for (double hz = 100.0; hz <= 8'000.0; hz *= 1.02) {
      power += std::pow(10.0, trench::core::cascade_response_db(cascade, hz, EditorState::kDatumHz) / 10.0);
      ++count;
    }
    return 10.0 * std::log10(power / static_cast<double>(count));
  };
  const double trim = trench::audio::level_trim_db(state.view(), EditorState::kDatumHz);
  CHECK_NEAR(mean_db(state.view(), trim), 0.0, 0.05);
  EditorState skeleton;
  voiceLadder(skeleton, 0, 64.0, false);
  const double bare_trim = trench::audio::level_trim_db(skeleton.view(), EditorState::kDatumHz);
  CHECK_NEAR(mean_db(skeleton.view(), bare_trim), 0.0, 0.05);
  CHECK(state.packed().words == before);
  const auto solo = state.soloView(2);
  CHECK_NEAR(mean_db(solo, trim) - mean_db(state.view(), trim),
             mean_db(solo, 0.0) - mean_db(state.view(), 0.0), 1e-9);
}

TRENCH_TEST(packed_body_opens_as_an_editable_four_corner_clone) {
  EditorState authored;
  voiceLadder(authored, 0, 64.0);
  voiceLadder(authored, 1, 96.0);
  authored.setCutAt(1, 2, 1);
  voiceLadder(authored, 3, 128.0);
  const auto words = authored.packed().words;
  CHECK(authored.packed().is_legacy_representable());
  const auto bytes = authored.packed().legacy_bytes();

  EditorState clone;
  clone.setDocument(EditorState::documentFrom(trench::core::native::import_p2k(bytes)));
  CHECK(clone.packed().words == words);
  CHECK(clone.sectionEnabledAt(1, 2));
  CHECK(clone.zeroPresentAt(1, 2));
  CHECK(!clone.sectionEnabledAt(2, 0));
  CHECK(clone.cutAt(1, 2) == 1);
  CHECK(clone.cutAt(1, 1) == 0);

  trench::core::native::Body body = authored.body();
  body.corners[0].sections[3].pole = trench::core::native::RealRoots{120.0, 400.0};
  body.corners[0].sections[3].zero = trench::core::native::RealRoots{
      std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  EditorState seated;
  seated.setDocument(EditorState::documentFrom(body));
  CHECK(seated.sectionEnabledAt(0, 3));
  CHECK(!seated.zeroPresentAt(0, 3));
  const auto* kept = std::get_if<trench::core::native::RealRoots>(
      &seated.sectionAt(0, 3).pole);
  CHECK(kept != nullptr);
  CHECK_NEAR(kept->a_hz, 120.0, 1e-9);
  CHECK_NEAR(kept->b_hz, 400.0, 1e-9);
}

TRENCH_TEST(switching_s6_on_brings_the_cage) {
  EditorState state;
  state.toggleSection(5);
  CHECK(state.sectionEnabled(5));
  CHECK(state.rootPresent(5, Lane::kZero));
  const auto& cage = std::get<Resonant>(state.section(5).zero);
  CHECK_NEAR(cage.hz, 20'277.05, 1e-6);
  CHECK(cage.bw_hz < 1.0);
  state.toggleSection(2);
  CHECK(!state.rootPresent(2, Lane::kZero));
}
