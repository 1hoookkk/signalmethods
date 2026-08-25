#pragma once

#include "trench/core/packed_body.hpp"

#include <QWidget>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

class ArmadilloView final : public QWidget {
  Q_OBJECT

 public:
  struct Marker {
    std::size_t section{};
    bool zero{};
    QPointF position;
    double hz{};
    double radius{};
    bool real{};
    bool ghost{};
  };

  explicit ArmadilloView(QWidget* parent = nullptr);

  void setBody(const trench::core::PackedBody* body, double sample_rate_hz);
  void setCorner(std::size_t corner);
  void setSelected(std::optional<std::size_t> section, bool zero);
  void refresh();

  [[nodiscard]] const std::vector<Marker>& markers() const noexcept;
  [[nodiscard]] double xForFrequency(double hz) const;
  [[nodiscard]] double yForRadius(double radius) const;
  [[nodiscard]] double frequencyForX(double x) const;
  [[nodiscard]] double radiusForY(double y) const;

 signals:
  void rootPressed(std::size_t section, bool zero);
  void rootDragged(std::size_t section, bool zero, double hz, double radius);
  void rootReleased();
  void zeroParked(std::size_t section);
  void poleParked(std::size_t section);
  void placeRequested(double hz, double radius);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  [[nodiscard]] QRectF plane() const;
  [[nodiscard]] std::optional<std::size_t> hitMarker(const QPointF& at) const;
  void rebuildMarkers();

  const trench::core::PackedBody* body_{};
  double sample_rate_hz_{44100.0};
  std::size_t corner_{};
  std::vector<Marker> markers_;
  std::optional<std::size_t> selected_section_;
  bool selected_zero_{};
  std::optional<std::pair<std::size_t, bool>> drag_;
};
