#include "rows_table.hpp"

#include "number_box.hpp"

#include "trench/core/p2k.hpp"

#include <QAction>
#include <QBoxLayout>
#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>
#include <QMenu>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace p2k = trench::core::p2k;

constexpr int kTopDial = static_cast<int>(p2k::kDialCount) - 1;
constexpr int kOffsetSpan = 48;
constexpr double kPairWidthRatio = 4.0;
constexpr double kEdgeDecayHz = 1.0;
constexpr double kFreshRowQ = 35.0;
constexpr double kHiddenBandwidthHz = 1.0e8;
constexpr double kHarmonicToleranceCents = 50.0;
constexpr double kRealPoleSlowRadius = 0.75;
constexpr double kRealPoleFastRadius = 0.02;
constexpr int kRingRuleBase = 0x76;
constexpr int kRingRuleSlope = 0x7c;
constexpr int kCellWidth = 96;
constexpr int kNarrowWidth = 60;
constexpr int kSoFarWidth = 72;
constexpr int kSoFarHeight = 22;
constexpr int kSoFarPoints = 96;
constexpr double kSoFarLowHz = 40.0;
constexpr double kSoFarHighHz = 16'000.0;
constexpr double kSoFarFrameDb = 30.0;
constexpr QColor kSoFarPanel{255, 255, 255};
constexpr QColor kSoFarEdge{200, 200, 200};
constexpr QColor kSoFarZero{150, 150, 150};
constexpr QColor kSoFarInk{0, 0, 0};

const std::vector<double>& soFarGrid() {
  static const std::vector<double> grid = trench::core::logarithmic_frequency_grid(
      kSoFarLowHz, kSoFarHighHz, kSoFarPoints);
  return grid;
}

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

const std::array<const char*, 6> kTypeNames{"OFF", "EQ", "LOWPASS", "HIGHPASS", "POLE",
                                            "NOTCH"};

const std::array<const char*, 16> kHarmonicNames{
    "1x root",      "2x 8ve",       "3x 8ve+5th",   "4x 2-8ve",
    "5x 2-8ve+M3",  "6x 2-8ve+5th", "7x 2-8ve+m7",  "8x 3-8ve",
    "9x 3-8ve+M2",  "10x 3-8ve+M3", "11x 3-8ve+#4", "12x 3-8ve+5th",
    "13x 3-8ve+m6", "14x 3-8ve+m7", "15x 3-8ve+M7", "16x 4-8ve"};

const std::array<const char*, 2> kPoleNames{"RING", "REAL"};

constexpr Root kRingSeed{1'000.0, 100.0};

bool isRealPole(const trench::core::PackedSection& words) {
  const auto [p, q] = p2k::pq(words[2], words[3]);
  return std::holds_alternative<trench::core::native::RealRoots>(
      trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz));
}

double decayHzOf(double radius) {
  return -EditorState::kDatumHz * std::log(radius) / std::numbers::pi;
}

double radiusOfDecay(double decay_hz) {
  const double magnitude = std::exp(-std::numbers::pi * std::abs(decay_hz) / EditorState::kDatumHz);
  return decay_hz < 0.0 ? -magnitude : magnitude;
}

QString realPoleText(const trench::core::PackedSection& words) {
  const auto [p, q] = p2k::pq(words[2], words[3]);
  const auto roots = trench::core::native::roots_from_coefficients(p, q, EditorState::kDatumHz);
  const auto& real = std::get<trench::core::native::RealRoots>(roots);
  return QString::asprintf("r %.2f/%.2f", radiusOfDecay(real.b_hz), radiusOfDecay(real.a_hz));
}

constexpr std::size_t kCeiling = RowsTable::kCeilingSection;
constexpr double kCeilingWallHz = 20'277.05;

std::uint16_t rootReferenceRsq() {
  static const std::uint16_t value = p2k::words_from_root(1'000.0, radiusOf(100.0)).second;
  return value;
}

QString intervalText(double hz, double root_hz) {
  if (!(hz > 0.0) || !(root_hz > 0.0) || !std::isfinite(hz)) return QStringLiteral("-");
  static const std::array<const char*, 12> kSteps{"", "m2", "M2", "m3", "M3", "P4",
                                                  "#4", "5th", "m6", "M6", "m7", "M7"};
  const int semitones = static_cast<int>(std::lround(12.0 * std::log2(hz / root_hz)));
  const int magnitude = std::abs(semitones);
  const int octaves = magnitude / 12;
  const int step = magnitude % 12;
  QString name;
  if (octaves == 1) {
    name = QStringLiteral("8ve");
  } else if (octaves > 1) {
    name = QStringLiteral("%1-8ve").arg(octaves);
  }
  if (step > 0) {
    name += (name.isEmpty() ? QString() : QStringLiteral("+")) + QString::fromUtf8(kSteps[step]);
  }
  if (name.isEmpty()) name = QStringLiteral("unison");
  return QStringLiteral("%1%2st %3")
      .arg(semitones < 0 ? QStringLiteral("-") : QStringLiteral("+"))
      .arg(magnitude)
      .arg(semitones < 0 ? name + QStringLiteral(" below") : name);
}

QString noteName(double hz) {
  if (!(hz > 0.0) || !std::isfinite(hz)) return QString();
  static const std::array<const char*, 12> kNames{"C",  "C#", "D",  "D#", "E",  "F",
                                                  "F#", "G",  "G#", "A",  "A#", "B"};
  const double midi = 69.0 + 12.0 * std::log2(hz / 440.0);
  const int nearest = static_cast<int>(std::lround(midi));
  const int cents = static_cast<int>(std::lround((midi - nearest) * 100.0));
  const int octave = nearest / 12 - 1;
  return QStringLiteral("%1%2%3%4")
      .arg(QString::fromUtf8(kNames[static_cast<std::size_t>(((nearest % 12) + 12) % 12)]))
      .arg(octave)
      .arg(cents >= 0 ? QStringLiteral("+") : QString())
      .arg(cents);
}

double firstNumber(const QString& text, bool* ok) {
  for (const QString& piece : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
    bool parsed = false;
    const double value = piece.toDouble(&parsed);
    if (parsed) {
      *ok = true;
      return value;
    }
  }
  *ok = false;
  return 0.0;
}

std::optional<double> framePoleHz(const trench::app::PoleTemplate& frame, std::size_t slot) {
  if (slot >= trench::core::native::kSections) return std::nullopt;
  const auto& words = frame.words[slot];
  bool identity = true;
  for (std::size_t wi = 0; wi < words.size(); ++wi) {
    if (words[wi] != trench::core::kIdentitySection[wi]) identity = false;
  }
  if (identity) return std::nullopt;
  const Root root = rootOf(words[2], words[3]);
  if (!(root.hz > 0.0) || !std::isfinite(root.hz) || !(root.bw_hz > 0.0)) return std::nullopt;
  return root.hz;
}

QString frameEntry(const trench::app::PoleTemplate& frame, std::size_t slot) {
  const auto hz = framePoleHz(frame, slot);
  return frame.label() + QStringLiteral(" · ") +
         (hz ? QString::asprintf("%.0f Hz", *hz) : QStringLiteral("-"));
}

std::vector<trench::app::PoleTemplate> keyframeFrames() {
  const auto keys = trench::app::loadKeyframes();
  std::vector<QString> groups;
  for (const auto& key : keys)
    if (std::find(groups.begin(), groups.end(), key.group) == groups.end())
      groups.push_back(key.group);
  std::vector<trench::app::PoleTemplate> out;
  out.reserve(keys.size());
  for (const QString& group : groups)
    for (const auto& key : keys)
      if (key.group == group) out.push_back(trench::app::keyframeFrame(key));
  return out;
}

QLabel* caption(QWidget* parent, const QString& text) {
  auto* label = new QLabel(text, parent);
  label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  return label;
}

}

