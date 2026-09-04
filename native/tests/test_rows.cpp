#include "harness.hpp"

#include "body_io.hpp"
#include "editor_state.hpp"
#include "main_window.hpp"
#include "morph_pad.hpp"
#include "number_box.hpp"
#include "rows_table.hpp"
#include "skin.hpp"
#include "ladder.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPoint>
#include <QSpinBox>
#include <QStyle>
#include <QTest>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <variant>

namespace {

namespace p2k = trench::core::p2k;
using Lane = EditorState::Lane;
using Resonant = trench::core::native::Resonant;

constexpr int kTopDial = static_cast<int>(p2k::kDialCount) - 1;

QString smokeFolder() {
  const char* configured = std::getenv("TRENCH_SMOKE_DIR");
  const QString folder = configured != nullptr && *configured != '\0'
                             ? QString::fromLocal8Bit(configured)
                             : QDir::tempPath();
  QDir().mkpath(folder);
  return folder;
}

std::uint16_t dial(int byte) {
  return p2k::dial_word(static_cast<std::size_t>(byte));
}

struct Root {
  double hz{};
  double bw_hz{};
};

Root rootOf(std::uint16_t mag, std::uint16_t rsq) {
  const auto [p, q] = p2k::pq(mag, rsq);
  const auto roots =
      trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz);
  if (const auto* tone = std::get_if<Resonant>(&roots)) return {tone->hz, tone->bw_hz};
  const auto& real = std::get<trench::core::native::RealRoots>(roots);
  return {real.a_hz, 0.0};
}

double resonanceOf(const Root& root) {
  return root.bw_hz > 0.0 ? root.hz / root.bw_hz : 0.0;
}

bool onTheNote(const trench::core::PackedSection& words) {
  return words[0] == p2k::mag_word_for(rootOf(words[2], words[3]).hz, words[1]);
}

QString hzTextOf(std::uint16_t mag, std::uint16_t rsq) {
  return QString::asprintf("%.1f Hz", rootOf(mag, rsq).hz);
}

QString resonanceTextOf(std::uint16_t mag, std::uint16_t rsq) {
  return QString::asprintf("Q %.1f", resonanceOf(rootOf(mag, rsq)));
}

double gainDbOf(const trench::core::PackedSection& words) {
  return trench::core::section_response_db(trench::core::section_words_to_biquad(words),
                                           rootOf(words[2], words[3]).hz,
                                           EditorState::kDatumHz);
}

QString gainTextOf(const trench::core::PackedSection& words) {
  return QString::asprintf("%+.1f dB", gainDbOf(words));
}

void pickType(QComboBox* box, const QString& name) {
  box->setCurrentIndex(box->findText(name));
}

void openExtras(RowsTable& table) {
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    auto* unlock = table.findChild<QToolButton*>(QStringLiteral("unlock%1").arg(index));
    if (unlock != nullptr && !unlock->isChecked()) unlock->click();
  }
}

int ringDialOf(const trench::core::PackedSection& words) {
  return static_cast<int>(p2k::dial_of_word(words[3]));
}

int magDialOf(const trench::core::PackedSection& words) {
  return static_cast<int>(p2k::dial_of_word(words[2]));
}

}

TRENCH_TEST(pole_dial_words_survive_the_export) {
  EditorState state;
  state.toggleSectionAt(0, 1);
  std::size_t legal = 0;
  std::size_t moved = 0;
  for (int res : {0, 60, 120, 180}) {
    const std::uint16_t rsq = dial(kTopDial - res);
    for (int pitch = 0; pitch <= static_cast<int>(p2k::kMaxMagByte); ++pitch) {
      const std::uint16_t mag = dial(pitch);
      const auto [p, q] = p2k::pq(mag, rsq);
      if (!p2k::is_legal(p, q, true)) continue;
      ++legal;
      state.setWordsAt(0, 1, Lane::kPole, mag, rsq);
      const auto& words = state.packed().words[0][1];
      if (words[2] != mag || words[3] != rsq) {
        if (moved == 0) {
          std::printf("pitch %d res %d wrote %04X %04X, packed %04X %04X\n", pitch, res,
                      mag, rsq, words[2], words[3]);
        }
        ++moved;
        continue;
      }
      CHECK(static_cast<int>(p2k::dial_of_word(words[2])) == pitch);
      CHECK(static_cast<int>(p2k::dial_of_word(words[3])) == kTopDial - res);
    }
  }
  std::printf("%zu legal pole dial pairs, %zu moved by the export\n", legal, moved);
  CHECK(legal > 600);
  CHECK(moved == 0);
}

