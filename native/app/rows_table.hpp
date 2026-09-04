#pragma once

#include "editor_state.hpp"
#include "pole_templates.hpp"

#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

class NumberBox;
class QBoxLayout;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QSpinBox;
class QToolButton;
class SoFarCell;

class RowsTable final : public QWidget {
  Q_OBJECT

 public:
  enum class Type { kOff, kEq, kLowPass, kHighPass, kPole, kNotch };
  enum class Side { kLo, kHi };
  enum class Pole { kRing, kReal };

  explicit RowsTable(EditorState* state, QWidget* parent = nullptr);

  static constexpr std::size_t kCeilingSection = trench::core::native::kSections - 1;
  static constexpr int kHarmonicCount = 16;

  [[nodiscard]] double rootHz() const noexcept { return root_hz_; }
  void setRootHz(double hz);
  [[nodiscard]] std::size_t pairBase() const;
  [[nodiscard]] std::size_t cornerOf(Side side) const;
  [[nodiscard]] Type typeAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] int harmonicOf(Side side, std::size_t index) const;
  [[nodiscard]] bool followingAt(Side side, std::size_t index) const;
  [[nodiscard]] static int ringRule(int mag_byte);
  [[nodiscard]] double soFarDbAt(std::size_t row, double hz) const;
  std::function<void(std::size_t)> onSoFarHover;
  std::function<void()> onSoFarLeave;
  void followNote(Side side, std::size_t index);
  void rowFromFrame(std::size_t index, const trench::app::PoleTemplate& frame,
                    std::size_t slot);
  [[nodiscard]] std::size_t nearestFrameSlot(std::size_t index,
                                             const trench::app::PoleTemplate& frame) const;
  void refresh();

  void handleBegin(std::size_t index);
  void handleNote(std::size_t index, double hz);
  void handleHeight(std::size_t index, double db);
  void handleRingSteps(std::size_t index, int steps);
  void handleWheelRing(std::size_t index, int steps);
  void handleCreate(double hz, double db);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  enum class Shape { kResonator, kPair, kNotch, kEdgeHigh, kEdgeLow };
  enum class Kind { kNote, kRing, kHeight, kOffset };

  struct Cell {
    NumberBox* note{};
    NumberBox* ring{};
    NumberBox* height{};
    NumberBox* offset{};
    QSpinBox* cut{};
    QComboBox* pole{};
    QComboBox* harm{};
    bool ring_touched{};
  };

  struct Row {
    QLabel* number{};
    QComboBox* type{};
    QToolButton* unlock{};
    QWidget* extra{};
    SoFarCell* so_far{};
    std::array<Cell, 2> sides{};
  };

  [[nodiscard]] Cell& cellOf(Side side, std::size_t index);
  [[nodiscard]] const Cell& cellOf(Side side, std::size_t index) const;
  [[nodiscard]] Side sideOfEditingCorner() const;
  [[nodiscard]] Shape shapeAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] Shape shapeOf(Type type, std::size_t index) const;
  [[nodiscard]] bool ceilingAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] Pole poleAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] double offsetNow(Side side, std::size_t index) const;
  [[nodiscard]] int nearestFrequencyDial(Side side, std::size_t index, double hz) const;
  [[nodiscard]] int nearestGainDial(Side side, std::size_t index, double db) const;

  NumberBox* buildCell(const QString& name, int minimum, int maximum);
  void buildRoot(QBoxLayout* into);
  void buildHeader(QGridLayout* grid);
  void buildRow(std::size_t index, QGridLayout* grid);
  void refreshSoFar();
  void watch(QWidget* control, std::size_t index, EditorState::Lane lane);
  void armRowMenu(QWidget* control, std::size_t index);
  void showRowMenu(std::size_t index, const QPoint& global);

  void setSide(Side side);
  void selectFrom(std::size_t index, EditorState::Lane lane);
  void pushRoot();
  void landHarmonic(Side side, std::size_t index, int harmonic);
  void seatFreshRow(Side side, std::size_t index);
  void seatZero(Side side, std::size_t index, double semitones, std::uint16_t rsq);
  void pushPole(Side side, std::size_t index, Kind moved);
  void pushGain(Side side, std::size_t index);
  void pushCeiling(Side side, std::size_t index);
  void pushOffset(Side side, std::size_t index);
  void pushShape(Side side, std::size_t index, Shape shape);
  void pushType(Side side, std::size_t index, Type type);
  void pushPoleWord(Side side, std::size_t index, Pole pole);
  void landEntry(Side side, std::size_t index, Kind kind, const QString& entry);

  EditorState* state_{};
  NumberBox* root_dial_{};
  QLineEdit* root_entry_{};
  QLabel* root_note_{};
  double root_hz_{64.0};
  std::array<Row, trench::core::native::kSections> rows_{};
  Side drag_side_{Side::kLo};
  int drag_ring_{};
  bool refreshing_{};
};
