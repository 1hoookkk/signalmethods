#include "morph_pad.hpp"

#include "path_meter.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

#include <cstddef>

namespace {

constexpr double kCornerSize = 12.0;
constexpr QColor kPanel{255, 255, 255};
constexpr QColor kPanelEdge{200, 200, 200};
constexpr QColor kGrid{230, 230, 230};
constexpr QColor kLive{196, 103, 79};
constexpr QColor kQuiet{128, 128, 128};
constexpr QColor kCaption{64, 64, 64};
constexpr QColor kWarn{214, 150, 38};
constexpr QColor kHot{196, 60, 48};

double morphOf(std::size_t index) {
  return index == 1 || index == 3 ? 1.0 : 0.0;
}

double qOf(std::size_t index) { return index >= 2 ? 1.0 : 0.0; }

}

MorphPad::MorphPad(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setFixedSize(170, 170);
  setAccessibleName(QStringLiteral("Morph and Q interior"));
  connect(state_, &EditorState::changed, this, qOverload<>(&MorphPad::update));
  connect(state_, &EditorState::selectionChanged, this, [this] { update(); });
}

void MorphPad::setWorst(double morph, double q, double db) {
  worst_morph_ = morph;
  worst_q_ = q;
  worst_db_ = db;
  update();
}

QString MorphPad::morphLabel() const {
  const QString name = state_->axisNames().first;
  return name.isEmpty() ? QStringLiteral("MORPH") : name.toUpper();
}

QString MorphPad::qLabel() const {
  const QString name = state_->axisNames().second;
  return name.isEmpty() ? QStringLiteral("Q") : name.toUpper();
}

void MorphPad::renameMorphAxis(const QString& name) {
  state_->setAxisNames(name.trimmed(), state_->axisNames().second);
}

void MorphPad::renameQAxis(const QString& name) {
  state_->setAxisNames(state_->axisNames().first, name.trimmed());
}

QRectF MorphPad::field() const {
  return QRectF(rect()).adjusted(22.0, 12.0, -12.0, -22.0);
}

QRectF MorphPad::morphNameRect() const {
  const QRectF bounds = field();
  return {bounds.left(), bounds.bottom() + 4.0, bounds.width(), 16.0};
}

QRectF MorphPad::qNameRect() const {
  const QRectF bounds = field();
  return {4.0, bounds.top(), 16.0, bounds.height()};
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
  painter.fillRect(rect(), palette().window().color());
  const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kPanel);
  painter.drawRect(card);
  painter.setPen(QPen(kPanelEdge, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(card);
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
      painter.setPen(QPen(kQuiet, 1.0));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(seat.adjusted(0.6, 0.6, -0.6, -0.6));
    }
  }

  if (worst_db_ >= PathMeter::kWarnDb) {
    painter.setPen(QPen(worst_db_ >= PathMeter::kFrameDb ? kHot : kWarn, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pointFor(worst_morph_, worst_q_), 6.0, 6.0);
  }
  const QPointF here = pointFor(state_->morphPos(), state_->qPos());
  painter.setPen(Qt::NoPen);
  painter.setBrush(kLive);
  painter.drawEllipse(here, 3.0, 3.0);
  painter.setBrush(Qt::NoBrush);

  painter.setPen(kCaption);
  const QFontMetrics metrics(painter.font());
  const QRectF morph_seat = morphNameRect();
  painter.drawText(morph_seat, Qt::AlignCenter,
                   metrics.elidedText(morphLabel(), Qt::ElideRight,
                                      static_cast<int>(morph_seat.width())));
  painter.save();
  painter.translate(12.0, bounds.center().y());
  painter.rotate(-90.0);
  const QRectF q_seat{-0.5 * bounds.height(), -8.0, bounds.height(), 16.0};
  painter.drawText(q_seat, Qt::AlignCenter,
                   metrics.elidedText(qLabel(), Qt::ElideRight,
                                      static_cast<int>(q_seat.width())));
  painter.restore();
}

void MorphPad::trackTo(const QPointF& position) {
  const QRectF bounds = field();
  if (!(bounds.width() > 0.0) || !(bounds.height() > 0.0)) return;
  state_->setPadPosition((position.x() - bounds.left()) / bounds.width(),
                         (bounds.bottom() - position.y()) / bounds.height());
}

void MorphPad::openEditor(Axis axis) {
  if (editor_ == nullptr) {
    editor_ = new QLineEdit(this);
    editor_->setObjectName(QStringLiteral("axisEntry"));
    editor_->setAlignment(Qt::AlignCenter);
    editor_->installEventFilter(this);
    connect(editor_, &QLineEdit::returnPressed, this, [this] { commitEditor(); });
  }
  editing_ = axis;
  const QRectF seat = axis == Axis::kMorph ? morphNameRect() : field();
  editor_->setGeometry(axis == Axis::kMorph
                           ? seat.toRect()
                           : QRectF(seat.left(), seat.center().y() - 9.0, seat.width(),
                                    18.0)
                                 .toRect());
  editor_->setText(axis == Axis::kMorph ? state_->axisNames().first
                                        : state_->axisNames().second);
  editor_->selectAll();
  editor_->show();
  editor_->raise();
  editor_->setFocus(Qt::MouseFocusReason);
}

void MorphPad::commitEditor() {
  if (editor_ == nullptr || editing_ == Axis::kNone) return;
  const QString typed = editor_->text();
  const Axis axis = editing_;
  closeEditor();
  if (axis == Axis::kMorph) {
    renameMorphAxis(typed);
  } else {
    renameQAxis(typed);
  }
}

void MorphPad::closeEditor() {
  editing_ = Axis::kNone;
  if (editor_ == nullptr) return;
  editor_->hide();
  editor_->clearFocus();
}

bool MorphPad::eventFilter(QObject* watched, QEvent* event) {
  if (watched == editor_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      closeEditor();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void MorphPad::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  if (morphNameRect().contains(event->position())) {
    openEditor(Axis::kMorph);
    return;
  }
  if (qNameRect().contains(event->position())) {
    openEditor(Axis::kQ);
    return;
  }
  closeEditor();
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