TRENCH_TEST(zero_dial_words_survive_the_export_until_the_cage) {
  EditorState state;
  state.toggleSectionAt(0, 1);
  const std::uint16_t mag = dial(0x80);
  std::size_t legal = 0;
  std::size_t caged = 0;
  std::size_t moved = 0;
  for (int depth = 1; depth <= kTopDial; ++depth) {
    const std::uint16_t rsq = dial(kTopDial - depth);
    const auto [p, q] = p2k::pq(mag, rsq);
    if (!p2k::is_legal(p, q, false)) continue;
    ++legal;
    state.setWordsAt(0, 1, Lane::kZero, mag, rsq);
    CHECK(state.zeroPresentAt(0, 1));
    const auto& words = state.packed().words[0][1];
    const auto roots =
        trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz);
    const auto* tone = std::get_if<Resonant>(&roots);
    if (tone != nullptr && tone->bw_hz < 1.0) {
      CHECK(words[1] == p2k::kS6ZeroRsqWord);
      ++caged;
      continue;
    }
    if (words[0] != mag || words[1] != rsq) {
      if (moved == 0) {
        std::printf("depth %d wrote %04X %04X, packed %04X %04X\n", depth, mag, rsq,
                    words[0], words[1]);
      }
      ++moved;
    }
  }
  std::printf("%zu legal zero depths, %zu on the cage, %zu moved by the export\n", legal,
              caged, moved);
  CHECK(legal > 150);
  CHECK(caged > 0);
  CHECK(moved == 0);
  state.removeZeroAt(0, 1);
  CHECK(!state.zeroPresentAt(0, 1));
  CHECK(state.packed().words[0][1][0] == trench::core::kIdentitySection[0]);
  CHECK(state.packed().words[0][1][1] == trench::core::kIdentitySection[1]);
}

