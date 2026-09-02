#include "row_table.hpp"

#include "trench/core/p2k.hpp"

#include <QCheckBox>
#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>

#include <array>
#include <cstdint>
#include <variant>

namespace {

namespace p2k = trench::core::p2k;

constexpr int kTopDial = static_cast<int>(p2k::kDialCount) - 1;

int maxPoleRes() {
  static const int value = kTopDial - static_cast<int>(p2k::dial_of_word(p2k::kPoleCeilingRsqWord));
  return value;
}

QSpinBox* dial(int maximum, QWidget* parent) {
  auto* box = new QSpinBox(parent);
  box->setRange(0, maximum);
  box->setKeyboardTracking(false);
  box->setMaximumWidth(56);
  return box;
}

QString readout(std::uint16_t mag, std::uint16_t rsq) {
  const auto [p, q] = p2k::pq(mag, rsq);
  const auto roots =
      trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz);
  if (const auto* tone = std::get_if<trench::core::native::Resonant>(&roots)) {
    return QString::asprintf("%.0f Hz · %.0f", tone->hz, tone->bw_hz);
  }
  const auto& real = std::get<trench::core::native::RealRoots>(roots);
  return QString::asprintf("R %.0f · %.0f", real.a_hz, real.b_hz);
}

}

RowTable::RowTable(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  auto* grid = new QGridLayout(this);
  grid->setSpacing(2);
  grid->setContentsMargins(4, 4, 4, 4);

  from_header_ = new QLabel(this);
  from_header_->setObjectName(QStringLiteral("fromHeader"));
  to_header_ = new QLabel(this);
  to_header_->setObjectName(QStringLiteral("toHeader"));
  grid->addWidget(from_header_, 0, 0, 1, 9);
  grid->addWidget(to_header_, 8, 0, 1, 9);

  const std::array<const char*, 8> headings{"ON", "PITCH", "RES", "POLE Hz · BW",
                                            "PITCH", "DEPTH", "ZERO Hz · BW", "CUT ×6dB"};
  for (int line : {1, 9}) {
    grid->addWidget(new QLabel(QStringLiteral("ROW"), this), line, 0);
    for (int column = 0; column < static_cast<int>(headings.size()); ++column) {
      grid->addWidget(new QLabel(QString::fromUtf8(headings[column]), this), line,
                      1 + column);
    }
  }

  for (std::size_t index = 0; index < labels_.size(); ++index) {
    labels_[index] = new QLabel(QString::number(index + 1), this);
    labels_[index]->setObjectName(QStringLiteral("rowLabel%1").arg(index));
    labels_[index]->installEventFilter(this);
    grid->addWidget(labels_[index], static_cast<int>(index) + 2, 0);
    grid->addWidget(new QLabel(QString::number(index + 1), this),
                    static_cast<int>(index) + 10, 0);
  }

  buildGroup(0, "from", 2, grid);
  buildGroup(1, "to", 10, grid);

  connect(state_, &EditorState::changed, this, &RowTable::refresh);
  connect(state_, &EditorState::selectionChanged, this,
          [this](std::size_t) { refresh(); });
  refresh();
  setFixedWidth(sizeHint().width());
}

