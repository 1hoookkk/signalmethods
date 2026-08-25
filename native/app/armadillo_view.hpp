#pragma once

#include "trench/core/native_body.hpp"
#include "trench/core/formants.hpp"

#include <QWidget>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

class QMenu;
class QPainter;
class QToolButton;

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

  void setBody(const trench::core::native::Body* body, double sample_rate_hz);
  void setCorner(std::size_t corner);
  void setSelected(std::optional<std::size_t> section, bool zero);
  void addBodyOverlay(const QString& name, const trench::core::native::Body& body);
  bool setOverlay(const QString& name);
  void refresh();

  [[nodiscard]] const std::vector<Marker>& markers() const noexcept;
  [[nodiscard]] double xForFrequency(double hz) const;
  [[nodiscard]] double yForRadius(double radius) const;
  [[nodiscard]] double frequencyForX(double x) const;
  [[nodiscard]] double radiusForY(double y) const;
  [[nodiscard]] QMenu* overlayMenu() const noexcept;
  [[nodiscard]] QToolButton* overlayPicker() const noexcept;
  [[nodiscard]] QString overlay() const;
  [[nodiscard]] std::size_t overlayGhostCount() const noexcept;
  [[nodiscard]] std::size_t overlayPoleCount() const noexcept;
  [[nodiscard]] std::size_t overlayZeroCount() const noexcept;
  [[nodiscard]] std::optional<trench::core::native::Roots> overlayRoot(
      std::size_t section, bool zero) const;
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] QSize minimumSizeHint() const override;

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
  void buildOverlayPicker();
  void paintOverlay(QPainter& painter) const;

  struct OverlayRoot {
    std::size_t section{};
    bool zero{};
    trench::core::native::Roots roots;
  };

  struct CornerOverlay {
    QString name;
    trench::core::native::Corner corner;
  };

  const trench::core::native::Body* body_{};
  double sample_rate_hz_{44100.0};
  std::size_t corner_{};
  std::vector<Marker> markers_;
  std::optional<std::size_t> selected_section_;
  bool selected_zero_{};
  std::optional<std::pair<std::size_t, bool>> drag_;
  QMenu* overlay_menu_{};
  QMenu* body_overlay_menu_{};
  QToolButton* overlay_button_{};
  QString overlay_name_;
  std::vector<CornerOverlay> corner_overlays_;
  std::vector<OverlayRoot> overlay_roots_;
};
