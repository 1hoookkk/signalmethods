#include "user_postures.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QString& base_dir() {
  static QString dir = QString::fromUtf8(TRENCH_SOURCE_ROOT) + QStringLiteral("/profiles");
  return dir;
}

}  // namespace

void UserPostures::setBaseDir(const QString& dir) { base_dir() = dir; }

QString UserPostures::baseDir() { return base_dir(); }

UserPostures::UserPostures() { reload(); }

QString UserPostures::filePath() const {
  return base_dir() + QStringLiteral("/user_postures.json");
}

void UserPostures::reload() {
  entries_.clear();
  QFile file(filePath());
  if (!file.open(QIODevice::ReadOnly)) return;
  const auto document = QJsonDocument::fromJson(file.readAll());
  if (!document.isArray()) return;
  for (const auto value : document.array()) {
    const auto object = value.toObject();
    const auto name = object.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) continue;
    Entry entry{name, {}};
    for (const auto row_value : object.value(QStringLiteral("rows")).toArray()) {
      const auto row = row_value.toObject();
      entry.poles.push_back({static_cast<std::size_t>(row.value(QStringLiteral("row")).toInt()),
                             static_cast<std::uint16_t>(row.value(QStringLiteral("mag")).toInt()),
                             static_cast<std::uint16_t>(row.value(QStringLiteral("rsq")).toInt())});
    }
    if (entry.poles.empty()) continue;
    entries_.push_back(std::move(entry));
  }
}

const QList<UserPostures::Entry>& UserPostures::entries() const noexcept { return entries_; }

const UserPostures::Entry* UserPostures::find(const QString& name) const {
  for (const auto& entry : entries_) {
    if (entry.name == name) return &entry;
  }
  return nullptr;
}

QString UserPostures::keep(std::vector<Pole> poles) {
  if (poles.empty()) return {};
  int next = 1;
  for (const auto& entry : entries_) {
    if (!entry.name.startsWith(QStringLiteral("mine "))) continue;
    bool numeric = false;
    const auto index = entry.name.mid(5).toInt(&numeric);
    if (numeric && index >= next) next = index + 1;
  }
  Entry entry{QStringLiteral("mine %1").arg(next), std::move(poles)};
  entries_.push_back(entry);
  write();
  return entry.name;
}

bool UserPostures::write() const {
  QJsonArray array;
  for (const auto& entry : entries_) {
    QJsonArray rows;
    for (const auto& pole : entry.poles) {
      QJsonObject row;
      row.insert(QStringLiteral("row"), static_cast<int>(pole.row));
      row.insert(QStringLiteral("mag"), static_cast<int>(pole.mag));
      row.insert(QStringLiteral("rsq"), static_cast<int>(pole.rsq));
      rows.push_back(row);
    }
    QJsonObject object;
    object.insert(QStringLiteral("name"), entry.name);
    object.insert(QStringLiteral("rows"), rows);
    array.push_back(object);
  }
  QDir().mkpath(base_dir());
  QSaveFile file(filePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
  return file.commit();
}
