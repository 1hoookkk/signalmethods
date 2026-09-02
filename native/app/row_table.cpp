#include "row_table.hpp"

#include "trench/core/p2k.hpp"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <variant>

namespace {

namespace p2k = trench::core::p2k;

constexpr int kTopDial = static_cast<int>(p2k::kDialCount) - 1;
constexpr int kEntryWidth = 40;
constexpr int kFaderHeight = 96;

int maxPoleRes() {
  static const int value = kTopDial - static_cast<int>(p2k::dial_of_word(p2k::kPoleCeilingRsqWord));
  return value;
}

struct Root {
  double hz{};
  double bw_hz{};
};

Root rootOf(std::uint16_t mag, std::uint16_t rsq) {
  const auto [p, q] = p2k::pq(mag, rsq);
  const auto roots =
      trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz);
  if (const auto* tone = std::get_if<trench::core::native::Resonant>(&roots)) {
    return {tone->hz, tone->bw_hz};
  }
  const auto& real = std::get<trench::core::native::RealRoots>(roots);
  return {real.a_hz, 0.0};
}

double resonanceOf(const Root& root) {
  return root.bw_hz > 0.0 ? root.hz / root.bw_hz : 0.0;
}

QString hzText(std::uint16_t mag, std::uint16_t rsq) {
  return QString::asprintf("%.1f Hz", rootOf(mag, rsq).hz);
}

QString resonanceText(std::uint16_t mag, std::uint16_t rsq) {
  return QString::asprintf("Q %.1f", resonanceOf(rootOf(mag, rsq)));
}

std::uint16_t zeroMagOnPole(const trench::core::PackedSection& words, std::uint16_t zero_rsq) {
  return p2k::mag_word_for(rootOf(words[2], words[3]).hz, zero_rsq);
}

bool sameNote(const trench::core::PackedSection& words) {
  return words[0] == zeroMagOnPole(words, words[1]);
}

double gainDbOf(const trench::core::PackedSection& words) {
  const Root pole = rootOf(words[2], words[3]);
  return trench::core::section_response_db(trench::core::section_words_to_biquad(words),
                                           pole.hz, EditorState::kDatumHz);
}

QString gainText(const trench::core::PackedSection& words) {
  return QString::asprintf("%+.1f dB", gainDbOf(words));
}

}

RowTable::RowTable(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(4, 4, 4, 4);
  column->setSpacing(4);

  auto* picker = new QHBoxLayout;
  picker->setSpacing(4);
  buildPicker(picker);
  picker->addStretch(1);
  column->addLayout(picker);

  auto* console = new QHBoxLayout;
  console->setSpacing(3);
  for (std::size_t index = 0; index < strips_.size(); ++index) {
    buildStrip(index, console);
  }
  console->addStretch(1);
  column->addLayout(console);

  connect(state_, &EditorState::changed, this, &RowTable::refresh);
  connect(state_, &EditorState::selectionChanged, this,
          [this](std::size_t) { refresh(); });
  refresh();
  setFixedHeight(sizeHint().height());
}

void RowTable::buildPicker(QBoxLayout* into) {
  static const std::array<const char*, 4> kTexts{"FROM · Q0", "TO · Q0", "FROM · Q100",
                                                 "TO · Q100"};
  auto* group = new QButtonGroup(this);
  group->setExclusive(true);
  for (std::size_t index = 0; index < corner_buttons_.size(); ++index) {
    auto* button = new QPushButton(QString::fromUtf8(kTexts[index]), this);
    button->setObjectName(QStringLiteral("corner%1").arg(index));
    button->setCheckable(true);
    button->setFocusPolicy(Qt::NoFocus);
    group->addButton(button, static_cast<int>(index));
    into->addWidget(button);
    corner_buttons_[index] = button;
    connect(button, &QPushButton::clicked, this, [this, index] {
      if (refreshing_) return;
      state_->setEditingCorner(index);
      refresh();
    });
  }
}

