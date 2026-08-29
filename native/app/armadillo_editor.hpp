#pragma once

#include "editor_state.hpp"

#include <QPointF>
#include <QWidget>

#include <cstddef>
#include <optional>
#include <set>
#include <utility>
#include <vector>

class ArmadilloEditor final : public QWidget {
 public:
  enum class Projection { kArmadillo, kZPlane };

  explicit ArmadilloEditor(EditorState* state, QWidget* parent = nullptr);

  void setProjection(Projection projection);
  void setGhost(std::vector<std::pair<double, double>> poles);
  void clearGhost();

  [[nodiscard]] QPointF discCentre() const;
  [[nodiscard]] double discRadius() const;
  [[nodiscard]] QPointF pointFor(double frequency_hz,
                                 double bandwidth_hz) const;
  [[nodiscard]] std::optional<std::pair<double, double>> placementAt(
      const QPointF& position) const;
  [[nodiscard]] std::pair<double, double> dragTargetAt(
      const QPointF& position) const;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  struct Handle {
    std::size_t section{};
    EditorState::Lane lane{EditorState::Lane::kPole};
    QPointF position;
  };

  struct Member {
    std::size_t section{};
    EditorState::Lane lane{EditorState::Lane::kPole};
    double hz{};
    double bw_hz{};
  };

  using Key = std::pair<std::size_t, EditorState::Lane>;

  [[nodiscard]] QRectF field() const;
  [[nodiscard]] std::vector<Handle> handles() const;
  [[nodiscard]] std::optional<Handle> hitHandle(const QPointF& position) const;
  void groupPrimaryOnly();
  void beginDrag(const QPointF& position);
  void applyPointer(const QPointF& position);

  EditorState* state_{};
  Projection projection_{Projection::kArmadillo};
  std::optional<Handle> drag_;
  std::set<Key> group_;
  std::vector<Member> members_;
  std::vector<std::pair<double, double>> ghost_;
  double press_hz_{};
  double press_bw_hz_{};
};