class SoFarCell final : public QWidget {
 public:
  explicit SoFarCell(QWidget* parent) : QWidget(parent) {
    setFixedSize(kSoFarWidth, kSoFarHeight);
  }

  void setCurve(std::vector<double> db) {
    db_ = std::move(db);
    update();
  }

  std::function<void()> onEnter;
  std::function<void()> onLeave;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(kSoFarEdge, 1.0));
    painter.setBrush(kSoFarPanel);
    painter.drawRect(card);
    const QRectF pane = card.adjusted(2.0, 2.0, -2.0, -2.0);
    const double middle = pane.center().y();
    painter.setPen(QPen(kSoFarZero, 1.0));
    painter.drawLine(QPointF(pane.left(), middle), QPointF(pane.right(), middle));
    if (db_.size() < 2) return;
    QPainterPath path;
    const double span = pane.height() * 0.5;
    for (std::size_t i = 0; i < db_.size(); ++i) {
      const double x = pane.left() + pane.width() * static_cast<double>(i) /
                                         static_cast<double>(db_.size() - 1);
      const double level = std::clamp(db_[i], -kSoFarFrameDb, kSoFarFrameDb);
      const double y = middle - span * level / kSoFarFrameDb;
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(kSoFarInk, 1.0));
    painter.drawPath(path);
  }

  void enterEvent(QEnterEvent* event) override {
    if (onEnter) onEnter();
    QWidget::enterEvent(event);
  }

  void leaveEvent(QEvent* event) override {
    if (onLeave) onLeave();
    QWidget::leaveEvent(event);
  }

 private:
  std::vector<double> db_;
};

RowsTable::RowsTable(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setObjectName(QStringLiteral("rowsTable"));
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(4, 4, 4, 4);
  column->setSpacing(4);

  auto* head = new QHBoxLayout;
  head->setSpacing(6);
  buildRoot(head);
  head->addStretch(1);
  column->addLayout(head);

  auto* grid = new QGridLayout;
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(3);
  grid->setVerticalSpacing(2);
  buildHeader(grid);
  for (std::size_t index = 0; index < rows_.size(); ++index) buildRow(index, grid);
  column->addLayout(grid);
  column->addStretch(1);

  connect(state_, &EditorState::changed, this, &RowsTable::refresh);
  connect(state_, &EditorState::selectionChanged, this, [this](std::size_t) { refresh(); });
  refresh();
}

std::size_t RowsTable::pairBase() const { return state_->qPos() < 0.5 ? 0u : 2u; }

std::size_t RowsTable::cornerOf(Side side) const {
  return pairBase() + (side == Side::kHi ? 1u : 0u);
}

RowsTable::Side RowsTable::sideOfEditingCorner() const {
  return (state_->editingCorner() & 1u) != 0u ? Side::kHi : Side::kLo;
}

RowsTable::Cell& RowsTable::cellOf(Side side, std::size_t index) {
  return rows_[index].sides[side == Side::kHi ? 1u : 0u];
}

const RowsTable::Cell& RowsTable::cellOf(Side side, std::size_t index) const {
  return rows_[index].sides[side == Side::kHi ? 1u : 0u];
}

int RowsTable::ringRule(int mag_byte) {
  return std::clamp(kRingRuleBase + ((mag_byte * kRingRuleSlope) >> 8), 0, 255);
}

void RowsTable::setSide(Side side) {
  refreshing_ = true;
  state_->setEditingCorner(cornerOf(side));
  refreshing_ = false;
}

NumberBox* RowsTable::buildCell(const QString& name, int minimum, int maximum) {
  auto* box = new NumberBox(this);
  box->setObjectName(name);
  box->setRange(minimum, maximum);
  box->setFixedWidth(kCellWidth);
  return box;
}

void RowsTable::buildRoot(QBoxLayout* into) {
  root_note_ = new QLabel(QStringLiteral("ROOT"), this);
  root_note_->setObjectName(QStringLiteral("rootNote"));
  root_dial_ = buildCell(QStringLiteral("rootDial"), 0, static_cast<int>(p2k::kMaxMagByte));
  root_entry_ = new QLineEdit(this);
  root_entry_->setObjectName(QStringLiteral("rootEntry"));
  root_entry_->setFixedWidth(kNarrowWidth);
  root_entry_->setAlignment(Qt::AlignRight);
  into->addWidget(root_note_);
  into->addWidget(root_dial_);
  into->addWidget(root_entry_);

  connect(root_dial_, &NumberBox::valueChanged, this, [this](int) {
    if (refreshing_) return;
    pushRoot();
  });
  connect(root_dial_, &NumberBox::typed, this, [this](const QString& entry) {
    if (refreshing_) return;
    bool ok = false;
    const double typed = firstNumber(entry, &ok);
    if (ok && typed > 0.0) setRootHz(typed);
    refresh();
  });
  connect(root_entry_, &QLineEdit::editingFinished, this, [this] {
    if (refreshing_) return;
    bool ok = false;
    const double typed = root_entry_->text().toDouble(&ok);
    if (ok && typed > 0.0) setRootHz(typed);
    refresh();
  });
}

void RowsTable::buildHeader(QGridLayout* grid) {
  grid->addWidget(caption(this, QStringLiteral("#")), 0, 0, 2, 1);
  grid->addWidget(caption(this, QStringLiteral("TYPE")), 0, 1, 2, 1);
  grid->addWidget(caption(this, QStringLiteral("LO")), 0, 2, 1, 3);
  grid->addWidget(caption(this, QStringLiteral("HI")), 0, 5, 1, 3);
  grid->addWidget(caption(this, QStringLiteral("…")), 0, 8, 2, 1);
  grid->addWidget(caption(this, QStringLiteral("SO FAR")), 0, 9, 2, 1);
  static const std::array<const char*, 3> kSubs{"NOTE", "RING", "HEIGHT"};
  for (int side = 0; side < 2; ++side) {
    for (int cell = 0; cell < 3; ++cell) {
      grid->addWidget(caption(this, QString::fromUtf8(kSubs[static_cast<std::size_t>(cell)])), 1,
                      2 + side * 3 + cell);
    }
  }
}