RowTable::Column RowTable::buildColumn(QBoxLayout* into, const QString& caption,
                                       const QString& prefix, std::size_t index,
                                       int maximum) {
  Column cell;
  auto* stack = new QVBoxLayout;
  stack->setSpacing(2);
  stack->setContentsMargins(0, 0, 0, 0);

  cell.caption = new QLabel(caption, this);
  cell.caption->setAlignment(Qt::AlignHCenter);
  cell.fader = new QSlider(Qt::Vertical, this);
  cell.fader->setObjectName(QStringLiteral("%1Fader%2").arg(prefix).arg(index));
  cell.fader->setRange(0, maximum);
  cell.fader->setMinimumHeight(kFaderHeight);
  cell.entry = new QLineEdit(this);
  cell.entry->setObjectName(QStringLiteral("%1Entry%2").arg(prefix).arg(index));
  cell.entry->setFixedWidth(kEntryWidth);
  cell.entry->setAlignment(Qt::AlignRight);
  cell.readout = new QLabel(this);
  cell.readout->setObjectName(QStringLiteral("%1Readout%2").arg(prefix).arg(index));
  cell.readout->setFixedWidth(kEntryWidth);
  cell.readout->setAlignment(Qt::AlignHCenter);

  stack->addWidget(cell.caption);
  stack->addWidget(cell.fader, 1, Qt::AlignHCenter);
  stack->addWidget(cell.entry);
  stack->addWidget(cell.readout);
  into->addLayout(stack);
  return cell;
}

void RowTable::buildStrip(std::size_t index, QBoxLayout* into) {
  Strip& strip = strips_[index];
  auto* stack = new QVBoxLayout;
  stack->setSpacing(2);
  stack->setContentsMargins(0, 0, 0, 0);

  strip.on = new QCheckBox(QString::number(index + 1), this);
  strip.on->setObjectName(QStringLiteral("on%1").arg(index));
  stack->addWidget(strip.on);

  auto* columns = new QHBoxLayout;
  columns->setSpacing(1);
  columns->setContentsMargins(0, 0, 0, 0);
  strip.freq = buildColumn(columns, QStringLiteral("FREQ"), QStringLiteral("freq"), index,
                           static_cast<int>(p2k::kMaxMagByte));
  strip.q = buildColumn(columns, QStringLiteral("Q"), QStringLiteral("q"), index,
                        maxPoleRes());
  strip.gain = buildColumn(columns, QStringLiteral("GAIN"), QStringLiteral("gain"), index,
                           kTopDial);
  strip.zero = buildColumn(columns, QStringLiteral("ZERO"), QStringLiteral("zero"), index,
                           static_cast<int>(p2k::kMaxMagByte));
  stack->addLayout(columns);

  strip.lock = new QPushButton(QStringLiteral("LOCK"), this);
  strip.lock->setObjectName(QStringLiteral("lock%1").arg(index));
  strip.lock->setCheckable(true);
  stack->addWidget(strip.lock);

  auto* cut_row = new QHBoxLayout;
  cut_row->setSpacing(2);
  cut_row->setContentsMargins(0, 0, 0, 0);
  cut_row->addWidget(new QLabel(QStringLiteral("CUT"), this));
  strip.cut = new QSpinBox(this);
  strip.cut->setObjectName(QStringLiteral("cut%1").arg(index));
  strip.cut->setRange(0, EditorState::kMaxCut);
  strip.cut->setKeyboardTracking(false);
  strip.cut->setFixedWidth(kEntryWidth);
  cut_row->addWidget(strip.cut);
  cut_row->addStretch(1);
  stack->addLayout(cut_row);

  into->addLayout(stack);

  for (QWidget* control : {static_cast<QWidget*>(strip.on),
                           static_cast<QWidget*>(strip.freq.fader),
                           static_cast<QWidget*>(strip.freq.entry),
                           static_cast<QWidget*>(strip.q.fader),
                           static_cast<QWidget*>(strip.q.entry),
                           static_cast<QWidget*>(strip.gain.fader),
                           static_cast<QWidget*>(strip.gain.entry),
                           static_cast<QWidget*>(strip.zero.fader),
                           static_cast<QWidget*>(strip.zero.entry),
                           static_cast<QWidget*>(strip.lock),
                           static_cast<QWidget*>(strip.cut)}) {
    control->installEventFilter(this);
  }

  connect(strip.on, &QCheckBox::toggled, this, [this, index](bool checked) {
    if (refreshing_) return;
    if (checked != state_->sectionEnabledAt(corner(), index)) {
      state_->toggleSectionAt(corner(), index);
    }
  });
  connect(strip.freq.fader, &QSlider::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushPole(index, lockedNow(index));
  });
  connect(strip.q.fader, &QSlider::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushPole(index, false);
  });
  connect(strip.gain.fader, &QSlider::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushGain(index);
  });
  connect(strip.zero.fader, &QSlider::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushZeroMag(index);
  });
  connect(strip.freq.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kFreq); });
  connect(strip.q.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kQ); });
  connect(strip.gain.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kGain); });
  connect(strip.zero.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kZero); });
  connect(strip.lock, &QPushButton::clicked, this, [this, index](bool checked) {
    if (refreshing_) return;
    Strip& mine = strips_[index];
    if (!checked) {
      mine.unlocked_by_hand = true;
      refresh();
      return;
    }
    mine.unlocked_by_hand = false;
    const std::size_t which = corner();
    if (state_->zeroPresentAt(which, index)) {
      const auto words = state_->packed().words[which][index];
      state_->setWordsAt(which, index, EditorState::Lane::kZero, zeroMagOnPole(words, words[1]),
                         words[1]);
    }
    refresh();
  });
  connect(strip.cut, qOverload<int>(&QSpinBox::valueChanged), this,
          [this, index](int value) {
            if (refreshing_) return;
            state_->setCutAt(corner(), index, value);
          });
}

