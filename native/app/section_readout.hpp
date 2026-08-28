#pragma once

#include "trench/core/native_body.hpp"

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
                  const std::optional<trench::core::native::Resonant>& pole,
                  const std::optional<trench::core::native::Resonant>& zero,
                  bool zero_selected = false);

 signals:
  void poleEdited(std::size_t section, double frequency_hz, double bw_hz);
  void zeroEdited(std::size_t section, double frequency_hz, double bw_hz);

 protected:
  void paintEvent(QPaintEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void showValues();
  void commit(QLineEdit* field);
  [[nodiscard]] bool isZeroField(const QObject* field) const;

  std::size_t section_{};
  bool zero_selected_{};
  bool committing_{};
  std::optional<trench::core::native::Resonant> pole_;
  std::optional<trench::core::native::Resonant> zero_;
  QLabel* head_{};
  QLineEdit* pole_hz_{};
  QLineEdit* pole_bw_{};
  QLineEdit* zero_hz_{};
  QLineEdit* zero_bw_{};
};
