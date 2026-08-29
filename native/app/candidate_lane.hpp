#pragma once

#include <QRectF>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <vector>

class CandidateLane final : public QWidget {
 public:
  explicit CandidateLane(QWidget* parent = nullptr);

  void setReference(const std::vector<double>& frequency_hz,
                    const std::vector<double>& magnitude_db);
  void clearReference();

  std::function<void(double hz, double bw_hz)> onPick;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  struct Candidate {
    double hz{};
    double bw_hz{};
    double prominence_db{};
  };

  [[nodiscard]] QRectF lane() const;
  [[nodiscard]] double xForFrequency(double frequency_hz,
                                     const QRectF& bounds) const;
  [[nodiscard]] int candidateAt(double x) const;

  std::vector<Candidate> candidates_;
  int hovered_{-1};
};
