#include "body_io.hpp"

#include "trench/core/packed_body.hpp"

#include <QByteArray>
#include <QFileInfo>
#include <QSaveFile>

namespace trench::app {
namespace {

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

}  // namespace trench::app