TRENCH_TEST(rows_table_edits_reach_the_state) {
  EditorState state;
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  openExtras(table);
  auto* type = table.findChild<QComboBox*>(QStringLiteral("type0"));
  auto* note = table.findChild<NumberBox*>(QStringLiteral("noteLo0"));
  auto* ring = table.findChild<NumberBox*>(QStringLiteral("ringLo0"));
  auto* height = table.findChild<NumberBox*>(QStringLiteral("heightLo0"));
  auto* offset = table.findChild<NumberBox*>(QStringLiteral("offsetLo0"));
  auto* cut = table.findChild<QSpinBox*>(QStringLiteral("cutLo0"));
  CHECK(type != nullptr);
  CHECK(note != nullptr && ring != nullptr && height != nullptr && offset != nullptr);
  CHECK(cut != nullptr);
  CHECK(!note->isEnabled());
  CHECK(type->currentText() == QStringLiteral("OFF"));
  CHECK(height->text() == QStringLiteral("—"));
  CHECK(offset->text() == QStringLiteral("—"));

  pickType(type, QStringLiteral("EQ"));
  CHECK(state.sectionEnabled(0));
  CHECK(note->isEnabled());
  CHECK(type->isEnabled());
  CHECK(type->currentText() == QStringLiteral("EQ"));
  const auto& words = state.packed().words[0][0];
  CHECK(state.rootPresent(0, Lane::kZero));
  CHECK(onTheNote(words));
  {
    const Root fresh = rootOf(words[2], words[3]);
    std::printf("fresh row: %s  %s  height %s\n", note->text().toUtf8().constData(),
                ring->text().toUtf8().constData(), height->text().toUtf8().constData());
    CHECK(std::abs(std::log2(fresh.hz / table.rootHz())) < 0.05);
    CHECK(resonanceOf(fresh) > 8.0);
    CHECK(height->isEnabled() && offset->isEnabled());
  }
  pickType(type, QStringLiteral("POLE"));
  CHECK(!state.rootPresent(0, Lane::kZero));
  ring->setValue(150);
  {
    const Root after_q = rootOf(words[2], words[3]);
    CHECK(words[3] == dial(kTopDial - 150));
    CHECK(std::abs(std::log2(after_q.hz / table.rootHz())) < 0.05);
  }
  const double q_before_freq = resonanceOf(rootOf(words[2], words[3]));
  note->setValue(0x90);
  CHECK(words[2] == dial(0x90));
  CHECK(note->value() == 0x90);
  CHECK(std::abs(std::log2(resonanceOf(rootOf(words[2], words[3])) / q_before_freq)) < 0.1);
  CHECK(note->text().startsWith(hzTextOf(words[2], words[3]) + QStringLiteral(" ")));
  CHECK(ring->text() == resonanceTextOf(words[2], words[3]));
  std::printf("note %s  ring %s (Q held across the NOTE move)\n",
              note->text().toUtf8().constData(), ring->text().toUtf8().constData());

  CHECK(!height->isEnabled());
  CHECK(!offset->isEnabled());
  pickType(type, QStringLiteral("EQ"));
  CHECK(state.rootPresent(0, Lane::kZero));
  CHECK(onTheNote(words));
  CHECK(height->isEnabled());
  CHECK(offset->isEnabled());
  CHECK(height->value() == static_cast<int>(p2k::dial_of_word(words[1])));
  CHECK(offset->value() == 0);
  const Root pole = rootOf(words[2], words[3]);
  const Root zero = rootOf(words[0], words[1]);
  CHECK(zero.bw_hz > pole.bw_hz * 3.0);
  height->setValue(100);
  CHECK(words[1] == dial(100));
  CHECK(onTheNote(words));
  CHECK(height->text() == gainTextOf(words));
  CHECK(std::abs(12.0 * std::log2(rootOf(words[0], words[1]).hz / pole.hz)) < 1.0);
  CHECK(offset->text().endsWith(QStringLiteral(" st")));
  std::printf("height %s  offset %s\n", height->text().toUtf8().constData(),
              offset->text().toUtf8().constData());

  offset->setValue(12);
  CHECK(!onTheNote(words));
  const double semitones = 12.0 * std::log2(rootOf(words[0], words[1]).hz / pole.hz);
  CHECK(std::abs(semitones - 12.0) < 1.0);
  CHECK(words[1] == dial(100));
  std::printf("offset 12 landed on %s\n", offset->text().toUtf8().constData());

  pickType(type, QStringLiteral("NOTCH"));
  CHECK(words[1] == p2k::kS6ZeroRsqWord);
  CHECK(!height->isEnabled());
  CHECK(offset->isEnabled());
  CHECK(std::abs(12.0 * std::log2(rootOf(words[0], words[1]).hz / pole.hz) - 12.0) < 1.0);

  pickType(type, QStringLiteral("HIGHPASS"));
  const auto* edge = std::get_if<trench::core::native::RealRoots>(&state.sectionAt(0, 0).zero);
  CHECK(edge != nullptr);
  CHECK(edge->a_hz > 0.0);
  CHECK(!offset->isEnabled());
  CHECK(offset->text() == QStringLiteral("—"));

  pickType(type, QStringLiteral("POLE"));
  CHECK(!state.rootPresent(0, Lane::kZero));
  CHECK(!height->isEnabled());
  CHECK(height->text() == QStringLiteral("—"));

  cut->setValue(2);
  CHECK(state.cutAt(0, 0) == 2);
  state.undo();
  CHECK(cut->value() == 0);

  pickType(type, QStringLiteral("OFF"));
  CHECK(!state.sectionEnabled(0));
  CHECK(!cut->isEnabled());
  table.close();
}

