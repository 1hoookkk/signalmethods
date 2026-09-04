#include "cascade_plot.hpp"

#include "trench/core/native_body.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

namespace {

constexpr double kLowHz = 20.0;
constexpr double kHighHz = 20'000.0;
constexpr double kNyquistHz = 22'050.0;

constexpr QColor kPanel{255, 255, 255};
constexpr QColor kPanelEdge{200, 200, 200};
constexpr QColor kGrid{230, 230, 230};
constexpr QColor kText{64, 64, 64};
constexpr QColor kResponse{0, 0, 0};
constexpr QColor kAddressed{196, 103, 79};
constexpr QColor kGhost{160, 160, 160};

constexpr double kTopMargin = 14.0;

constexpr double kLowDb = -30.0;
constexpr double kHighDb = 30.0;
constexpr int kStepDb = 10;

constexpr double kGlideTick = 1.5;
constexpr double kGlideDot = 2.5;

constexpr double kHandleRadius = 9.0;
constexpr double kHandleGrab = 13.0;
constexpr double kHandlePen = 2.0;
constexpr double kRingPixelsPerWord = 3.0;
constexpr int kShiftRingSteps = 4;

constexpr std::size_t kSections = trench::core::native::kSections;

double finiteDb(double value) {
  if (!std::isfinite(value)) return value < 0.0 ? -400.0 : 400.0;
  return std::clamp(value, -400.0, 400.0);
}

double cascadeDb(const trench::core::Cascade& cascade, double hz, double sample_rate_hz) {
  const std::span<const trench::core::Biquad> six{cascade.data(), kSections};
  return finiteDb(trench::core::cascade_response_db(six, hz, sample_rate_hz));
}

}

CascadePlot::CascadePlot(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(160);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(false);
  base_hz_ = trench::core::logarithmic_frequency_grid(kLowHz, kHighHz, 640);
  grid_hz_ = base_hz_;
}

void CascadePlot::setView(const View& view) {
  enabled_ = view.enabled;
  peaked_ = view.peaked;
  selected_ = view.selected;
  pad_at_corner_ = view.pad_at_corner;
  grid_hz_ = base_hz_;
  grid_hz_.insert(grid_hz_.end(), view.seed_hz.begin(), view.seed_hz.end());
  std::sort(grid_hz_.begin(), grid_hz_.end());
  grid_hz_.erase(std::unique(grid_hz_.begin(), grid_hz_.end(),
                             [](double left, double right) {
                               return std::abs(right - left) < 0.01;
                             }),
                 grid_hz_.end());
  pad_cascade_ = view.pad;
  sample_rate_hz_ = view.sample_rate_hz;
  pad_db_.clear();
  pad_db_.reserve(grid_hz_.size());
  for (const double hz : grid_hz_) {
    pad_db_.push_back(cascadeDb(pad_cascade_, hz, sample_rate_hz_));
  }
  glide_lo_ = view.lo_pole_hz;
  glide_hi_ = view.hi_pole_hz;
  glide_now_ = view.now_pole_hz;
  for (std::size_t index = 0; index < kSections; ++index) {
    edit_pole_hz_[index] = view.pole_hz[index];
    const double blended = view.now_pole_hz[index];
    handle_hz_[index] = blended > 0.0 && std::isfinite(blended) ? blended
                                                                : view.pole_hz[index];
    handle_db_[index] = enabled_[index] && handle_hz_[index] > 0.0
                            ? cascadeDb(pad_cascade_, handle_hz_[index], sample_rate_hz_)
                            : 0.0;
  }
  update();
}

double CascadePlot::curveDbAt(double hz) const {
  return cascadeDb(pad_cascade_, hz, sample_rate_hz_);
}

double CascadePlot::xForFrequency(double frequency_hz) const {
  return xForFrequency(frequency_hz, plotRect());
}

const std::vector<double>& CascadePlot::gridHz() const { return grid_hz_; }

bool CascadePlot::glided(std::size_t slot) const {
  if (slot >= kSections) return false;
  return glide_lo_[slot] > 0.0 && std::isfinite(glide_lo_[slot]) &&
         glide_hi_[slot] > 0.0 && std::isfinite(glide_hi_[slot]);
}

int CascadePlot::glideCount() const {
  int count = 0;
  for (std::size_t slot = 0; slot < kSections; ++slot) {
    if (glided(slot)) ++count;
  }
  return count;
}

std::optional<double> CascadePlot::glideNowHz(std::size_t slot) const {
  if (!glided(slot)) return std::nullopt;
  if (!(glide_now_[slot] > 0.0) || !std::isfinite(glide_now_[slot])) return std::nullopt;
  return glide_now_[slot];
}

