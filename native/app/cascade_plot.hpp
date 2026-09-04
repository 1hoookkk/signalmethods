#pragma once

#include "trench/core/native_body.hpp"

#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

class CascadePlot final : public QWidget {
 public:
  struct View {
    trench::core::Cascade pad{};
    std::array<bool, trench::core::native::kSections> enabled{};
    std::array<bool, trench::core::native::kSections> peaked{};
    std::array<double, trench::core::native::kSections> pole_hz{};
    std::array<double, trench::core::native::kSections> lo_pole_hz{};
    std::array<double, trench::core::native::kSections> hi_pole_hz{};
    std::array<double, trench::core::native::kSections> now_pole_hz{};
    std::vector<double> seed_hz;
    std::size_t selected{};
    bool pad_at_corner{true};
    double sample_rate_hz{44'100.0};
  };

  explicit CascadePlot(QWidget* parent = nullptr);

  void setView(const View& view);
  [[nodiscard]] const std::vector<double>& gridHz() const;
  void setReference(std::vector<double> db_on_grid);
  void clearReference();
  [[nodiscard]] bool hasReference() const;
  [[nodiscard]] std::optional<QPointF> handleCentre(std::size_t section) const;
  [[nodiscard]] double curveDbAt(double hz) const;
  [[nodiscard]] double xForFrequency(double frequency_hz) const;
  [[nodiscard]] int glideCount() const;
  [[nodiscard]] std::optional<double> glideNowHz(std::size_t slot) const;

  std::function<void(std::size_t)> onSelect;
  std::function<void(std::size_t)> onDragBegin;
  std::function<void()> onDragEnd;
  std::function<void(std::size_t, double)> onNote;
  std::function<void(std::size_t, double)> onHeight;
  std::function<void(std::size_t, int)> onRingSteps;
  std::function<void(std::size_t, int)> onWheelRing;
  std::function<void(double, double)> onCreate;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  [[nodiscard]] QRectF plotRect() const;
  [[nodiscard]] double xForFrequency(double frequency_hz, const QRectF& plot) const;
  [[nodiscard]] double yForDb(double db, const QRectF& plot) const;
  [[nodiscard]] double frequencyForX(double x, const QRectF& plot) const;
  [[nodiscard]] double dbForY(double y, const QRectF& plot) const;
  [[nodiscard]] std::size_t handleAt(const QPointF& point) const;
  [[nodiscard]] bool glided(std::size_t slot) const;

  std::vector<double> base_hz_;
  std::vector<double> grid_hz_;
  std::vector<double> pad_db_;
  std::vector<double> reference_db_;
  trench::core::Cascade pad_cascade_{};
  double sample_rate_hz_{44'100.0};
  std::array<bool, trench::core::native::kSections> enabled_{};
  std::array<bool, trench::core::native::kSections> peaked_{};
  std::array<double, trench::core::native::kSections> handle_hz_{};
  std::array<double, trench::core::native::kSections> handle_db_{};
  std::array<double, trench::core::native::kSections> edit_pole_hz_{};
  std::array<double, trench::core::native::kSections> glide_lo_{};
  std::array<double, trench::core::native::kSections> glide_hi_{};
  std::array<double, trench::core::native::kSections> glide_now_{};
  std::size_t selected_{};
  bool pad_at_corner_{true};
  std::size_t drag_{trench::core::native::kSections};
  QPointF press_;
  double press_hz_{};
  double press_pole_hz_{};
};