void RowsTable::watch(QWidget* control, std::size_t index, EditorState::Lane lane) {
  if (control == nullptr) return;
  control->setProperty("rowIndex", static_cast<int>(index));
  control->setProperty("rowLane", lane == EditorState::Lane::kPole ? 0 : 1);
  control->installEventFilter(this);
}

void RowsTable::buildRow(std::size_t index, QGridLayout* grid) {
  Row& row = rows_[index];
  const int line = 2 + static_cast<int>(index) * 2;
  const bool ceiling = index == kCeiling;

  row.number = new QLabel(QString::number(index + 1), this);
  row.number->setObjectName(QStringLiteral("num%1").arg(index));
  row.number->setAlignment(Qt::AlignCenter);
  grid->addWidget(row.number, line, 0);

  row.type = new QComboBox(this);
  row.type->setObjectName(QStringLiteral("type%1").arg(index));
  row.type->setFocusPolicy(Qt::ClickFocus);
  for (std::size_t which = 0; which < kTypeNames.size(); ++which) {
    row.type->addItem(QString::fromUtf8(kTypeNames[which]), static_cast<int>(which));
  }
  grid->addWidget(row.type, line, 1);

  row.extra = new QWidget(this);
  auto* extras = new QHBoxLayout(row.extra);
  extras->setContentsMargins(0, 0, 0, 0);
  extras->setSpacing(4);
  row.extra->setVisible(false);

  for (int which = 0; which < 2; ++which) {
    const Side side = which == 0 ? Side::kLo : Side::kHi;
    const QString tag = which == 0 ? QStringLiteral("Lo") : QStringLiteral("Hi");
    Cell& cell = row.sides[static_cast<std::size_t>(which)];

    cell.note = buildCell(QStringLiteral("note%1%2").arg(tag).arg(index), 0,
                          static_cast<int>(p2k::kMaxMagByte));
    cell.ring = buildCell(QStringLiteral("ring%1%2").arg(tag).arg(index), 0, maxPoleRes());
    cell.height = ceiling ? buildCell(QStringLiteral("height%1%2").arg(tag).arg(index), 0,
                                      static_cast<int>(p2k::kMaxMagByte))
                          : buildCell(QStringLiteral("height%1%2").arg(tag).arg(index), 1,
                                      kTopDial);
    grid->addWidget(cell.note, line, 2 + which * 3);
    grid->addWidget(cell.ring, line, 3 + which * 3);
    grid->addWidget(cell.height, line, 4 + which * 3);

    cell.offset = buildCell(QStringLiteral("offset%1%2").arg(tag).arg(index), -kOffsetSpan,
                            kOffsetSpan);
    cell.offset->setFixedWidth(kNarrowWidth);
    cell.cut = new QSpinBox(row.extra);
    cell.cut->setObjectName(QStringLiteral("cut%1%2").arg(tag).arg(index));
    cell.cut->setRange(0, EditorState::kMaxCut);
    cell.cut->setKeyboardTracking(false);
    cell.cut->setFixedWidth(kNarrowWidth);
    cell.pole = new QComboBox(row.extra);
    cell.pole->setObjectName(QStringLiteral("pole%1%2").arg(tag).arg(index));
    for (const char* name : kPoleNames) cell.pole->addItem(QString::fromUtf8(name));
    cell.pole->setFocusPolicy(Qt::ClickFocus);
    cell.harm = new QComboBox(row.extra);
    cell.harm->setObjectName(QStringLiteral("harm%1%2").arg(tag).arg(index));
    cell.harm->addItem(QStringLiteral("-"));
    for (const char* name : kHarmonicNames) cell.harm->addItem(QString::fromUtf8(name));
    cell.harm->setFocusPolicy(Qt::ClickFocus);

    extras->addWidget(caption(row.extra, tag.toUpper()));
    if (cell.offset != nullptr) {
      cell.offset->setParent(row.extra);
      extras->addWidget(new QLabel(QStringLiteral("OFFSET"), row.extra));
      extras->addWidget(cell.offset);
    }
    extras->addWidget(new QLabel(QStringLiteral("CUT"), row.extra));
    extras->addWidget(cell.cut);
    extras->addWidget(cell.pole);
    extras->addWidget(cell.harm);

    auto* follow = new QAction(QStringLiteral("FOLLOW NOTE"), cell.ring);
    cell.ring->setContextMenuPolicy(Qt::ActionsContextMenu);
    cell.ring->addAction(follow);
    connect(follow, &QAction::triggered, this, [this, side, index] { followNote(side, index); });

    connect(cell.note, &NumberBox::valueChanged, this, [this, side, index](int) {
      if (refreshing_) return;
      pushPole(side, index, Kind::kNote);
    });
    connect(cell.ring, &NumberBox::valueChanged, this, [this, side, index](int) {
      if (refreshing_) return;
      pushPole(side, index, Kind::kRing);
    });
    connect(cell.ring, &NumberBox::touched, this,
            [this, side, index] { cellOf(side, index).ring_touched = true; });
    connect(cell.height, &NumberBox::valueChanged, this, [this, side, index](int) {
      if (refreshing_) return;
      if (ceilingAt(cornerOf(side), index)) {
        pushCeiling(side, index);
      } else {
        pushGain(side, index);
      }
    });
    connect(cell.note, &NumberBox::typed, this, [this, side, index](const QString& entry) {
      landEntry(side, index, Kind::kNote, entry);
    });
    connect(cell.ring, &NumberBox::typed, this, [this, side, index](const QString& entry) {
      landEntry(side, index, Kind::kRing, entry);
    });
    connect(cell.height, &NumberBox::typed, this, [this, side, index](const QString& entry) {
      landEntry(side, index, Kind::kHeight, entry);
    });
    if (cell.offset != nullptr) {
      connect(cell.offset, &NumberBox::valueChanged, this, [this, side, index](int) {
        if (refreshing_) return;
        pushOffset(side, index);
      });
      connect(cell.offset, &NumberBox::typed, this, [this, side, index](const QString& entry) {
        landEntry(side, index, Kind::kOffset, entry);
      });
    }
    connect(cell.cut, qOverload<int>(&QSpinBox::valueChanged), this,
            [this, side, index](int value) {
              if (refreshing_) return;
              setSide(side);
              state_->setCutAt(cornerOf(side), index, value);
            });
    connect(cell.pole, &QComboBox::currentIndexChanged, this, [this, side, index](int which) {
      if (refreshing_) return;
      pushPoleWord(side, index, static_cast<Pole>(which));
    });
    connect(cell.harm, &QComboBox::currentIndexChanged, this, [this, side, index](int which) {
      if (refreshing_) return;
      if (which > 0) landHarmonic(side, index, which);
      refresh();
    });

    watch(cell.note, index, EditorState::Lane::kPole);
    watch(cell.ring, index, EditorState::Lane::kPole);
    watch(cell.height, index, EditorState::Lane::kZero);
    watch(cell.offset, index, EditorState::Lane::kZero);
    watch(cell.cut, index, EditorState::Lane::kPole);
    watch(cell.pole, index, EditorState::Lane::kPole);
    watch(cell.harm, index, EditorState::Lane::kPole);
  }
  extras->addStretch(1);
  grid->addWidget(row.extra, line + 1, 0, 1, 10);

  row.unlock = new QToolButton(this);
  row.unlock->setObjectName(QStringLiteral("unlock%1").arg(index));
  row.unlock->setText(QStringLiteral("…"));
  row.unlock->setCheckable(true);
  row.unlock->setFocusPolicy(Qt::NoFocus);
  grid->addWidget(row.unlock, line, 8);
  connect(row.unlock, &QToolButton::toggled, this,
          [this, index](bool open) { rows_[index].extra->setVisible(open); });

  row.so_far = new SoFarCell(this);
  row.so_far->setObjectName(QStringLiteral("soFar%1").arg(index));
  row.so_far->onEnter = [this, index] {
    if (onSoFarHover) onSoFarHover(index + 1u);
  };
  row.so_far->onLeave = [this] {
    if (onSoFarLeave) onSoFarLeave();
  };
  grid->addWidget(row.so_far, line, 9);

  connect(row.type, &QComboBox::currentIndexChanged, this, [this, index](int which) {
    if (refreshing_) return;
    const QVariant data = rows_[index].type->itemData(which);
    pushType(sideOfEditingCorner(), index, static_cast<Type>(data.toInt()));
  });

  watch(row.number, index, EditorState::Lane::kPole);
  watch(row.type, index, EditorState::Lane::kZero);
  watch(row.unlock, index, EditorState::Lane::kPole);
  armRowMenu(row.number, index);
  armRowMenu(row.type, index);
  for (Cell& cell : row.sides) {
    armRowMenu(cell.note, index);
    armRowMenu(cell.height, index);
    armRowMenu(cell.offset, index);
  }
}

