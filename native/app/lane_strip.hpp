#pragma once

#include "trench/core/native_body.hpp"

#include <QRectF>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <optional>

class QLabel;
class QToolButton;

class LaneStrip final : public QWidget {
  Q_OBJECT

 public:
  static constexpr std::size_t kFrom = 0;
  static constexpr std::size_t kTo = 1;

  explicit LaneStrip(QWidget* parent = nullptr);

  void setBody(const trench::core::native::Body* body, double sample_rate_hz);
  void setCorner(std::size_t corner);
  void setSelected(std::size_t section);
  void setEndpointNames(const QString& from, const QString& to);
  void setFreedomMask(std::uint32_t mask);
  void refresh();

  [[nodiscard]] std::size_t fromCorner() const noexcept;
  [[nodiscard]] std::size_t toCorner() const noexcept;
  [[nodiscard]] std::size_t selected() const noexcept { return selected_section_; }
  [[nodiscard]] QString travelText(std::size_t section) const;
  [[nodiscard]] bool stageLive(std::size_t section) const;
  [[nodiscard]] bool stageLocked(std::size_t section) const;
  [[nodiscard]] QRectF stagePad(std::size_t section) const;
  [[nodiscard]] QRectF lockPad(std::size_t section) const;
  [[nodiscard]] QRectF lane(std::size_t section) const;

 signals:
  void laneSelected(std::size_t section, std::size_t corner);
  void stageToggled(std::size_t section, bool on);
  void lockToggled(std::size_t section);
  void endpointPicked(std::size_t corner);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  [[nodiscard]] std::optional<trench::core::native::Resonant> poleAt(
      std::size_t section, std::size_t end) const;

  const trench::core::native::Body* body_{};
  double sample_rate_hz_{44100.0};
  std::size_t corner_{};
  std::size_t selected_section_{};
  std::uint32_t freedom_mask_{0xFFFFFFFFU};
  QToolButton* pad_from_{};
  QToolButton* pad_to_{};
  QLabel* row_label_{};
};
