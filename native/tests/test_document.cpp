#include "harness.hpp"

#include "body_io.hpp"
#include "editor_state.hpp"
#include "import_routing.hpp"
#include "trench/core/native_body.hpp"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QTemporaryDir>

#include <cstddef>
#include <filesystem>
#include <variant>

namespace {

namespace native = trench::core::native;
using Lane = EditorState::Lane;
using trench::app::ImportKind;

QByteArray readAll(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return file.readAll();
}

void writeAll(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  CHECK(file.open(QIODevice::WriteOnly));
  CHECK(file.write(bytes) == bytes.size());
}

std::filesystem::path native_path(const QString& path) {
  return std::filesystem::path(path.toStdWString());
}

void shape(EditorState& state) {
  state.setRoot(0, Lane::kPole, 90.0, 100.0);
  state.setRoot(0, Lane::kZero, 110.0, 60.0);
  state.selectSection(3);
  state.removeZero();
  state.toggleSection(4);

  state.setEditingCorner(1);
  state.setRoot(1, Lane::kPole, 300.0, 180.0);
  state.setRoot(2, Lane::kZero, 640.0, 220.0);
  state.selectSection(5);
  state.removeZero();
  state.toggleSection(2);

  state.setEditingCorner(2);
  state.setRoot(3, Lane::kPole, 2'500.0, 400.0);

  state.setEditingCorner(3);
  state.setRoot(4, Lane::kPole, 6'200.0, 900.0);
  state.setRoot(5, Lane::kZero, 11'000.0, 3'000.0);

  state.setPadPosition(0.37, 0.61);
  state.setEditingCorner(2);
}

const native::Resonant& resonant(const native::Roots& roots) {
  const auto* value = std::get_if<native::Resonant>(&roots);
  CHECK(value != nullptr);
  return *value;
}

}  // namespace

TRENCH_TEST(document_roundtrip_is_lossless_and_deterministic) {
  EditorState state;
  shape(state);
  QTemporaryDir dir;
  CHECK(dir.isValid());

  const QString a = dir.filePath(QStringLiteral("a.trenchbody"));
  CHECK(trench::app::saveDocument(state.document(), a).isEmpty());

  QString error;
  const auto loaded = trench::app::loadDocument(a, &error);
  CHECK(loaded.has_value());
  CHECK(error.isEmpty());
  CHECK(*loaded == state.document());

  EditorState second;
  second.setDocument(*loaded);
  const QString b = dir.filePath(QStringLiteral("b.trenchbody"));
  CHECK(trench::app::saveDocument(second.document(), b).isEmpty());
  CHECK(readAll(a) == readAll(b));
  CHECK(!readAll(a).isEmpty());
  CHECK(second.packed().legacy_bytes() == state.packed().legacy_bytes());
}

TRENCH_TEST(document_load_restores_editable_state_not_overlay) {
  EditorState state;
  shape(state);
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("kept.trenchbody"));
  CHECK(trench::app::saveDocument(state.document(), path).isEmpty());

  QString error;
  const auto loaded = trench::app::loadDocument(path, &error);
  CHECK(loaded.has_value());
  EditorState restored;
  restored.setDocument(*loaded);

  for (std::size_t corner = 0; corner < native::kCorners; ++corner) {
    for (std::size_t index = 0; index < native::kSections; ++index) {
      CHECK(restored.sectionEnabledAt(corner, index) ==
            state.sectionEnabledAt(corner, index));
      CHECK(restored.zeroPresentAt(corner, index) ==
            state.zeroPresentAt(corner, index));
      const auto& want = state.sectionAt(corner, index);
      const auto& got = restored.sectionAt(corner, index);
      CHECK(resonant(got.pole).hz == resonant(want.pole).hz);
      CHECK(resonant(got.pole).bw_hz == resonant(want.pole).bw_hz);
      CHECK(resonant(got.zero).hz == resonant(want.zero).hz);
      CHECK(resonant(got.zero).bw_hz == resonant(want.zero).bw_hz);
    }
  }
  CHECK(restored.editingCorner() == state.editingCorner());
  CHECK(restored.morphPos() == state.morphPos());
  CHECK(restored.qPos() == state.qPos());
}

TRENCH_TEST(document_rejects_malformed) {
  EditorState state;
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString good = dir.filePath(QStringLiteral("good.trenchbody"));
  CHECK(trench::app::saveDocument(state.document(), good).isEmpty());
  const QByteArray original = readAll(good);
  CHECK(!original.isEmpty());
  const QJsonObject root = QJsonDocument::fromJson(original).object();

  const auto reject = [&dir](const QString& name, const QByteArray& bytes) {
    const QString path = dir.filePath(name);
    writeAll(path, bytes);
    QString error;
    const auto loaded = trench::app::loadDocument(path, &error);
    CHECK(!loaded.has_value());
    CHECK(!error.isEmpty());
  };

  reject(QStringLiteral("junk.trenchbody"),
         QByteArrayLiteral("{ this is not json"));

  QJsonObject other = root;
  other[QStringLiteral("format")] = QStringLiteral("other");
  reject(QStringLiteral("format.trenchbody"), QJsonDocument(other).toJson());

  QJsonObject version = root;
  version[QStringLiteral("version")] = 2;
  reject(QStringLiteral("version.trenchbody"), QJsonDocument(version).toJson());

  QJsonObject three = root;
  QJsonArray shortened = root.value(QStringLiteral("corners")).toArray();
  shortened.removeLast();
  three[QStringLiteral("corners")] = shortened;
  reject(QStringLiteral("three.trenchbody"), QJsonDocument(three).toJson());

  QJsonObject textual = root;
  QJsonArray corners = root.value(QStringLiteral("corners")).toArray();
  QJsonObject corner = corners.at(0).toObject();
  QJsonArray sections = corner.value(QStringLiteral("sections")).toArray();
  QJsonObject section = sections.at(0).toObject();
  QJsonObject pole = section.value(QStringLiteral("pole")).toObject();
  pole[QStringLiteral("hz")] = QStringLiteral("nan");
  section[QStringLiteral("pole")] = pole;
  sections[0] = section;
  corner[QStringLiteral("sections")] = sections;
  corners[0] = corner;
  textual[QStringLiteral("corners")] = corners;
  reject(QStringLiteral("textual.trenchbody"), QJsonDocument(textual).toJson());

  const QString missing =
      dir.filePath(QStringLiteral("no-such-folder/x.trenchbody"));
  CHECK(!trench::app::saveDocument(state.document(), missing).isEmpty());
  CHECK(readAll(good) == original);
}

TRENCH_TEST(import_classification_by_extension) {
  CHECK(trench::app::classify_import("x.WAV") == ImportKind::kSound);
  CHECK(trench::app::classify_import("x.aiff") == ImportKind::kSound);
  CHECK(trench::app::classify_import("x.flac") == ImportKind::kSound);
  CHECK(trench::app::classify_import("x.fbw") == ImportKind::kPoleMaterial);
  CHECK(trench::app::classify_import("x.csv") == ImportKind::kResponseTable);
  CHECK(trench::app::classify_import("x.TXT") == ImportKind::kResponseTable);
  CHECK(trench::app::classify_import("x.trenchbody") == ImportKind::kDocument);
  CHECK(trench::app::classify_import("x.body240") == ImportKind::kPackedBody);
  CHECK(trench::app::classify_import("x.bin") == ImportKind::kPackedBody);
  CHECK(trench::app::classify_import("x.par") == ImportKind::kUnknown);
  CHECK(trench::app::classify_import("x") == ImportKind::kUnknown);
}

TRENCH_TEST(positive_response_table_is_not_pole_material) {
  QTemporaryDir dir;
  CHECK(dir.isValid());

  const QString curve_path = dir.filePath(QStringLiteral("curve.csv"));
  writeAll(curve_path,
           QByteArrayLiteral("20 3.0\n100 2.5\n1000 1.0\n10000 0.5\n"));
  CHECK(trench::app::classify_import(native_path(curve_path)) ==
        ImportKind::kResponseTable);
  const auto curve = trench::app::read_response_curve(native_path(curve_path));
  CHECK(curve.has_value());
  CHECK(curve->frequency_hz.size() == 4);
  CHECK(curve->magnitude_db.size() == 4);

  const QString rew_path = dir.filePath(QStringLiteral("rew.txt"));
  writeAll(rew_path,
           QByteArrayLiteral("* Measurement data exported by REW\n"
                             "* Freq(Hz) SPL(dB) Phase(degrees)\n"
                             "20.000 71.2 -12.4\n"
                             "100.000 74.9 8.1\n"
                             "1000.000 78.0 -95.3\n"));
  const auto rew = trench::app::read_response_curve(native_path(rew_path));
  CHECK(rew.has_value());
  CHECK(rew->frequency_hz.size() == 3);
  CHECK(rew->magnitude_db[2] == 78.0);
  CHECK(curve->frequency_hz[0] == 20.0);
  CHECK(curve->frequency_hz[3] == 10'000.0);
  CHECK(curve->magnitude_db[0] == 3.0);
  CHECK(curve->magnitude_db[1] == 2.5);
  CHECK(curve->magnitude_db[2] == 1.0);
  CHECK(curve->magnitude_db[3] == 0.5);

  const QString jumbled = dir.filePath(QStringLiteral("jumbled.csv"));
  writeAll(jumbled, QByteArrayLiteral("20 3.0\n1000 2.0\n100 1.0\n"));
  CHECK(!trench::app::read_response_curve(native_path(jumbled)).has_value());

  const QString junk = dir.filePath(QStringLiteral("junk.csv"));
  writeAll(junk, QByteArrayLiteral("20 3.0\nhello there\n100 1.0\n"));
  CHECK(!trench::app::read_response_curve(native_path(junk)).has_value());
}

TRENCH_TEST(pole_material_rejects_negative_rows) {
  QTemporaryDir dir;
  CHECK(dir.isValid());

  const QString negative = dir.filePath(QStringLiteral("poles.fbw"));
  writeAll(negative, QByteArrayLiteral("500 80\n1500 -20\n"));
  CHECK(trench::app::classify_import(native_path(negative)) ==
        ImportKind::kPoleMaterial);
  CHECK(!trench::app::read_pole_material(native_path(negative)).has_value());

  const QString named = dir.filePath(QStringLiteral("named.fbw"));
  writeAll(named, QByteArrayLiteral("# name\n500 80\n1500 120\n"));
  CHECK(trench::app::classify_import(native_path(named)) ==
        ImportKind::kPoleMaterial);
  const auto rows = trench::app::read_pole_material(native_path(named));
  CHECK(rows.has_value());
  CHECK(rows->size() == 2);
  CHECK((*rows)[0].first == 500.0);
  CHECK((*rows)[0].second == 80.0);
  CHECK((*rows)[1].first == 1'500.0);
  CHECK((*rows)[1].second == 120.0);
}
