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
  setFixedSize(86, 44);
  setCursor(Qt::SizeVerCursor);
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
  painter.drawText(QRectF(0.0, 5.0, span, 13.0),
                   Qt::AlignHCenter | Qt::AlignVCenter, label_);

  QFont value_font = painter.font();
  value_font.setCapitalization(QFont::MixedCase);
  value_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
  value_font.setWeight(QFont::Normal);
  value_font.setPixelSize(13);
  painter.setFont(value_font);
  painter.setPen(dragging_ ? kAccent : kInk);
  painter.drawText(QRectF(0.0, 18.0, span, 20.0),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   formatter_ ? formatter_(total_) : QString());

  painter.setPen(QPen(dragging_ ? kAccent : kRule, 1.0));
  const double rule_y = static_cast<double>(height()) - 4.5;
  painter.drawLine(QPointF{0.0, rule_y}, QPointF{span, rule_y});
}

void GestureDial::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  dragging_ = true;
  last_y_ = event->position().y();
  total_ = 0.0;
  update();
}

void GestureDial::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) return;
  const double y = event->position().y();
  const double pixels = last_y_ - y;
  last_y_ = y;
  if (pixels == 0.0) return;
  const double scale =
      event->modifiers().testFlag(Qt::ShiftModifier) ? kFineScale : 1.0;
  const double delta = pixels * units_per_pixel_ * scale;
  if (onDelta) onDelta(delta);
  total_ += delta;
  update();
}

void GestureDial::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !dragging_) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  dragging_ = false;
  total_ = 0.0;
  update();
}

void GestureDial::mouseDoubleClickEvent(QMouseEvent*) {}
