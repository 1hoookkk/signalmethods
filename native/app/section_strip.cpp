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

constexpr QColor kCell{255, 255, 255};
constexpr QColor kCellOff{244, 244, 244};
constexpr QColor kPanelEdge{200, 200, 200};
constexpr QColor kRule{176, 176, 176};
constexpr QColor kInk{0, 0, 0};
constexpr QColor kLampOff{128, 128, 128};
constexpr QColor kText{64, 64, 64};
constexpr QColor kAccent{196, 103, 79};

constexpr double kCellGap = 12.0;
constexpr double kCellMax = 120.0;
constexpr double kMiniLowDb = -120.0;
constexpr double kMiniHighDb = 24.0;
constexpr int kCurvePoints = 96;

}  // namespace

SectionStrip::SectionStrip(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  policy.setHeightForWidth(true);
  setSizePolicy(policy);
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

bool SectionStrip::hasHeightForWidth() const { return true; }

int SectionStrip::heightForWidth(int width) const {
  const double size = std::min(
      kCellMax, (static_cast<double>(width) - kCellGap * (trench::core::native::kSections - 1)) /
                    trench::core::native::kSections);
  return static_cast<int>(std::ceil(std::max(size, 1.0)));
}

QSize SectionStrip::sizeHint() const {
  const int width = static_cast<int>(kCellMax * trench::core::native::kSections +
                                     kCellGap * (trench::core::native::kSections - 1));
  return {width, heightForWidth(width)};
}

QSize SectionStrip::minimumSizeHint() const {
  const int width = static_cast<int>(48.0 * trench::core::native::kSections +
                                     kCellGap * (trench::core::native::kSections - 1));
  return {width, heightForWidth(width)};
}

double SectionStrip::cellSize() const {
  return std::min(kCellMax, (static_cast<double>(width()) -
                             kCellGap * (trench::core::native::kSections - 1)) /
                                trench::core::native::kSections);
}

QRectF SectionStrip::cell(std::size_t index) const {
  const double size = cellSize();
  const double row = size * trench::core::native::kSections +
                     kCellGap * (trench::core::native::kSections - 1);
  const double left = 0.5 * (static_cast<double>(width()) - row);
  return {left + static_cast<double>(index) * (size + kCellGap), 0.0, size, size};
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
  painter.fillRect(rect(), palette().window().color());
  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    const QRectF bounds = cell(index);
    const bool enabled = state_->sectionEnabled(index);
    const bool selected = index == state_->selectedSection();
    const QColor color = selected ? kAccent : kInk;
    const QRectF card = bounds.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(enabled ? kCell : kCellOff);
    painter.drawRect(card);
    painter.setPen(QPen(selected ? kAccent : kPanelEdge, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(card);

    painter.setPen(kText);
    painter.drawText(bounds.adjusted(7.0, 5.0, -7.0, 0.0),
                     Qt::AlignLeft | Qt::AlignTop,
                     QString::number(index + 1));

    // A LAMP, NOT A WORD (Tyson 2026-08-28 "fewer labels"): the same hit zone
    // now reads as lit, live, or dark.
    const QRectF toggle = toggleRect(index);
    const QPointF lamp = toggle.center();
    painter.setPen(QPen(enabled ? kInk : kLampOff, 1.0));
    painter.setBrush(enabled ? QBrush(kInk) : QBrush(Qt::NoBrush));
    painter.drawEllipse(lamp, 3.0, 3.0);


    // An OFF slot claims nothing (Tyson 2026-08-28 "make it transparent to
    // whats happening"): no curve until the stage exists in the cascade.
    if (!enabled) continue;
    const QRectF plot = bounds.adjusted(8.0, 24.0, -8.0, -10.0);
    painter.setPen(QPen(kRule, 1.0));
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