void CascadePlot::setReference(std::vector<double> db_on_grid) {
  reference_db_ = std::move(db_on_grid);
  update();
}

void CascadePlot::clearReference() {
  reference_db_.clear();
  update();
}

bool CascadePlot::hasReference() const { return !reference_db_.empty(); }

QRectF CascadePlot::plotRect() const {
  return QRectF(rect()).adjusted(62.0, kTopMargin, -22.0, -38.0);
}

double CascadePlot::xForFrequency(double frequency_hz, const QRectF& plot) const {
  const double fraction = std::log(frequency_hz / kLowHz) / std::log(kHighHz / kLowHz);
  return plot.left() + std::clamp(fraction, 0.0, 1.0) * plot.width();
}

double CascadePlot::yForDb(double db, const QRectF& plot) const {
  const double fraction = (db - kLowDb) / (kHighDb - kLowDb);
  return plot.bottom() - fraction * plot.height();
}

double CascadePlot::frequencyForX(double x, const QRectF& plot) const {
  if (plot.width() <= 0.0) return kLowHz;
  const double fraction = std::clamp((x - plot.left()) / plot.width(), 0.0, 1.0);
  return kLowHz * std::pow(kHighHz / kLowHz, fraction);
}

double CascadePlot::dbForY(double y, const QRectF& plot) const {
  if (plot.height() <= 0.0) return 0.0;
  const double fraction = (plot.bottom() - y) / plot.height();
  return kLowDb + fraction * (kHighDb - kLowDb);
}

std::optional<QPointF> CascadePlot::handleCentre(std::size_t section) const {
  if (section >= kSections || !enabled_[section]) return std::nullopt;
  if (!(handle_hz_[section] > 0.0) || !std::isfinite(handle_hz_[section])) return std::nullopt;
  const QRectF plot = plotRect();
  return QPointF{xForFrequency(handle_hz_[section], plot),
                 yForDb(std::clamp(handle_db_[section], kLowDb, kHighDb), plot)};
}

std::size_t CascadePlot::handleAt(const QPointF& point) const {
  std::size_t found = kSections;
  double best = kHandleGrab;
  for (std::size_t index = 0; index < kSections; ++index) {
    const auto centre = handleCentre(index);
    if (!centre) continue;
    const double distance = std::hypot(centre->x() - point.x(), centre->y() - point.y());
    if (distance <= best) {
      best = distance;
      found = index;
    }
  }
  return found;
}

void CascadePlot::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const std::size_t found = handleAt(event->position());
  if (found >= kSections) return;
  drag_ = found;
  press_ = event->position();
  press_hz_ = frequencyForX(press_.x(), plotRect());
  press_pole_hz_ = edit_pole_hz_[found];
  if (onSelect) onSelect(found);
  if (onDragBegin) onDragBegin(found);
  event->accept();
}

void CascadePlot::mouseMoveEvent(QMouseEvent* event) {
  if (drag_ >= kSections) return;
  const QRectF plot = plotRect();
  const QPointF at = event->position();
  const double dx = at.x() - press_.x();
  const double dy = at.y() - press_.y();
  if (std::abs(dx) >= 1.0 && onNote) {
    const double under = frequencyForX(at.x(), plot);
    const bool ratioed = press_hz_ > 0.0 && press_pole_hz_ > 0.0;
    onNote(drag_, ratioed ? press_pole_hz_ * (under / press_hz_) : under);
  }
  if (std::abs(dy) >= 1.0) {
    if (peaked_[drag_]) {
      if (onHeight) onHeight(drag_, dbForY(at.y(), plot));
    } else if (onRingSteps) {
      onRingSteps(drag_, static_cast<int>(std::lround(-dy / kRingPixelsPerWord)));
    }
  }
  event->accept();
}

void CascadePlot::mouseReleaseEvent(QMouseEvent* event) {
  if (drag_ >= kSections) return;
  drag_ = kSections;
  if (onDragEnd) onDragEnd();
  event->accept();
}

void CascadePlot::mouseDoubleClickEvent(QMouseEvent* event) {
  if (handleAt(event->position()) < kSections) return;
  const QRectF plot = plotRect();
  if (!plot.contains(event->position())) return;
  if (onCreate) onCreate(frequencyForX(event->position().x(), plot), dbForY(event->position().y(), plot));
  event->accept();
}

void CascadePlot::wheelEvent(QWheelEvent* event) {
  const std::size_t found = handleAt(event->position());
  if (found >= kSections) return;
  const int notches = event->angleDelta().y() / 120;
  if (notches == 0) return;
  const int steps =
      (event->modifiers() & Qt::ShiftModifier) != 0 ? notches * kShiftRingSteps : notches;
  if (onWheelRing) onWheelRing(found, steps);
  event->accept();
}

