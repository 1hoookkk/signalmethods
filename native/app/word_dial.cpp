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
constexpr int kBarHeight = 3;
constexpr int kHeight = 40;
constexpr QColor kWell{255, 255, 255};
constexpr QColor kEdge{128, 128, 128};
constexpr QColor kBar{0, 0, 0};
constexpr QColor kBarWell{224, 224, 224};

}

WordDial::WordDial(QWidget* parent) : QWidget(parent) {
  setFocusPolicy(Qt::ClickFocus);
  setCursor(Qt::SizeVerCursor);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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

QSize WordDial::sizeHint() const { return {60, kHeight}; }

void WordDial::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
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
  readout_->setGeometry(1, 1, width() - 2, height() - kBarHeight - 4);
}

void WordDial::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  const QRect well = rect().adjusted(0, 0, -1, -1);
  painter.setPen(QPen(kEdge, 1.0));
  painter.setBrush(isEnabled() ? kWell : palette().window().color());
  painter.drawRect(well);
  const QRect track(2, height() - kBarHeight - 2, width() - 4, kBarHeight);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kBarWell);
  painter.drawRect(track);
  if (!isEnabled() || maximum_ <= minimum_) return;
  const double fraction =
      static_cast<double>(value_ - minimum_) / static_cast<double>(maximum_ - minimum_);
  const int filled = static_cast<int>(fraction * track.width() + 0.5);
  painter.setBrush(kBar);
  painter.drawRect(QRect(track.left(), track.top(), filled, track.height()));
}
