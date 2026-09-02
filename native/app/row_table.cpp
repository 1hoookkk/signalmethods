#include "row_table.hpp"

#include "word_dial.hpp"

#include "trench/core/p2k.hpp"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <variant>

namespace {

namespace p2k = trench::core::p2k;

constexpr int kTopDial = static_cast<int>(p2k::kDialCount) - 1;
constexpr int kEntryWidth = 48;
constexpr int kOffsetSpan = 48;
constexpr double kPairWidthRatio = 4.0;
constexpr double kEdgeDecayHz = 1.0;

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

double gainDbOf(const trench::core::PackedSection& words) {
  const Root pole = rootOf(words[2], words[3]);
  return trench::core::section_response_db(trench::core::section_words_to_biquad(words),
                                           pole.hz, EditorState::kDatumHz);
}

QString gainText(const trench::core::PackedSection& words) {
  return QString::asprintf("%+.1f dB", gainDbOf(words));
}

double semitonesOf(const trench::core::PackedSection& words) {
  const double zero_hz = rootOf(words[0], words[1]).hz;
  const double pole_hz = rootOf(words[2], words[3]).hz;
  if (!(zero_hz > 0.0) || !(pole_hz > 0.0)) return 0.0;
  return 12.0 * std::log2(zero_hz / pole_hz);
}

QString offsetText(const trench::core::PackedSection& words) {
  return QString::asprintf("%+.1f st", semitonesOf(words));
}

double radiusOf(double bw_hz) {
  return std::exp(-std::numbers::pi * bw_hz / EditorState::kDatumHz);
}

const std::array<const char*, 5> kShapeNames{"RESONATOR", "PAIR", "NOTCH", "EDGE HP",
                                             "EDGE LP"};

}

RowTable::RowTable(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(4, 4, 4, 4);
  column->setSpacing(4);

  buildPicker();
  column->addWidget(picker_, 0, Qt::AlignLeft);

  auto* console = new QHBoxLayout;
  console->setSpacing(6);
  for (std::size_t index = 0; index < strips_.size(); ++index) {
    buildStrip(index, console);
  }
  console->addStretch(1);
  column->addLayout(console);

  connect(state_, &EditorState::changed, this, &RowTable::refresh);
  connect(state_, &EditorState::selectionChanged, this,
          [this](std::size_t) { refresh(); });
  refresh();
}

void RowTable::buildPicker() {
  static const std::array<const char*, 4> kTexts{"FROM · Q0", "TO · Q0", "FROM · Q100",
                                                 "TO · Q100"};
  picker_ = new QWidget(this);
  picker_->setObjectName(QStringLiteral("cornerPicker"));
  auto* grid = new QGridLayout(picker_);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(3);
  auto* group = new QButtonGroup(picker_);
  group->setExclusive(true);
  for (std::size_t index = 0; index < corner_buttons_.size(); ++index) {
    auto* button = new QPushButton(QString::fromUtf8(kTexts[index]), picker_);
    button->setObjectName(QStringLiteral("corner%1").arg(index));
    button->setCheckable(true);
    button->setFocusPolicy(Qt::NoFocus);
    group->addButton(button, static_cast<int>(index));
    grid->addWidget(button, static_cast<int>(index / 2), static_cast<int>(index % 2));
    corner_buttons_[index] = button;
    connect(button, &QPushButton::clicked, this, [this, index] {
      if (refreshing_) return;
      state_->setEditingCorner(index);
      refresh();
    });
  }
}

QWidget* RowTable::takePicker() {
  layout()->removeWidget(picker_);
  picker_->setParent(nullptr);
  return picker_;
}

