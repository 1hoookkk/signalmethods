#pragma once

#include "trench/core/packed_body.hpp"

#include <QWidget>

#include <QString>

#include <vector>

class CascadePlot final : public QWidget {
 public:
  explicit CascadePlot(QWidget* parent = nullptr);

  void setCascade(const trench::core::Cascade& cascade,
                  double sample_rate_hz);
  void setReference(QString name, std::vector<double> frequency_hz,
                    std::vector<double> magnitude_db);
  void clearReference();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  [[nodiscard]] double xForFrequency(double frequency_hz,
                                     const QRectF& plot) const;
  [[nodiscard]] double yForDb(double db, const QRectF& plot,
                              double low_db, double high_db) const;

  QString reference_name_;
  std::vector<double> grid_hz_;
  std::vector<double> response_db_;
  std::vector<double> reference_hz_;
  std::vector<double> reference_db_;
};
