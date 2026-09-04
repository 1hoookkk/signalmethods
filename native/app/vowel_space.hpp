#pragma once

#include "pole_templates.hpp"

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <array>
#include <functional>
#include <utility>
#include <vector>

class NumberBox;

class VowelSpace final : public QWidget {
 public:
  struct Mark {
    QString label;
    QString name;
    bool man{};
    double f1{};
    double f2{};
    double f3{};
  };

  static constexpr double kF1Top = 200.0;
  static constexpr double kF1Bottom = 850.0;
  static constexpr double kF2TopLeft = 2600.0;
  static constexpr double kF2TopRight = 500.0;
  static constexpr double kF2BottomLeft = 1800.0;
  static constexpr double kF2BottomRight = 880.0;
  static constexpr double kSnapPixels = 6.0;
  static constexpr double kSlant = 0.28;

  explicit VowelSpace(QWidget* parent = nullptr);

  [[nodiscard]] QPointF pointFor(double f1, double f2) const;
  [[nodiscard]] std::pair<double, double> formantsAt(QPointF point) const;
  void setFormants(double f1, double f2);
  [[nodiscard]] std::array<double, 4> formants() const;
  [[nodiscard]] trench::app::Keyframe frame() const;
  [[nodiscard]] int markCount() const;
  [[nodiscard]] const std::vector<Mark>& marks() const noexcept { return marks_; }
  void pickForTest(bool partner);
  void hoverForTest(double f1, double f2);

  std::function<void(const trench::app::Keyframe&)> onHover;
  std::function<void()> onHoverLeave;
  std::function<void(const trench::app::Keyframe&, bool)> onPick;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  [[nodiscard]] QRectF chartRect() const;
  [[nodiscard]] double f2Left(double v) const;
  [[nodiscard]] double f2Right(double v) const;
  [[nodiscard]] double leftEdgeX(double v, const QRectF& chart) const;
  [[nodiscard]] double f3For(double f1, double f2) const;
  [[nodiscard]] int markNear(const QPointF& point) const;
  void moveTo(const QPointF& point, bool snap);
  void applyCell(int cell, double value);
  void refreshCells();
  void hear();
  void pick(bool partner);
  void placeCells();

  std::vector<Mark> marks_;
  std::array<NumberBox*, 4> cells_{};
  double f1_{500.0};
  double f2_{1500.0};
  double f3_override_{};
  double f4_override_{};
  QString cursor_name_;
  bool refreshing_{};
};