std::size_t RowTable::corner() const noexcept { return state_->editingCorner(); }

bool RowTable::lockedNow(std::size_t index) const {
  const std::size_t which = state_->editingCorner();
  if (!state_->zeroPresentAt(which, index)) return false;
  const auto& words = state_->packed().words[which][index];
  return sameNote(words) && !strips_[index].unlocked_by_hand;
}

void RowTable::pushPole(std::size_t index, bool carry_zero) {
  const Strip& strip = strips_[index];
  const std::size_t which = corner();
  const auto words = state_->packed().words[which][index];
  const std::uint16_t mag = p2k::dial_word(static_cast<std::size_t>(strip.freq.fader->value()));
  state_->setWordsAt(
      which, index, EditorState::Lane::kPole, mag,
      p2k::dial_word(static_cast<std::size_t>(kTopDial - strip.q.fader->value())));
  if (carry_zero && state_->zeroPresentAt(which, index)) {
    const auto moved = state_->packed().words[which][index];
    state_->setWordsAt(which, index, EditorState::Lane::kZero, zeroMagOnPole(moved, words[1]),
                       words[1]);
  }
}

void RowTable::pushGain(std::size_t index) {
  const Strip& strip = strips_[index];
  const std::size_t which = corner();
  const int value = strip.gain.fader->value();
  if (value == 0) {
    state_->removeZeroAt(which, index);
    return;
  }
  const auto words = state_->packed().words[which][index];
  const std::uint16_t rsq = p2k::dial_word(static_cast<std::size_t>(kTopDial - value));
  const bool on_pole = !state_->zeroPresentAt(which, index) || lockedNow(index);
  const std::uint16_t mag =
      on_pole ? zeroMagOnPole(words, rsq)
              : p2k::dial_word(static_cast<std::size_t>(strip.zero.fader->value()));
  state_->setWordsAt(which, index, EditorState::Lane::kZero, mag, rsq);
}