TRENCH_TEST(lo_and_hi_edit_the_morph_pair) {
  EditorState state;
  state.toggleSectionAt(1, 0);
  state.setWordsAt(1, 0, Lane::kPole, dial(0x60), dial(kTopDial - 100));
  state.toggleSectionAt(3, 0);
  state.setWordsAt(3, 0, Lane::kPole, dial(0xB0), dial(kTopDial - 100));
  state.setEditingCorner(0);
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* note_lo = table.findChild<NumberBox*>(QStringLiteral("noteLo0"));
  auto* note_hi = table.findChild<NumberBox*>(QStringLiteral("noteHi0"));
  auto* ring_hi = table.findChild<NumberBox*>(QStringLiteral("ringHi0"));
  CHECK(note_lo != nullptr && note_hi != nullptr && ring_hi != nullptr);
  CHECK(table.pairBase() == 0);
  CHECK(table.cornerOf(RowsTable::Side::kHi) == 1);
  CHECK(state.editingCorner() == 0);
  CHECK(!note_lo->isEnabled());
  CHECK(note_hi->isEnabled());
  CHECK(note_hi->value() == static_cast<int>(p2k::dial_of_word(state.packed().words[1][0][2])));
  CHECK(state.packed().words[1][0][2] == dial(0x60));

  note_hi->setValue(0x70);
  CHECK(state.editingCorner() == 1);
  CHECK(state.packed().words[1][0][2] == dial(0x70));
  CHECK(state.packed().words[3][0][2] == dial(0xB0));

  state.setPadPosition(state.morphPos(), 1.0);
  QTest::qWait(10);
  CHECK(table.pairBase() == 2);
  CHECK(table.cornerOf(RowsTable::Side::kHi) == 3);
  const auto& words = state.packed().words[3][0];
  CHECK(note_hi->value() == static_cast<int>(p2k::dial_of_word(words[2])));
  CHECK(words[2] == dial(0xB0));
  note_hi->setValue(0xA0);
  CHECK(state.editingCorner() == 3);
  CHECK(state.packed().words[3][0][2] == dial(0xA0));
  CHECK(state.packed().words[1][0][2] == dial(0x70));
  table.close();
}

TRENCH_TEST(rows_table_reads_back_the_packed_words) {
  EditorState state;
  voiceLadder(state, 0);
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  openExtras(table);
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const auto& words = state.packed().words[0][index];
    const bool ceiling = index == RowsTable::kCeilingSection;
    auto* type = table.findChild<QComboBox*>(QStringLiteral("type%1").arg(index));
    auto* note = table.findChild<NumberBox*>(QStringLiteral("noteLo%1").arg(index));
    auto* ring = table.findChild<NumberBox*>(QStringLiteral("ringLo%1").arg(index));
    auto* height = table.findChild<NumberBox*>(QStringLiteral("heightLo%1").arg(index));
    auto* offset = table.findChild<NumberBox*>(QStringLiteral("offsetLo%1").arg(index));
    CHECK(type != nullptr);
    CHECK(note != nullptr && ring != nullptr && height != nullptr);
    CHECK(offset != nullptr);
    CHECK(state.zeroPresentAt(0, index));
    const bool notch = words[1] == p2k::kS6ZeroRsqWord;
    CHECK(type->currentText() == (ceiling ? QStringLiteral("LOWPASS")
                                          : notch ? QStringLiteral("NOTCH")
                                                  : QStringLiteral("EQ")));
    CHECK(note->value() == static_cast<int>(p2k::dial_of_word(words[2])));
    CHECK(ring->value() == kTopDial - static_cast<int>(p2k::dial_of_word(words[3])));
    CHECK(height->isEnabled() == (ceiling || !notch));
    if (!ceiling && !notch) {
      CHECK(height->value() == static_cast<int>(p2k::dial_of_word(words[1])));
    }
    const double semitones = 12.0 * std::log2(rootOf(words[0], words[1]).hz /
                                              rootOf(words[2], words[3]).hz);
    if (ceiling) {
      CHECK(height->value() == static_cast<int>(p2k::dial_of_word(words[0])));
      CHECK(height->text() == hzTextOf(words[0], words[1]));
    } else {
      CHECK(offset->value() == static_cast<int>(std::lround(semitones)));
      CHECK(offset->text() == QString::asprintf("%+.1f st", semitones));
      CHECK(height->text() == (notch ? QStringLiteral("—") : gainTextOf(words)));
    }
    CHECK(note->text().startsWith(hzTextOf(words[2], words[3]) + QStringLiteral(" ")));
    CHECK(ring->text() == resonanceTextOf(words[2], words[3]));
    std::printf("row %zu  %-9s %-12s %-8s %-9s %s\n", index + 1,
                type->currentText().toUtf8().constData(),
                note->text().toUtf8().constData(),
                ring->text().toUtf8().constData(),
                height->text().toUtf8().constData(),
                ceiling ? "-" : offset->text().toUtf8().constData());
  }
  CHECK(state.packed().words[0][5][1] == p2k::kS6ZeroRsqWord);
  table.close();
}

