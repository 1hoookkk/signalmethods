#pragma once

#include <QString>
#include <QWidget>

#include <functional>

inline constexpr double kThrowPixels = 120.0;

class GestureDial final : public QWidget {
 public:
  GestureDial(QString label, double units_per_pixel,
              std::function<QString(double total)> formatter,
              QWidget* parent = nullptr);

  std::function<void(double delta)> onDelta;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

 private:
  QString label_;
  double units_per_pixel_{};
  std::function<QString(double)> formatter_;
  double total_{};
  double deflect_ = 0.0;
  double last_y_{};
  bool dragging_{};
};
