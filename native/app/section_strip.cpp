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
  setAccessibleName(QStringLiteral("Six serial signal-so-far sections"));
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
  const std::size_t active = state_->activeSections();

  painter.setPen(QPen(QColor{76, 84, 87}, 1.0));
  const std::size_t visible_links =
      std::min(active, trench::core::native::kSections - 1);
  for (std::size_t index = 0; index < visible_links; ++index) {
    const QRectF before = cell(index);
    const QRectF after = cell(index + 1);
    const double centre_y = before.center().y();
    const double left = before.right() + 3.0;
    const double right = after.left() - 3.0;
    painter.drawLine(QPointF{left, centre_y}, QPointF{right, centre_y});
    painter.drawLine(QPointF{right - 3.0, centre_y - 3.0},
                     QPointF{right, centre_y});
    painter.drawLine(QPointF{right - 3.0, centre_y + 3.0},
                     QPointF{right, centre_y});
  }

  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    const QRectF bounds = cell(index);
    const bool active_section = index < active;
    const bool addable = index == active &&
                         active < trench::core::native::kSections;
    const bool selected = index == state_->selectedSection();
    const QColor color = kSectionColors[index];
    painter.fillRect(bounds, active_section ? QColor{20, 23, 26}
                                            : QColor{15, 18, 20});
    painter.setPen(QPen(selected && active_section
                            ? QColor{185, 236, 224}
                            : QColor{42, 48, 51},
                        1.0));
    painter.drawRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5));

    QFont number_font = painter.font();
    number_font.setBold(true);
    number_font.setPointSizeF(8.5);
    painter.setFont(number_font);
    painter.setPen(active_section ? color : QColor{59, 67, 70});
    painter.drawText(bounds.adjusted(7.0, 5.0, -7.0, 0.0),
                     Qt::AlignLeft | Qt::AlignTop,
                     QString::number(index + 1));

    if (!active_section) {
      if (addable) {
        QFont add_font = painter.font();
        add_font.setBold(false);
        add_font.setPointSizeF(17.0);
        painter.setFont(add_font);
        painter.setPen(QColor{128, 143, 146});
        painter.drawText(bounds, Qt::AlignCenter, QStringLiteral("+"));
      }
      continue;
    }

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
      const std::span<const trench::core::Biquad> prefix{cascade.data(),
                                                         index + 1};
      const double raw_db = trench::core::cascade_response_db(
          prefix, hz, EditorState::kDatumHz);
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
    if (index < state_->activeSections()) {
      state_->selectSection(index);
    } else if (index == state_->activeSections()) {
      state_->activateNextSection();
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
      selected + 1 < state_->activeSections()) {
    state_->selectSection(selected + 1);
    return;
  }
  if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_6) {
    const std::size_t wanted =
        static_cast<std::size_t>(event->key() - Qt::Key_1);
    if (wanted < state_->activeSections()) state_->selectSection(wanted);
    return;
  }
  QWidget::keyPressEvent(event);
}
