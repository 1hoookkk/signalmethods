#pragma once

#include <QLineEdit>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

class ChassisBar final : public QWidget {
  Q_OBJECT

 public:
  enum class Verb { kUnity, kSource, kTarget, kFit, kKeep, kDiscard };

  struct Readout {
    std::size_t section{};
    bool pole{};
    QString text;
  };

  explicit ChassisBar(QWidget* parent = nullptr);

  void setBodyName(const QString& name);
  void setTargetName(const QString& name);
  void setState(bool has_target, bool running);
  void setDcDriftDb(double db);
  void setSourceSawtooth(bool sawtooth);
  void setReadout(const std::optional<Readout>& readout);
  [[nodiscard]] QString readoutText() const;

 signals:
  void verbClicked(ChassisBar::Verb verb);
  void rootTyped(const QString& text);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  struct Pad {
    Verb verb{Verb::kTarget};
    QRectF rect;
    bool available{};
  };

  [[nodiscard]] std::vector<Pad> pads() const;
  [[nodiscard]] std::optional<Verb> hit(const QPointF& at) const;
  [[nodiscard]] QString labelFor(Verb verb) const;

  QString body_name_;
  QString target_name_;
  bool has_target_{};
  bool running_{};
  double dc_drift_db_{};
  bool source_sawtooth_{};
  std::optional<Readout> readout_;
  QLineEdit* entry_{};
  double entry_left_{};
  std::optional<Verb> hover_;
  std::optional<Verb> pressed_;
};
