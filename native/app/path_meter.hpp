#pragma once

#include "trench/core/packed_body.hpp"

#include <QWidget>

class QLabel;

class PathMeter final : public QWidget {
  Q_OBJECT

 public:
  static constexpr double kWarnDb = 20.0;
  static constexpr double kFrameDb = 30.0;
  static constexpr int kSteps = 9;

  explicit PathMeter(QWidget* parent = nullptr);

  void setBody(const trench::core::PackedBody& packed, double morph, double q);
  [[nodiscard]] double worstDb() const noexcept { return worst_db_; }
  [[nodiscard]] double worstMorph() const noexcept { return worst_morph_; }
  [[nodiscard]] double worstQ() const noexcept { return worst_q_; }
  [[nodiscard]] double hereDb() const noexcept { return here_db_; }
  [[nodiscard]] QLabel* readout() const noexcept { return readout_; }

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QLabel* readout_{};
  double worst_db_{};
  double worst_morph_{};
  double worst_q_{};
  double here_db_{};
};
