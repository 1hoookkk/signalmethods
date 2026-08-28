#pragma once

#include "editor_state.hpp"

#include <QWidget>

class RootEditor final : public QWidget {
 public:
  explicit RootEditor(EditorState* state, QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  [[nodiscard]] QRectF polePanel() const;
  [[nodiscard]] QRectF zeroPanel() const;
  [[nodiscard]] QPointF pointFor(double frequency_hz, double bandwidth_hz,
                                 const QRectF& panel) const;
  void applyPointer(const QPointF& position);

  EditorState* state_{};
  bool dragging_{};
  EditorState::Lane drag_lane_{EditorState::Lane::kPole};
};
