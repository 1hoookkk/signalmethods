#include "body_io.hpp"

#include "trench/core/packed_body.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <variant>

namespace trench::app {
namespace {

using Resonant = trench::core::native::Resonant;
using RealRoots = trench::core::native::RealRoots;
using Roots = trench::core::native::Roots;

QString refuseWrite(const QString& path) {
  return QStringLiteral("WRITE FAILED · %1")
      .arg(QFileInfo(path).fileName().toUpper());
}

QString writeAtomically(const QString& path, const QByteArray& bytes) {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return refuseWrite(path);
  if (file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    return refuseWrite(path);
  }
  if (!file.commit()) return refuseWrite(path);
  return {};
}

QJsonObject rootJson(const Roots& root) {
  QJsonObject object;
  if (const auto* tone = std::get_if<Resonant>(&root)) {
    object[QStringLiteral("hz")] = tone->hz;
    object[QStringLiteral("bw_hz")] = tone->bw_hz;
    return object;
  }
  const auto& real = std::get<RealRoots>(root);
  object[QStringLiteral("a_hz")] = real.a_hz;
  object[QStringLiteral("b_hz")] = real.b_hz;
  return object;
}

bool finiteRoot(const Roots& root) {
  if (const auto* tone = std::get_if<Resonant>(&root)) {
    return std::isfinite(tone->hz) && std::isfinite(tone->bw_hz);
  }
  const auto& real = std::get<RealRoots>(root);
  return std::isfinite(real.a_hz) && std::isfinite(real.b_hz);
}

bool readFinite(const QJsonValue& value, double* out) {
  if (!value.isDouble()) return false;
  const double number = value.toDouble();
  if (!std::isfinite(number)) return false;
  *out = number;
  return true;
}

bool readRoot(const QJsonValue& value, Roots* out) {
  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  double a = 0.0;
  double b = 0.0;
  if (readFinite(object.value(QStringLiteral("a_hz")), &a) &&
      readFinite(object.value(QStringLiteral("b_hz")), &b)) {
    *out = RealRoots{a, b};
    return true;
  }
  if (!readFinite(object.value(QStringLiteral("hz")), &a)) return false;
  if (!readFinite(object.value(QStringLiteral("bw_hz")), &b)) return false;
  if (!(a >= 0.0) || !(b >= 0.0)) return false;
  *out = Resonant{a, b};
  return true;
}

}

QString saveBody240(const EditorState& state, const QString& path) {
  const auto& packed = state.packed();
  if (!packed.is_legacy_representable()) {
    return QStringLiteral("NOT A 240-BYTE BODY");
  }
  const auto bytes = packed.legacy_bytes();
  return writeAtomically(
      path, QByteArray(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<qsizetype>(bytes.size())));
}

QString saveDocument(const EditorState::Document& document, const QString& path) {
  QJsonArray corners;
  for (const auto& state : document.corners) {
    QJsonArray sections;
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      const auto& authored = state.corner.sections[index];
      if (!finiteRoot(authored.pole) || !finiteRoot(authored.zero)) {
        return QStringLiteral("NOT A FINITE DOCUMENT");
      }
      QJsonObject zero_object = rootJson(authored.zero);
      zero_object[QStringLiteral("present")] = state.zero_present[index];
      QJsonObject section;
      section[QStringLiteral("enabled")] = state.enabled[index];
      section[QStringLiteral("pole")] = rootJson(authored.pole);
      section[QStringLiteral("zero")] = zero_object;
      section[QStringLiteral("cut")] = state.cut[index];
      sections.append(section);
    }
    QJsonObject corner;
    corner[QStringLiteral("sections")] = sections;
    corners.append(corner);
  }
  QJsonObject root;
  root[QStringLiteral("format")] = QStringLiteral("trenchbody");
  root[QStringLiteral("version")] = 1;
  root[QStringLiteral("editing_corner")] =
      static_cast<int>(document.editing_corner);
  root[QStringLiteral("morph")] = document.morph;
  root[QStringLiteral("q")] = document.q;
  root[QStringLiteral("morph_axis")] = document.morph_axis;
  root[QStringLiteral("q_axis")] = document.q_axis;
  root[QStringLiteral("corners")] = corners;
  return writeAtomically(path,
                         QJsonDocument(root).toJson(QJsonDocument::Indented));
}

