#pragma once

#include <QWidget>

class QSlider;

class MorphStrip final : public QWidget {
  Q_OBJECT

 public:
  explicit MorphStrip(QWidget* parent = nullptr);

  void setView(float morph, float q);
  [[nodiscard]] float morph() const;
  [[nodiscard]] float q() const;
  [[nodiscard]] double character() const;
  void setTranspose(int semitones);
  [[nodiscard]] int transpose() const;
  void setWorstStepDb(double db);
  [[nodiscard]] double worstStepDb() const;

 signals:
  void viewEdited(float morph, float q);
  void transposeEdited(int semitones);
  void characterEdited(double amount);
  void characterGestureStarted();
  void characterGestureFinished();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QSlider* morph_{};
  QSlider* q_{};
  QSlider* character_{};
  QSlider* transpose_{};
  double worst_step_db_{};
  bool updating_{};
};
