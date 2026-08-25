#pragma once

#include "trench/core/fit_target.hpp"
#include "trench/core/native_body.hpp"

#include <QList>
#include <QString>
#include <QWidget>

#include <vector>

class QListWidget;
class QPushButton;

class FitRoom final : public QWidget {
  Q_OBJECT

 public:
  struct Overlay {
    QString name;
    trench::core::FitTarget target;
    std::vector<double> marks_hz;
    std::vector<trench::core::native::Resonant> suggested_poles;
  };

  explicit FitRoom(QWidget* parent = nullptr);

  void setOverlays(QList<Overlay> overlays, int selected);

  [[nodiscard]] int overlayCount() const;
  [[nodiscard]] int selectedOverlay() const;
  [[nodiscard]] QListWidget* overlayList() const noexcept;
  [[nodiscard]] QPushButton* loadButton() const noexcept;

 signals:
  void overlaySelected(int index);
  void overlayRemoved(int index);
  void loadRequested();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  QList<Overlay> overlays_;
  int selected_{-1};
  bool populating_{false};
  QListWidget* list_{};
  QPushButton* load_{};
};
