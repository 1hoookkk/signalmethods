#include "harness.hpp"

#include "editor_state.hpp"
#include "ladder.hpp"
#include "main_window.hpp"
#include "number_box.hpp"
#include "path_meter.hpp"
#include "rows_table.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QTest>
#include <QToolButton>

#include <cmath>
#include <cstdio>
#include <variant>

namespace {

namespace p2k = trench::core::p2k;
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

void pickType(QComboBox* box, const QString& name) {
  box->setCurrentIndex(box->findText(name));
}

void unlockRow(RowsTable& table, std::size_t index) {
  auto* unlock = table.findChild<QToolButton*>(QStringLiteral("unlock%1").arg(index));
  if (unlock != nullptr && !unlock->isChecked()) unlock->click();
}

}

TRENCH_TEST(harmonic_helpers_land_root_times_n) {
  EditorState state;
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  unlockRow(table, 0);
  unlockRow(table, 1);
  auto* root_entry = table.findChild<QLineEdit*>(QStringLiteral("rootEntry"));
  auto* root_note = table.findChild<QLabel*>(QStringLiteral("rootNote"));
  auto* type0 = table.findChild<QComboBox*>(QStringLiteral("type0"));
  auto* type1 = table.findChild<QComboBox*>(QStringLiteral("type1"));
  auto* harm0 = table.findChild<QComboBox*>(QStringLiteral("harmLo0"));
  auto* harm1 = table.findChild<QComboBox*>(QStringLiteral("harmLo1"));
  auto* note0 = table.findChild<NumberBox*>(QStringLiteral("noteLo0"));
  CHECK(root_entry != nullptr && root_note != nullptr);
  CHECK(type0 != nullptr && type1 != nullptr && harm0 != nullptr && harm1 != nullptr);
  CHECK(note0 != nullptr);
  CHECK_NEAR(table.rootHz(), 64.0, 1e-9);
  CHECK(root_note->text().contains(QStringLiteral("C2")));
  CHECK(harm0->count() == RowsTable::kHarmonicCount + 1);
  CHECK(!harm0->isEnabled());

  pickType(type0, QStringLiteral("EQ"));
  pickType(type1, QStringLiteral("EQ"));
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

  CHECK(note0->text().contains(QStringLiteral("D4")));
  std::printf("row1 reads %s\n", note0->text().toUtf8().constData());

  note0->setValue(note0->value() + 20);
  CHECK(harm0->currentIndex() == 0);
  CHECK(harm0->currentText().contains(QStringLiteral("st ")));
  CHECK(harm0->currentText().contains(QStringLiteral("8ve")));
  CHECK(harm1->currentIndex() == 2);
  std::printf("off-grid row reads %s\n", harm0->currentText().toUtf8().constData());

  state.undo();
  CHECK(harm0->currentIndex() == 3);
}

