#pragma once

#include "editor_state.hpp"

#include <QPainterPath>
#include <QWidget>

#include <array>

class SectionStrip final : public QWidget {
 public:
  explicit SectionStrip(EditorState* state, QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  [[nodiscard]] QRectF cell(std::size_t index) const;
  [[nodiscard]] QRectF toggleRect(std::size_t index) const;
  void rebuildCurves();

  EditorState* state_{};
  std::array<QPainterPath, trench::core::native::kSections> curves_{};
  bool curves_dirty_{true};
};