void RowsTable::armRowMenu(QWidget* control, std::size_t index) {
  if (control == nullptr) return;
  control->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(control, &QWidget::customContextMenuRequested, this,
          [this, index, control](const QPoint& at) {
            showRowMenu(index, control->mapToGlobal(at));
          });
}

void RowsTable::showRowMenu(std::size_t index, const QPoint& global) {
  static const auto frames = trench::app::loadFrames();
  if (frames.empty() || index >= rows_.size()) return;
  QMenu menu(QStringLiteral("ROW FROM FRAME"), this);
  for (const auto& frame : frames) {
    const trench::app::PoleTemplate* one = &frame;
    QAction* action = menu.addAction(frameEntry(frame, index));
    connect(action, &QAction::triggered, this,
            [this, index, one] { rowFromFrame(index, *one, index); });
  }
  QMenu* nearest = menu.addMenu(QStringLiteral("NEAREST SLOT"));
  for (const auto& frame : frames) {
    const trench::app::PoleTemplate* one = &frame;
    const std::size_t slot = nearestFrameSlot(index, frame);
    QAction* action = nearest->addAction(frameEntry(frame, slot));
    connect(action, &QAction::triggered, this,
            [this, index, one, slot] { rowFromFrame(index, *one, slot); });
  }
  static const auto keyframes = keyframeFrames();
  if (!keyframes.empty()) {
    menu.addSeparator();
    QMenu* keyed_near = menu.addMenu(QStringLiteral("KEYFRAME NEAREST SLOT"));
    QString group;
    QMenu* here = nullptr;
    QMenu* here_near = nullptr;
    for (const auto& frame : keyframes) {
      if (here == nullptr || frame.family != group) {
        group = frame.family;
        here = menu.addMenu(group);
        here_near = keyed_near->addMenu(group);
      }
      const trench::app::PoleTemplate* one = &frame;
      QAction* action = here->addAction(frameEntry(frame, index));
      connect(action, &QAction::triggered, this,
              [this, index, one] { rowFromFrame(index, *one, index); });
      const std::size_t slot = nearestFrameSlot(index, frame);
      QAction* close = here_near->addAction(frameEntry(frame, slot));
      connect(close, &QAction::triggered, this,
              [this, index, one, slot] { rowFromFrame(index, *one, slot); });
    }
  }
  menu.exec(global);
}

std::size_t RowsTable::nearestFrameSlot(std::size_t index,
                                        const trench::app::PoleTemplate& frame) const {
  const auto here = state_->poleHzAt(state_->editingCorner(), index);
  std::size_t best = index;
  double best_cost = std::numeric_limits<double>::infinity();
  for (std::size_t slot = 0; slot < trench::core::native::kSections; ++slot) {
    const auto hz = framePoleHz(frame, slot);
    if (!hz) continue;
    const double cost = here ? std::abs(std::log2(*hz / *here))
                             : static_cast<double>(slot > index ? slot - index : index - slot);
    if (cost >= best_cost) continue;
    best_cost = cost;
    best = slot;
  }
  return best;
}

void RowsTable::rowFromFrame(std::size_t index, const trench::app::PoleTemplate& frame,
                             std::size_t slot) {
  if (index >= rows_.size() || slot >= trench::core::native::kSections) return;
  const auto seeded = trench::app::frameCornerState(frame);
  if (!seeded.enabled[slot]) return;
  const std::size_t corner = state_->editingCorner();
  const auto& words = frame.words[slot];
  state_->beginUndoGroup();
  if (!state_->sectionEnabledAt(corner, index)) state_->toggleSectionAt(corner, index);
  state_->setWordsAt(corner, index, EditorState::Lane::kPole, words[2], words[3]);
  if (seeded.zero_present[slot]) {
    state_->setWordsAt(corner, index, EditorState::Lane::kZero, words[0], words[1]);
  } else if (state_->zeroPresentAt(corner, index)) {
    state_->removeZeroAt(corner, index);
  }
  state_->setCutAt(corner, index, seeded.cut[slot]);
  state_->endUndoGroup();
  cellOf(sideOfEditingCorner(), index).ring_touched = true;
  refresh();
}

bool RowsTable::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
    const QVariant index = watched->property("rowIndex");
    if (index.isValid()) {
      selectFrom(static_cast<std::size_t>(index.toInt()),
                 watched->property("rowLane").toInt() == 0 ? EditorState::Lane::kPole
                                                           : EditorState::Lane::kZero);
    }
  }
  return QWidget::eventFilter(watched, event);
}

