#include "harness.hpp"

#include "editor_state.hpp"
#include "ladder.hpp"
#include "main_window.hpp"
#include "path_meter.hpp"
#include "row_table.hpp"
#include "word_dial.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QTest>

#include <cmath>
#include <cstdio>
#include <variant>

namespace {

namespace p2k = trench::core::p2k;
using Lane = EditorState::Lane;
using Resonant = trench::core::native::Resonant;

double poleHz(const EditorState& state, std::size_t corner, std::size_t index) {
  return std::get<Resonant>(state.sectionAt(corner, index).pole).hz;
}

double zeroHz(const EditorState& state, std::size_t corner, std::size_t index) {
  return std::get<Resonant>(state.sectionAt(corner, index).zero).hz;
}

bool within(double actual, double expected, double ratio) {
  return std::abs(actual / expected - 1.0) <= ratio;
}

}

TRENCH_TEST(harmonic_helpers_land_root_times_n) {
  EditorState state;
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* root_entry = table.findChild<QLineEdit*>(QStringLiteral("rootEntry"));
  auto* root_note = table.findChild<QLabel*>(QStringLiteral("rootNote"));
  auto* on0 = table.findChild<QCheckBox*>(QStringLiteral("on0"));
  auto* on1 = table.findChild<QCheckBox*>(QStringLiteral("on1"));
  auto* harm0 = table.findChild<QComboBox*>(QStringLiteral("harm0"));
  auto* harm1 = table.findChild<QComboBox*>(QStringLiteral("harm1"));
  auto* freq0 = table.findChild<WordDial*>(QStringLiteral("freqDial0"));
  CHECK(root_entry != nullptr && root_note != nullptr);
  CHECK(on0 != nullptr && on1 != nullptr && harm0 != nullptr && harm1 != nullptr);
  CHECK(freq0 != nullptr);
  CHECK_NEAR(table.rootHz(), 64.0, 1e-9);
  CHECK(root_note->text().contains(QStringLiteral("C2")));
  CHECK(harm0->count() == RowTable::kHarmonicCount + 1);
  CHECK(!harm0->isEnabled());

  on0->click();
  on1->click();
  CHECK(harm0->isEnabled());
  harm0->setCurrentIndex(3);
  CHECK(within(poleHz(state, 0, 0), 192.0, 0.03));
  CHECK(harm0->currentIndex() == 3);
  harm1->setCurrentIndex(2);
  CHECK(within(poleHz(state, 0, 1), 128.0, 0.03));
  CHECK(harm1->currentIndex() == 2);

  root_entry->setText(QStringLiteral("100"));
  emit root_entry->editingFinished();
  CHECK_NEAR(table.rootHz(), 100.0, 1e-9);
  CHECK(within(poleHz(state, 0, 0), 300.0, 0.03));
  CHECK(within(poleHz(state, 0, 1), 200.0, 0.03));
  CHECK(harm0->currentIndex() == 3);
  CHECK(harm1->currentIndex() == 2);
  std::printf("root %.1f Hz  row1 %.1f Hz  row2 %.1f Hz\n", table.rootHz(),
              poleHz(state, 0, 0), poleHz(state, 0, 1));

  auto* freq_readout0 = table.findChild<QLabel*>(QStringLiteral("freqReadout0"));
  CHECK(freq_readout0 != nullptr);
  CHECK(freq_readout0->text().contains(QStringLiteral("D4")));
  std::printf("row1 reads %s\n", freq_readout0->text().toUtf8().constData());

  freq0->setValue(freq0->value() + 20);
  CHECK(harm0->currentIndex() == 0);
  CHECK(harm0->currentText().contains(QStringLiteral("st ")));
  CHECK(harm0->currentText().contains(QStringLiteral("8ve")));
  CHECK(harm1->currentIndex() == 2);
  std::printf("off-grid row reads %s\n", harm0->currentText().toUtf8().constData());

  state.undo();
  CHECK(harm0->currentIndex() == 3);
}