void RowTable::buildGroup(std::size_t group, const char* prefix, int first_row,
                          QGridLayout* grid) {
  const QString name = QString::fromUtf8(prefix);
  const int readout_width =
      fontMetrics().horizontalAdvance(QStringLiteral("22050 Hz · 20000")) + 8;
  for (std::size_t index = 0; index < groups_[group].size(); ++index) {
    const int line = static_cast<int>(index) + first_row;
    Cell& cell = groups_[group][index];
    cell.on = new QCheckBox(this);
    cell.on->setObjectName(QStringLiteral("%1On%2").arg(name).arg(index));
    cell.pole_pitch = dial(static_cast<int>(p2k::kMaxMagByte), this);
    cell.pole_pitch->setObjectName(QStringLiteral("%1PolePitch%2").arg(name).arg(index));
    cell.pole_res = dial(maxPoleRes(), this);
    cell.pole_res->setObjectName(QStringLiteral("%1PoleRes%2").arg(name).arg(index));
    cell.pole_readout = new QLabel(this);
    cell.pole_readout->setObjectName(QStringLiteral("%1PoleReadout%2").arg(name).arg(index));
    cell.pole_readout->setFixedWidth(readout_width);
    cell.zero_pitch = dial(kTopDial, this);
    cell.zero_pitch->setObjectName(QStringLiteral("%1ZeroPitch%2").arg(name).arg(index));
    cell.zero_depth = dial(kTopDial, this);
    cell.zero_depth->setObjectName(QStringLiteral("%1ZeroDepth%2").arg(name).arg(index));
    cell.zero_readout = new QLabel(this);
    cell.zero_readout->setObjectName(QStringLiteral("%1ZeroReadout%2").arg(name).arg(index));
    cell.zero_readout->setFixedWidth(readout_width);
    cell.cut = dial(EditorState::kMaxCut, this);
    cell.cut->setObjectName(QStringLiteral("%1Cut%2").arg(name).arg(index));

    grid->addWidget(cell.on, line, 1);
    grid->addWidget(cell.pole_pitch, line, 2);
    grid->addWidget(cell.pole_res, line, 3);
    grid->addWidget(cell.pole_readout, line, 4);
    grid->addWidget(cell.zero_pitch, line, 5);
    grid->addWidget(cell.zero_depth, line, 6);
    grid->addWidget(cell.zero_readout, line, 7);
    grid->addWidget(cell.cut, line, 8);

    for (QWidget* control : {static_cast<QWidget*>(cell.on),
                             static_cast<QWidget*>(cell.pole_pitch),
                             static_cast<QWidget*>(cell.pole_res),
                             static_cast<QWidget*>(cell.zero_pitch),
                             static_cast<QWidget*>(cell.zero_depth),
                             static_cast<QWidget*>(cell.cut)}) {
      control->installEventFilter(this);
    }

    connect(cell.on, &QCheckBox::toggled, this, [this, group, index](bool checked) {
      if (refreshing_) return;
      const std::size_t corner = cornerOf(group);
      if (checked != state_->sectionEnabledAt(corner, index)) {
        state_->toggleSectionAt(corner, index);
      }
    });
    for (QSpinBox* box : {cell.pole_pitch, cell.pole_res}) {
      connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [this, group, index](int) {
        if (refreshing_) return;
        pushPole(group, index);
      });
    }
    for (QSpinBox* box : {cell.zero_pitch, cell.zero_depth}) {
      connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [this, group, index](int) {
        if (refreshing_) return;
        pushZero(group, index);
      });
    }
    connect(cell.cut, qOverload<int>(&QSpinBox::valueChanged), this,
            [this, group, index](int value) {
              if (refreshing_) return;
              state_->setCutAt(cornerOf(group), index, value);
            });
  }
}

std::size_t RowTable::cornerOf(std::size_t group) const noexcept {
  return from_corner_ + group;
}

void RowTable::pushPole(std::size_t group, std::size_t index) {
  const Cell& cell = groups_[group][index];
  state_->setWordsAt(cornerOf(group), index, EditorState::Lane::kPole,
                     p2k::dial_word(static_cast<std::size_t>(cell.pole_pitch->value())),
                     p2k::dial_word(static_cast<std::size_t>(kTopDial - cell.pole_res->value())));
}

void RowTable::pushZero(std::size_t group, std::size_t index) {
  const Cell& cell = groups_[group][index];
  const std::size_t corner = cornerOf(group);
  if (cell.zero_depth->value() == 0) {
    state_->removeZeroAt(corner, index);
    return;
  }
  state_->setWordsAt(corner, index, EditorState::Lane::kZero,
                     p2k::dial_word(static_cast<std::size_t>(cell.zero_pitch->value())),
                     p2k::dial_word(static_cast<std::size_t>(kTopDial - cell.zero_depth->value())));
}

