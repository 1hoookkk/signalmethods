#pragma once

#include <QWidget>

class QComboBox;
class QSlider;

class VowelJourney final : public QWidget {
  Q_OBJECT

 public:
  explicit VowelJourney(QWidget* parent = nullptr);

  [[nodiscard]] QString fromSymbol() const;
  [[nodiscard]] QString toSymbol() const;
  [[nodiscard]] double highQBandwidthScale() const noexcept;
  [[nodiscard]] QComboBox* fromBox() const noexcept;
  [[nodiscard]] QComboBox* toBox() const noexcept;
  [[nodiscard]] QSlider* qSlider() const noexcept;
  bool setEndpoints(const QString& from, const QString& to,
                    double high_q_bandwidth_scale);

 signals:
  void journeyChosen(QString from, QString to, double high_q_bandwidth_scale);

 private:
  void emitJourney();

  QComboBox* from_{};
  QComboBox* to_{};
  QSlider* q_{};
};
