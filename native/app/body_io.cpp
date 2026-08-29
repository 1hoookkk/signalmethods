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

QJsonObject rootJson(const Resonant& root) {
  QJsonObject object;
  object[QStringLiteral("hz")] = root.hz;
  object[QStringLiteral("bw_hz")] = root.bw_hz;
  return object;
}

bool readFinite(const QJsonValue& value, double* out) {
  if (!value.isDouble()) return false;
  const double number = value.toDouble();
  if (!std::isfinite(number)) return false;
  *out = number;
  return true;
}

bool readRoot(const QJsonValue& value, Resonant* out) {
  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  double hz = 0.0;
  double bw_hz = 0.0;
  if (!readFinite(object.value(QStringLiteral("hz")), &hz)) return false;
  if (!readFinite(object.value(QStringLiteral("bw_hz")), &bw_hz)) return false;
  if (!(hz > 0.0) || !(bw_hz > 0.0)) return false;
  *out = Resonant{hz, bw_hz};
  return true;
}

}  // namespace

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

void applyPeqList(EditorState& state, const PeqList& list) {
  state.loadPoles(list.poles);
  const std::size_t sections =
      std::min(list.poles.size(), trench::core::native::kSections);
  for (const auto& [hz, bw_hz] : list.zeros) {
    std::optional<std::size_t> best;
    double best_distance = 0.0;
    for (std::size_t index = 0; index < sections; ++index) {
      if (state.rootPresent(index, EditorState::Lane::kZero)) continue;
      const double distance = std::abs(std::log2(hz / list.poles[index].first));
      if (!best || distance < best_distance) {
        best = index;
        best_distance = distance;
      }
    }
    if (!best) break;
    state.selectSection(*best);
    state.addZeroAt(hz, bw_hz);
  }
  state.selectSection(0);
}

QString saveDocument(const EditorState::Document& document, const QString& path) {
  QJsonArray corners;
  for (const auto& state : document.corners) {
    QJsonArray sections;
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      const auto& authored = state.corner.sections[index];
      const auto* pole = std::get_if<Resonant>(&authored.pole);
      const auto* zero = std::get_if<Resonant>(&authored.zero);
      if (pole == nullptr || zero == nullptr) {
        return QStringLiteral("NOT A RESONANT DOCUMENT");
      }
      QJsonObject zero_object = rootJson(*zero);
      zero_object[QStringLiteral("present")] = state.zero_present[index];
      QJsonObject section;
      section[QStringLiteral("enabled")] = state.enabled[index];
      section[QStringLiteral("pole")] = rootJson(*pole);
      section[QStringLiteral("zero")] = zero_object;
      sections.append(section);
    }
    QJsonObject corner;
    corner[QStringLiteral("gain_db")] = state.corner.gain_db;
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
    if (!readFinite(corner.value(QStringLiteral("gain_db")),
                    &state.corner.gain_db)) {
      return refuse(QStringLiteral("NON-FINITE GAIN"));
    }
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
      Resonant pole{};
      if (!readRoot(section.value(QStringLiteral("pole")), &pole)) {
        return refuse(QStringLiteral("NON-FINITE ROOT"));
      }
      const QJsonValue zero_value = section.value(QStringLiteral("zero"));
      Resonant zero{};
      if (!readRoot(zero_value, &zero)) {
        return refuse(QStringLiteral("NON-FINITE ROOT"));
      }
      const QJsonValue present =
          zero_value.toObject().value(QStringLiteral("present"));
      if (!present.isBool()) return refuse(QStringLiteral("BAD ZERO PRESENT"));
      state.corner.sections[slot] = {pole, zero, true};
      state.enabled[slot] = enabled.toBool();
      state.zero_present[slot] = present.toBool();
    }
  }

  if (error != nullptr) error->clear();
  return document;
}

}  // namespace trench::app
