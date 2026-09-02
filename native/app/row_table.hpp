#pragma once

#include "editor_state.hpp"

#include <QWidget>

#include <array>
#include <cstddef>

class QCheckBox;
class QSpinBox;
class QLabel;

class RowTable final : public QWidget {
  Q_OBJECT

 public:
  explicit RowTable(EditorState* state, QWidget* parent = nullptr);

  void refresh();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  struct Cell {
    QCheckBox* on{};
    QSpinBox* pole_pitch{};
    QSpinBox* pole_res{};
    QLabel* pole_readout{};
    QSpinBox* zero_pitch{};
    QSpinBox* zero_depth{};
    QLabel* zero_readout{};
    QSpinBox* cut{};
  };

  using Group = std::array<Cell, trench::core::native::kSections>;

  void buildGroup(std::size_t group, const char* prefix, int first_row,
                  class QGridLayout* grid);
  void pushPole(std::size_t group, std::size_t index);
  void pushZero(std::size_t group, std::size_t index);
  [[nodiscard]] std::size_t cornerOf(std::size_t group) const noexcept;

  EditorState* state_{};
  std::array<QLabel*, trench::core::native::kSections> labels_{};
  std::array<Group, 2> groups_{};
  QLabel* from_header_{};
  QLabel* to_header_{};
  std::size_t from_corner_{};
  bool refreshing_{};
};