void RowsTable::selectFrom(std::size_t index, EditorState::Lane lane) {
  state_->selectSection(index);
  state_->selectRoot(index, lane);
}

RowsTable::Shape RowsTable::shapeAt(std::size_t which, std::size_t index) const {
  if (!state_->zeroPresentAt(which, index)) return Shape::kResonator;
  const auto& zero = state_->sectionAt(which, index).zero;
  if (const auto* real = std::get_if<trench::core::native::RealRoots>(&zero)) {
    return real->a_hz > 0.0 ? Shape::kEdgeHigh : Shape::kEdgeLow;
  }
  const auto& words = state_->packed().words[which][index];
  return words[1] == p2k::kS6ZeroRsqWord ? Shape::kNotch : Shape::kPair;
}

RowsTable::Shape RowsTable::shapeOf(Type type, std::size_t index) const {
  if (index == kCeiling && (type == Type::kLowPass || type == Type::kNotch)) {
    return Shape::kNotch;
  }
  switch (type) {
    case Type::kEq: return Shape::kPair;
    case Type::kLowPass: return Shape::kEdgeLow;
    case Type::kHighPass: return Shape::kEdgeHigh;
    case Type::kNotch: return Shape::kNotch;
    case Type::kPole:
    case Type::kOff: break;
  }
  return Shape::kResonator;
}

RowsTable::Type RowsTable::typeAt(std::size_t which, std::size_t index) const {
  if (!state_->sectionEnabledAt(which, index)) return Type::kOff;
  switch (shapeAt(which, index)) {
    case Shape::kPair: return Type::kEq;
    case Shape::kNotch: return index == kCeiling ? Type::kLowPass : Type::kNotch;
    case Shape::kEdgeHigh: return Type::kHighPass;
    case Shape::kEdgeLow: return Type::kLowPass;
    case Shape::kResonator: break;
  }
  return Type::kPole;
}

bool RowsTable::ceilingAt(std::size_t which, std::size_t index) const {
  return index == kCeiling && typeAt(which, index) == Type::kLowPass;
}

RowsTable::Pole RowsTable::poleAt(std::size_t which, std::size_t index) const {
  if (!state_->sectionEnabledAt(which, index)) return Pole::kRing;
  return isRealPole(state_->packed().words[which][index]) ? Pole::kReal : Pole::kRing;
}

bool RowsTable::followingAt(Side side, std::size_t index) const {
  const std::size_t which = cornerOf(side);
  if (!state_->sectionEnabledAt(which, index)) return false;
  if (cellOf(side, index).ring_touched) return false;
  const Type type = typeAt(which, index);
  if (type != Type::kEq && type != Type::kNotch) return false;
  if (poleAt(which, index) == Pole::kReal) return false;
  const auto& words = state_->packed().words[which][index];
  return static_cast<int>(p2k::dial_of_word(words[3])) ==
         ringRule(static_cast<int>(p2k::dial_of_word(words[2])));
}

double RowsTable::offsetNow(Side side, std::size_t index) const {
  const std::size_t which = cornerOf(side);
  const Shape shape = shapeAt(which, index);
  if (shape != Shape::kPair && shape != Shape::kNotch) return 0.0;
  return semitonesOf(state_->packed().words[which][index]);
}

void RowsTable::seatZero(Side side, std::size_t index, double semitones, std::uint16_t rsq) {
  const std::size_t which = cornerOf(side);
  const auto words = state_->packed().words[which][index];
  const double pole_hz = rootOf(words[2], words[3]).hz;
  const double target_hz = pole_hz * std::exp2(semitones / 12.0);
  state_->setWordsAt(which, index, EditorState::Lane::kZero, p2k::mag_word_for(target_hz, rsq),
                     rsq);
}

int RowsTable::nearestFrequencyDial(Side side, std::size_t index, double hz) const {
  const auto& words = state_->packed().words[cornerOf(side)][index];
  const double target = std::log2(hz);
  int best = -1;
  double best_cost = std::numeric_limits<double>::infinity();
  for (int dial = 0; dial <= static_cast<int>(p2k::kMaxMagByte); ++dial) {
    const Root root = rootOf(p2k::dial_word(static_cast<std::size_t>(dial)), words[3]);
    if (!(root.hz > 0.0) || !std::isfinite(root.hz)) continue;
    const double cost = std::abs(std::log2(root.hz) - target);
    if (cost < best_cost) {
      best_cost = cost;
      best = dial;
    }
  }
  return best;
}

int RowsTable::nearestGainDial(Side side, std::size_t index, double db) const {
  const std::size_t which = cornerOf(side);
  const auto words = state_->packed().words[which][index];
  const double target_hz =
      rootOf(words[2], words[3]).hz * std::exp2(offsetNow(side, index) / 12.0);
  int best = -1;
  double best_cost = std::numeric_limits<double>::infinity();
  for (int dial = 1; dial <= kTopDial; ++dial) {
    trench::core::PackedSection candidate = words;
    candidate[1] = p2k::dial_word(static_cast<std::size_t>(dial));
    candidate[0] = p2k::mag_word_for(target_hz, candidate[1]);
    const double cost = std::abs(gainDbOf(candidate) - db);
    if (!std::isfinite(cost) || cost >= best_cost) continue;
    best_cost = cost;
    best = dial;
  }
  return best;
}

int RowsTable::harmonicOf(Side side, std::size_t index) const {
  const std::size_t which = cornerOf(side);
  if (!state_->sectionEnabledAt(which, index)) return 0;
  const auto& words = state_->packed().words[which][index];
  if (isRealPole(words)) return 0;
  const double hz = rootOf(words[2], words[3]).hz;
  if (!(hz > 0.0) || !std::isfinite(hz)) return 0;
  const int harmonic = static_cast<int>(std::lround(hz / root_hz_));
  if (harmonic < 1 || harmonic > kHarmonicCount) return 0;
  const double cents = 1200.0 * std::log2(hz / (root_hz_ * harmonic));
  return std::abs(cents) <= kHarmonicToleranceCents ? harmonic : 0;
}

void RowsTable::landHarmonic(Side side, std::size_t index, int harmonic) {
  if (harmonic < 1 || !state_->sectionEnabledAt(cornerOf(side), index)) return;
  const int dial = nearestFrequencyDial(side, index, root_hz_ * harmonic);
  if (dial < 0) return;
  refreshing_ = true;
  cellOf(side, index).note->setValue(dial);
  refreshing_ = false;
  pushPole(side, index, Kind::kNote);
}

void RowsTable::pushRoot() {
  setRootHz(rootOf(p2k::dial_word(static_cast<std::size_t>(root_dial_->value())),
                   rootReferenceRsq())
                .hz);
}

