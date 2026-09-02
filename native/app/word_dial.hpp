#pragma once

#include <QWidget>

class QLabel;

class WordDial final : public QWidget {
  Q_OBJECT

 public:
  explicit WordDial(QWidget* parent = nullptr);

  void setRange(int minimum, int maximum);
  void setValue(int value);
  [[nodiscard]] int value() const noexcept { return value_; }
  [[nodiscard]] int minimum() const noexcept { return minimum_; }
  [[nodiscard]] int maximum() const noexcept { return maximum_; }
  [[nodiscard]] QLabel* readout() const noexcept { return readout_; }

  [[nodiscard]] QSize sizeHint() const override;

 signals:
  void valueChanged(int value);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  QLabel* readout_{};
  int minimum_{};
  int maximum_{255};
  int value_{};
  int press_y_{};
  int press_value_{};
  bool dragging_{};
};