TRENCH_TEST(typed_frequency_lands_on_the_nearest_word) {
  EditorState state;
  voiceLadder(state, 0);
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  openExtras(table);
  auto* offset = table.findChild<NumberBox*>(QStringLiteral("offsetLo2"));
  CHECK(offset != nullptr);
  const auto& words = state.packed().words[0][2];
  offset->type(QStringLiteral("0"));
  CHECK(onTheNote(words));

  auto* note = table.findChild<NumberBox*>(QStringLiteral("noteLo2"));
  CHECK(note != nullptr);
  note->type(QStringLiteral("440"));
  {
    const double landed =
        std::abs(std::log2(rootOf(words[2], words[3]).hz) - std::log2(440.0));
    for (int candidate = 0; candidate <= static_cast<int>(p2k::kMaxMagByte); ++candidate) {
      const Root root = rootOf(dial(candidate), words[3]);
      if (!(root.hz > 0.0)) continue;
      CHECK(std::abs(std::log2(root.hz) - std::log2(440.0)) >= landed - 1e-9);
    }
    std::printf("typed 440 landed on %.1f Hz\n", rootOf(words[2], words[3]).hz);
    CHECK(onTheNote(words));
  }

  auto* ring = table.findChild<NumberBox*>(QStringLiteral("ringLo2"));
  CHECK(ring != nullptr);
  ring->type(QStringLiteral("20"));
  {
    const double landed =
        std::abs(std::log2(resonanceOf(rootOf(words[2], words[3]))) - std::log2(20.0));
    const int span = kTopDial - static_cast<int>(p2k::dial_of_word(p2k::kPoleCeilingRsqWord));
    for (int candidate = 0; candidate <= span; ++candidate) {
      const double resonance =
          resonanceOf(rootOf(words[2], dial(kTopDial - candidate)));
      if (!(resonance > 0.0)) continue;
      CHECK(std::abs(std::log2(resonance) - std::log2(20.0)) >= landed - 1e-9);
    }
    std::printf("typed Q 20 landed on Q %.2f\n", resonanceOf(rootOf(words[2], words[3])));
    CHECK(onTheNote(words));
  }

  auto* height = table.findChild<NumberBox*>(QStringLiteral("heightLo2"));
  CHECK(height != nullptr);
  height->type(QStringLiteral("6"));
  {
    const double landed = std::abs(gainDbOf(words) - 6.0);
    for (int candidate = 1; candidate <= kTopDial; ++candidate) {
      trench::core::PackedSection trial = words;
      trial[1] = dial(candidate);
      trial[0] = p2k::mag_word_for(rootOf(words[2], words[3]).hz, trial[1]);
      CHECK(std::abs(gainDbOf(trial) - 6.0) >= landed - 1e-9);
    }
    std::printf("typed 6 dB landed on %.2f dB\n", gainDbOf(words));
    CHECK(onTheNote(words));
  }
  table.close();
}

