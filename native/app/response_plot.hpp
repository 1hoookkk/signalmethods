#pragma once

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/section_param.hpp"

#include "trench/core/formants.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class QMenu;
class QPainter;
class QToolButton;

class ResponsePlotWidget final : public QWidget {
  Q_OBJECT

 public:
  enum class Lane { kPole, kZero };
  Q_ENUM(Lane)

  struct TokenInfo {
    std::size_t section{};
    Lane lane{Lane::kPole};
    QPointF position;
    double radius{};
    bool live{};
    bool pinned{};
  };

  explicit ResponsePlotWidget(QWidget* parent = nullptr);

  void setBody(const trench::core::PackedBody* body, double sample_rate_hz,
               std::string source_label);
  void setCorner(std::size_t corner);
  void setSpace(const trench::core::p2k::PerceptualSpace& space);
  void setView(float morph, float q, double semitones = 0.0);
  void setFreedomMask(std::uint32_t mask);
  void setTarget(const std::vector<double>* target);
  void setFitRunning(bool running);
  void setSelectedSection(std::size_t section, Lane lane = Lane::kPole);
  bool setOverlay(const QString& name);
  void setHighlightedSection(std::optional<std::size_t> section);
  void flashLane(std::size_t section);
  void refresh();

  [[nodiscard]] std::size_t responsePointCount() const noexcept;
  [[nodiscard]] double frequencyAt(std::size_t index) const;
  [[nodiscard]] double xForFrequency(double frequency_hz) const;
  [[nodiscard]] std::pair<double, double> dbRange() const;
  [[nodiscard]] double dbForY(double y) const;
  [[nodiscard]] double responseDbAt(std::size_t index) const;
  [[nodiscard]] double runningPeakDb(std::size_t section) const;
  [[nodiscard]] std::size_t residualPointCount() const noexcept;
  [[nodiscard]] double residualDbAt(std::size_t index) const;
  [[nodiscard]] double alignedTargetDbAt(std::size_t index) const;
  [[nodiscard]] std::vector<TokenInfo> tokens() const;
  [[nodiscard]] std::vector<double> exposedDb() const;
  [[nodiscard]] std::size_t primitiveCount() const noexcept;
  [[nodiscard]] std::optional<std::size_t> highlightedSection() const noexcept;
  [[nodiscard]] QMenu* overlayMenu() const noexcept;
  [[nodiscard]] QToolButton* overlayPicker() const noexcept;
  [[nodiscard]] QString overlay() const;
  [[nodiscard]] std::size_t overlayGhostCount() const noexcept;

 signals:
  void pinToggled(std::size_t section, ResponsePlotWidget::Lane lane);
  void tokenSelected(std::size_t section, ResponsePlotWidget::Lane lane);
  void tokenHovered(std::optional<std::size_t> section);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  [[nodiscard]] QRectF plotRect() const;
  [[nodiscard]] trench::core::Cascade viewCascade() const;
  [[nodiscard]] double viewRatio() const;
  [[nodiscard]] double viewHz(double hz, double radius, bool zero) const;
  [[nodiscard]] double authorHzForX(double x, const QRectF& plot) const;
  [[nodiscard]] double contributionAt(std::size_t section, double hz) const;
  [[nodiscard]] double responseDbAtHz(double hz) const;
  [[nodiscard]] std::optional<TokenInfo> hit(const QPointF& at) const;
  void ensureTrace(const QRectF& plot, double low_db, double high_db);
  void strokeTrace(QPainter& painter, const QColor& colour);
  void rebuildResidual();
  void rebuildExposed();
  void buildOverlayPicker();
  void placeOverlayPicker();
  void paintOverlay(QPainter& painter, const QRectF& plot) const;

  const trench::core::PackedBody* body_{};
  std::size_t corner_{};
  float view_morph_{0.0F};
  float view_q_{0.0F};
  double view_semitones_{0.0};
  bool at_corner_{true};
  double sample_rate_hz_{trench::core::kP2kDatumHz};
  trench::core::p2k::PerceptualSpace space_{};
  std::uint32_t freedom_mask_{0xFFFFFFFFU};
  std::size_t selected_section_{};
  Lane selected_lane_{Lane::kPole};
  std::optional<std::size_t> highlight_section_;
  std::optional<std::size_t> last_hover_section_;
  std::vector<double> frequencies_hz_;
  std::vector<double> response_db_;
  std::array<double, trench::core::kLegacySectionCount> running_peak_db_{};
  std::vector<double> residual_db_;
  std::vector<double> aligned_target_db_;
  std::vector<double> exposed_db_;
  std::vector<std::pair<std::size_t, std::vector<double>>> primitives_;
  double residual_span_db_{1.0};
  QString source_label_;

  std::vector<std::vector<double>> contributions_;
  bool pressed_{};
  bool moved_{};
  bool pin_emitted_{};
  std::size_t press_section_{};
  Lane press_lane_{Lane::kPole};
  QPointF press_position_;

  std::vector<double> target_db_;
  bool fit_running_{};
  std::optional<std::size_t> flash_section_;
  QElapsedTimer flash_age_;

  QMenu* overlay_menu_{};
  QToolButton* overlay_button_{};
  QString overlay_name_;
  std::vector<trench::core::p2k::Formant> overlay_poles_;

  std::uint64_t body_revision_{};
  QPainterPath trace_path_;
  std::optional<std::tuple<double, double, double, double, std::uint64_t>> trace_key_;
  QImage trace_image_;
  QColor trace_image_color_;
};
