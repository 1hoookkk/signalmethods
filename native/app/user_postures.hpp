#pragma once

#include <QList>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <vector>

class UserPostures final {
 public:
  struct Pole {
    std::size_t row{};
    std::uint16_t mag{};
    std::uint16_t rsq{};
  };

  struct Entry {
    QString name;
    std::vector<Pole> poles;
  };

  static void setBaseDir(const QString& dir);
  static QString baseDir();

  UserPostures();

  void reload();
  [[nodiscard]] const QList<Entry>& entries() const noexcept;
  [[nodiscard]] const Entry* find(const QString& name) const;
  QString keep(std::vector<Pole> poles);

 private:
  [[nodiscard]] QString filePath() const;
  bool write() const;

  QList<Entry> entries_;
};
