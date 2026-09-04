#include "number_box.hpp"

#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>

#include <algorithm>

namespace {

constexpr int kWidth = 96;
constexpr int kHeight = 22;
constexpr int kPixelsPerStep = 3;
constexpr int kFinePixelsPerStep = 12;
constexpr int kPageStep = 8;
constexpr QColor kWell{255, 255, 255};
constexpr QColor kEdge{128, 128, 128};
constexpr QColor kFocus{196, 103, 79};
constexpr QColor kInk{0, 0, 0};
constexpr QColor kQuiet{128, 128, 128};

}

NumberBox::NumberBox(QWidget* parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::SizeVerCursor);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setMinimumSize(kWidth, kHeight);
}

QSize NumberBox::sizeHint() const { return {kWidth, kHeight}; }

void NumberBox::setRange(int minimum, int maximum) {
  minimum_ = std::min(minimum, maximum);
  maximum_ = std::max(minimum, maximum);
  const int clamped = std::clamp(value_, minimum_, maximum_);
  if (clamped != value_) {
    value_ = clamped;
    update();
    emit valueChanged(value_);
  }
}

void NumberBox::setValue(int value) {
  const int clamped = std::clamp(value, minimum_, maximum_);
  if (clamped == value_) return;
  value_ = clamped;
  update();
  emit valueChanged(value_);
}

void NumberBox::setText(const QString& text) {
  if (text == text_) return;
  text_ = text;
  update();
}

void NumberBox::setFollowing(bool following) {
  if (following == following_) return;
  following_ = following;
  update();
}

void NumberBox::unfollow() {
  if (!following_) return;
  following_ = false;
  update();
  emit touched();
}

void NumberBox::type(const QString& entry) {
  unfollow();
  emit typed(entry);
}

void NumberBox::openEditor() {
  if (!isEnabled()) return;
  if (editor_ == nullptr) {
    editor_ = new QLineEdit(this);
    editor_->setAlignment(Qt::AlignCenter);
    editor_->installEventFilter(this);
    connect(editor_, &QLineEdit::returnPressed, this, &NumberBox::commitEditor);
  }
  editor_->setGeometry(rect());
  editor_->setText(text_);
  editor_->selectAll();
  editor_->show();
  editor_->setFocus(Qt::OtherFocusReason);
}

void NumberBox::closeEditor() {
  if (editor_ == nullptr) return;
  editor_->hide();
  setFocus(Qt::OtherFocusReason);
}

void NumberBox::commitEditor() {
  if (editor_ == nullptr) return;
  const QString entry = editor_->text();
  closeEditor();
  type(entry);
}

bool NumberBox::eventFilter(QObject* watched, QEvent* event) {
  if (watched == editor_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      closeEditor();
      return true;
    }
  }
  if (watched == editor_ && event->type() == QEvent::FocusOut) closeEditor();
  return QWidget::eventFilter(watched, event);
}

void NumberBox::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  setFocus(Qt::MouseFocusReason);
  dragging_ = true;
  press_y_ = event->position().toPoint().y();
  press_value_ = value_;
  event->accept();
}

void NumberBox::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) return;
  const int per_step =
      (event->modifiers() & Qt::ShiftModifier) != 0 ? kFinePixelsPerStep : kPixelsPerStep;
  const int travel = press_y_ - event->position().toPoint().y();
  const int wanted = std::clamp(press_value_ + travel / per_step, minimum_, maximum_);
  if (wanted != value_) unfollow();
  setValue(wanted);
  event->accept();
}

void NumberBox::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  dragging_ = false;
  event->accept();
}

void NumberBox::mouseDoubleClickEvent(QMouseEvent* event) {
  dragging_ = false;
  openEditor();
  event->accept();
}

void NumberBox::wheelEvent(QWheelEvent* event) {
  const int steps = event->angleDelta().y() / 120;
  if (steps == 0) return;
  unfollow();
  setValue(value_ + steps);
  event->accept();
}

void NumberBox::keyPressEvent(QKeyEvent* event) {
  switch (event->key()) {
    case Qt::Key_Up:
      unfollow();
      setValue(value_ + 1);
      break;
    case Qt::Key_Down:
      unfollow();
      setValue(value_ - 1);
      break;
    case Qt::Key_PageUp:
      unfollow();
      setValue(value_ + kPageStep);
      break;
    case Qt::Key_PageDown:
      unfollow();
      setValue(value_ - kPageStep);
      break;
    case Qt::Key_F2:
    case Qt::Key_Return:
    case Qt::Key_Enter:
      openEditor();
      break;
    default:
      QWidget::keyPressEvent(event);
      return;
  }
  event->accept();
}

void NumberBox::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  const QRectF well = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  if (following_) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().window().color());
    painter.drawRect(well);
  } else {
    painter.setPen(QPen(hasFocus() ? kFocus : kEdge, hasFocus() ? 2.0 : 1.0));
    painter.setBrush(isEnabled() ? kWell : palette().window().color());
    painter.drawRect(well);
  }
  QFont face = font();
  face.setItalic(following_);
  painter.setFont(face);
  painter.setPen(following_ || !isEnabled() ? kQuiet : kInk);
  painter.drawText(rect().adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignHCenter, text_);
}