void CascadePlot::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), palette().window().color());
  const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kPanel);
  painter.drawRect(card);
  painter.setPen(QPen(kPanelEdge, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(card);

  const QRectF plot = plotRect();

  painter.setPen(QPen(kGrid, 1.0));

  constexpr std::array<double, 11> frequency_lines{
      20.0, 40.0, 80.0, 160.0, 320.0, 640.0, 1'280.0,
      2'560.0, 5'120.0, 10'240.0, 20'480.0};
  for (const double hz : frequency_lines) {
    const double x = xForFrequency(hz, plot);
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
    QString label = hz >= 1000.0
                        ? QStringLiteral("%1k").arg(hz / 1000.0, 0, 'g', 2)
                        : QString::number(static_cast<int>(hz));
    if (hz == kNyquistHz) label = QStringLiteral("NYQ");
    painter.setPen(kText);
    painter.drawText(QRectF{x - 28.0, plot.bottom() + 8.0, 56.0, 18.0},
                     Qt::AlignHCenter | Qt::AlignTop, label);
    painter.setPen(QPen(kGrid, 1.0));
  }
  const int first_db = static_cast<int>(std::ceil(kLowDb / kStepDb) * kStepDb);
  for (int db = first_db; db <= static_cast<int>(kHighDb); db += kStepDb) {
    const double y = yForDb(static_cast<double>(db), plot);
    const bool unity = db == 0;
    painter.setPen(QPen(unity ? kResponse : kGrid, 1.0));
    painter.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
    painter.setPen(kText);
    painter.drawText(QRectF{8.0, y - 9.0, 46.0, 18.0},
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("%1 dB").arg(db));
    painter.setPen(QPen(kGrid, 1.0));
  }

  const auto draw_curve = [&](const std::vector<double>& hz,
                              const std::vector<double>& db,
                              const QPen& pen) {
    if (hz.size() != db.size() || hz.size() < 2) return;
    QPainterPath path;
    bool started = false;
    for (std::size_t index = 0; index < hz.size(); ++index) {
      if (!(hz[index] >= kLowHz && hz[index] <= kHighHz) ||
          !std::isfinite(db[index])) {
        continue;
      }
      const QPointF point{xForFrequency(hz[index], plot), yForDb(db[index], plot)};
      if (!started) {
        path.moveTo(point);
        started = true;
      } else {
        path.lineTo(point);
      }
    }
    painter.save();
    painter.setClipRect(plot);
    painter.setPen(pen);
    painter.drawPath(path);
    painter.restore();
  };

  painter.setBrush(Qt::NoBrush);

  if (reference_db_.size() == grid_hz_.size() && !grid_hz_.empty()) {
    QPen reference_pen(kGhost, 1.0);
    reference_pen.setCosmetic(true);
    reference_pen.setStyle(Qt::DashLine);
    draw_curve(grid_hz_, reference_db_, reference_pen);
  }

  QPen response_pen(kResponse, 2.5);
  response_pen.setCosmetic(true);
  response_pen.setCapStyle(Qt::FlatCap);
  draw_curve(grid_hz_, pad_db_, response_pen);

  if (selected_ < kSections && glided(selected_)) {
    painter.save();
    const double y = static_cast<double>(rect().bottom()) - 7.0;
    const double lo = xForFrequency(glide_lo_[selected_], plot);
    const double hi = xForFrequency(glide_hi_[selected_], plot);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kGhost, 1.0));
    painter.drawLine(QPointF{lo, y}, QPointF{hi, y});
    painter.drawLine(QPointF{lo, y - kGlideTick}, QPointF{lo, y + kGlideTick});
    painter.drawLine(QPointF{hi, y - kGlideTick}, QPointF{hi, y + kGlideTick});
    if (const auto now = glideNowHz(selected_)) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(kAddressed);
      painter.drawEllipse(QPointF{xForFrequency(*now, plot), y}, kGlideDot, kGlideDot);
    }
    painter.restore();
  }

  painter.save();
  painter.setClipRect(plot.adjusted(-kHandleRadius, -kHandleRadius, kHandleRadius, kHandleRadius));
  painter.setBrush(kPanel);
  for (std::size_t index = 0; index < kSections; ++index) {
    const auto centre = handleCentre(index);
    if (!centre) continue;
    painter.setPen(QPen(index == selected_ ? kAddressed : kResponse, kHandlePen));
    painter.drawEllipse(*centre, kHandleRadius, kHandleRadius);
  }
  painter.restore();
}
