#pragma once

#include "editor_state.hpp"

#include "trench/core/native_body.hpp"

#include <QPointF>
#include <QWidget>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

class CascadePlot final : public QWidget {
 public:
  explicit CascadePlot(EditorState* state, QWidget* parent = nullptr);

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

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  struct Handle {
    std::size_t section{};
    QPointF position;
  };

  [[nodiscard]] QRectF plotRect() const;
  [[nodiscard]] double xForFrequency(double frequency_hz,
                                     const QRectF& plot) const;
  [[nodiscard]] double yForDb(double db, const QRectF& plot,
                              double low_db, double high_db) const;
  [[nodiscard]] double frequencyForX(double x, const QRectF& plot) const;
  [[nodiscard]] double dbForY(double y, const QRectF& plot, double low_db,
                              double high_db) const;
  [[nodiscard]] double responseDbAt(double frequency_hz) const;
  [[nodiscard]] std::vector<Handle> zeroHandles() const;
  [[nodiscard]] std::optional<Handle> hitHandle(const QPointF& position) const;
  [[nodiscard]] double solveBandwidth(std::size_t section, double frequency_hz,
                                      double target_db) const;
  void applyPointer(const QPointF& position);

  EditorState* state_{};
  std::optional<Handle> drag_;
  std::vector<double> base_hz_;
  std::vector<double> grid_hz_;
  std::vector<double> response_db_;
  std::array<std::vector<double>, trench::core::native::kSections>
      section_db_;
  std::array<bool, trench::core::native::kSections> enabled_{};
  std::size_t selected_section_{};
  double selected_frequency_hz_{20.0};
  double sample_rate_hz_{EditorState::kDatumHz};
  std::vector<double> formant_hz_;
  std::vector<double> reference_hz_;
  std::vector<double> reference_db_;
};
