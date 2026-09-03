#include "harness.hpp"

#include "editor_state.hpp"
#include "ladder.hpp"
#include "main_window.hpp"

#include <QComboBox>
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

TRENCH_TEST(template_pick_seeds_the_editing_corner_poles) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  auto* box = window.findChild<QComboBox*>(QStringLiteral("template"));
  CHECK(box != nullptr);
  CHECK(box->count() > 1);
  const auto before = state.document();
  int vow = -1;
  for (int i = 1; i < box->count(); ++i)
    if (box->itemText(i).contains(QStringLiteral("VOW"))) vow = i;
  CHECK(vow > 0);
  box->setCurrentIndex(vow);
  emit box->activated(vow);
  CHECK(state.document() != before);
  CHECK(state.editingCorner() == 0);
  double last = 0.0;
  int enabled = 0;
  for (std::size_t section = 0; section < 6; ++section) {
    if (!state.sectionEnabledAt(0, section)) continue;
    ++enabled;
    const auto& p = pole(state, 0, section);
    CHECK(p.hz > last);
    CHECK(p.hz >= 20.0 && p.hz <= 22'050.0);
    last = p.hz;
  }
  CHECK(enabled >= 4);
  CHECK(box->currentIndex() == 0);
  state.undo();
  CHECK(state.document() == before);
  window.close();
}
