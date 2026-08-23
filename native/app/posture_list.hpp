#pragma once

#include <QList>
#include <QListWidget>
#include <QString>
#include <QStringList>

class PostureList final : public QListWidget {
  Q_OBJECT

 public:
  struct Group {
    QString type;
    QStringList names;
  };

  explicit PostureList(QWidget* parent = nullptr);

  void setGroups(QList<Group> groups);
  void setMatched(const QString& name);

  [[nodiscard]] QString matched() const;
  [[nodiscard]] int rowOf(const QString& name) const;

 signals:
  void postureChosen(const QString& name);

 private:
  void paintRows();

  QString matched_;
};