bool RowTable::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
    for (std::size_t index = 0; index < labels_.size(); ++index) {
      if (watched == labels_[index]) {
        state_->selectSection(index);
        return QWidget::eventFilter(watched, event);
      }
    }
    for (std::size_t group = 0; group < groups_.size(); ++group) {
      for (std::size_t index = 0; index < groups_[group].size(); ++index) {
        const Cell& cell = groups_[group][index];
        const bool pole = watched == cell.pole_pitch || watched == cell.pole_res;
        const bool zero = watched == cell.zero_pitch || watched == cell.zero_depth;
        if (watched == cell.on || watched == cell.cut || pole || zero) {
          state_->setEditingCorner(cornerOf(group));
          state_->selectSection(index);
          if (pole) state_->selectRoot(index, EditorState::Lane::kPole);
          if (zero) state_->selectRoot(index, EditorState::Lane::kZero);
          return QWidget::eventFilter(watched, event);
        }
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void RowTable::refresh() {
  refreshing_ = true;
  from_corner_ = state_->editingCorner() >= 2 ? 2 : 0;
  const std::size_t selected = state_->selectedSection();
  for (std::size_t group = 0; group < groups_.size(); ++group) {
    const std::size_t corner = cornerOf(group);
    QLabel* header = group == 0 ? from_header_ : to_header_;
    const QString title = QStringLiteral("%1 · CORNER %2")
                              .arg(group == 0 ? QStringLiteral("FROM") : QStringLiteral("TO"))
                              .arg(corner + 1);
    header->setText(corner == state_->editingCorner() ? QStringLiteral("> ") + title : title);
    for (std::size_t index = 0; index < groups_[group].size(); ++index) {
      const Cell& cell = groups_[group][index];
      const QSignalBlocker on_blocker(cell.on);
      const QSignalBlocker pole_pitch_blocker(cell.pole_pitch);
      const QSignalBlocker pole_res_blocker(cell.pole_res);
      const QSignalBlocker zero_pitch_blocker(cell.zero_pitch);
      const QSignalBlocker zero_depth_blocker(cell.zero_depth);
      const QSignalBlocker cut_blocker(cell.cut);
      const bool enabled = state_->sectionEnabledAt(corner, index);
      const bool has_zero = enabled && state_->zeroPresentAt(corner, index);
      const auto& words = state_->packed().words[corner][index];
      const int pole_pitch = static_cast<int>(p2k::dial_of_word(words[2]));

      cell.on->setChecked(enabled);
      cell.pole_pitch->setValue(pole_pitch);
      cell.pole_res->setValue(kTopDial - static_cast<int>(p2k::dial_of_word(words[3])));
      cell.pole_readout->setText(enabled ? readout(words[2], words[3]) : QStringLiteral("—"));
      cell.zero_pitch->setValue(has_zero ? static_cast<int>(p2k::dial_of_word(words[0]))
                                         : pole_pitch);
      cell.zero_depth->setValue(
          has_zero ? kTopDial - static_cast<int>(p2k::dial_of_word(words[1])) : 0);
      cell.zero_readout->setText(has_zero ? readout(words[0], words[1]) : QStringLiteral("—"));
      cell.cut->setValue(state_->cutAt(corner, index));

      cell.pole_pitch->setEnabled(enabled);
      cell.pole_res->setEnabled(enabled);
      cell.zero_pitch->setEnabled(has_zero);
      cell.zero_depth->setEnabled(enabled);
      cell.cut->setEnabled(enabled);
    }
  }
  for (std::size_t index = 0; index < labels_.size(); ++index) {
    labels_[index]->setText(index == selected ? QStringLiteral("> %1").arg(index + 1)
                                              : QString::number(index + 1));
  }
  refreshing_ = false;
}
