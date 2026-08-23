#pragma once

#include "trench/core/bisection.hpp"

#include <QWidget>

#include <filesystem>
#include <optional>

class MainWindow;
class QTimer;

class BisectRoom final : public QWidget {
  Q_OBJECT

 public:
  enum class Axis { kMorph, kQ, kCharacter };

  [[nodiscard]] static std::optional<Axis> axisFromName(const QString& name);

  BisectRoom(MainWindow* window, Axis axis, std::filesystem::path body_path,
             QWidget* parent = nullptr);

  [[nodiscard]] std::filesystem::path curvePath() const;

 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  void beginTrial();
  void driveTo(double value);
  void nudge(double fraction);
  void finish();

  MainWindow* window_{};
  Axis axis_{};
  std::filesystem::path body_path_;
  trench::core::bisect::Session session_;
  double candidate_{};
  bool done_{};
  double pulse_{};
  QTimer* pulse_timer_{};
};
