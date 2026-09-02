#pragma once

#include "trench/core/native_body.hpp"

#include <QWidget>

#include <array>
#include <cstddef>
#include <vector>

class CascadePlot final : public QWidget {
 public:
  explicit CascadePlot(QWidget* parent = nullptr);

  void setCascade(
      const trench::core::Cascade& cascade,
      const std::array<trench::core::Biquad,
                       trench::core::native::kSections>& sections,
      const std::array<bool, trench::core::native::kSections>& enabled,
      const std::vector<double>& seed_hz, std::size_t selected_section,
      double selected_frequency_hz, double sample_rate_hz);
  [[nodiscard]] const std::vector<double>& gridHz() const;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  [[nodiscard]] double xForFrequency(double frequency_hz,
                                     const QRectF& plot) const;
  [[nodiscard]] double yForDb(double db, const QRectF& plot,
                              double low_db, double high_db) const;

  std::vector<double> base_hz_;
  std::vector<double> grid_hz_;
  std::vector<double> response_db_;
  std::vector<double> row_db_;
  std::array<std::vector<double>, trench::core::native::kSections> rung_db_{};
  std::array<bool, trench::core::native::kSections> enabled_{};
  double selected_frequency_hz_{20.0};
};
