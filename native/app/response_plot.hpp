#pragma once

#include "trench/core/packed_body.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class QPainter;

class ResponsePlotWidget final : public QWidget {
  Q_OBJECT

 public:
  enum class Lane { kPole, kZero };
  Q_ENUM(Lane)

  struct TokenInfo {
    std::size_t section{};
    Lane lane{Lane::kPole};
    QPointF position;
    bool live{};
    bool pinned{};
  };

  explicit ResponsePlotWidget(QWidget* parent = nullptr);

  void setBody(const trench::core::PackedBody* body, double sample_rate_hz,
               std::string source_label);
  void setFreedomMask(std::uint32_t mask);
  void refresh();

  [[nodiscard]] std::size_t responsePointCount() const noexcept;
  [[nodiscard]] double frequencyAt(std::size_t index) const;
  [[nodiscard]] double responseDbAt(std::size_t index) const;
  [[nodiscard]] std::vector<TokenInfo> tokens() const;
  [[nodiscard]] bool refusalVisible() const noexcept;

 signals:
  void gestureStarted(std::size_t section);
  void gestureFinished(std::size_t section);
  void sectionEdited(std::size_t section, const trench::core::PackedSection& words);
  void pinToggled(std::size_t section, ResponsePlotWidget::Lane lane);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  [[nodiscard]] QRectF plotRect() const;
  [[nodiscard]] std::pair<double, double> dbRange() const;
  [[nodiscard]] std::optional<TokenInfo> hit(const QPointF& at) const;
  void moveTo(const QPointF& at);
  void refuse(double frequency_hz);
  void ensureTrace(const QRectF& plot, double low_db, double high_db);
  void strokeTrace(QPainter& painter, const QColor& colour);

  const trench::core::PackedBody* body_{};
  double sample_rate_hz_{trench::core::kP2kDatumHz};
  std::uint32_t freedom_mask_{0xFFFFFFFFU};
  std::vector<double> frequencies_hz_;
  std::vector<double> response_db_;
  QString source_label_;

  bool pressed_{};
  bool moved_{};
  bool dragging_{};
  std::size_t press_section_{};
  Lane press_lane_{Lane::kPole};
  QPointF press_position_;
  trench::core::PackedSection origin_words_{};
  std::optional<std::pair<double, double>> latched_db_;

  bool refusal_active_{};
  double refusal_hz_{};
  QElapsedTimer refusal_age_;

  std::uint64_t body_revision_{};
  QPainterPath trace_path_;
  std::optional<std::tuple<double, double, double, double, std::uint64_t>> trace_key_;
  QImage trace_image_;
  QColor trace_image_color_;
};
