#pragma once

#include "trench/core/section_param.hpp"

#include <QWidget>

#include <cstddef>
#include <optional>

class QLabel;
class QLineEdit;

class SectionReadout final : public QWidget {
  Q_OBJECT

 public:
  explicit SectionReadout(QWidget* parent = nullptr);

  void setReading(std::size_t section,
                  const std::optional<trench::core::p2k::PoleReading>& pole,
                  const trench::core::p2k::MaskParam& mask, bool zero_live);

 signals:
  void poleEdited(std::size_t section, double frequency_hz, double bw_hz);

 protected:
  void paintEvent(QPaintEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void showPole();
  void commit(QLineEdit* field);

  std::size_t section_{};
  bool live_{};
  bool committing_{};
  double pole_hz_{};
  double pole_bw_hz_{};
  QLineEdit* hz_{};
  QLineEdit* bw_{};
  QLabel* zero_{};
};