TRENCH_TEST(slot_six_defaults_to_the_ceiling_and_can_be_an_eq) {
  EditorState state;
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  constexpr std::size_t kSix = RowsTable::kCeilingSection;
  auto* type5 = table.findChild<QComboBox*>(QStringLiteral("type5"));
  auto* ceil5 = table.findChild<NumberBox*>(QStringLiteral("heightLo5"));
  auto* note5 = table.findChild<NumberBox*>(QStringLiteral("noteLo5"));
  CHECK(type5 != nullptr && ceil5 != nullptr && note5 != nullptr);
  CHECK(table.findChild<NumberBox*>(QStringLiteral("offsetLo5")) != nullptr);
  CHECK(ceil5->maximum() == static_cast<int>(p2k::kMaxMagByte));

  pickType(type5, QStringLiteral("LOWPASS"));
  CHECK(state.zeroPresentAt(0, kSix));
  CHECK(type5->currentText() == QStringLiteral("LOWPASS"));
  CHECK(type5->count() == 6);
  CHECK(ceil5->isEnabled());
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);
  CHECK(within(zeroHz(state, 0, kSix), 20'277.05, 0.01));

  ceil5->type(QStringLiteral("8000"));
  CHECK(within(zeroHz(state, 0, kSix), 8'000.0, 0.03));
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);
  CHECK(ceil5->text().endsWith(QStringLiteral("Hz")));
  const double ceiling_before = zeroHz(state, 0, kSix);
  note5->setValue(note5->value() + 30);
  CHECK_NEAR(zeroHz(state, 0, kSix), ceiling_before, 1e-9);
  ceil5->setValue(ceil5->value() - 40);
  CHECK(zeroHz(state, 0, kSix) < ceiling_before);
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);
  std::printf("ceiling %.0f Hz -> %.0f Hz, pole %.0f Hz\n", ceiling_before,
              zeroHz(state, 0, kSix), poleHz(state, 0, kSix));

  pickType(type5, QStringLiteral("EQ"));
  CHECK(type5->currentText() == QStringLiteral("EQ"));
  CHECK(std::holds_alternative<Resonant>(state.sectionAt(0, kSix).zero));
  CHECK(within(zeroHz(state, 0, kSix), poleHz(state, 0, kSix), 0.02));
  CHECK(state.packed().words[0][kSix][1] != p2k::kS6ZeroRsqWord);
  CHECK(ceil5->text().endsWith(QStringLiteral("dB")));
  std::printf("row six as an EQ: zero %.0f Hz, pole %.0f Hz, height %s\n",
              zeroHz(state, 0, kSix), poleHz(state, 0, kSix),
              ceil5->text().toUtf8().constData());

  pickType(type5, QStringLiteral("LOWPASS"));
  CHECK(type5->currentText() == QStringLiteral("LOWPASS"));
  CHECK(state.packed().words[0][kSix][1] == p2k::kS6ZeroRsqWord);
}

TRENCH_TEST(real_pole_word_makes_a_tilt_row) {
  EditorState state;
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  unlockRow(table, 2);
  auto* type2 = table.findChild<QComboBox*>(QStringLiteral("type2"));
  auto* pole2 = table.findChild<QComboBox*>(QStringLiteral("poleLo2"));
  auto* harm2 = table.findChild<QComboBox*>(QStringLiteral("harmLo2"));
  auto* note2 = table.findChild<NumberBox*>(QStringLiteral("noteLo2"));
  auto* ring2 = table.findChild<NumberBox*>(QStringLiteral("ringLo2"));
  CHECK(type2 != nullptr && pole2 != nullptr && harm2 != nullptr);
  CHECK(note2 != nullptr && ring2 != nullptr);
  CHECK(pole2->count() == 2);
  CHECK(!pole2->isEnabled());

  pickType(type2, QStringLiteral("EQ"));
  CHECK(pole2->isEnabled());
  CHECK(pole2->currentText() == QStringLiteral("RING"));
  pole2->setCurrentIndex(pole2->findText(QStringLiteral("REAL")));
  CHECK(std::holds_alternative<trench::core::native::RealRoots>(state.sectionAt(0, 2).pole));
  const auto& words = state.packed().words[0][2];
  const auto [p, q] = p2k::pq(words[2], words[3]);
  CHECK(std::holds_alternative<trench::core::native::RealRoots>(
      trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz)));
  CHECK(pole2->currentText() == QStringLiteral("REAL"));
  CHECK(note2->text() == QStringLiteral("TILT"));
  CHECK(ring2->text().startsWith(QStringLiteral("r ")));
  CHECK(!harm2->isEnabled());
  CHECK(!note2->isEnabled());
  std::printf("real pole row: %s %s\n", note2->text().toUtf8().constData(),
              ring2->text().toUtf8().constData());

  pole2->setCurrentIndex(pole2->findText(QStringLiteral("RING")));
  CHECK(std::holds_alternative<Resonant>(state.sectionAt(0, 2).pole));
  CHECK(pole2->currentText() == QStringLiteral("RING"));
  CHECK(harm2->isEnabled());
  CHECK(note2->text().contains(QStringLiteral("Hz ")));

  state.undo();
  CHECK(pole2->currentText() == QStringLiteral("REAL"));
}

TRENCH_TEST(plot_owns_the_height) {
  MainWindow window;
  window.resize(1600, 1000);
  window.show();
  QTest::qWait(60);
  CHECK(window.findChild<QSplitter*>(QStringLiteral("workSplitter")) == nullptr);
  auto* plot = window.findChild<QWidget*>(QStringLiteral("cascadePlot"));
  auto* table = window.findChild<RowsTable*>(QStringLiteral("rowsTable"));
  auto* note = window.findChild<NumberBox*>(QStringLiteral("noteLo0"));
  CHECK(plot != nullptr && table != nullptr && note != nullptr);
  std::printf("window %dx%d, plot %dx%d, rows %dx%d\n", window.width(), window.height(),
              plot->width(), plot->height(), table->width(), table->height());
  CHECK(plot->height() * 100 >= window.height() * 55);
  CHECK(table->width() >= 700);
  window.close();
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
