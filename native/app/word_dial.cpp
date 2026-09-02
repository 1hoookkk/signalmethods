#include "word_dial.hpp"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>

namespace {

constexpr int kPixelsPerWord = 2;
constexpr int kFinePixelsPerWord = 10;
constexpr int kHeight = 132;
constexpr int kWidth = 48;
constexpr int kReadoutHeight = 18;
constexpr int kTrackWidth = 8;
constexpr int kKnobWidth = 26;
constexpr int kKnobHeight = 12;
constexpr int kMargin = 6;
constexpr QColor kWell{255, 255, 255};
constexpr QColor kEdge{128, 128, 128};
constexpr QColor kFocus{196, 103, 79};
constexpr QColor kTrack{224, 224, 224};
constexpr QColor kFill{0, 0, 0};
constexpr QColor kKnob{64, 64, 64};
constexpr QColor kKnobLine{255, 255, 255};

}

WordDial::WordDial(QWidget* parent) : QWidget(parent) {
  setFocusPolicy(Qt::ClickFocus);
  setCursor(Qt::SizeVerCursor);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  readout_ = new QLabel(this);
  readout_->setAlignment(Qt::AlignCenter);
  readout_->setAttribute(Qt::WA_TransparentForMouseEvents);
}

void WordDial::setRange(int minimum, int maximum) {
  minimum_ = std::min(minimum, maximum);
  maximum_ = std::max(minimum, maximum);
  setValue(value_);
}

void WordDial::setValue(int value) {
  const int clamped = std::clamp(value, minimum_, maximum_);
  if (clamped == value_) return;
  value_ = clamped;
  update();
  emit valueChanged(value_);
}

QSize WordDial::sizeHint() const { return {kWidth, kHeight}; }

void WordDial::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  setFocus(Qt::MouseFocusReason);
  dragging_ = true;
  press_y_ = event->position().toPoint().y();
  press_value_ = value_;
  event->accept();
}

void WordDial::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) return;
  const int per_word =
      (event->modifiers() & Qt::ShiftModifier) != 0 ? kFinePixelsPerWord : kPixelsPerWord;
  const int travel = press_y_ - event->position().toPoint().y();
  setValue(press_value_ + travel / per_word);
  event->accept();
}

void WordDial::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  dragging_ = false;
  event->accept();
}

void WordDial::wheelEvent(QWheelEvent* event) {
  const int steps = event->angleDelta().y() / 120;
  if (steps == 0) return;
  setValue(value_ + steps);
  event->accept();
}

void WordDial::keyPressEvent(QKeyEvent* event) {
  switch (event->key()) {
    case Qt::Key_Up: setValue(value_ + 1); break;
    case Qt::Key_Down: setValue(value_ - 1); break;
    case Qt::Key_PageUp: setValue(value_ + 12); break;
    case Qt::Key_PageDown: setValue(value_ - 12); break;
    default: QWidget::keyPressEvent(event); return;
  }
  event->accept();
}

void WordDial::resizeEvent(QResizeEvent*) {
  readout_->setGeometry(1, 1, width() - 2, kReadoutHeight);
}

void WordDial::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  const QRect well = rect().adjusted(0, 0, -1, -1);
  painter.setPen(QPen(hasFocus() ? kFocus : kEdge, hasFocus() ? 2.0 : 1.0));
  painter.setBrush(isEnabled() ? kWell : palette().window().color());
  painter.drawRect(well);
  const int top = kReadoutHeight + kMargin + kKnobHeight / 2;
  const int bottom = height() - kMargin - kKnobHeight / 2;
  if (bottom <= top) return;
  const QRect track((width() - kTrackWidth) / 2, top, kTrackWidth, bottom - top);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kTrack);
  painter.drawRect(track);
  if (!isEnabled() || maximum_ <= minimum_) return;
  const double fraction =
      static_cast<double>(value_ - minimum_) / static_cast<double>(maximum_ - minimum_);
  const int y = bottom - static_cast<int>(fraction * (bottom - top) + 0.5);
  painter.setBrush(kFill);
  painter.drawRect(QRect(track.left(), y, track.width(), bottom - y));
  const QRect knob((width() - kKnobWidth) / 2, y - kKnobHeight / 2, kKnobWidth, kKnobHeight);
  painter.setBrush(kKnob);
  painter.drawRect(knob);
  painter.setPen(QPen(kKnobLine, 1.0));
  painter.drawLine(knob.left() + 3, y, knob.right() - 3, y);
}