TRENCH_TEST(offset_holds_the_zero_to_the_pole) {
  EditorState state;
  voiceLadder(state, 0);
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  openExtras(table);
  auto* note = table.findChild<NumberBox*>(QStringLiteral("noteLo1"));
  auto* offset = table.findChild<NumberBox*>(QStringLiteral("offsetLo1"));
  CHECK(note != nullptr && offset != nullptr);
  const auto& words = state.packed().words[0][1];
  offset->type(QStringLiteral("0"));
  CHECK(onTheNote(words));
  CHECK(offset->value() == 0);

  note->setValue(note->value() + 10);
  CHECK(onTheNote(words));

  offset->setValue(7);
  CHECK(!onTheNote(words));
  const auto semitones = [&] {
    return 12.0 * std::log2(rootOf(words[0], words[1]).hz / rootOf(words[2], words[3]).hz);
  };
  CHECK(std::abs(semitones() - 7.0) < 1.0);
  std::printf("offset 7 landed on %+.2f st\n", semitones());

  note->setValue(note->value() + 10);
  CHECK(std::abs(semitones() - 7.0) < 1.0);
  CHECK(offset->value() == 7);

  offset->setValue(0);
  CHECK(onTheNote(words));
  table.close();
}

TRENCH_TEST(ring_rule_follows_the_note) {
  const std::array<int, 4> mags{0x20, 0x60, 0xA0, 0xE0};
  std::array<double, 4> qs{};
  for (std::size_t which = 0; which < mags.size(); ++which) {
    const int mag = mags[which];
    const int rsq = RowsTable::ringRule(mag);
    const Root root = rootOf(dial(mag), dial(rsq));
    qs[which] = resonanceOf(root);
    std::printf("mag 0x%02X -> rsq dial %3d : %9.2f Hz  bw %8.2f  Q %7.3f\n", mag, rsq,
                root.hz, root.bw_hz, qs[which]);
  }
  const double lowest = *std::min_element(qs.begin(), qs.end());
  const double highest = *std::max_element(qs.begin(), qs.end());
  std::printf("ring rule Q span %.3f .. %.3f, ratio %.3f\n", lowest, highest,
              highest / lowest);
  CHECK(lowest > 0.0);
  CHECK(highest / lowest < 4.0);
}

TRENCH_TEST(ring_follows_until_touched) {
  EditorState state;
  RowsTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* type = table.findChild<QComboBox*>(QStringLiteral("type0"));
  auto* note = table.findChild<NumberBox*>(QStringLiteral("noteLo0"));
  auto* ring = table.findChild<NumberBox*>(QStringLiteral("ringLo0"));
  CHECK(type != nullptr && note != nullptr && ring != nullptr);
  pickType(type, QStringLiteral("EQ"));
  table.followNote(RowsTable::Side::kLo, 0);
  const auto& words = state.packed().words[0][0];
  CHECK(ring->following());
  CHECK(ringDialOf(words) == RowsTable::ringRule(magDialOf(words)));

  note->setValue(0x90);
  CHECK(magDialOf(words) == 0x90);
  CHECK(ringDialOf(words) == RowsTable::ringRule(0x90));
  CHECK(std::abs(std::log2(rootOf(words[0], words[1]).hz /
                           rootOf(words[2], words[3]).hz)) < 0.05);
  CHECK(ring->following());
  std::printf("following: note %s  ring %s\n", note->text().toUtf8().constData(),
              ring->text().toUtf8().constData());

  const int rsq_before = ringDialOf(words);
  const QPoint start(ring->width() / 2, ring->height() / 2);
  QTest::mousePress(ring, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(ring, start + QPoint(0, -60));
  QTest::mouseRelease(ring, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, -60));
  CHECK(!ring->following());
  CHECK(ringDialOf(words) != rsq_before);
  std::printf("touched: ring %s\n", ring->text().toUtf8().constData());

  note->setValue(0x70);
  CHECK(magDialOf(words) == 0x70);
  CHECK(ringDialOf(words) != RowsTable::ringRule(0x70));
  CHECK(!ring->following());
  table.close();
}

