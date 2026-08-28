#pragma once

#include "editor_state.hpp"

#include <QPointF>
#include <QWidget>

#include <cstddef>
#include <optional>
#include <vector>

class ArmadilloEditor final : public QWidget {
 public:
  explicit ArmadilloEditor(EditorState* state, QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  struct Handle {
    std::size_t section{};
    EditorState::Lane lane{EditorState::Lane::kPole};
    QPointF position;
  };

  [[nodiscard]] QRectF field() const;
  [[nodiscard]] QPointF pointFor(double frequency_hz,
                                 double bandwidth_hz) const;
  [[nodiscard]] std::vector<Handle> handles() const;
  [[nodiscard]] std::optional<Handle> hitHandle(const QPointF& position) const;
  void applyPointer(const QPointF& position);

  EditorState* state_{};
  std::optional<Handle> drag_;
};