RowTable::Column RowTable::buildColumn(QBoxLayout* into, const QString& caption,
                                       const QString& prefix, std::size_t index,
                                       int minimum, int maximum) {
  Column cell;
  auto* stack = new QVBoxLayout;
  stack->setSpacing(2);
  stack->setContentsMargins(0, 0, 0, 0);

  cell.caption = new QLabel(caption, this);
  cell.caption->setAlignment(Qt::AlignHCenter);
  cell.dial = new WordDial(this);
  cell.dial->setObjectName(QStringLiteral("%1Dial%2").arg(prefix).arg(index));
  cell.dial->setRange(minimum, maximum);
  cell.dial->setFixedWidth(kEntryWidth);
  cell.dial->readout()->setObjectName(QStringLiteral("%1Readout%2").arg(prefix).arg(index));
  cell.entry = new QLineEdit(this);
  cell.entry->setObjectName(QStringLiteral("%1Entry%2").arg(prefix).arg(index));
  cell.entry->setFixedWidth(kEntryWidth);
  cell.entry->setAlignment(Qt::AlignRight);

  stack->addWidget(cell.caption);
  stack->addWidget(cell.dial);
  stack->addWidget(cell.entry);
  into->addLayout(stack);
  return cell;
}

void RowTable::buildStrip(std::size_t index, QBoxLayout* into) {
  Strip& strip = strips_[index];
  auto* card = new QWidget(this);
  card->setObjectName(QStringLiteral("card%1").arg(index));
  auto* stack = new QVBoxLayout(card);
  stack->setSpacing(3);
  stack->setContentsMargins(4, 2, 4, 2);

  auto* head = new QHBoxLayout;
  head->setSpacing(4);
  head->setContentsMargins(0, 0, 0, 0);
  strip.on = new QCheckBox(QString::number(index + 1), card);
  strip.on->setObjectName(QStringLiteral("on%1").arg(index));
  QFont big = strip.on->font();
  big.setPointSize(big.pointSize() + 6);
  big.setBold(true);
  strip.on->setFont(big);
  head->addWidget(strip.on);
  head->addStretch(1);
  strip.shape = new QComboBox(card);
  strip.shape->setObjectName(QStringLiteral("shape%1").arg(index));
  for (const char* name : kShapeNames) strip.shape->addItem(QString::fromUtf8(name));
  strip.shape->setFocusPolicy(Qt::ClickFocus);
  head->addWidget(strip.shape);
  stack->addLayout(head);

  auto* columns = new QHBoxLayout;
  columns->setSpacing(3);
  columns->setContentsMargins(0, 0, 0, 0);
  strip.freq = buildColumn(columns, QStringLiteral("FREQ"), QStringLiteral("freq"), index, 0,
                           static_cast<int>(p2k::kMaxMagByte));
  strip.q = buildColumn(columns, QStringLiteral("Q"), QStringLiteral("q"), index, 0,
                        maxPoleRes());
  strip.gain = buildColumn(columns, QStringLiteral("GAIN"), QStringLiteral("gain"), index, 1,
                           kTopDial);
  strip.offset = buildColumn(columns, QStringLiteral("OFFSET"), QStringLiteral("offset"),
                             index, -kOffsetSpan, kOffsetSpan);
  stack->addLayout(columns);

  auto* cut_row = new QHBoxLayout;
  cut_row->setSpacing(4);
  cut_row->setContentsMargins(0, 0, 0, 0);
  cut_row->addStretch(1);
  cut_row->addWidget(new QLabel(QStringLiteral("CUT"), card));
  strip.cut = new QSpinBox(card);
  strip.cut->setObjectName(QStringLiteral("cut%1").arg(index));
  strip.cut->setRange(0, EditorState::kMaxCut);
  strip.cut->setKeyboardTracking(false);
  strip.cut->setFixedWidth(kEntryWidth);
  cut_row->addWidget(strip.cut);
  stack->addLayout(cut_row);

  into->addWidget(card);

  for (QWidget* control : {static_cast<QWidget*>(strip.on),
                           static_cast<QWidget*>(strip.shape),
                           static_cast<QWidget*>(strip.freq.dial),
                           static_cast<QWidget*>(strip.freq.entry),
                           static_cast<QWidget*>(strip.q.dial),
                           static_cast<QWidget*>(strip.q.entry),
                           static_cast<QWidget*>(strip.gain.dial),
                           static_cast<QWidget*>(strip.gain.entry),
                           static_cast<QWidget*>(strip.offset.dial),
                           static_cast<QWidget*>(strip.offset.entry),
                           static_cast<QWidget*>(strip.cut)}) {
    control->installEventFilter(this);
  }

  connect(strip.on, &QCheckBox::toggled, this, [this, index](bool checked) {
    if (refreshing_) return;
    if (checked != state_->sectionEnabledAt(corner(), index)) {
      state_->toggleSectionAt(corner(), index);
    }
  });
  connect(strip.shape, &QComboBox::currentIndexChanged, this, [this, index](int which) {
    if (refreshing_) return;
    pushShape(index, static_cast<Shape>(which));
  });
  connect(strip.freq.dial, &WordDial::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushPole(index);
  });
  connect(strip.q.dial, &WordDial::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushPole(index);
  });
  connect(strip.gain.dial, &WordDial::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushGain(index);
  });
  connect(strip.offset.dial, &WordDial::valueChanged, this, [this, index](int) {
    if (refreshing_) return;
    pushOffset(index);
  });
  connect(strip.freq.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kFreq); });
  connect(strip.q.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kQ); });
  connect(strip.gain.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kGain); });
  connect(strip.offset.entry, &QLineEdit::editingFinished, this,
          [this, index] { landEntry(index, Kind::kOffset); });
  connect(strip.cut, qOverload<int>(&QSpinBox::valueChanged), this,
          [this, index](int value) {
            if (refreshing_) return;
            state_->setCutAt(corner(), index, value);
          });
}