void RowTable::pushZeroMag(std::size_t index) {
  const Strip& strip = strips_[index];
  const std::size_t which = corner();
  if (!state_->zeroPresentAt(which, index)) return;
  const auto words = state_->packed().words[which][index];
  state_->setWordsAt(which, index, EditorState::Lane::kZero,
                     p2k::dial_word(static_cast<std::size_t>(strip.zero.fader->value())),
                     words[1]);
}

void RowTable::landEntry(std::size_t index, Kind kind) {
  if (refreshing_) return;
  Strip& strip = strips_[index];
  QLineEdit* entry = kind == Kind::kFreq   ? strip.freq.entry
                     : kind == Kind::kQ    ? strip.q.entry
                     : kind == Kind::kGain ? strip.gain.entry
                                           : strip.zero.entry;
  bool ok = false;
  const double typed = entry->text().toDouble(&ok);
  if (!ok) {
    refresh();
    return;
  }
  const std::size_t which = corner();
  const auto words = state_->packed().words[which][index];
  int best = -1;
  double best_cost = std::numeric_limits<double>::infinity();
  const auto consider = [&](int dial, double cost) {
    if (!std::isfinite(cost) || cost >= best_cost) return;
    best_cost = cost;
    best = dial;
  };

  if (kind == Kind::kFreq || kind == Kind::kZero) {
    if (!(typed > 0.0)) {
      refresh();
      return;
    }
    const double target = std::log2(typed);
    const std::uint16_t rsq = kind == Kind::kFreq ? words[3] : words[1];
    for (int dial = 0; dial <= static_cast<int>(p2k::kMaxMagByte); ++dial) {
      const Root root = rootOf(p2k::dial_word(static_cast<std::size_t>(dial)), rsq);
      if (!(root.hz > 0.0) || !std::isfinite(root.hz)) continue;
      consider(dial, std::abs(std::log2(root.hz) - target));
    }
    if (best >= 0) {
      (kind == Kind::kFreq ? strip.freq.fader : strip.zero.fader)->setValue(best);
    }
  } else if (kind == Kind::kQ) {
    if (!(typed > 0.0)) {
      refresh();
      return;
    }
    const double target = std::log2(typed);
    for (int dial = 0; dial <= maxPoleRes(); ++dial) {
      const Root root = rootOf(
          words[2], p2k::dial_word(static_cast<std::size_t>(kTopDial - dial)));
      const double resonance = resonanceOf(root);
      if (!(resonance > 0.0) || !std::isfinite(resonance)) continue;
      consider(dial, std::abs(std::log2(resonance) - target));
    }
    if (best >= 0) strip.q.fader->setValue(best);
  } else {
    const bool on_pole = !state_->zeroPresentAt(which, index) || lockedNow(index);
    for (int dial = 0; dial <= kTopDial; ++dial) {
      trench::core::PackedSection candidate = words;
      if (dial == 0) {
        candidate[0] = trench::core::kIdentitySection[0];
        candidate[1] = trench::core::kIdentitySection[1];
      } else {
        candidate[1] = p2k::dial_word(static_cast<std::size_t>(kTopDial - dial));
        candidate[0] = on_pole ? zeroMagOnPole(words, candidate[1])
                               : p2k::dial_word(static_cast<std::size_t>(
                                     strip.zero.fader->value()));
      }
      consider(dial, std::abs(gainDbOf(candidate) - typed));
    }
    if (best >= 0) strip.gain.fader->setValue(best);
  }
  refresh();
}

void RowTable::selectFrom(std::size_t index, EditorState::Lane lane) {
  state_->selectSection(index);
  state_->selectRoot(index, lane);
}

