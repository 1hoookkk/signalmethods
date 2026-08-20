#pragma once

#include "trench/core/packed_body.hpp"

#include <QWidget>

#include <cstddef>
#include <string>
#include <vector>

class ResponsePlotWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit ResponsePlotWidget(QWidget* parent = nullptr);

  void setBody(const trench::core::PackedBody& body, double sample_rate_hz,
               std::string source_label);

  [[nodiscard]] std::size_t responsePointCount() const noexcept;
  [[nodiscard]] double frequencyAt(std::size_t index) const;
  [[nodiscard]] double responseDbAt(std::size_t index) const;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  std::vector<double> frequencies_hz_;
  std::vector<double> response_db_;
  QString source_label_;
};
