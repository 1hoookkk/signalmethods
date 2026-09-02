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
#include <QPalette>
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
  auto* on = table.findChild<QCheckBox*>(QStringLiteral("fromOn0"));
  auto* pole_pitch = table.findChild<QSpinBox*>(QStringLiteral("fromPolePitch0"));
  auto* pole_res = table.findChild<QSpinBox*>(QStringLiteral("fromPoleRes0"));
  auto* pole_readout = table.findChild<QLabel*>(QStringLiteral("fromPoleReadout0"));
  auto* zero_pitch = table.findChild<QSpinBox*>(QStringLiteral("fromZeroPitch0"));
  auto* zero_depth = table.findChild<QSpinBox*>(QStringLiteral("fromZeroDepth0"));
  auto* zero_readout = table.findChild<QLabel*>(QStringLiteral("fromZeroReadout0"));
  auto* cut = table.findChild<QSpinBox*>(QStringLiteral("fromCut0"));
  CHECK(on != nullptr);
  CHECK(pole_pitch != nullptr);
  CHECK(pole_res != nullptr);
  CHECK(pole_readout != nullptr);
  CHECK(zero_pitch != nullptr);
  CHECK(zero_depth != nullptr);
  CHECK(zero_readout != nullptr);
  CHECK(cut != nullptr);
  CHECK(!pole_pitch->isEnabled());
  CHECK(pole_readout->text() == QStringLiteral("—"));

  on->click();
  CHECK(state.sectionEnabled(0));
  CHECK(pole_pitch->isEnabled());
  pole_pitch->setValue(0x90);
  pole_res->setValue(150);
  const auto& words = state.packed().words[0][0];
  CHECK(words[2] == dial(0x90));
  CHECK(words[3] == dial(kTopDial - 150));
  CHECK(pole_pitch->value() == 0x90);
  CHECK(pole_res->value() == 150);
  CHECK(pole_readout->text() == readoutOf(words[2], words[3]));
  std::printf("pole readout %s\n", pole_readout->text().toUtf8().constData());

  CHECK(!state.rootPresent(0, Lane::kZero));
  CHECK(!zero_pitch->isEnabled());
  CHECK(zero_pitch->value() == 0x90);
  CHECK(zero_depth->value() == 0);
  CHECK(zero_readout->text() == QStringLiteral("—"));
  zero_depth->setValue(100);
  CHECK(state.rootPresent(0, Lane::kZero));
  CHECK(zero_pitch->isEnabled());
  CHECK(words[0] == dial(0x90));
  CHECK(words[1] == dial(kTopDial - 100));
  zero_pitch->setValue(0xA0);
  CHECK(words[0] == dial(0xA0));
  CHECK(zero_readout->text() == readoutOf(words[0], words[1]));
  std::printf("zero readout %s\n", zero_readout->text().toUtf8().constData());
  zero_depth->setValue(0);
  CHECK(!state.rootPresent(0, Lane::kZero));
  CHECK(!zero_pitch->isEnabled());
  CHECK(zero_pitch->value() == 0x90);
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

TRENCH_TEST(row_table_shows_the_morph_pair_side_by_side) {
  EditorState state;
  RowTable table(&state);
  table.show();
  QTest::qWait(30);
  auto* from_header = table.findChild<QLabel*>(QStringLiteral("fromHeader"));
  auto* to_header = table.findChild<QLabel*>(QStringLiteral("toHeader"));
  CHECK(from_header != nullptr);
  CHECK(to_header != nullptr);
  CHECK(from_header->text().contains(QStringLiteral("CORNER 1")));
  CHECK(from_header->text().startsWith(QStringLiteral("> ")));
  CHECK(to_header->text().contains(QStringLiteral("CORNER 2")));

  auto* to_pitch = table.findChild<QSpinBox*>(QStringLiteral("toPolePitch0"));
  auto* to_on = table.findChild<QCheckBox*>(QStringLiteral("toOn0"));
  CHECK(to_pitch != nullptr);
  CHECK(to_on != nullptr);

  to_on->click();
  to_pitch->setValue(0xB0);
  CHECK(state.packed().words[1][0][2] == dial(0xB0));
  CHECK(state.sectionEnabledAt(1, 0));
  CHECK(!state.sectionEnabled(0));
  CHECK(state.editingCorner() == 0);

  state.setPadPosition(0.0, 1.0);
  QTest::qWait(10);
  CHECK(from_header->text().contains(QStringLiteral("CORNER 3")));
  CHECK(to_header->text().contains(QStringLiteral("CORNER 4")));
  CHECK(!to_on->isChecked());

  QTest::mouseClick(to_on, Qt::LeftButton);
  CHECK(state.editingCorner() == 3);
  CHECK(state.sectionEnabledAt(3, 0));
  CHECK(to_header->text().startsWith(QStringLiteral("> ")));
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
    auto* pitch = table.findChild<QSpinBox*>(QStringLiteral("fromPolePitch%1").arg(index));
    auto* res = table.findChild<QSpinBox*>(QStringLiteral("fromPoleRes%1").arg(index));
    auto* pole = table.findChild<QLabel*>(QStringLiteral("fromPoleReadout%1").arg(index));
    auto* zero_pitch = table.findChild<QSpinBox*>(QStringLiteral("fromZeroPitch%1").arg(index));
    auto* depth = table.findChild<QSpinBox*>(QStringLiteral("fromZeroDepth%1").arg(index));
    auto* zero = table.findChild<QLabel*>(QStringLiteral("fromZeroReadout%1").arg(index));
    CHECK(pitch != nullptr && res != nullptr && pole != nullptr);
    CHECK(zero_pitch != nullptr && depth != nullptr && zero != nullptr);
    CHECK(pitch->value() == static_cast<int>(p2k::dial_of_word(words[2])));
    CHECK(res->value() == kTopDial - static_cast<int>(p2k::dial_of_word(words[3])));
    CHECK(pole->text() == readoutOf(words[2], words[3]));
    CHECK(state.zeroPresentAt(0, index));
    CHECK(zero_pitch->value() == static_cast<int>(p2k::dial_of_word(words[0])));
    CHECK(depth->value() == kTopDial - static_cast<int>(p2k::dial_of_word(words[1])));
    CHECK(zero->text() == readoutOf(words[0], words[1]));
    std::printf("row %zu  pole %3d/%3d %-18s  zero %3d/%3d %s\n", index + 1, pitch->value(),
                res->value(), pole->text().toUtf8().constData(), zero_pitch->value(),
                depth->value(), zero->text().toUtf8().constData());
  }
  auto* s6_depth = table.findChild<QSpinBox*>(QStringLiteral("fromZeroDepth5"));
  CHECK(s6_depth->value() == kTopDial);
  CHECK(state.packed().words[0][5][1] == p2k::kS6ZeroRsqWord);
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
