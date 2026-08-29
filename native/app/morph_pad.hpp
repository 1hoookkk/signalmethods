#pragma once

#include "editor_state.hpp"

#include <QWidget>

class MorphPad final : public QWidget {
 public:
  explicit MorphPad(EditorState* state, QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

 private:
  [[nodiscard]] QRectF field() const;
  [[nodiscard]] QRectF cornerRect(std::size_t index) const;
  [[nodiscard]] QPointF pointFor(double morph, double q) const;
  void trackTo(const QPointF& position);

  EditorState* state_{};
};
