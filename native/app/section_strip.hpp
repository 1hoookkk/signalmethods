#pragma once

#include "editor_state.hpp"

#include <QWidget>

class SectionStrip final : public QWidget {
 public:
  explicit SectionStrip(EditorState* state, QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  [[nodiscard]] QRectF cell(std::size_t index) const;

  EditorState* state_{};
};