TRENCH_TEST(cuts_and_real_roots_survive_save_and_open) {
  EditorState state;
  state.toggleSectionAt(0, 0);
  state.setRealRootAt(0, 0, Lane::kPole, 120.0, 400.0);
  state.setCutAt(0, 0, 2);
  state.toggleSectionAt(0, 1);
  state.setWordsAt(0, 1, Lane::kPole, dial(0x90), dial(0x60));
  state.setWordsAt(0, 1, Lane::kZero, dial(0xA0), dial(0x40));
  state.setCutAt(0, 1, 1);
  state.toggleSectionAt(2, 5);

  const QString path = smokeFolder() + QStringLiteral("/dials.trenchbody");
  CHECK(trench::app::saveDocument(state.document(), path).isEmpty());
  QString error;
  const auto opened = trench::app::loadDocument(path, &error);
  CHECK(opened.has_value());
  CHECK(*opened == state.document());

  EditorState reopened;
  reopened.setDocument(*opened);
  CHECK(reopened.cutAt(0, 0) == 2);
  CHECK(reopened.cutAt(0, 1) == 1);
  CHECK(reopened.packed().words == state.packed().words);
  const auto* real = std::get_if<trench::core::native::RealRoots>(&reopened.sectionAt(0, 0).pole);
  CHECK(real != nullptr);
  CHECK_NEAR(real->a_hz, 120.0, 1e-9);
  CHECK_NEAR(real->b_hz, 400.0, 1e-9);

  QFile file(path);
  CHECK(file.open(QIODevice::ReadOnly));
  QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
  file.close();
  QJsonArray corners = root.value(QStringLiteral("corners")).toArray();
  QJsonObject corner = corners.at(0).toObject();
  corner[QStringLiteral("gain_db")] = 3.0;
  QJsonArray sections = corner.value(QStringLiteral("sections")).toArray();
  QJsonObject section = sections.at(0).toObject();
  QJsonObject row;
  row[QStringLiteral("type")] = QStringLiteral("bell");
  section[QStringLiteral("row")] = row;
  section.remove(QStringLiteral("cut"));
  sections[0] = section;
  corner[QStringLiteral("sections")] = sections;
  corners[0] = corner;
  root[QStringLiteral("corners")] = corners;
  const QString legacy = smokeFolder() + QStringLiteral("/legacy_rows.trenchbody");
  {
    QFile out(legacy);
    CHECK(out.open(QIODevice::WriteOnly));
    out.write(QJsonDocument(root).toJson());
  }
  const auto old = trench::app::loadDocument(legacy, &error);
  CHECK(old.has_value());
  CHECK(old->corners[0].cut[0] == 0);
  CHECK(old->corners[0].cut[1] == 1);
  CHECK(old->corners[0].corner == state.document().corners[0].corner);
}