void RowsTable::setRootHz(double hz) {
  if (!(hz > 0.0) || !std::isfinite(hz)) return;
  std::array<std::array<int, trench::core::native::kSections>, 2> held{};
  for (int which = 0; which < 2; ++which) {
    const Side side = which == 0 ? Side::kLo : Side::kHi;
    for (std::size_t index = 0; index < rows_.size(); ++index) {
      held[static_cast<std::size_t>(which)][index] = harmonicOf(side, index);
    }
  }
  root_hz_ = std::clamp(hz, EditorState::kLowHz, EditorState::kNyquistHz);
  state_->beginUndoGroup();
  for (int which = 0; which < 2; ++which) {
    const Side side = which == 0 ? Side::kLo : Side::kHi;
    for (std::size_t index = 0; index < rows_.size(); ++index) {
      const int harmonic = held[static_cast<std::size_t>(which)][index];
      if (harmonic > 0) landHarmonic(side, index, harmonic);
    }
  }
  state_->endUndoGroup();
  refresh();
}

void RowsTable::seatFreshRow(Side side, std::size_t index) {
  const std::size_t which = cornerOf(side);
  const double bw_hz = std::clamp(root_hz_ / kFreshRowQ, EditorState::kMinBandwidthHz,
                                  EditorState::kMaxBandwidthHz);
  state_->setRootAt(which, index, EditorState::Lane::kPole, root_hz_, bw_hz);
  if (index == kCeiling) return;
  const double zero_bw = std::clamp(bw_hz * kPairWidthRatio, EditorState::kMinBandwidthHz,
                                    EditorState::kMaxBandwidthHz);
  seatZero(side, index, 0.0, p2k::words_from_root(root_hz_, radiusOf(zero_bw)).second);
}

void RowsTable::pushPole(Side side, std::size_t index, Kind moved) {
  const std::size_t which = cornerOf(side);
  Cell& cell = cellOf(side, index);
  int mag_dial = cell.note->value();
  const double semitones =
      cell.offset != nullptr ? static_cast<double>(cell.offset->value()) : 0.0;
  const bool follow = moved == Kind::kNote && followingAt(side, index);
  int rsq_dial = kTopDial - cell.ring->value();
  setSide(side);
  const Shape shape = shapeAt(which, index);
  if (follow) {
    rsq_dial = ringRule(mag_dial);
    refreshing_ = true;
    cell.ring->setValue(kTopDial - rsq_dial);
    refreshing_ = false;
  } else {
    const auto before = state_->packed().words[which][index];
    const Root held = rootOf(before[2], before[3]);
    const double held_q = resonanceOf(held);
    if (held.hz > 0.0 && held_q > 0.0 && std::isfinite(held_q)) {
      int best = -1;
      double best_cost = std::numeric_limits<double>::infinity();
      if (moved == Kind::kNote) {
        const auto mag = p2k::dial_word(static_cast<std::size_t>(mag_dial));
        for (int dial = 0; dial <= maxPoleRes(); ++dial) {
          const Root root = rootOf(mag, p2k::dial_word(static_cast<std::size_t>(kTopDial - dial)));
          const double q = resonanceOf(root);
          if (!(q > 0.0) || !std::isfinite(q)) continue;
          const double cost = std::abs(std::log2(q) - std::log2(held_q));
          if (cost < best_cost) {
            best_cost = cost;
            best = dial;
          }
        }
        if (best >= 0) {
          refreshing_ = true;
          cell.ring->setValue(best);
          refreshing_ = false;
          rsq_dial = kTopDial - best;
        }
      } else if (moved == Kind::kRing) {
        const auto rsq = p2k::dial_word(static_cast<std::size_t>(rsq_dial));
        for (int dial = 0; dial <= static_cast<int>(p2k::kMaxMagByte); ++dial) {
          const Root root = rootOf(p2k::dial_word(static_cast<std::size_t>(dial)), rsq);
          if (!(root.hz > 0.0) || !std::isfinite(root.hz)) continue;
          const double cost = std::abs(std::log2(root.hz) - std::log2(held.hz));
          if (cost < best_cost) {
            best_cost = cost;
            best = dial;
          }
        }
        if (best >= 0) {
          refreshing_ = true;
          cell.note->setValue(best);
          refreshing_ = false;
          mag_dial = best;
        }
      }
    }
  }
  state_->beginUndoGroup();
  state_->setWordsAt(which, index, EditorState::Lane::kPole,
                     p2k::dial_word(static_cast<std::size_t>(mag_dial)),
                     p2k::dial_word(static_cast<std::size_t>(rsq_dial)));
  if (!ceilingAt(which, index) && (shape == Shape::kPair || shape == Shape::kNotch)) {
    seatZero(side, index, semitones, state_->packed().words[which][index][1]);
  }
  state_->endUndoGroup();
}

void RowsTable::followNote(Side side, std::size_t index) {
  const std::size_t which = cornerOf(side);
  if (!state_->sectionEnabledAt(which, index)) return;
  cellOf(side, index).ring_touched = false;
  setSide(side);
  const auto words = state_->packed().words[which][index];
  const int mag_dial = static_cast<int>(p2k::dial_of_word(words[2]));
  const int rsq_dial = ringRule(mag_dial);
  const Shape shape = shapeAt(which, index);
  const double semitones = offsetNow(side, index);
  state_->beginUndoGroup();
  state_->setWordsAt(which, index, EditorState::Lane::kPole,
                     p2k::dial_word(static_cast<std::size_t>(mag_dial)),
                     p2k::dial_word(static_cast<std::size_t>(rsq_dial)));
  if (!ceilingAt(which, index) && (shape == Shape::kPair || shape == Shape::kNotch)) {
    seatZero(side, index, semitones, state_->packed().words[which][index][1]);
  }
  state_->endUndoGroup();
  refresh();
}

void RowsTable::pushGain(Side side, std::size_t index) {
  const std::size_t which = cornerOf(side);
  Cell& cell = cellOf(side, index);
  const int height_dial = cell.height->value();
  const double semitones =
      cell.offset != nullptr ? static_cast<double>(cell.offset->value()) : 0.0;
  setSide(side);
  if (shapeAt(which, index) != Shape::kPair) return;
  seatZero(side, index, semitones, p2k::dial_word(static_cast<std::size_t>(height_dial)));
}

void RowsTable::pushCeiling(Side side, std::size_t index) {
  const std::size_t which = cornerOf(side);
  const int dial = cellOf(side, index).height->value();
  setSide(side);
  if (!state_->sectionEnabledAt(which, index)) return;
  state_->setWordsAt(which, index, EditorState::Lane::kZero,
                     p2k::dial_word(static_cast<std::size_t>(dial)), p2k::kS6ZeroRsqWord);
}

void RowsTable::pushOffset(Side side, std::size_t index) {
  const std::size_t which = cornerOf(side);
  Cell& cell = cellOf(side, index);
  if (cell.offset == nullptr) return;
  const double semitones = static_cast<double>(cell.offset->value());
  setSide(side);
  if (ceilingAt(which, index)) return;
  const Shape shape = shapeAt(which, index);
  if (shape != Shape::kPair && shape != Shape::kNotch) return;
  seatZero(side, index, semitones, state_->packed().words[which][index][1]);
}

