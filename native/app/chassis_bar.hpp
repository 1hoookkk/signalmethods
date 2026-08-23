#pragma once

#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

class ChassisBar final : public QWidget {
  Q_OBJECT

 public:
  enum class Verb { kSource, kTarget, kFit, kKeep, kDiscard };

  explicit ChassisBar(QWidget* parent = nullptr);

  void setBodyName(const QString& name);
  void setTargetName(const QString& name);
  void setState(bool has_target, bool running);
  void setSourceSawtooth(bool sawtooth);
  void setScoreDb(double db);
  [[nodiscard]] double scoreDb() const noexcept;

 signals:
  void verbClicked(ChassisBar::Verb verb);

 protected:
  void paintEvent(QPaintEvent* event) override;
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
  bool source_sawtooth_{};
  double score_db_{std::numeric_limits<double>::quiet_NaN()};
  std::optional<Verb> hover_;
  std::optional<Verb> pressed_;
};