TRENCH_TEST(axis_names_round_trip) {
  MainWindow window;
  window.resize(1560, 860);
  window.show();
  QTest::qWait(60);
  auto& state = window.state();
  voiceLadder(state, 0);
  window.setAxisNamesForTest(QStringLiteral("OO → EE"), QStringLiteral("F2"));
  CHECK(state.axisNames().first == QStringLiteral("OO → EE"));
  CHECK(state.axisNames().second == QStringLiteral("F2"));
  MorphPad* pad = window.pad();
  CHECK(pad != nullptr);
  CHECK(pad->objectName() == QStringLiteral("morphPad"));
  std::printf("pad labels %s / %s\n", pad->morphLabel().toUtf8().constData(),
              pad->qLabel().toUtf8().constData());
  CHECK(pad->morphLabel() == QStringLiteral("OO → EE"));
  CHECK(pad->qLabel() == QStringLiteral("F2"));

  const QString document = smokeFolder() + QStringLiteral("/axes.trenchbody");
  CHECK(trench::app::saveDocument(state.document(), document).isEmpty());
  QString error;
  const auto opened = trench::app::loadDocument(document, &error);
  CHECK(opened.has_value());
  CHECK(opened->morph_axis == QStringLiteral("OO → EE"));
  CHECK(opened->q_axis == QStringLiteral("F2"));
  window.openPath(document);
  CHECK(state.axisNames().first == QStringLiteral("OO → EE"));
  CHECK(state.axisNames().second == QStringLiteral("F2"));

  const QString packed = smokeFolder() + QStringLiteral("/axes.body240");
  CHECK(trench::app::saveBody240(state, packed).isEmpty());
  window.openPath(packed);
  CHECK(state.axisNames().first.isEmpty());
  CHECK(state.axisNames().second.isEmpty());
  CHECK(pad->morphLabel() == QStringLiteral("MORPH"));
  CHECK(pad->qLabel() == QStringLiteral("Q"));
  window.close();
}

TRENCH_TEST(cut_survives_the_body240_round_trip) {
  EditorState state;
  voiceLadder(state, 0);
  state.setCutAt(0, 1, 2);
  state.setCutAt(0, 3, 1);
  voiceLadder(state, 2, 96.0);
  state.setCutAt(2, 4, 3);
  const auto words = state.packed().words;

  const QString path = smokeFolder() + QStringLiteral("/cuts.body240");
  CHECK(trench::app::saveBody240(state, path).isEmpty());
  QFile file(path);
  CHECK(file.open(QIODevice::ReadOnly));
  const QByteArray bytes = file.readAll();
  CHECK(bytes.size() == static_cast<qsizetype>(trench::core::kLegacyBodyBytes));
  const std::span<const std::uint8_t> view(
      reinterpret_cast<const std::uint8_t*>(bytes.constData()),
      static_cast<std::size_t>(bytes.size()));

  EditorState opened;
  opened.setDocument(EditorState::documentFrom(trench::core::native::import_p2k(view)));
  CHECK(opened.cutAt(0, 0) == 0);
  CHECK(opened.cutAt(0, 1) == 2);
  CHECK(opened.cutAt(0, 3) == 1);
  CHECK(opened.cutAt(2, 4) == 3);
  CHECK(opened.cutAt(2, 0) == 0);
  CHECK(opened.packed().words == words);
}

TRENCH_TEST(skin_is_system_grey_and_aliased) {
  trench::app::applyNinetiesSkin(*qApp);
  CHECK(qApp->style()->objectName().toLower() == QStringLiteral("windows"));
  CHECK(qApp->palette().color(QPalette::Window) == QColor(0xc0, 0xc0, 0xc0));
  CHECK((qApp->font().styleStrategy() & QFont::NoAntialias) != 0);
}

TRENCH_TEST(row_from_frame_writes_that_slot) {
  EditorState state;
  state.setDocument(EditorState::blank());
  RowsTable table(&state);
  const auto frames = trench::app::loadFrames();
  CHECK(!frames.empty());
  const trench::app::PoleTemplate* chosen = nullptr;
  for (const auto& frame : frames) {
    if (trench::app::frameCornerState(frame).enabled[1]) {
      chosen = &frame;
      break;
    }
  }
  CHECK(chosen != nullptr);
  const auto before = state.document();
  table.rowFromFrame(1, *chosen, 1);
  CHECK(state.sectionEnabledAt(0, 1));
  const auto& landed = state.packed().words[0][1];
  for (std::size_t index = 0; index < 4; ++index) {
    std::printf("row from frame word %zu %04X wants %04X\n", index, landed[index],
                chosen->words[1][index]);
    CHECK(landed[index] == chosen->words[1][index]);
  }
  std::printf("gain word %04X, frame carried %04X\n", landed[4], chosen->words[1][4]);
  state.undo();
  CHECK(state.document() == before);
  table.close();
}
