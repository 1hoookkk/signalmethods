#pragma once

#include "trench/core/native_body.hpp"

#include <QWidget>

#include <array>
#include <cstddef>
#include <optional>
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
  void setReference(std::vector<double> frequency_hz,
                    std::vector<double> magnitude_db);
  void clearReference();
  void setFormantMarks(std::vector<double> frequency_hz);
  void clearFormantMarks();
  [[nodiscard]] const std::vector<double>& gridHz() const;
  [[nodiscard]] double maxResidualDb() const;
  [[nodiscard]] const std::vector<double>& referenceHz() const noexcept { return reference_hz_; }
  [[nodiscard]] const std::vector<double>& referenceDb() const noexcept { return reference_db_; }
  [[nodiscard]] const std::vector<double>& residualDb() const noexcept { return residual_db_; }

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  [[nodiscard]] double xForFrequency(double frequency_hz,
                                     const QRectF& plot) const;
  [[nodiscard]] double yForDb(double db, const QRectF& plot,
                              double low_db, double high_db) const;
  void buildResidual();

  std::vector<double> base_hz_;
  std::vector<double> grid_hz_;
  std::vector<double> response_db_;
  std::vector<double> without_row_db_;
  std::array<bool, trench::core::native::kSections> enabled_{};
  double selected_frequency_hz_{20.0};
  std::vector<double> formant_hz_;
  std::vector<double> reference_hz_;
  std::vector<double> reference_db_;
  std::vector<double> residual_db_;
  double level_db_{};
  double reference_mean_db_{};
};
