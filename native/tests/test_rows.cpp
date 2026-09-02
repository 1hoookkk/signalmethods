#include "harness.hpp"

#include "body_io.hpp"
#include "editor_state.hpp"
#include "row_table.hpp"
#include "skin.hpp"
#include "ladder.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTest>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

QString readoutOf(std::uint16_t mag, std::uint16_t rsq) {
  const auto [p, q] = p2k::pq(mag, rsq);
  const auto roots =
      trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz);
  if (const auto* tone = std::get_if<Resonant>(&roots)) {
    return QString::asprintf("%.0f Hz · %.0f", tone->hz, tone->bw_hz);
  }
  const auto& real = std::get<trench::core::native::RealRoots>(roots);
  return QString::asprintf("R %.0f · %.0f", real.a_hz, real.b_hz);
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

void typeInto(QLineEdit* entry, const QString& text) {
  entry->setText(text);
  QTest::keyClick(entry, Qt::Key_Return);
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

TRENCH_TEST(row_table_edits_reach_the_state) {
  EditorState state;
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* on = table.findChild<QCheckBox*>(QStringLiteral("on0"));
  auto* freq = table.findChild<QSlider*>(QStringLiteral("freqFader0"));
  auto* resonance = table.findChild<QSlider*>(QStringLiteral("qFader0"));
  auto* gain = table.findChild<QSlider*>(QStringLiteral("gainFader0"));
  auto* zero = table.findChild<QSlider*>(QStringLiteral("zeroFader0"));
  auto* freq_readout = table.findChild<QLabel*>(QStringLiteral("freqReadout0"));
  auto* q_readout = table.findChild<QLabel*>(QStringLiteral("qReadout0"));
  auto* gain_readout = table.findChild<QLabel*>(QStringLiteral("gainReadout0"));
  auto* zero_readout = table.findChild<QLabel*>(QStringLiteral("zeroReadout0"));
  auto* cut = table.findChild<QSpinBox*>(QStringLiteral("cut0"));
  CHECK(on != nullptr);
  CHECK(freq != nullptr && resonance != nullptr && gain != nullptr && zero != nullptr);
  CHECK(freq_readout != nullptr && q_readout != nullptr);
  CHECK(gain_readout != nullptr && zero_readout != nullptr);
  CHECK(cut != nullptr);
  CHECK(!freq->isEnabled());
  CHECK(gain_readout->text() == QStringLiteral("—"));
  CHECK(zero_readout->text() == QStringLiteral("—"));

  on->click();
  CHECK(state.sectionEnabled(0));
  CHECK(freq->isEnabled());
  freq->setValue(0x90);
  resonance->setValue(150);
  const auto& words = state.packed().words[0][0];
  CHECK(words[2] == dial(0x90));
  CHECK(words[3] == dial(kTopDial - 150));
  CHECK(freq->value() == 0x90);
  CHECK(resonance->value() == 150);
  CHECK(freq_readout->text() == hzTextOf(words[2], words[3]));
  CHECK(q_readout->text() == resonanceTextOf(words[2], words[3]));
  std::printf("freq %s  q %s\n", freq_readout->text().toUtf8().constData(),
              q_readout->text().toUtf8().constData());

  CHECK(!state.rootPresent(0, Lane::kZero));
  CHECK(!zero->isEnabled());
  gain->setValue(100);
  CHECK(state.rootPresent(0, Lane::kZero));
  CHECK(words[0] == dial(0x90));
  CHECK(words[1] == dial(kTopDial - 100));
  CHECK(gain_readout->text() == gainTextOf(words));
  CHECK(zero_readout->text() == hzTextOf(words[0], words[1]));
  std::printf("gain %s  zero %s\n", gain_readout->text().toUtf8().constData(),
              zero_readout->text().toUtf8().constData());
  gain->setValue(0);
  CHECK(!state.rootPresent(0, Lane::kZero));
  CHECK(!zero->isEnabled());
  CHECK(zero_readout->text() == QStringLiteral("—"));

  cut->setValue(2);
  CHECK(state.cutAt(0, 0) == 2);
  state.undo();
  CHECK(cut->value() == 0);

  on->click();
  CHECK(!state.sectionEnabled(0));
  CHECK(!cut->isEnabled());
  table.close();
}

TRENCH_TEST(corner_picker_moves_the_editing_corner) {
  EditorState state;
  state.toggleSectionAt(3, 0);
  state.setWordsAt(3, 0, Lane::kPole, dial(0xB0), dial(kTopDial - 100));
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* corner0 = table.findChild<QPushButton*>(QStringLiteral("corner0"));
  auto* corner3 = table.findChild<QPushButton*>(QStringLiteral("corner3"));
  auto* on = table.findChild<QCheckBox*>(QStringLiteral("on0"));
  auto* freq = table.findChild<QSlider*>(QStringLiteral("freqFader0"));
  CHECK(corner0 != nullptr && corner3 != nullptr);
  CHECK(on != nullptr && freq != nullptr);
  CHECK(table.findChild<QPushButton*>(QStringLiteral("corner1")) != nullptr);
  CHECK(table.findChild<QPushButton*>(QStringLiteral("corner2")) != nullptr);
  CHECK(corner0->isChecked());
  CHECK(state.editingCorner() == 0);
  CHECK(!on->isChecked());

  corner3->click();
  CHECK(state.editingCorner() == 3);
  CHECK(corner3->isChecked());
  CHECK(!corner0->isChecked());
  CHECK(on->isChecked());
  const auto& words = state.packed().words[3][0];
  CHECK(freq->value() == static_cast<int>(p2k::dial_of_word(words[2])));
  CHECK(words[2] == dial(0xB0));
  table.close();
}

TRENCH_TEST(row_table_reads_back_the_packed_words) {
  EditorState state;
  voiceLadder(state, 0);
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const auto& words = state.packed().words[0][index];
    auto* freq = table.findChild<QSlider*>(QStringLiteral("freqFader%1").arg(index));
    auto* resonance = table.findChild<QSlider*>(QStringLiteral("qFader%1").arg(index));
    auto* gain = table.findChild<QSlider*>(QStringLiteral("gainFader%1").arg(index));
    auto* zero = table.findChild<QSlider*>(QStringLiteral("zeroFader%1").arg(index));
    auto* freq_readout =
        table.findChild<QLabel*>(QStringLiteral("freqReadout%1").arg(index));
    auto* q_readout = table.findChild<QLabel*>(QStringLiteral("qReadout%1").arg(index));
    auto* gain_readout =
        table.findChild<QLabel*>(QStringLiteral("gainReadout%1").arg(index));
    auto* zero_readout =
        table.findChild<QLabel*>(QStringLiteral("zeroReadout%1").arg(index));
    CHECK(freq != nullptr && resonance != nullptr && gain != nullptr && zero != nullptr);
    CHECK(freq_readout != nullptr && q_readout != nullptr && gain_readout != nullptr);
    CHECK(zero_readout != nullptr);
    CHECK(state.zeroPresentAt(0, index));
    CHECK(freq->value() == static_cast<int>(p2k::dial_of_word(words[2])));
    CHECK(resonance->value() == kTopDial - static_cast<int>(p2k::dial_of_word(words[3])));
    CHECK(gain->value() == kTopDial - static_cast<int>(p2k::dial_of_word(words[1])));
    CHECK(zero->value() == static_cast<int>(p2k::dial_of_word(words[0])));
    CHECK(freq_readout->text() == hzTextOf(words[2], words[3]));
    CHECK(q_readout->text() == resonanceTextOf(words[2], words[3]));
    CHECK(gain_readout->text() == gainTextOf(words));
    CHECK(zero_readout->text() == hzTextOf(words[0], words[1]));
    std::printf("row %zu  %-12s %-8s %-9s zero %s\n", index + 1,
                freq_readout->text().toUtf8().constData(),
                q_readout->text().toUtf8().constData(),
                gain_readout->text().toUtf8().constData(),
                zero_readout->text().toUtf8().constData());
  }
  auto* s6_gain = table.findChild<QSlider*>(QStringLiteral("gainFader5"));
  CHECK(s6_gain->value() == kTopDial);
  CHECK(state.packed().words[0][5][1] == p2k::kS6ZeroRsqWord);
  table.close();
}

