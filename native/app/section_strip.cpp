#include "section_strip.hpp"

#include "trench/core/packed_body.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace {

constexpr std::array<QColor, trench::core::native::kSections> kSectionColors{
    QColor{66, 224, 207}, QColor{231, 158, 76}, QColor{226, 210, 90},
    QColor{224, 99, 151}, QColor{92, 170, 238}, QColor{155, 213, 96}};

constexpr double kCellGap = 14.0;

}  // namespace

SectionStrip::SectionStrip(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setMinimumHeight(72);
  setMaximumHeight(82);
  setFocusPolicy(Qt::StrongFocus);
  setAccessibleName(QStringLiteral("Six addressable filter sections"));
  connect(state_, &EditorState::changed, this,
          qOverload<>(&SectionStrip::update));
  connect(state_, &EditorState::selectionChanged, this,
          [this] { update(); });
}

QRectF SectionStrip::toggleRect(std::size_t index) const {
  const QRectF bounds = cell(index);
  return {bounds.right() - 42.0, bounds.top() + 5.0, 34.0, 16.0};
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
  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    const QRectF bounds = cell(index);
    const bool enabled = state_->sectionEnabled(index);
    const bool selected = index == state_->selectedSection();
    const QColor color = kSectionColors[index];
    painter.fillRect(bounds, QColor{enabled ? 20 : 15, enabled ? 23 : 18,
                                    enabled ? 26 : 20});
    painter.setPen(QPen(selected ? color : QColor{42, 48, 51}, 1.0));
    painter.drawRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5));

    QFont number_font = painter.font();
    number_font.setBold(true);
    number_font.setPointSizeF(8.5);
    painter.setFont(number_font);
    painter.setPen(selected || enabled ? color : QColor{79, 87, 90});
    painter.drawText(bounds.adjusted(7.0, 5.0, -7.0, 0.0),
                     Qt::AlignLeft | Qt::AlignTop,
                     QString::number(index + 1));

    const QRectF toggle = toggleRect(index);
    painter.fillRect(toggle, enabled ? color.darker(310) : QColor{24, 29, 31});
    painter.setPen(QPen(enabled ? color : QColor{83, 93, 96}, 1.0));
    painter.drawRect(toggle.adjusted(0.5, 0.5, -0.5, -0.5));
    QFont state_font = painter.font();
    state_font.setBold(true);
    state_font.setPointSizeF(7.0);
    painter.setFont(state_font);
    painter.drawText(toggle, Qt::AlignCenter,
                     enabled ? QStringLiteral("ON") : QStringLiteral("OFF"));

    painter.setPen(QPen(enabled ? color : QColor{70, 79, 82}, 1.0));
    painter.setBrush(Qt::NoBrush);
    const QPointF pole_mark{bounds.left() + 29.0, bounds.top() + 13.0};
    painter.drawEllipse(pole_mark, 3.0, 3.0);
    if (state_->rootPresent(index, EditorState::Lane::kZero)) {
      const QPointF zero_mark{bounds.left() + 43.0, bounds.top() + 13.0};
      QPainterPath diamond;
      diamond.moveTo(zero_mark + QPointF{0.0, -3.5});
      diamond.lineTo(zero_mark + QPointF{3.5, 0.0});
      diamond.lineTo(zero_mark + QPointF{0.0, 3.5});
      diamond.lineTo(zero_mark + QPointF{-3.5, 0.0});
      diamond.closeSubpath();
      painter.drawPath(diamond);
    }

    // An OFF slot claims nothing (Tyson 2026-08-28 "make it transparent to
    // whats happening"): no curve until the stage exists in the cascade.
    if (!enabled) continue;
    const QRectF plot = bounds.adjusted(8.0, 22.0, -8.0, -8.0);
    painter.setPen(QPen(QColor{44, 50, 53}, 1.0));
    constexpr double kMiniLowDb = -120.0;
    constexpr double kMiniHighDb = 24.0;
    const double zero_y = plot.top() +
                          (kMiniHighDb / (kMiniHighDb - kMiniLowDb)) *
                              plot.height();
    painter.drawLine(QPointF{plot.left(), zero_y},
                     QPointF{plot.right(), zero_y});

    QPainterPath path;
    constexpr int kPoints = 96;
    for (int point = 0; point < kPoints; ++point) {
      const double fraction = static_cast<double>(point) / (kPoints - 1);
      const double hz = EditorState::kLowHz *
                        std::pow(EditorState::kHighHz / EditorState::kLowHz,
                                 fraction);
      const auto section = state_->sectionBiquad(index);
      const std::span<const trench::core::Biquad> one{&section, 1};
      const double raw_db = trench::core::cascade_response_db(
          one, hz, EditorState::kDatumHz);
      const double db = std::clamp(std::isfinite(raw_db) ? raw_db : kMiniLowDb,
                                   kMiniLowDb, kMiniHighDb);
      const QPointF position{
          plot.left() + fraction * plot.width(),
          plot.top() + (kMiniHighDb - db) /
                           (kMiniHighDb - kMiniLowDb) * plot.height()};
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
    if (toggleRect(index).contains(event->position())) {
      state_->toggleSection(index);
    } else {
      state_->selectSection(index);
    }
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
    const std::size_t wanted =
        static_cast<std::size_t>(event->key() - Qt::Key_1);
    state_->selectSection(wanted);
    return;
  }
  if (event->key() == Qt::Key_Space) {
    state_->toggleSection(selected);
    return;
  }
  QWidget::keyPressEvent(event);
}
