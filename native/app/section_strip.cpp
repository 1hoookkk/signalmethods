#include "section_strip.hpp"

#include "trench/core/packed_body.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>
#include <span>

namespace {

constexpr QColor kChassis{237, 235, 230};
constexpr QColor kCell{30, 34, 38};
constexpr QColor kCellOff{26, 29, 33};
constexpr QColor kHairline{50, 55, 59};
constexpr QColor kInk{210, 207, 198};
constexpr QColor kQuiet{210, 207, 198, 105};
constexpr QColor kAccent{196, 103, 79};

constexpr double kCellGap = 12.0;
constexpr double kMiniLowDb = -120.0;
constexpr double kMiniHighDb = 24.0;
constexpr int kCurvePoints = 96;

}  // namespace

SectionStrip::SectionStrip(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setFixedHeight(64);
  setFocusPolicy(Qt::StrongFocus);
  setAccessibleName(QStringLiteral("Six addressable filter sections"));
  connect(state_, &EditorState::changed, this, [this] {
    curves_dirty_ = true;
    update();
  });
  connect(state_, &EditorState::selectionChanged, this, [this] {
    curves_dirty_ = true;
    update();
  });
}

QRectF SectionStrip::toggleRect(std::size_t index) const {
  const QRectF bounds = cell(index);
  return {bounds.right() - 42.0, bounds.top() + 5.0, 34.0, 16.0};
}

QRectF SectionStrip::cell(std::size_t index) const {
  const QRectF bounds = QRectF(rect());
  const double width =
      (bounds.width() - kCellGap * (trench::core::native::kSections - 1)) /
      trench::core::native::kSections;
  return {bounds.left() + static_cast<double>(index) * (width + kCellGap),
          bounds.top(), width, bounds.height()};
}

void SectionStrip::rebuildCurves() {
  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    QPainterPath path;
    if (state_->sectionEnabled(index)) {
      const QRectF plot = cell(index).adjusted(8.0, 24.0, -8.0, -10.0);
      const auto section = state_->sectionBiquad(index);
      const std::span<const trench::core::Biquad> one{&section, 1};
      for (int point = 0; point < kCurvePoints; ++point) {
        const double fraction =
            static_cast<double>(point) / (kCurvePoints - 1);
        const double hz = EditorState::kLowHz *
                          std::pow(EditorState::kHighHz / EditorState::kLowHz,
                                   fraction);
        const double raw_db = trench::core::cascade_response_db(
            one, hz, EditorState::kDatumHz);
        const double db =
            std::clamp(std::isfinite(raw_db) ? raw_db : kMiniLowDb,
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
    }
    curves_[index] = std::move(path);
  }
  curves_dirty_ = false;
}

void SectionStrip::resizeEvent(QResizeEvent* event) {
  curves_dirty_ = true;
  QWidget::resizeEvent(event);
}

void SectionStrip::paintEvent(QPaintEvent*) {
  if (curves_dirty_) rebuildCurves();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kChassis);
  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    const QRectF bounds = cell(index);
    const bool enabled = state_->sectionEnabled(index);
    const bool selected = index == state_->selectedSection();
    const QColor color = selected ? kAccent : (enabled ? kInk : kQuiet);
    const QRectF card = bounds.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(enabled ? kCell : kCellOff);
    painter.drawRoundedRect(card, 4.0, 4.0);
    painter.setPen(QPen(selected ? kAccent : kHairline, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(card, 4.0, 4.0);

    QFont number_font = painter.font();
    number_font.setWeight(QFont::DemiBold);
    number_font.setPixelSize(11);
    painter.setFont(number_font);
    painter.setPen(color);
    painter.drawText(bounds.adjusted(7.0, 5.0, -7.0, 0.0),
                     Qt::AlignLeft | Qt::AlignTop,
                     QString::number(index + 1));

    // A LAMP, NOT A WORD (Tyson 2026-08-28 "fewer labels"): the same hit zone
    // now reads as lit, live, or dark.
    const QRectF toggle = toggleRect(index);
    const QPointF lamp = toggle.center();
    painter.setPen(QPen(color, 1.0));
    painter.setBrush(enabled ? QBrush(selected ? kAccent : kInk)
                             : QBrush(Qt::NoBrush));
    painter.drawEllipse(lamp, 3.0, 3.0);


    // An OFF slot claims nothing (Tyson 2026-08-28 "make it transparent to
    // whats happening"): no curve until the stage exists in the cascade.
    if (!enabled) continue;
    const QRectF plot = bounds.adjusted(8.0, 24.0, -8.0, -10.0);
    painter.setPen(QPen(kHairline, 1.0));
    const double zero_y = plot.top() +
                          (kMiniHighDb / (kMiniHighDb - kMiniLowDb)) *
                              plot.height();
    painter.drawLine(QPointF{plot.left(), zero_y},
                     QPointF{plot.right(), zero_y});

    QPen curve_pen(color, 1.0);
    curve_pen.setCosmetic(true);
    curve_pen.setCapStyle(Qt::FlatCap);
    painter.setPen(curve_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(curves_[index]);
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
  QWidget::keyPressEvent(event);
}