std::size_t RowTable::corner() const noexcept { return state_->editingCorner(); }

RowTable::Shape RowTable::shapeAt(std::size_t which, std::size_t index) const {
  if (!state_->zeroPresentAt(which, index)) return Shape::kResonator;
  const auto& zero = state_->sectionAt(which, index).zero;
  if (const auto* real = std::get_if<trench::core::native::RealRoots>(&zero)) {
    return real->a_hz > 0.0 ? Shape::kEdgeHigh : Shape::kEdgeLow;
  }
  const auto& words = state_->packed().words[which][index];
  return words[1] == p2k::kS6ZeroRsqWord ? Shape::kNotch : Shape::kPair;
}

double RowTable::offsetNow(std::size_t index) const {
  const Shape shape = shapeAt(corner(), index);
  if (shape != Shape::kPair && shape != Shape::kNotch) return 0.0;
  return semitonesOf(state_->packed().words[corner()][index]);
}

void RowTable::seatZero(std::size_t index, double semitones, std::uint16_t rsq) {
  const std::size_t which = corner();
  const auto words = state_->packed().words[which][index];
  const double pole_hz = rootOf(words[2], words[3]).hz;
  const double target_hz = pole_hz * std::exp2(semitones / 12.0);
  state_->setWordsAt(which, index, EditorState::Lane::kZero, p2k::mag_word_for(target_hz, rsq),
                     rsq);
}

void RowTable::pushPole(std::size_t index) {
  const Strip& strip = strips_[index];
  const std::size_t which = corner();
  const Shape shape = shapeAt(which, index);
  const double semitones = static_cast<double>(strip.offset.dial->value());
  state_->beginUndoGroup();
  state_->setWordsAt(
      which, index, EditorState::Lane::kPole,
      p2k::dial_word(static_cast<std::size_t>(strip.freq.dial->value())),
      p2k::dial_word(static_cast<std::size_t>(kTopDial - strip.q.dial->value())));
  if (shape == Shape::kPair || shape == Shape::kNotch) {
    seatZero(index, semitones, state_->packed().words[which][index][1]);
  }
  state_->endUndoGroup();
}

void RowTable::pushGain(std::size_t index) {
  const Strip& strip = strips_[index];
  if (shapeAt(corner(), index) != Shape::kPair) return;
  seatZero(index, static_cast<double>(strip.offset.dial->value()),
           p2k::dial_word(static_cast<std::size_t>(strip.gain.dial->value())));
}

