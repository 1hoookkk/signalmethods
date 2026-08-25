#pragma once

#include "trench/core/native_body.hpp"

#include <QWidget>

#include <cstddef>
#include <optional>

class QLabel;
class QLineEdit;
class QToolButton;

class SectionReadout final : public QWidget {
  Q_OBJECT

 public:
  explicit SectionReadout(QWidget* parent = nullptr);

  void setReading(std::size_t section,
                  const std::optional<trench::core::native::Resonant>& pole,
                  const std::optional<trench::core::native::Resonant>& zero);
  void setPins(bool pole_free, bool zero_free);

 signals:
  void poleEdited(std::size_t section, double frequency_hz, double bw_hz);
  void pinToggled(std::size_t section, bool pole);

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
  QToolButton* pole_pin_{};
  QToolButton* zero_pin_{};
};
