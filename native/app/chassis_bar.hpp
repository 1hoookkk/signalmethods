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
  enum class Verb {
    kSave,
    kTarget,
    kUndo,
    kRedo,
    kReset,
    kFit,
    kKeep,
    kDiscard
  };

  explicit ChassisBar(QWidget* parent = nullptr);

  void setBodyName(const QString& name);
  void setTargetName(const QString& name);
  void setState(bool has_target, bool running);
  void setHistoryState(bool can_undo, bool can_redo, bool can_reset);
  void setPowerDb(double db);
  [[nodiscard]] double powerDb() const noexcept;
  void setScoreDb(double db);
  [[nodiscard]] double scoreDb() const noexcept;

 signals:
  void verbClicked(ChassisBar::Verb verb);

 protected:
  bool event(QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 public:
  struct Pad {
    Verb verb{Verb::kTarget};
    QRectF rect;
    bool available{};
  };

  [[nodiscard]] std::vector<Pad> pads() const;

 private:
  [[nodiscard]] std::optional<Verb> hit(const QPointF& at) const;
  [[nodiscard]] QString labelFor(Verb verb) const;

  QString body_name_;
  QString target_name_;
  bool has_target_{};
  bool running_{};
  bool can_undo_{};
  bool can_redo_{};
  bool can_reset_{};
  double power_db_{std::numeric_limits<double>::quiet_NaN()};
  double score_db_{std::numeric_limits<double>::quiet_NaN()};
  std::optional<Verb> hover_;
  std::optional<Verb> pressed_;
};