TRENCH_TEST(slot_six_is_always_a_unit_circle_notch) {
  EditorState state;
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  constexpr std::size_t kSix = RowTable::kCeilingSection;
  auto* on5 = table.findChild<QCheckBox*>(QStringLiteral("on5"));
  auto* shape5 = table.findChild<QComboBox*>(QStringLiteral("shape5"));
  auto* gain5 = table.findChild<WordDial*>(QStringLiteral("gainDial5"));
  auto* ceil5 = table.findChild<WordDial*>(QStringLiteral("offsetDial5"));
  auto* ceil_entry = table.findChild<QLineEdit*>(QStringLiteral("offsetEntry5"));
  auto* ceil_readout = table.findChild<QLabel*>(QStringLiteral("offsetReadout5"));
  auto* freq5 = table.findChild<WordDial*>(QStringLiteral("freqDial5"));
  CHECK(on5 != nullptr && shape5 != nullptr && gain5 != nullptr);
  CHECK(ceil5 != nullptr && ceil_entry != nullptr && ceil_readout != nullptr);
  CHECK(freq5 != nullptr);
  CHECK(ceil5->maximum() == static_cast<int>(p2k::kMaxMagByte));

  on5->click();
  CHECK(state.zeroPresentAt(0, kSix));
  CHECK(shape5->currentText() == QStringLiteral("NOTCH"));
  CHECK(!shape5->isEnabled());
  CHECK(!gain5->isEnabled());
  CHECK(ceil5->isEnabled());
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);
  CHECK(within(zeroHz(state, 0, kSix), 20'277.05, 0.01));

  state.removeZeroAt(0, kSix);
  CHECK(state.zeroPresentAt(0, kSix));
  state.setRealRootAt(0, kSix, Lane::kZero, 1.0, 1.0);
  CHECK(std::holds_alternative<Resonant>(state.sectionAt(0, kSix).zero));
  state.setZeroAt(0, kSix, 5'000.0, 500.0);
  CHECK_NEAR(std::get<Resonant>(state.sectionAt(0, kSix).zero).bw_hz, 0.0, 1e-12);
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);

  ceil_entry->setText(QStringLiteral("8000"));
  emit ceil_entry->editingFinished();
  CHECK(within(zeroHz(state, 0, kSix), 8'000.0, 0.03));
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);
  CHECK(ceil_readout->text().endsWith(QStringLiteral("Hz")));
  const double ceiling_before = zeroHz(state, 0, kSix);
  freq5->setValue(freq5->value() + 30);
  CHECK_NEAR(zeroHz(state, 0, kSix), ceiling_before, 1e-9);
  ceil5->setValue(ceil5->value() - 40);
  CHECK(zeroHz(state, 0, kSix) < ceiling_before);
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);
  std::printf("ceiling %.0f Hz -> %.0f Hz, pole %.0f Hz\n", ceiling_before,
              zeroHz(state, 0, kSix), poleHz(state, 0, kSix));
}

TRENCH_TEST(real_pole_word_makes_a_tilt_row) {
  EditorState state;
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* on2 = table.findChild<QCheckBox*>(QStringLiteral("on2"));
  auto* pole2 = table.findChild<QComboBox*>(QStringLiteral("pole2"));
  auto* harm2 = table.findChild<QComboBox*>(QStringLiteral("harm2"));
  auto* freq_readout = table.findChild<QLabel*>(QStringLiteral("freqReadout2"));
  auto* q_readout = table.findChild<QLabel*>(QStringLiteral("qReadout2"));
  auto* freq_entry = table.findChild<QLineEdit*>(QStringLiteral("freqEntry2"));
  CHECK(on2 != nullptr && pole2 != nullptr && harm2 != nullptr);
  CHECK(freq_readout != nullptr && q_readout != nullptr && freq_entry != nullptr);
  CHECK(pole2->count() == 2);
  CHECK(!pole2->isEnabled());

  on2->click();
  CHECK(pole2->isEnabled());
  CHECK(pole2->currentText() == QStringLiteral("RING"));
  pole2->setCurrentIndex(pole2->findText(QStringLiteral("REAL")));
  CHECK(std::holds_alternative<trench::core::native::RealRoots>(state.sectionAt(0, 2).pole));
  const auto& words = state.packed().words[0][2];
  const auto [p, q] = p2k::pq(words[2], words[3]);
  CHECK(std::holds_alternative<trench::core::native::RealRoots>(
      trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz)));
  CHECK(pole2->currentText() == QStringLiteral("REAL"));
  CHECK(freq_readout->text() == QStringLiteral("TILT"));
  CHECK(q_readout->text().startsWith(QStringLiteral("r ")));
  CHECK(!harm2->isEnabled());
  CHECK(!freq_entry->isEnabled());
  std::printf("real pole row: %s %s\n", freq_readout->text().toUtf8().constData(),
              q_readout->text().toUtf8().constData());

  pole2->setCurrentIndex(pole2->findText(QStringLiteral("RING")));
  CHECK(std::holds_alternative<Resonant>(state.sectionAt(0, 2).pole));
  CHECK(pole2->currentText() == QStringLiteral("RING"));
  CHECK(harm2->isEnabled());
  CHECK(freq_readout->text().contains(QStringLiteral("Hz ")));

  state.undo();
  CHECK(pole2->currentText() == QStringLiteral("REAL"));
}

TRENCH_TEST(path_meter_reports_the_interior_peak) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto* meter = window.findChild<PathMeter*>(QStringLiteral("pathMeter"));
  auto* readout = window.findChild<QLabel*>(QStringLiteral("pathReadout"));
  CHECK(meter != nullptr && readout != nullptr);
  CHECK(meter->worstDb() <= 0.5);

  EditorState& state = window.state();
  state.setDocument(EditorState::blank());
  CHECK(meter->worstDb() <= 0.5);
  CHECK(std::abs(meter->worstDb() - meter->hereDb()) < 1e-9);
  voiceLadder(state, 0);
  state.setEditingCorner(0);
  for (std::size_t corner = 1; corner < trench::core::native::kCorners; ++corner) {
    state.copyCornerTo(corner);
  }
  state.setPadPosition(0.0, 0.0);
  const double flat_worst = meter->worstDb();
  CHECK(flat_worst > 5.0);
  CHECK_NEAR(flat_worst, meter->hereDb(), 0.5);

  state.setEditingCorner(1);
  state.applyAffine(12.0, 1.0, 1.0, 1.0);
  state.setPadPosition(0.0, 0.0);
  const double at_from = meter->hereDb();
  state.setPadPosition(1.0, 0.0);
  const double at_to = meter->hereDb();
  CHECK(meter->worstDb() >= std::max(at_from, at_to) - 0.5);
  CHECK(readout->text().startsWith(QStringLiteral("PATH")));
  auto* here = window.findChild<QLabel*>(QStringLiteral("hereReadout"));
  CHECK(here != nullptr && here->text().startsWith(QStringLiteral("HERE")));
  state.setDocument(EditorState::blank());
  CHECK(meter->worstDb() <= 0.5);
  CHECK(std::abs(meter->worstDb() - meter->hereDb()) < 1e-9);
  std::printf("blank after ladder: %s / %s\n", readout->text().toUtf8().constData(),
              here->text().toUtf8().constData());
  std::printf("path worst %+.1f dB at M%.2f Q%.2f (from %+.1f, to %+.1f)\n",
              meter->worstDb(), meter->worstMorph(), meter->worstQ(), at_from, at_to);
}