void RowsTable::pushShape(Side side, std::size_t index, Shape shape) {
  const std::size_t which = cornerOf(side);
  const Shape current = shapeAt(which, index);
  if (shape == current) return;
  const double semitones = ceilingAt(which, index) ? 0.0 : offsetNow(side, index);
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
      seatZero(side, index, semitones, rsq);
      break;
    }
    case Shape::kNotch:
      if (index == kCeiling) {
        state_->setWordsAt(which, index, EditorState::Lane::kZero,
                           p2k::mag_word_for(kCeilingWallHz, p2k::kS6ZeroRsqWord),
                           p2k::kS6ZeroRsqWord);
      } else {
        seatZero(side, index, semitones, p2k::kS6ZeroRsqWord);
      }
      break;
    case Shape::kEdgeHigh:
      state_->setRealRootAt(which, index, EditorState::Lane::kZero, kEdgeDecayHz, kEdgeDecayHz);
      break;
    case Shape::kEdgeLow:
      state_->setRealRootAt(which, index, EditorState::Lane::kZero, -kEdgeDecayHz, -kEdgeDecayHz);
      break;
  }
}

void RowsTable::pushType(Side side, std::size_t index, Type type) {
  const std::size_t held = state_->editingCorner();
  const std::array<Side, 2> order{side, side == Side::kLo ? Side::kHi : Side::kLo};
  state_->beginUndoGroup();
  for (const Side each : order) {
    const std::size_t corner = cornerOf(each);
    setSide(each);
    const bool enabled = state_->sectionEnabledAt(corner, index);
    if (type == Type::kOff) {
      if (enabled) state_->toggleSectionAt(corner, index);
      continue;
    }
    if (!enabled) {
      const auto* tone =
          std::get_if<trench::core::native::Resonant>(&state_->sectionAt(corner, index).pole);
      const bool fresh = tone != nullptr && tone->bw_hz >= kHiddenBandwidthHz;
      state_->toggleSectionAt(corner, index);
      if (fresh) seatFreshRow(each, index);
    }
    pushShape(each, index, shapeOf(type, index));
    cellOf(each, index).ring_touched = false;
  }
  state_->endUndoGroup();
  refreshing_ = true;
  state_->setEditingCorner(held);
  refreshing_ = false;
  refresh();
}

void RowsTable::pushPoleWord(Side side, std::size_t index, Pole pole) {
  const std::size_t which = cornerOf(side);
  setSide(side);
  if (pole == poleAt(which, index)) return;
  if (pole == Pole::kReal) {
    state_->setRealRootAt(which, index, EditorState::Lane::kPole,
                          decayHzOf(kRealPoleSlowRadius), decayHzOf(kRealPoleFastRadius));
  } else {
    state_->setRootAt(which, index, EditorState::Lane::kPole, kRingSeed.hz, kRingSeed.bw_hz);
  }
  refresh();
}

