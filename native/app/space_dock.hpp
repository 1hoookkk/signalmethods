#pragma once

#include "trench/core/p2k.hpp"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;

class SpaceDock final : public QWidget {
  Q_OBJECT

 public:
  explicit SpaceDock(QWidget* parent = nullptr);

  void setSpace(const trench::core::p2k::PerceptualSpace& space);
  [[nodiscard]] trench::core::p2k::PerceptualSpace space() const;

 signals:
  void spaceEdited(const trench::core::p2k::PerceptualSpace& space);

 private:
  void relay();

  QDoubleSpinBox* lo_{};
  QDoubleSpinBox* hi_{};
  QComboBox* weight_{};
  QDoubleSpinBox* smooth_{};
  QDoubleSpinBox* band_lo_{};
  QDoubleSpinBox* band_hi_{};
  QDoubleSpinBox* band_gain_{};
  bool updating_{};
};
