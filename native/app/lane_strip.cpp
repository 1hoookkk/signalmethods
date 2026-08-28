#include "lane_strip.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

const QColor kChassis{26, 31, 35};
const QColor kInk{174, 186, 190};
const QColor kDim{118, 127, 131};
const QColor kLine{55, 63, 67};
const QColor kAccent{87, 222, 205};
const QColor kHeld{247, 184, 92};

constexpr int kStripHeight = 46;
constexpr double kRowTop = 24.0;
constexpr double kRowHigh = 18.0;
constexpr double kPadWide = 12.0;
constexpr double kPadHigh = 9.0;
constexpr double kSignEpsilon = 0.05;

const QString kPadStyle = QStringLiteral(
    "QToolButton { background: #1a1f23; color: #7d888c; border: 1px solid #373f43; "
    "border-radius: 2px; font-size: 9px; font-weight: 600; padding: 0 6px; }"
    "QToolButton:checked { background: #57decd; color: #0d1113; border-color: #57decd; }");

QFont head_font() { return QFont(QStringLiteral("Segoe UI"), 7, QFont::DemiBold); }

QToolButton* make_pad(QWidget* parent, const QString& name) {
  auto* pad = new QToolButton(parent);
  pad->setObjectName(name);
  pad->setCheckable(true);
  pad->setFocusPolicy(Qt::NoFocus);
  pad->setCursor(Qt::PointingHandCursor);
  pad->setFixedHeight(16);
  pad->setMinimumWidth(46);
  pad->setFont(head_font());
  pad->setStyleSheet(kPadStyle);
  return pad;
}

QLabel* make_caption(QWidget* parent, const QString& text) {
  auto* label = new QLabel(text, parent);
  label->setFont(head_font());
  label->setStyleSheet(QStringLiteral("color: %1; letter-spacing: 1px;").arg(kDim.name()));
  return label;
}

}  // namespace

LaneStrip::LaneStrip(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("laneStrip"));
  setFixedHeight(kStripHeight);

  auto* header = new QHBoxLayout();
  header->setContentsMargins(8, 3, 8, 0);
  header->setSpacing(6);
  header->addWidget(make_caption(this, QStringLiteral("M0 FROM")), 0);
  pad_from_ = make_pad(this, QStringLiteral("lanePadFrom"));
  header->addWidget(pad_from_, 0);
  header->addSpacing(12);
  header->addWidget(make_caption(this, QStringLiteral("M100 TO")), 0);
  pad_to_ = make_pad(this, QStringLiteral("lanePadTo"));
  header->addWidget(pad_to_, 0);
  header->addSpacing(12);
  row_label_ = make_caption(this, QStringLiteral("Q0"));
  row_label_->setObjectName(QStringLiteral("laneRowLabel"));
  header->addWidget(row_label_, 0);
  header->addStretch(1);

  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  column->addLayout(header, 0);
  column->addStretch(1);

  connect(pad_from_, &QToolButton::clicked, this,
          [this] { emit endpointPicked(fromCorner()); });
  connect(pad_to_, &QToolButton::clicked, this,
          [this] { emit endpointPicked(toCorner()); });
}

std::size_t LaneStrip::fromCorner() const noexcept { return corner_ & ~std::size_t{1}; }

std::size_t LaneStrip::toCorner() const noexcept { return fromCorner() | std::size_t{1}; }

QRectF LaneStrip::lane(std::size_t section) const {
  const auto width = std::max(120.0, static_cast<double>(this->width()) - 20.0);
  const auto slot = width / static_cast<double>(trench::core::native::kSections);
  return QRectF(10.0 + slot * static_cast<double>(section), kRowTop, slot, kRowHigh);
}

QRectF LaneStrip::stagePad(std::size_t section) const {
  const auto column = lane(section);
  return QRectF(column.left() + 4.0, column.top() + 4.0, kPadWide, kPadHigh);
}

QRectF LaneStrip::lockPad(std::size_t section) const {
  const auto column = lane(section);
  return QRectF(column.left() + 4.0 + kPadWide + 3.0, column.top() + 4.0, kPadWide,
                kPadHigh);
}

void LaneStrip::setBody(const trench::core::native::Body* body, double sample_rate_hz) {
  body_ = body;
  sample_rate_hz_ = sample_rate_hz;
  refresh();
}

void LaneStrip::setCorner(std::size_t corner) {
  if (corner >= trench::core::native::kCorners || corner == corner_) return;
  corner_ = corner;
  refresh();
}