void RowsTable::landEntry(Side side, std::size_t index, Kind kind, const QString& entry) {
  if (refreshing_) return;
  Cell& cell = cellOf(side, index);
  bool ok = false;
  const double typed = firstNumber(entry, &ok);
  if (!ok) {
    refresh();
    return;
  }
  const std::size_t which = cornerOf(side);
  setSide(side);
  const auto words = state_->packed().words[which][index];
  const Shape shape = shapeAt(which, index);
  int best = -1;
  double best_cost = std::numeric_limits<double>::infinity();
  const auto consider = [&](int dial, double cost) {
    if (!std::isfinite(cost) || cost >= best_cost) return;
    best_cost = cost;
    best = dial;
  };

  if (kind == Kind::kNote) {
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
    if (best >= 0) cell.note->setValue(best);
  } else if (kind == Kind::kRing) {
    if (!(typed > 0.0)) {
      refresh();
      return;
    }
    const double target = std::log2(typed);
    for (int dial = 0; dial <= maxPoleRes(); ++dial) {
      const Root root =
          rootOf(words[2], p2k::dial_word(static_cast<std::size_t>(kTopDial - dial)));
      const double resonance = resonanceOf(root);
      if (!(resonance > 0.0) || !std::isfinite(resonance)) continue;
      consider(dial, std::abs(std::log2(resonance) - target));
    }
    if (best >= 0) cell.ring->setValue(best);
  } else if (kind == Kind::kHeight && ceilingAt(which, index)) {
    if (!(typed > 0.0)) {
      refresh();
      return;
    }
    const double target = std::log2(typed);
    for (int dial = 0; dial <= static_cast<int>(p2k::kMaxMagByte); ++dial) {
      const double zero_hz =
          rootOf(p2k::dial_word(static_cast<std::size_t>(dial)), p2k::kS6ZeroRsqWord).hz;
      if (!(zero_hz > 0.0) || !std::isfinite(zero_hz)) continue;
      consider(dial, std::abs(std::log2(zero_hz) - target));
    }
    if (best >= 0) cell.height->setValue(best);
  } else if (kind == Kind::kHeight) {
    if (shape != Shape::kPair) {
      refresh();
      return;
    }
    best = nearestGainDial(side, index, typed);
    if (best >= 0) cell.height->setValue(best);
  } else {
    if (ceilingAt(which, index) || (shape != Shape::kPair && shape != Shape::kNotch)) {
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

void RowsTable::handleBegin(std::size_t index) {
  drag_side_ = sideOfEditingCorner();
  drag_ring_ = cellOf(drag_side_, index).ring->value();
}

void RowsTable::handleNote(std::size_t index, double hz) {
  const Side side = drag_side_;
  const int dial = nearestFrequencyDial(side, index, hz);
  if (dial < 0) return;
  refreshing_ = true;
  cellOf(side, index).note->setValue(dial);
  refreshing_ = false;
  pushPole(side, index, Kind::kNote);
}

void RowsTable::handleHeight(std::size_t index, double db) {
  const Side side = drag_side_;
  if (typeAt(cornerOf(side), index) != Type::kEq) return;
  const int dial = nearestGainDial(side, index, db);
  if (dial < 0) return;
  refreshing_ = true;
  cellOf(side, index).height->setValue(dial);
  refreshing_ = false;
  pushGain(side, index);
}

void RowsTable::handleRingSteps(std::size_t index, int steps) {
  const Side side = drag_side_;
  Cell& cell = cellOf(side, index);
  cell.ring_touched = true;
  cell.ring->setFollowing(false);
  refreshing_ = true;
  cell.ring->setValue(drag_ring_ + steps);
  refreshing_ = false;
  pushPole(side, index, Kind::kRing);
}

void RowsTable::handleWheelRing(std::size_t index, int steps) {
  const Side side = sideOfEditingCorner();
  Cell& cell = cellOf(side, index);
  cell.ring_touched = true;
  cell.ring->setFollowing(false);
  refreshing_ = true;
  cell.ring->setValue(cell.ring->value() + steps);
  refreshing_ = false;
  pushPole(side, index, Kind::kRing);
}

void RowsTable::handleCreate(double hz, double db) {
  const Side side = sideOfEditingCorner();
  const std::size_t which = cornerOf(side);
  for (std::size_t index = 0; index < kCeiling; ++index) {
    if (state_->sectionEnabledAt(which, index)) continue;
    state_->beginUndoGroup();
    pushType(side, index, Type::kEq);
    drag_side_ = side;
    handleNote(index, hz);
    handleHeight(index, db);
    state_->endUndoGroup();
    state_->selectSection(index);
    refresh();
    return;
  }
}

void RowsTable::refresh() {
  if (refreshing_) return;
  refreshing_ = true;
  const std::size_t selected = state_->selectedSection();
  {
    const QSignalBlocker root_blocker(root_dial_);
    const double target = std::log2(root_hz_);
    int best = 0;
    double best_cost = std::numeric_limits<double>::infinity();
    for (int dial = 0; dial <= static_cast<int>(p2k::kMaxMagByte); ++dial) {
      const double hz =
          rootOf(p2k::dial_word(static_cast<std::size_t>(dial)), rootReferenceRsq()).hz;
      if (!(hz > 0.0) || !std::isfinite(hz)) continue;
      const double cost = std::abs(std::log2(hz) - target);
      if (cost < best_cost) {
        best_cost = cost;
        best = dial;
      }
    }
    root_dial_->setValue(best);
    root_dial_->setText(QString::asprintf("%.1f Hz", root_hz_));
    root_entry_->setText(QString::asprintf("%.1f", root_hz_));
    root_note_->setText(QStringLiteral("ROOT %1").arg(noteName(root_hz_)));
  }
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    Row& row = rows_[index];
    row.number->setText(index == selected ? QStringLiteral("> %1").arg(index + 1)
                                          : QString::number(index + 1));
    {
      const QSignalBlocker type_blocker(row.type);
      const Type shown = typeAt(cornerOf(Side::kLo), index);
      const int found = row.type->findData(static_cast<int>(shown));
      row.type->setCurrentIndex(found >= 0 ? found : 0);
    }
    for (int which = 0; which < 2; ++which) {
      const Side side = which == 0 ? Side::kLo : Side::kHi;
      const std::size_t corner = cornerOf(side);
      Cell& cell = row.sides[static_cast<std::size_t>(which)];
      const auto& words = state_->packed().words[corner][index];
      const bool enabled = state_->sectionEnabledAt(corner, index);
      const Shape shape = shapeAt(corner, index);
      const bool real_pole = enabled && poleAt(corner, index) == Pole::kReal;
      const bool ceiling = index == kCeiling && (!enabled || ceilingAt(corner, index));
      const bool paired = enabled && !ceiling && shape == Shape::kPair;
      const bool offset_live =
          enabled && !ceiling && (shape == Shape::kPair || shape == Shape::kNotch);

      const QSignalBlocker note_blocker(cell.note);
      const QSignalBlocker ring_blocker(cell.ring);
      const QSignalBlocker height_blocker(cell.height);
      const QSignalBlocker pole_blocker(cell.pole);
      const QSignalBlocker harm_blocker(cell.harm);
      const QSignalBlocker cut_blocker(cell.cut);

      cell.note->setValue(static_cast<int>(p2k::dial_of_word(words[2])));
      cell.note->setText(!enabled    ? QStringLiteral("—")
                         : real_pole ? QStringLiteral("TILT")
                                     : hzText(words[2], words[3]) + QStringLiteral(" ") +
                                           noteName(rootOf(words[2], words[3]).hz));
      cell.ring->setValue(kTopDial - static_cast<int>(p2k::dial_of_word(words[3])));
      cell.ring->setText(!enabled    ? QStringLiteral("—")
                         : real_pole ? realPoleText(words)
                                     : resonanceText(words[2], words[3]));
      cell.ring->setFollowing(followingAt(side, index));
      if (ceiling) {
        cell.height->setRange(0, static_cast<int>(p2k::kMaxMagByte));
        cell.height->setValue(static_cast<int>(p2k::dial_of_word(words[0])));
        cell.height->setText(enabled ? hzText(words[0], words[1]) : QStringLiteral("—"));
      } else {
        cell.height->setRange(1, kTopDial);
        cell.height->setValue(paired ? static_cast<int>(p2k::dial_of_word(words[1])) : 1);
        cell.height->setText(paired ? gainText(words) : QStringLiteral("—"));
      }
      if (cell.offset != nullptr) {
        const QSignalBlocker offset_blocker(cell.offset);
        const double semitones = offset_live ? semitonesOf(words) : 0.0;
        cell.offset->setValue(static_cast<int>(std::lround(semitones)));
        cell.offset->setText(offset_live ? offsetText(words) : QStringLiteral("—"));
        cell.offset->setEnabled(offset_live);
      }
      cell.pole->setCurrentIndex(static_cast<int>(poleAt(corner, index)));
      cell.harm->setItemText(0, enabled && !real_pole
                                    ? intervalText(rootOf(words[2], words[3]).hz, root_hz_)
                                    : QStringLiteral("-"));
      cell.harm->setCurrentIndex(harmonicOf(side, index));
      cell.cut->setValue(state_->cutAt(corner, index));

      cell.note->setEnabled(enabled && !real_pole);
      cell.ring->setEnabled(enabled);
      cell.height->setEnabled(ceiling ? enabled : paired);
      cell.pole->setEnabled(enabled);
      cell.harm->setEnabled(enabled && !real_pole);
      cell.cut->setEnabled(enabled);
    }
  }
  refreshing_ = false;
  refreshSoFar();
}

double RowsTable::soFarDbAt(std::size_t row, double hz) const {
  const std::size_t through = std::min(row, trench::core::native::kSections);
  const trench::core::Cascade at_pad = state_->cascade(EditorState::kDatumHz);
  double db = 0.0;
  for (std::size_t index = 0; index < through; ++index) {
    db += trench::core::section_response_db(at_pad[index], hz, EditorState::kDatumHz);
  }
  return db;
}

void RowsTable::refreshSoFar() {
  const std::vector<double>& grid = soFarGrid();
  const trench::core::Cascade at_pad = state_->cascade(EditorState::kDatumHz);
  std::vector<double> running(grid.size(), 0.0);
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    for (std::size_t point = 0; point < grid.size(); ++point) {
      running[point] += trench::core::section_response_db(at_pad[index], grid[point],
                                                          EditorState::kDatumHz);
    }
    if (rows_[index].so_far != nullptr) rows_[index].so_far->setCurve(running);
  }
}