void RowTable::pushOffset(std::size_t index) {
  const Strip& strip = strips_[index];
  const Shape shape = shapeAt(corner(), index);
  if (shape != Shape::kPair && shape != Shape::kNotch) return;
  seatZero(index, static_cast<double>(strip.offset.dial->value()),
           state_->packed().words[corner()][index][1]);
}

void RowTable::pushShape(std::size_t index, Shape shape) {
  const std::size_t which = corner();
  const Shape current = shapeAt(which, index);
  if (shape == current) return;
  const double semitones = offsetNow(index);
  const auto words = state_->packed().words[which][index];
  const Root pole = rootOf(words[2], words[3]);
  switch (shape) {
    case Shape::kResonator:
      state_->removeZeroAt(which, index);
      break;
    case Shape::kPair: {
      const double bw_hz = std::clamp(pole.bw_hz * kPairWidthRatio, EditorState::kMinBandwidthHz,
                                      EditorState::kMaxBandwidthHz);
      const auto [mag, rsq] = p2k::words_from_root(pole.hz, radiusOf(bw_hz));
      seatZero(index, semitones, rsq);
      break;
    }
    case Shape::kNotch:
      seatZero(index, semitones, p2k::kS6ZeroRsqWord);
      break;
    case Shape::kEdgeHigh:
      state_->setRealRootAt(which, index, EditorState::Lane::kZero, kEdgeDecayHz, kEdgeDecayHz);
      break;
    case Shape::kEdgeLow:
      state_->setRealRootAt(which, index, EditorState::Lane::kZero, -kEdgeDecayHz,
                            -kEdgeDecayHz);
      break;
  }
  refresh();
}