bool RowTable::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
    for (std::size_t index = 0; index < strips_.size(); ++index) {
      const Strip& strip = strips_[index];
      const bool pole = watched == strip.freq.fader || watched == strip.freq.entry ||
                        watched == strip.q.fader || watched == strip.q.entry;
      const bool zero = watched == strip.gain.fader || watched == strip.gain.entry ||
                        watched == strip.zero.fader || watched == strip.zero.entry;
      if (pole || zero || watched == strip.on || watched == strip.cut ||
          watched == strip.lock) {
        selectFrom(index, pole ? EditorState::Lane::kPole : EditorState::Lane::kZero);
        break;
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void RowTable::refresh() {
  refreshing_ = true;
  const std::size_t which = state_->editingCorner();
  for (std::size_t index = 0; index < corner_buttons_.size(); ++index) {
    const QSignalBlocker blocker(corner_buttons_[index]);
    corner_buttons_[index]->setChecked(index == which);
  }
  const std::size_t selected = state_->selectedSection();
  for (std::size_t index = 0; index < strips_.size(); ++index) {
    Strip& strip = strips_[index];
    const auto& words = state_->packed().words[which][index];
    const bool enabled = state_->sectionEnabledAt(which, index);
    const bool has_zero = enabled && state_->zeroPresentAt(which, index);
    const bool derived = has_zero && sameNote(words);
    if (!derived) strip.unlocked_by_hand = false;
    const bool locked = derived && !strip.unlocked_by_hand;

    const QSignalBlocker on_blocker(strip.on);
    const QSignalBlocker freq_blocker(strip.freq.fader);
    const QSignalBlocker q_blocker(strip.q.fader);
    const QSignalBlocker gain_blocker(strip.gain.fader);
    const QSignalBlocker zero_blocker(strip.zero.fader);
    const QSignalBlocker lock_blocker(strip.lock);
    const QSignalBlocker cut_blocker(strip.cut);

    const int pole_dial = static_cast<int>(p2k::dial_of_word(words[2]));
    strip.on->setChecked(enabled);
    strip.on->setText(index == selected
                          ? QStringLiteral("> %1").arg(index + 1)
                          : QString::number(index + 1));
    strip.freq.fader->setValue(pole_dial);
    strip.freq.readout->setText(enabled ? hzText(words[2], words[3]) : QStringLiteral("—"));
    strip.q.fader->setValue(kTopDial - static_cast<int>(p2k::dial_of_word(words[3])));
    strip.q.readout->setText(enabled ? resonanceText(words[2], words[3]) : QStringLiteral("—"));
    strip.gain.fader->setValue(
        has_zero ? kTopDial - static_cast<int>(p2k::dial_of_word(words[1])) : 0);
    strip.gain.readout->setText(enabled ? gainText(words) : QStringLiteral("—"));
    strip.zero.fader->setValue(has_zero ? static_cast<int>(p2k::dial_of_word(words[0]))
                                        : pole_dial);
    strip.zero.readout->setText(has_zero ? hzText(words[0], words[1])
                                         : QStringLiteral("—"));
    strip.lock->setChecked(locked);
    strip.cut->setValue(state_->cutAt(which, index));

    const Root pole = rootOf(words[2], words[3]);
    strip.freq.entry->setText(enabled ? QString::asprintf("%.1f", pole.hz) : QString());
    strip.q.entry->setText(enabled ? QString::asprintf("%.1f", resonanceOf(pole)) : QString());
    strip.gain.entry->setText(enabled ? QString::asprintf("%+.1f", gainDbOf(words))
                                      : QString());
    strip.zero.entry->setText(
        has_zero ? QString::asprintf("%.1f", rootOf(words[0], words[1]).hz) : QString());

    strip.freq.fader->setEnabled(enabled);
    strip.freq.entry->setEnabled(enabled);
    strip.q.fader->setEnabled(enabled);
    strip.q.entry->setEnabled(enabled);
    strip.gain.fader->setEnabled(enabled);
    strip.gain.entry->setEnabled(enabled);
    strip.lock->setEnabled(enabled);
    strip.cut->setEnabled(enabled);
    strip.zero.fader->setEnabled(has_zero && !locked);
    strip.zero.entry->setEnabled(has_zero && !locked);
  }
  refreshing_ = false;
}
