#include "section_strip.hpp"

#include "trench/core/packed_body.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr std::array<QColor, trench::core::native::kSections> kSectionColors{
    QColor{66, 224, 207}, QColor{231, 158, 76}, QColor{226, 210, 90},
    QColor{224, 99, 151}, QColor{92, 170, 238}, QColor{155, 213, 96}};

constexpr double kCellGap = 6.0;

}  // namespace

SectionStrip::SectionStrip(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setMinimumHeight(72);
  setMaximumHeight(82);
  setFocusPolicy(Qt::StrongFocus);
  setAccessibleName(QStringLiteral("Six filter sections"));
  connect(state_, &EditorState::changed, this,
          qOverload<>(&SectionStrip::update));
  connect(state_, &EditorState::selectionChanged, this,
          [this] { update(); });
}

QRectF SectionStrip::cell(std::size_t index) const {
  const QRectF bounds = QRectF(rect()).adjusted(0.0, 3.0, 0.0, -3.0);
  const double width =
      (bounds.width() - kCellGap * (trench::core::native::kSections - 1)) /
      trench::core::native::kSections;
  return {bounds.left() + static_cast<double>(index) * (width + kCellGap),
          bounds.top(), width, bounds.height()};
}

void SectionStrip::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor{12, 15, 17});
  const auto cascade = state_->cascade();

  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    const QRectF bounds = cell(index);
    const bool selected = index == state_->selectedSection();
    const QColor color = kSectionColors[index];
    painter.fillRect(bounds, QColor{20, 23, 26});
    painter.setPen(QPen(selected ? QColor{185, 236, 224}
                                 : QColor{54, 61, 64},
                        1.0));
    painter.drawRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5));

    QFont number_font = painter.font();
    number_font.setBold(true);
    number_font.setPointSizeF(8.5);
    painter.setFont(number_font);
    painter.setPen(color);
    painter.drawText(bounds.adjusted(7.0, 5.0, -7.0, 0.0),
                     Qt::AlignLeft | Qt::AlignTop,
                     QString::number(index + 1));

    const QRectF plot = bounds.adjusted(8.0, 22.0, -8.0, -8.0);
    painter.setPen(QPen(QColor{44, 50, 53}, 1.0));
    painter.drawLine(QPointF{plot.left(), plot.center().y()},
                     QPointF{plot.right(), plot.center().y()});

    QPainterPath path;
    constexpr int kPoints = 96;
    for (int point = 0; point < kPoints; ++point) {
      const double fraction = static_cast<double>(point) / (kPoints - 1);
      const double hz = EditorState::kLowHz *
                        std::pow(EditorState::kHighHz / EditorState::kLowHz,
                                 fraction);
      const double db = std::clamp(trench::core::section_response_db(
                                       cascade[index], hz,
                                       EditorState::kDatumHz),
                                   -18.0, 18.0);
      const QPointF position{
          plot.left() + fraction * plot.width(),
          plot.center().y() - (db / 18.0) * plot.height() * 0.48};
      if (point == 0) {
        path.moveTo(position);
      } else {
        path.lineTo(position);
      }
    }
    QPen curve_pen(color, 1.0);
    curve_pen.setCosmetic(true);
    curve_pen.setCapStyle(Qt::FlatCap);
    painter.setPen(curve_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
  }
}

void SectionStrip::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    if (!cell(index).contains(event->position())) continue;
    state_->selectSection(index);
    setFocus(Qt::MouseFocusReason);
    return;
  }
}

void SectionStrip::keyPressEvent(QKeyEvent* event) {
  const std::size_t selected = state_->selectedSection();
  if (event->key() == Qt::Key_Left && selected > 0) {
    state_->selectSection(selected - 1);
    return;
  }
  if (event->key() == Qt::Key_Right &&
      selected + 1 < trench::core::native::kSections) {
    state_->selectSection(selected + 1);
    return;
  }
  if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_6) {
    state_->selectSection(static_cast<std::size_t>(event->key() - Qt::Key_1));
    return;
  }
  QWidget::keyPressEvent(event);
}
