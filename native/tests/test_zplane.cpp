#include "harness.hpp"

#include "armadillo_editor.hpp"
#include "editor_state.hpp"
#include "trench/core/native_body.hpp"

#include <QPointF>
#include <QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <variant>

namespace {

using Lane = EditorState::Lane;
using Resonant = trench::core::native::Resonant;

void openPane(ArmadilloEditor& editor, int width) {
  editor.resize(width, 230);
  editor.setProjection(ArmadilloEditor::Projection::kZPlane);
}

const Resonant& zeroOf(const EditorState& state, std::size_t section) {
  return std::get<Resonant>(state.section(section).zero);
}

}  // namespace

TRENCH_TEST(zplane_frequency_bandwidth_roundtrip) {
  EditorState state;
  ArmadilloEditor editor(&state);
  openPane(editor, 1148);
  constexpr std::array<double, 8> frequencies{
      20.0, 30.0, 100.0, 440.0, 1'000.0, 5'000.0, 15'000.0, 22'050.0};
  constexpr std::array<double, 7> bandwidths{
      1.0, 2.0, 20.0, 200.0, 2'000.0, 15'000.0, 20'000.0};
  double worst_hz = 0.0;
  double worst_bw = 0.0;
  for (const double hz : frequencies) {
    for (const double bw : bandwidths) {
      const auto placement = editor.placementAt(editor.pointFor(hz, bw));
      CHECK(placement.has_value());
      CHECK_NEAR(placement->first, hz, 1e-6 * hz);
      CHECK_NEAR(placement->second, bw, 1e-6 * bw);
      worst_hz = std::max(worst_hz, std::abs(placement->first - hz) / hz);
      worst_bw = std::max(worst_bw, std::abs(placement->second - bw) / bw);
    }
  }
  std::printf("worst relative round trip: frequency %.3e  bandwidth %.3e\n",
              worst_hz, worst_bw);
}

TRENCH_TEST(zplane_rejects_outside_disc_and_lower_half) {
  EditorState state;
  ArmadilloEditor editor(&state);
  openPane(editor, 1148);
  editor.show();
  QTest::qWait(30);
  editor.resize(1148, 230);
  QTest::qWait(30);
  const QPointF centre = editor.discCentre();
  const double radius = editor.discRadius();

  CHECK(!editor.placementAt(centre + QPointF(0.0, 0.5 * radius)));
  CHECK(!editor.placementAt(centre + QPointF(0.5 * radius, 0.001 * radius)));
  CHECK(!editor.placementAt(centre + QPointF(1.3 * radius, 0.0)));
  CHECK(editor.placementAt(centre + QPointF(0.5 * radius, -0.5 * radius)));
  CHECK(!editor.placementAt(centre + QPointF(0.99 * radius, -0.0001 * radius)));
  CHECK(!editor.placementAt(centre + QPointF(0.5 * radius, 0.0)));

  state.selectSection(1);
  state.removeZero();
  CHECK(!state.rootPresent(1, Lane::kZero));
  QTest::mouseDClick(&editor, Qt::LeftButton, {},
                     (centre + QPointF(0.0, 0.5 * radius)).toPoint());
  CHECK(!state.rootPresent(1, Lane::kZero));
  QTest::mouseDClick(&editor, Qt::LeftButton, {},
                     (centre + QPointF(1.3 * radius, 0.0)).toPoint());
  CHECK(!state.rootPresent(1, Lane::kZero));

  const QPointF spot =
      QPointF((centre + QPointF(0.2 * radius, -0.6 * radius)).toPoint());
  const auto expected = editor.placementAt(spot);
  CHECK(expected.has_value());
  QTest::mouseDClick(&editor, Qt::LeftButton, {}, spot.toPoint());
  CHECK(state.rootPresent(1, Lane::kZero));
  const Resonant& zero = zeroOf(state, 1);
  CHECK_NEAR(zero.hz, expected->first, 1e-9 * expected->first);
  CHECK_NEAR(zero.bw_hz, expected->second, 1e-9 * expected->second);
  editor.close();
}

TRENCH_TEST(zplane_drag_is_bounded_and_mirrors) {
  EditorState state;
  ArmadilloEditor editor(&state);
  openPane(editor, 1148);
  const QPointF centre = editor.discCentre();
  const double radius = editor.discRadius();

  const auto upper =
      editor.dragTargetAt(centre + QPointF(0.5 * radius, -0.5 * radius));
  const auto lower =
      editor.dragTargetAt(centre + QPointF(0.5 * radius, 0.5 * radius));
  CHECK(lower.first == upper.first);
  CHECK(lower.second == upper.second);

  const auto far =
      editor.dragTargetAt(centre + QPointF(3.0 * radius, -3.0 * radius));
  CHECK_NEAR(far.second, EditorState::kMinBandwidthHz, 1e-9);
  CHECK_NEAR(far.first, upper.first, 1e-9 * upper.first);

  const auto origin =
      editor.dragTargetAt(centre + QPointF(0.001 * radius, -0.0001 * radius));
  CHECK_NEAR(origin.second, EditorState::kMaxBandwidthHz, 1e-6);

  const auto right = editor.dragTargetAt(centre + QPointF(0.9 * radius, 0.0));
  CHECK_NEAR(right.first, EditorState::kLowHz, 1e-9);
  const auto left = editor.dragTargetAt(centre + QPointF(-0.9 * radius, 0.0));
  CHECK_NEAR(left.first, EditorState::kNyquistHz, 1e-9);
}

TRENCH_TEST(zplane_circle_and_labels_fit_compact_pane) {
  EditorState state;
  ArmadilloEditor editor(&state);
  openPane(editor, 1148);
  for (const int width : {1148, 868}) {
    editor.resize(width, 230);
    const QPointF centre = editor.discCentre();
    const double radius = editor.discRadius();
    CHECK(centre.y() - radius >= 20.0);
    CHECK(centre.y() + radius <= editor.height() - 2.0);
    CHECK(centre.x() + radius + 30.0 <= editor.width());
    CHECK(radius >= 80.0);
  }
}
