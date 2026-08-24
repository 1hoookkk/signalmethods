#pragma once

#include "trench/core/section_param.hpp"

#include <QWidget>

#include <cstddef>
#include <optional>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSlider;

class SectionStrip final : public QWidget {
  Q_OBJECT

 public:
  explicit SectionStrip(std::size_t section, QWidget* parent = nullptr);

  void setReading(const std::optional<trench::core::p2k::PoleReading>& pole,
                  const trench::core::p2k::MaskParam& mask);
  void setSelected(bool selected);
  void setHighlighted(bool highlighted);
  [[nodiscard]] std::size_t section() const noexcept;
  [[nodiscard]] bool selected() const noexcept;
  [[nodiscard]] double offsetOct() const;

 signals:
  void selectRequested(std::size_t section);
  void gestureStarted(std::size_t section);
  void gestureFinished(std::size_t section);
  void maskEdited(std::size_t section, const trench::core::p2k::MaskParam& mask);
  void poleHzEdited(std::size_t section, double frequency_hz);
  void hoverChanged(std::size_t section, bool inside);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void relay();
  void showPoleReading();
  void commitPoleHz();

  std::size_t section_{};
  bool selected_{};
  bool highlighted_{};
  bool updating_{};
  bool live_{};
  QLineEdit* pole_{};
  double pole_hz_{};
  double pole_bw_hz_{};
  bool editing_pole_{};
  QSlider* offset_{};
  QLabel* offset_value_{};
  QDoubleSpinBox* width_{};
};