void LaneStrip::setSelected(std::size_t section) {
  if (section >= trench::core::native::kSections || section == selected_section_) return;
  selected_section_ = section;
  update();
}

void LaneStrip::setEndpointNames(const QString& from, const QString& to) {
  pad_from_->setText(from.isEmpty() ? QStringLiteral("—") : from);
  pad_to_->setText(to.isEmpty() ? QStringLiteral("—") : to);
}

void LaneStrip::setFreedomMask(std::uint32_t mask) {
  if (mask == freedom_mask_) return;
  freedom_mask_ = mask;
  update();
}

std::optional<trench::core::native::Resonant> LaneStrip::poleAt(std::size_t section,
                                                                std::size_t end) const {
  if (body_ == nullptr || section >= trench::core::native::kSections) return std::nullopt;
  const auto corner = end == kTo ? toCorner() : fromCorner();
  if (corner >= trench::core::native::kCorners) return std::nullopt;
  const auto& roots = body_->corners[corner].sections[section].pole;
  if (const auto* resonant = std::get_if<trench::core::native::Resonant>(&roots)) {
    return *resonant;
  }
  return std::nullopt;
}

bool LaneStrip::stageLive(std::size_t section) const {
  if (body_ == nullptr || section >= trench::core::native::kSections) return false;
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    const auto& roots = body_->corners[corner].sections[section].pole;
    if (std::get_if<trench::core::native::Resonant>(&roots) != nullptr) return true;
    const auto* real = std::get_if<trench::core::native::RealRoots>(&roots);
    if (real != nullptr && (std::isfinite(real->a_hz) || std::isfinite(real->b_hz))) {
      return true;
    }
  }
  return false;
}

bool LaneStrip::stageLocked(std::size_t section) const {
  if (section >= trench::core::native::kSections) return false;
  return (freedom_mask_ & trench::core::native::pole_bit(section)) == 0U;
}

QString LaneStrip::travelText(std::size_t section) const {
  const auto start = poleAt(section, kFrom);
  const auto finish = poleAt(section, kTo);
  if (!start || !finish || start->hz <= 0.0 || finish->hz <= 0.0) {
    return QStringLiteral("—");
  }
  const auto semitones = 12.0 * std::log2(finish->hz / start->hz);
  return QStringLiteral("%1%2 st")
      .arg(semitones > kSignEpsilon ? QStringLiteral("+") : QString())
      .arg(semitones, 0, 'f', 1);
}

void LaneStrip::refresh() {
  pad_from_->setChecked(corner_ == fromCorner());
  pad_to_->setChecked(corner_ == toCorner());
  row_label_->setText(corner_ >= 2 ? QStringLiteral("Q100") : QStringLiteral("Q0"));
  update();
}

void LaneStrip::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const auto at = event->position();
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (stagePad(section).adjusted(-2.0, -3.0, 2.0, 3.0).contains(at)) {
      emit stageToggled(section, !stageLive(section));
      return;
    }
    if (lockPad(section).adjusted(-2.0, -3.0, 2.0, 3.0).contains(at)) {
      emit lockToggled(section);
      return;
    }
    if (lane(section).contains(at)) {
      emit laneSelected(section, corner_);
      return;
    }
  }
}

void LaneStrip::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kChassis);
  painter.setPen(kLine);
  painter.drawLine(0, 0, width(), 0);
  painter.setFont(head_font());

  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    const auto column = lane(section);
    const bool chosen = section == selected_section_;
    const bool live = stageLive(section);
    const bool locked = stageLocked(section);

    const auto pad = stagePad(section);
    painter.setPen(QPen(live ? (chosen ? kAccent : kInk) : kLine, 1.0));
    painter.setBrush(live ? QBrush(chosen ? kAccent : kInk) : QBrush(Qt::NoBrush));
    painter.drawRoundedRect(pad, 1.5, 1.5);

    const auto lock = lockPad(section);
    painter.setPen(QPen(locked ? kHeld : kLine, 1.0));
    painter.setBrush(locked ? QBrush(kHeld) : QBrush(Qt::NoBrush));
    painter.drawRoundedRect(lock, 1.5, 1.5);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(live ? (chosen ? kAccent : kDim) : kLine);
    const auto text_left = lock.right() + 6.0;
    painter.drawText(QRectF(text_left, column.top(), 22.0, column.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("S%1").arg(section + 1));

    painter.setPen(chosen ? kInk : kDim);
    painter.drawText(
        QRectF(text_left + 22.0, column.top(), column.right() - text_left - 26.0,
               column.height()),
        Qt::AlignLeft | Qt::AlignVCenter, travelText(section));
  }
}