void RowTable::landEntry(std::size_t index, Kind kind) {
  if (refreshing_) return;
  Strip& strip = strips_[index];
  QLineEdit* entry = kind == Kind::kFreq   ? strip.freq.entry
                     : kind == Kind::kQ    ? strip.q.entry
                     : kind == Kind::kGain ? strip.gain.entry
                                           : strip.offset.entry;
  bool ok = false;
  const double typed = entry->text().toDouble(&ok);
  if (!ok) {
    refresh();
    return;
  }
  const std::size_t which = corner();
  const auto words = state_->packed().words[which][index];
  const Shape shape = shapeAt(which, index);
  const Root pole = rootOf(words[2], words[3]);
  int best = -1;
  double best_cost = std::numeric_limits<double>::infinity();
  const auto consider = [&](int dial, double cost) {
    if (!std::isfinite(cost) || cost >= best_cost) return;
    best_cost = cost;
    best = dial;
  };

  if (kind == Kind::kFreq) {
    if (!(typed > 0.0)) {
      refresh();
      return;
    }
    const double target = std::log2(typed);
    for (int dial = 0; dial <= static_cast<int>(p2k::kMaxMagByte); ++dial) {
      const Root root = rootOf(p2k::dial_word(static_cast<std::size_t>(dial)), words[3]);
      if (!(root.hz > 0.0) || !std::isfinite(root.hz)) continue;
      consider(dial, std::abs(std::log2(root.hz) - target));
    }
    if (best >= 0) strip.freq.dial->setValue(best);
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
    if (best >= 0) strip.q.dial->setValue(best);
  } else if (kind == Kind::kGain) {
    if (shape != Shape::kPair) {
      refresh();
      return;
    }
    const double target_hz = pole.hz * std::exp2(offsetNow(index) / 12.0);
    for (int dial = 1; dial <= kTopDial; ++dial) {
      trench::core::PackedSection candidate = words;
      candidate[1] = p2k::dial_word(static_cast<std::size_t>(dial));
      candidate[0] = p2k::mag_word_for(target_hz, candidate[1]);
      consider(dial, std::abs(gainDbOf(candidate) - typed));
    }
    if (best >= 0) strip.gain.dial->setValue(best);
  } else {
    if (shape != Shape::kPair && shape != Shape::kNotch) {
      refresh();
      return;
    }
    for (int dial = 0; dial <= static_cast<int>(p2k::kMaxMagByte); ++dial) {
      trench::core::PackedSection candidate = words;
      candidate[0] = p2k::dial_word(static_cast<std::size_t>(dial));
      const double zero_hz = rootOf(candidate[0], candidate[1]).hz;
      if (!(zero_hz > 0.0) || !std::isfinite(zero_hz)) continue;
      consider(dial, std::abs(semitonesOf(candidate) - typed));
    }
    if (best >= 0) {
      state_->setWordsAt(which, index, EditorState::Lane::kZero,
                         p2k::dial_word(static_cast<std::size_t>(best)), words[1]);
    }
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
      const bool pole = watched == strip.freq.dial || watched == strip.freq.entry ||
                        watched == strip.q.dial || watched == strip.q.entry;
      const bool zero = watched == strip.gain.dial || watched == strip.gain.entry ||
                        watched == strip.offset.dial || watched == strip.offset.entry ||
                        watched == strip.shape;
      if (pole || zero || watched == strip.on || watched == strip.cut) {
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
    const Shape shape = shapeAt(which, index);
    const bool paired = enabled && shape == Shape::kPair;
    const bool offset_live = enabled && (shape == Shape::kPair || shape == Shape::kNotch);

    const QSignalBlocker on_blocker(strip.on);
    const QSignalBlocker shape_blocker(strip.shape);
    const QSignalBlocker freq_blocker(strip.freq.dial);
    const QSignalBlocker q_blocker(strip.q.dial);
    const QSignalBlocker gain_blocker(strip.gain.dial);
    const QSignalBlocker offset_blocker(strip.offset.dial);
    const QSignalBlocker cut_blocker(strip.cut);

    strip.on->setChecked(enabled);
    strip.on->setText(index == selected
                          ? QStringLiteral("> %1").arg(index + 1)
                          : QString::number(index + 1));
    strip.shape->setCurrentIndex(static_cast<int>(shape));
    strip.freq.dial->setValue(static_cast<int>(p2k::dial_of_word(words[2])));
    strip.freq.dial->readout()->setText(enabled ? hzText(words[2], words[3])
                                                : QStringLiteral("—"));
    strip.q.dial->setValue(kTopDial - static_cast<int>(p2k::dial_of_word(words[3])));
    strip.q.dial->readout()->setText(enabled ? resonanceText(words[2], words[3])
                                             : QStringLiteral("—"));
    strip.gain.dial->setValue(paired ? static_cast<int>(p2k::dial_of_word(words[1])) : 1);
    strip.gain.dial->readout()->setText(paired ? gainText(words) : QStringLiteral("—"));
    const double semitones = offset_live ? semitonesOf(words) : 0.0;
    strip.offset.dial->setValue(static_cast<int>(std::lround(semitones)));
    strip.offset.dial->readout()->setText(offset_live ? offsetText(words)
                                                      : QStringLiteral("—"));
    strip.cut->setValue(state_->cutAt(which, index));

    const Root pole = rootOf(words[2], words[3]);
    strip.freq.entry->setText(enabled ? QString::asprintf("%.1f", pole.hz) : QString());
    strip.q.entry->setText(enabled ? QString::asprintf("%.1f", resonanceOf(pole)) : QString());
    strip.gain.entry->setText(paired ? QString::asprintf("%+.1f", gainDbOf(words))
                                     : QString());
    strip.offset.entry->setText(offset_live ? QString::asprintf("%+.1f", semitones)
                                            : QString());

    strip.shape->setEnabled(enabled);
    strip.freq.dial->setEnabled(enabled);
    strip.freq.entry->setEnabled(enabled);
    strip.q.dial->setEnabled(enabled);
    strip.q.entry->setEnabled(enabled);
    strip.gain.dial->setEnabled(paired);
    strip.gain.entry->setEnabled(paired);
    strip.offset.dial->setEnabled(offset_live);
    strip.offset.entry->setEnabled(offset_live);
    strip.cut->setEnabled(enabled);
  }
  refreshing_ = false;
}
