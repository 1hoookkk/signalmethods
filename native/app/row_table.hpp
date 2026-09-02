#pragma once

#include "editor_state.hpp"

#include <QWidget>

#include <array>
#include <cstddef>

class QBoxLayout;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;

class RowTable final : public QWidget {
  Q_OBJECT

 public:
  explicit RowTable(EditorState* state, QWidget* parent = nullptr);

  void refresh();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  enum class Kind { kFreq, kQ, kGain, kZero };

  struct Column {
    QLabel* caption{};
    QSlider* fader{};
    QLineEdit* entry{};
    QLabel* readout{};
  };

  struct Strip {
    QCheckBox* on{};
    Column freq{};
    Column q{};
    Column gain{};
    Column zero{};
    QPushButton* lock{};
    QSpinBox* cut{};
    bool unlocked_by_hand{};
  };

  Column buildColumn(QBoxLayout* into, const QString& caption, const QString& prefix,
                     std::size_t index, int maximum);
  void buildStrip(std::size_t index, QBoxLayout* into);
  void buildPicker(QBoxLayout* into);
  [[nodiscard]] std::size_t corner() const noexcept;
  [[nodiscard]] bool lockedNow(std::size_t index) const;
  void pushPole(std::size_t index, bool carry_zero);
  void pushGain(std::size_t index);
  void pushZeroMag(std::size_t index);
  void landEntry(std::size_t index, Kind kind);
  void selectFrom(std::size_t index, EditorState::Lane lane);

  EditorState* state_{};
  std::array<Strip, trench::core::native::kSections> strips_{};
  std::array<QPushButton*, trench::core::native::kCorners> corner_buttons_{};
  bool refreshing_{};
};
