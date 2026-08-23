#pragma once

#include "trench/core/section_param.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;

class SectionStrip final : public QWidget {
  Q_OBJECT

 public:
  explicit SectionStrip(std::size_t section, QWidget* parent = nullptr);

  void setParam(const trench::core::p2k::SectionParam& param);
  void setSelected(bool selected);
  [[nodiscard]] std::size_t section() const noexcept;
  [[nodiscard]] bool selected() const noexcept;
  [[nodiscard]] double faderDb() const;
  [[nodiscard]] int trenchFaderValue(const trench::core::p2k::SectionParam& param) const;
  [[nodiscard]] double trenchHzOfFader(double fc_hz) const;

  static QString typeName(trench::core::p2k::SectionType type);
  static trench::core::p2k::SectionType typeFromIndex(int index);
  static int typeIndex(trench::core::p2k::SectionType type);

 signals:
  void selectRequested(std::size_t section);
  void gestureStarted(std::size_t section);
  void gestureFinished(std::size_t section);
  void paramEdited(std::size_t section, trench::core::p2k::SectionEdit edit,
                   const trench::core::p2k::SectionParam& param);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void relay(trench::core::p2k::SectionEdit edit);

  std::size_t section_{};
  bool selected_{};
  bool updating_{};
  trench::core::p2k::SectionParam param_{};
  QComboBox* type_{};
  QSlider* gain_{};
  QLabel* gain_value_{};
  QDoubleSpinBox* bw_{};
  QLabel* fc_{};
};
