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

  void setParam(const trench::core::p2k::ShapeParam& param);
  void setSelected(bool selected);
  void setHighlighted(bool highlighted);
  [[nodiscard]] std::size_t section() const noexcept;
  [[nodiscard]] bool selected() const noexcept;
  [[nodiscard]] double faderDb() const;
  [[nodiscard]] int trenchFaderValue(const trench::core::p2k::ShapeParam& param) const;
  [[nodiscard]] double trenchHzOfFader(double fc_hz) const;

  static QString typeName(trench::core::p2k::Shape shape);
  static trench::core::p2k::Shape typeFromIndex(int index);
  static int typeIndex(trench::core::p2k::Shape shape);

 signals:
  void selectRequested(std::size_t section);
  void gestureStarted(std::size_t section);
  void gestureFinished(std::size_t section);
  void shapeEdited(std::size_t section, const trench::core::p2k::ShapeParam& param);
  void paramEdited(std::size_t section, trench::core::p2k::SectionEdit edit,
                   const trench::core::p2k::SectionParam& param);
  void hoverChanged(std::size_t section, bool inside);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  [[nodiscard]] trench::core::p2k::ShapeParam typed(trench::core::p2k::Shape shape) const;
  void relay(trench::core::p2k::Shape shape);

  std::size_t section_{};
  bool selected_{};
  bool highlighted_{};
  bool updating_{};
  trench::core::p2k::ShapeParam param_{};
  QComboBox* type_{};
  QSlider* gain_{};
  QLabel* gain_value_{};
  QDoubleSpinBox* q_{};
  QDoubleSpinBox* bw_{};
  QDoubleSpinBox* fc_{};
};
