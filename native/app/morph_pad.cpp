#include "morph_pad.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <cstddef>

namespace {

constexpr double kCornerSize = 12.0;
constexpr QColor kChassis{237, 235, 230};
constexpr QColor kCard{30, 34, 38};
constexpr QColor kHairline{50, 55, 59};
constexpr QColor kGrid{52, 58, 63};
constexpr QColor kLive{196, 103, 79};
constexpr QColor kQuiet{210, 207, 198};
constexpr QColor kCaption{139, 139, 132};

double morphOf(std::size_t index) {
  return index == 1 || index == 3 ? 1.0 : 0.0;
}

double qOf(std::size_t index) { return index >= 2 ? 1.0 : 0.0; }

}  // namespace

MorphPad::MorphPad(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setFixedSize(170, 170);
  setAccessibleName(QStringLiteral("Morph and Q interior"));
  connect(state_, &EditorState::changed, this, qOverload<>(&MorphPad::update));
  connect(state_, &EditorState::selectionChanged, this, [this] { update(); });
}

QRectF MorphPad::field() const {
  return QRectF(rect()).adjusted(22.0, 12.0, -12.0, -22.0);
}

QRectF MorphPad::cornerRect(std::size_t index) const {
  const QRectF bounds = field();
  return {morphOf(index) > 0.0 ? bounds.right() - kCornerSize : bounds.left(),
          qOf(index) > 0.0 ? bounds.top() : bounds.bottom() - kCornerSize,
          kCornerSize, kCornerSize};
}

QPointF MorphPad::pointFor(double morph, double q) const {
  const QRectF bounds = field();
  return {bounds.left() + morph * bounds.width(),
          bounds.bottom() - q * bounds.height()};
}

void MorphPad::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kChassis);
  const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kCard);
  painter.drawRoundedRect(card, 6.0, 6.0);
  painter.setPen(QPen(kHairline, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(card, 6.0, 6.0);
  const QRectF bounds = field();

  painter.setPen(QPen(kGrid, 1.0));
  for (int step = 1; step < 4; ++step) {
    const double fraction = static_cast<double>(step) / 4.0;
    const double x = bounds.left() + fraction * bounds.width();
    const double y = bounds.top() + fraction * bounds.height();
    painter.drawLine(QPointF{x, bounds.top()}, QPointF{x, bounds.bottom()});
    painter.drawLine(QPointF{bounds.left(), y}, QPointF{bounds.right(), y});
  }
  painter.setBrush(Qt::NoBrush);

  for (std::size_t index = 0; index < trench::core::native::kCorners; ++index) {
    const QRectF seat = cornerRect(index);
    if (index == state_->editingCorner()) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(kLive);
      painter.drawRect(seat);
      painter.setBrush(Qt::NoBrush);
    } else {
      painter.setPen(QPen(kQuiet, 1.2));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(seat.adjusted(0.6, 0.6, -0.6, -0.6));
    }
  }

  const QPointF here = pointFor(state_->morphPos(), state_->qPos());
  painter.setPen(Qt::NoPen);
  painter.setBrush(kLive);
  painter.drawEllipse(here, 3.0, 3.0);
  painter.setBrush(Qt::NoBrush);

  QFont axis_font = painter.font();
  axis_font.setCapitalization(QFont::AllUppercase);
  axis_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  axis_font.setWeight(QFont::DemiBold);
  axis_font.setPixelSize(10);
  painter.setFont(axis_font);
  painter.setPen(kCaption);
  painter.drawText(QRectF{bounds.left(), bounds.bottom() + 4.0, bounds.width(),
                          16.0},
                   Qt::AlignCenter, QStringLiteral("MORPH"));
  painter.save();
  painter.translate(12.0, bounds.center().y());
  painter.rotate(-90.0);
  painter.drawText(QRectF{-20.0, -8.0, 40.0, 16.0}, Qt::AlignCenter,
                   QStringLiteral("Q"));
  painter.restore();
}

void MorphPad::trackTo(const QPointF& position) {
  const QRectF bounds = field();
  if (!(bounds.width() > 0.0) || !(bounds.height() > 0.0)) return;
  state_->setPadPosition((position.x() - bounds.left()) / bounds.width(),
                         (bounds.bottom() - position.y()) / bounds.height());
}

void MorphPad::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  for (std::size_t index = 0; index < trench::core::native::kCorners; ++index) {
    if (!cornerRect(index).contains(event->position())) continue;
    state_->setEditingCorner(index);
    state_->setPadPosition(morphOf(index), qOf(index));
    return;
  }
  trackTo(event->position());
}

void MorphPad::mouseMoveEvent(QMouseEvent* event) {
  if (!event->buttons().testFlag(Qt::LeftButton)) return;
  trackTo(event->position());
}
