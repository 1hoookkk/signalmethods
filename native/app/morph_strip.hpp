#pragma once

#include <QWidget>

#include <array>

class QPushButton;
class QSlider;

class MorphStrip final : public QWidget {
  Q_OBJECT

 public:
  explicit MorphStrip(QWidget* parent = nullptr);

  void setCorner(std::size_t corner);
  void setView(float morph, float q);
  [[nodiscard]] float morph() const;
  [[nodiscard]] float q() const;
  [[nodiscard]] double character() const;
  void setTranspose(int semitones);
  [[nodiscard]] int transpose() const;
  void setWorstStepDb(double db);
  [[nodiscard]] double worstStepDb() const;

 signals:
  void cornerPicked(int corner);
  void cornerCopyRequested(int corner);
  void cornerMenuRequested(int corner, const QPoint& at);
  void viewEdited(float morph, float q);
  void transposeEdited(int semitones);
  void characterEdited(double amount);
  void characterGestureStarted();
  void characterGestureFinished();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  std::array<QPushButton*, 4> corners_{};
  QSlider* morph_{};
  QSlider* q_{};
  QSlider* character_{};
  QSlider* transpose_{};
  double worst_step_db_{};
  bool updating_{};
};
