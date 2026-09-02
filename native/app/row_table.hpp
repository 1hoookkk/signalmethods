#pragma once

#include "editor_state.hpp"

#include <QWidget>

#include <array>
#include <cstddef>

class QBoxLayout;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class WordDial;

class RowTable final : public QWidget {
  Q_OBJECT

 public:
  enum class Shape { kResonator, kPair, kNotch, kEdgeHigh, kEdgeLow };
  enum class Pole { kRing, kReal };

  explicit RowTable(EditorState* state, QWidget* parent = nullptr);

  static constexpr std::size_t kCeilingSection = trench::core::native::kSections - 1;
  static constexpr int kHarmonicCount = 16;

  [[nodiscard]] Shape shapeAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] Pole poleAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] QWidget* takePicker();
  [[nodiscard]] double rootHz() const noexcept { return root_hz_; }
  [[nodiscard]] int harmonicOf(std::size_t index) const;
  void setRootHz(double hz);
  void refresh();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  enum class Kind { kFreq, kQ, kGain, kOffset };

  struct Column {
    QLabel* caption{};
    WordDial* dial{};
    QLineEdit* entry{};
  };

  struct Strip {
    QCheckBox* on{};
    QComboBox* pole{};
    QComboBox* harm{};
    QComboBox* shape{};
    Column freq{};
    Column q{};
    Column gain{};
    Column offset{};
    QSpinBox* cut{};
  };

  Column buildColumn(QBoxLayout* into, const QString& caption, const QString& prefix,
                     std::size_t index, int minimum, int maximum);
  void buildStrip(std::size_t index, QBoxLayout* into);
  void buildRoot(QBoxLayout* into);
  void buildPicker();
  [[nodiscard]] int nearestFrequencyDial(std::size_t index, double hz) const;
  void landHarmonic(std::size_t index, int harmonic);
  void pushCeiling(std::size_t index);
  void pushRoot();
  [[nodiscard]] std::size_t corner() const noexcept;
  [[nodiscard]] double offsetNow(std::size_t index) const;
  void seatZero(std::size_t index, double semitones, std::uint16_t rsq);
  void pushPole(std::size_t index, Kind moved);
  void seatFreshRow(std::size_t index);
  void pushGain(std::size_t index);
  void pushOffset(std::size_t index);
  void pushShape(std::size_t index, Shape shape);
  void pushPoleWord(std::size_t index, Pole pole);
  void landEntry(std::size_t index, Kind kind);
  void selectFrom(std::size_t index, EditorState::Lane lane);

  EditorState* state_{};
  QWidget* picker_{};
  Column root_{};
  double root_hz_{64.0};
  std::array<Strip, trench::core::native::kSections> strips_{};
  std::array<QPushButton*, trench::core::native::kCorners> corner_buttons_{};
  bool refreshing_{};
};
