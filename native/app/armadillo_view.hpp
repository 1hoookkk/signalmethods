#pragma once

#include "trench/core/native_body.hpp"

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

  struct Travel {
    std::size_t section{};
    bool zero{};
    QPointF from;
    QPointF to;
  };

  explicit ArmadilloView(QWidget* parent = nullptr);

  void setBody(const trench::core::native::Body* body, double sample_rate_hz);
  void setCorner(std::size_t corner);
  void setTranspose(double semitones);
  void setSelected(std::optional<std::size_t> section, bool zero);
  void addBodyOverlay(const QString& name, const trench::core::native::Body& body);
  bool setOverlay(const QString& name);
  void setOverlayReveal(bool poles, bool zeros);
  void setLpcFormants(const QString& source, std::vector<double> hz);
  void refresh();

  [[nodiscard]] const std::vector<Marker>& markers() const noexcept;
  [[nodiscard]] const std::vector<Travel>& travels() const noexcept;
  [[nodiscard]] std::size_t fromCorner() const noexcept;
  [[nodiscard]] std::size_t toCorner() const noexcept;
  [[nodiscard]] std::optional<std::size_t> selectedSection() const noexcept {
    return selected_section_;
  }
  [[nodiscard]] double xForFrequency(double hz) const;
  [[nodiscard]] double yForRadius(double radius) const;
  [[nodiscard]] double frequencyForX(double x) const;
  [[nodiscard]] double radiusForY(double y) const;
  [[nodiscard]] QMenu* overlayMenu() const noexcept;
  [[nodiscard]] QToolButton* overlayPicker() const noexcept;
  [[nodiscard]] QString overlay() const;
  [[nodiscard]] QString overlayName() const noexcept { return overlay_name_; }
  [[nodiscard]] bool overlayRevealsPoles() const noexcept { return reveal_poles_; }
  [[nodiscard]] bool overlayRevealsZeros() const noexcept { return reveal_zeros_; }
  [[nodiscard]] std::size_t overlayGhostCount() const noexcept;
  [[nodiscard]] std::size_t overlayPoleCount() const noexcept;
  [[nodiscard]] std::size_t overlayZeroCount() const noexcept;
  [[nodiscard]] std::optional<trench::core::native::Roots> overlayRoot(
      std::size_t section, bool zero) const;
  [[nodiscard]] std::optional<QPointF> transposedPosition(std::size_t section,
                                                         bool zero) const;
  [[nodiscard]] std::size_t lpcFormantCount() const noexcept;
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] QSize minimumSizeHint() const override;

 signals:
  void rootPressed(std::size_t section, bool zero);
  void rootDragged(std::size_t section, bool zero, double hz, double radius);
  void rootReleased();
  void zeroParked(std::size_t section);
  void poleParked(std::size_t section);
  void placeRequested(double hz, double radius);
  void overlayChosen(const QString& name);

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
  void rebuildTravels();
  void paintTravel(QPainter& painter) const;
  void buildOverlayPicker();
  void chooseOverlay(const QString& name);
  void paintOverlay(QPainter& painter) const;
  void paintLpcFormants(QPainter& painter) const;
  [[nodiscard]] double transposedHz(double hz, double radius, bool zero) const;

  struct OverlayRoot {
    std::size_t section{};
    bool zero{};
    trench::core::native::Roots roots;
  };

  [[nodiscard]] bool revealed(const OverlayRoot& root) const noexcept;

  struct CornerOverlay {
    QString name;
    trench::core::native::Corner corner;
  };

  const trench::core::native::Body* body_{};
  double sample_rate_hz_{44100.0};
  double transpose_semitones_{};
  std::size_t corner_{};
  std::vector<Marker> markers_;
  std::vector<Travel> travels_;
  std::optional<std::size_t> selected_section_;
  bool selected_zero_{};
  std::optional<std::pair<std::size_t, bool>> drag_;
  double drag_hz_{};
  QMenu* overlay_menu_{};
  QMenu* body_overlay_menu_{};
  QToolButton* overlay_button_{};
  QString overlay_name_;
  std::vector<CornerOverlay> corner_overlays_;
  std::vector<OverlayRoot> overlay_roots_;
  bool reveal_poles_{true};
  bool reveal_zeros_{true};
  QString lpc_source_;
  std::vector<double> lpc_formants_hz_;
};
