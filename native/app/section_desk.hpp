#pragma once

#include "editor_state.hpp"

#include <QWidget>

#include <array>
#include <cstddef>

class QDoubleSpinBox;

class SectionDesk final : public QWidget {
 public:
  explicit SectionDesk(EditorState* state, QWidget* parent = nullptr);

 private:
  struct Row {
    QWidget* frame{};
    QDoubleSpinBox* centre{};
    QDoubleSpinBox* width{};
    QDoubleSpinBox* gain{};
  };

  void refreshRows();
  void writeCentre(std::size_t index, double value);
  void writeWidth(std::size_t index, double value);
  void writeGain(std::size_t index, double value);

  EditorState* state_{};
  std::array<Row, trench::core::native::kSections> rows_{};
  bool updating_{};
};
