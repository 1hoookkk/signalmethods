#include <QFontMetrics>
#include <algorithm>
#include "gesture_dial.hpp"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRectF>

#include <utility>

namespace {

constexpr QColor kChassis{237, 235, 230};
constexpr QColor kInk{38, 36, 31};
constexpr QColor kQuiet{139, 135, 124};
constexpr QColor kRule{201, 196, 184};
constexpr QColor kAccent{196, 103, 79};

constexpr double kFineScale = 0.2;

}  // namespace

GestureDial::GestureDial(QString label, double units_per_pixel,
                         std::function<QString(double)> formatter,
                         QWidget* parent)
    : QWidget(parent),
      label_(std::move(label)),
      units_per_pixel_(units_per_pixel),
      formatter_(std::move(formatter)) {
  QFont measure = font();
  measure.setPixelSize(10);
  measure.setWeight(QFont::DemiBold);
  measure.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  setFixedSize(std::max(150, QFontMetrics(measure).horizontalAdvance(label_.toUpper()) + 72), 46);
  setCursor(Qt::SizeHorCursor);
  setAccessibleName(label_);
}

void GestureDial::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kChassis);

  const double span = static_cast<double>(width());

  QFont label_font = painter.font();
  label_font.setCapitalization(QFont::AllUppercase);
  label_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  label_font.setWeight(QFont::DemiBold);
  label_font.setPixelSize(10);
  painter.setFont(label_font);
  painter.setPen(dragging_ ? kAccent : kQuiet);
  painter.drawText(QRectF(0.0, 3.0, span - 58.0, 13.0),
                   Qt::AlignLeft | Qt::AlignVCenter, label_);

  QFont value_font = painter.font();
  value_font.setCapitalization(QFont::MixedCase);
  value_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
  value_font.setWeight(QFont::Normal);
  value_font.setPixelSize(12);
  painter.setFont(value_font);
  painter.setPen(dragging_ ? kAccent : kInk);
  painter.drawText(QRectF(span - 58.0, 3.0, 58.0, 13.0),
                   Qt::AlignRight | Qt::AlignVCenter,
                   formatter_ ? formatter_(total_) : QString());

  // MIXER FADER, pitch-wheel law (Tyson 2026-08-29 "it should be mixer
  // faders"): a horizontal track with a centre detent; deflection applies the
  // gesture, release bakes it into the corner and the handle snaps home.
  const double track_y = 30.0;
  painter.setPen(QPen(kRule, 2.0, Qt::SolidLine, Qt::RoundCap));
  painter.drawLine(QPointF{4.0, track_y}, QPointF{span - 4.0, track_y});
  painter.setPen(QPen(dragging_ ? kAccent : kQuiet, 1.0));
  painter.drawLine(QPointF{span * 0.5, track_y - 5.0},
                   QPointF{span * 0.5, track_y + 5.0});
  const double travel = (span * 0.5) - 10.0;
  const double frac = std::clamp(deflect_ / kThrowPixels, -1.0, 1.0);
  const double hx = span * 0.5 + frac * travel;
  painter.setPen(QPen(kInk, 1.0));
  painter.setBrush(dragging_ ? kAccent : QColor{246, 244, 239});
  painter.drawRoundedRect(QRectF{hx - 5.0, track_y - 8.0, 10.0, 16.0}, 2.0, 2.0);
  painter.setBrush(Qt::NoBrush);
}

void GestureDial::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  dragging_ = true;
  last_y_ = event->position().x();
  deflect_ = 0.0;
  total_ = 0.0;
  if (onBegin) onBegin();
  update();
}

void GestureDial::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) return;
  const double x = event->position().x();
  const double pixels = x - last_y_;
  last_y_ = x;
  if (pixels == 0.0) return;
  const double scale =
      event->modifiers().testFlag(Qt::ShiftModifier) ? kFineScale : 1.0;
  const double delta = pixels * units_per_pixel_ * scale;
  if (onDelta) onDelta(delta);
  total_ += delta;
  deflect_ += pixels * scale;
  update();
}

void GestureDial::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !dragging_) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  dragging_ = false;
  deflect_ = 0.0;
  total_ = 0.0;
  if (onEnd) onEnd();
  update();
}

void GestureDial::mouseDoubleClickEvent(QMouseEvent*) {}