std::optional<EditorState::Document> loadDocument(const QString& path,
                                                  QString* error) {
  const auto refuse = [error](const QString& reason) {
    if (error != nullptr) *error = reason;
    return std::optional<EditorState::Document>{};
  };

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return refuse(QStringLiteral("UNREADABLE"));
  QJsonParseError parse{};
  const QJsonDocument parsed = QJsonDocument::fromJson(file.readAll(), &parse);
  if (parse.error != QJsonParseError::NoError || !parsed.isObject()) {
    return refuse(QStringLiteral("BAD JSON"));
  }
  const QJsonObject root = parsed.object();

  if (root.value(QStringLiteral("format")).toString() !=
      QStringLiteral("trenchbody")) {
    return refuse(QStringLiteral("BAD FORMAT"));
  }
  double version = 0.0;
  if (!readFinite(root.value(QStringLiteral("version")), &version) ||
      version != 1.0) {
    return refuse(QStringLiteral("BAD VERSION"));
  }

  EditorState::Document document{};
  double editing_corner = 0.0;
  if (!readFinite(root.value(QStringLiteral("editing_corner")), &editing_corner) ||
      editing_corner != std::floor(editing_corner) || editing_corner < 0.0 ||
      editing_corner >= static_cast<double>(trench::core::native::kCorners)) {
    return refuse(QStringLiteral("EDITING CORNER OUT OF RANGE"));
  }
  document.editing_corner = static_cast<std::size_t>(editing_corner);
  if (!readFinite(root.value(QStringLiteral("morph")), &document.morph)) {
    return refuse(QStringLiteral("NON-FINITE MORPH"));
  }
  if (!readFinite(root.value(QStringLiteral("q")), &document.q)) {
    return refuse(QStringLiteral("NON-FINITE Q"));
  }
  document.morph_axis = root.value(QStringLiteral("morph_axis")).toString();
  document.q_axis = root.value(QStringLiteral("q_axis")).toString();

  const QJsonValue corners_value = root.value(QStringLiteral("corners"));
  if (!corners_value.isArray()) return refuse(QStringLiteral("CORNERS != 4"));
  const QJsonArray corners = corners_value.toArray();
  if (corners.size() !=
      static_cast<qsizetype>(trench::core::native::kCorners)) {
    return refuse(QStringLiteral("CORNERS != 4"));
  }

  for (std::size_t index = 0; index < trench::core::native::kCorners; ++index) {
    const QJsonValue corner_value = corners.at(static_cast<qsizetype>(index));
    if (!corner_value.isObject()) return refuse(QStringLiteral("SECTIONS != 6"));
    const QJsonObject corner = corner_value.toObject();
    auto& state = document.corners[index];
    const QJsonValue sections_value = corner.value(QStringLiteral("sections"));
    if (!sections_value.isArray()) return refuse(QStringLiteral("SECTIONS != 6"));
    const QJsonArray sections = sections_value.toArray();
    if (sections.size() !=
        static_cast<qsizetype>(trench::core::native::kSections)) {
      return refuse(QStringLiteral("SECTIONS != 6"));
    }
    for (std::size_t slot = 0; slot < trench::core::native::kSections; ++slot) {
      const QJsonValue section_value = sections.at(static_cast<qsizetype>(slot));
      if (!section_value.isObject()) return refuse(QStringLiteral("BAD SECTION"));
      const QJsonObject section = section_value.toObject();
      const QJsonValue enabled = section.value(QStringLiteral("enabled"));
      if (!enabled.isBool()) return refuse(QStringLiteral("BAD ENABLED"));
      Roots pole{};
      if (!readRoot(section.value(QStringLiteral("pole")), &pole)) {
        return refuse(QStringLiteral("NON-FINITE ROOT"));
      }
      const QJsonValue zero_value = section.value(QStringLiteral("zero"));
      Roots zero{};
      if (!readRoot(zero_value, &zero)) {
        return refuse(QStringLiteral("NON-FINITE ROOT"));
      }
      const QJsonValue present =
          zero_value.toObject().value(QStringLiteral("present"));
      if (!present.isBool()) return refuse(QStringLiteral("BAD ZERO PRESENT"));
      const QJsonValue cut_value = section.value(QStringLiteral("cut"));
      double cut = 0.0;
      if (!cut_value.isUndefined() &&
          (!readFinite(cut_value, &cut) || cut != std::floor(cut) || cut < 0.0 ||
           cut > static_cast<double>(EditorState::kMaxCut))) {
        return refuse(QStringLiteral("BAD CUT"));
      }
      state.corner.sections[slot] = {pole, zero, true};
      state.enabled[slot] = enabled.toBool();
      state.zero_present[slot] = present.toBool();
      state.cut[slot] = static_cast<int>(cut);
    }
  }

  if (error != nullptr) error->clear();
  return document;
}

}
