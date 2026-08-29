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
  explicit ArmadilloEditor(EditorState* state, QWidget* parent = nullptr);

  void setGhost(std::vector<std::pair<double, double>> poles);
  void clearGhost();

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
  [[nodiscard]] QPointF pointFor(double frequency_hz,
                                 double bandwidth_hz) const;
  [[nodiscard]] std::pair<double, double> rootAt(const QPointF& position) const;
  [[nodiscard]] std::vector<Handle> handles() const;
  [[nodiscard]] std::optional<Handle> hitHandle(const QPointF& position) const;
  void groupPrimaryOnly();
  void beginDrag(const QPointF& position);
  void applyPointer(const QPointF& position);

  EditorState* state_{};
  std::optional<Handle> drag_;
  std::set<Key> group_;
  std::vector<Member> members_;
  std::vector<std::pair<double, double>> ghost_;
  double press_hz_{};
  double press_bw_hz_{};
};