TRENCH_TEST(typed_frequency_lands_on_the_nearest_word) {
  EditorState state;
  voiceLadder(state, 0);
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* lock = table.findChild<QPushButton*>(QStringLiteral("lock2"));
  CHECK(lock != nullptr);
  const auto& words = state.packed().words[0][2];
  if (!lock->isChecked()) lock->click();
  CHECK(lock->isChecked());
  CHECK(onTheNote(words));

  auto* freq_entry = table.findChild<QLineEdit*>(QStringLiteral("freqEntry2"));
  CHECK(freq_entry != nullptr);
  typeInto(freq_entry, QStringLiteral("440"));
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

  auto* q_entry = table.findChild<QLineEdit*>(QStringLiteral("qEntry2"));
  CHECK(q_entry != nullptr);
  typeInto(q_entry, QStringLiteral("20"));
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

  auto* gain_entry = table.findChild<QLineEdit*>(QStringLiteral("gainEntry2"));
  CHECK(gain_entry != nullptr);
  typeInto(gain_entry, QStringLiteral("6"));
  {
    const double landed = std::abs(gainDbOf(words) - 6.0);
    for (int candidate = 0; candidate <= kTopDial; ++candidate) {
      trench::core::PackedSection trial = words;
      if (candidate == 0) {
        trial[0] = trench::core::kIdentitySection[0];
        trial[1] = trench::core::kIdentitySection[1];
      } else {
        trial[1] = dial(kTopDial - candidate);
        trial[0] = p2k::mag_word_for(rootOf(words[2], words[3]).hz, trial[1]);
      }
      CHECK(std::abs(gainDbOf(trial) - 6.0) >= landed - 1e-9);
    }
    std::printf("typed 6 dB landed on %.2f dB\n", gainDbOf(words));
    CHECK(onTheNote(words));
  }
  table.close();
}

TRENCH_TEST(lock_moves_the_zero_with_the_pole) {
  EditorState state;
  voiceLadder(state, 0);
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* lock = table.findChild<QPushButton*>(QStringLiteral("lock1"));
  auto* freq = table.findChild<QSlider*>(QStringLiteral("freqFader1"));
  auto* zero = table.findChild<QSlider*>(QStringLiteral("zeroFader1"));
  CHECK(lock != nullptr && freq != nullptr && zero != nullptr);
  const auto& words = state.packed().words[0][1];
  if (!lock->isChecked()) lock->click();
  CHECK(lock->isChecked());
  CHECK(onTheNote(words));
  CHECK(!zero->isEnabled());

  freq->setValue(freq->value() + 10);
  CHECK(onTheNote(words));
  CHECK(lock->isChecked());

  lock->click();
  CHECK(!lock->isChecked());
  CHECK(zero->isEnabled());
  zero->setValue(zero->value() - 10);
  CHECK(!onTheNote(words));
  CHECK(!lock->isChecked());

  lock->click();
  CHECK(onTheNote(words));
  CHECK(lock->isChecked());
  CHECK(!zero->isEnabled());
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
