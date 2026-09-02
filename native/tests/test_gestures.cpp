#include "harness.hpp"

#include "editor_state.hpp"
#include "ladder.hpp"
#include "main_window.hpp"

#include <QPoint>
#include <QTest>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <variant>

namespace {

using Resonant = trench::core::native::Resonant;

const Resonant& pole(const EditorState& state, std::size_t corner, std::size_t section) {
  return std::get<Resonant>(state.sectionAt(corner, section).pole);
}

double radius(double bw_hz) {
  return std::exp(-std::numbers::pi * bw_hz / EditorState::kDatumHz);
}

void throwDial(QWidget* dial, int pixels) {
  const QPoint start(dial->width() / 2, dial->height() - 16);
  QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(dial, start + QPoint(pixels, 0));
  QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier, start + QPoint(pixels, 0));
}

}  // namespace

TRENCH_TEST(posture_throw_writes_the_morph_partner_transposed) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  voiceLadder(state, 0);
  CHECK(!state.sectionEnabledAt(1, 1));
  const auto before = state.document();
  auto* posture = window.findChild<QWidget*>(QStringLiteral("posture"));
  CHECK(posture != nullptr);
  throwDial(posture, 100);
  CHECK(state.editingCorner() == 0);
  CHECK(state.document().corners[0] == before.corners[0]);
  CHECK(state.sectionEnabledAt(1, 1));
  const double shift = std::exp2(3.0 / 12.0);
  for (std::size_t section = 0; section < 6; ++section) {
    CHECK_NEAR(pole(state, 1, section).hz, pole(state, 0, section).hz * shift, 1e-6);
    CHECK_NEAR(pole(state, 1, section).bw_hz, pole(state, 0, section).bw_hz, 1e-9);
  }
  CHECK(state.zeroPresentAt(1, 2));
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(sharpen_throw_writes_the_q_partner_with_raised_radii) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  voiceLadder(state, 0);
  CHECK(!state.sectionEnabledAt(2, 1));
  const auto before = state.document();
  auto* sharpen = window.findChild<QWidget*>(QStringLiteral("sharpen"));
  CHECK(sharpen != nullptr);
  throwDial(sharpen, 50);
  CHECK(state.editingCorner() == 0);
  CHECK(state.document().corners[0] == before.corners[0]);
  for (std::size_t section = 0; section < 6; ++section) {
    CHECK_NEAR(pole(state, 2, section).hz, pole(state, 0, section).hz, 1e-9);
    const double ceiling = radius(EditorState::kMinBandwidthHz);
    CHECK_NEAR(radius(pole(state, 2, section).bw_hz),
               std::min(radius(pole(state, 0, section).bw_hz) + 0.01, ceiling), 1e-9);
  }
  CHECK(state.sectionAt(2, 2).zero == state.sectionAt(0, 2).zero);
  state.undo();
  CHECK(state.document() == before);
  window.close();
}
