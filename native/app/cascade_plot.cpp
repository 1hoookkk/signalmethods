#include "cascade_plot.hpp"

#include "trench/core/native_body.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

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

double finiteDb(double value) {
  if (!std::isfinite(value)) return value < 0.0 ? -400.0 : 400.0;
  return std::clamp(value, -400.0, 400.0);
}

}

CascadePlot::CascadePlot(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(160);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  base_hz_ = trench::core::logarithmic_frequency_grid(kLowHz, kHighHz, 640);
  grid_hz_ = base_hz_;
}

void CascadePlot::setCascade(
    const trench::core::Cascade& cascade,
    const std::array<trench::core::Biquad,
                     trench::core::native::kSections>& sections,
    const std::array<bool, trench::core::native::kSections>& enabled,
    const std::vector<double>& seed_hz, std::size_t selected_section,
    double selected_frequency_hz, double sample_rate_hz) {
  enabled_ = enabled;
  selected_frequency_hz_ = selected_frequency_hz;
  grid_hz_ = base_hz_;
  grid_hz_.insert(grid_hz_.end(), seed_hz.begin(), seed_hz.end());
  std::sort(grid_hz_.begin(), grid_hz_.end());
  grid_hz_.erase(
      std::unique(grid_hz_.begin(), grid_hz_.end(),
                  [](double left, double right) {
                    return std::abs(right - left) < 0.01;
                  }),
      grid_hz_.end());
  response_db_.clear();
  response_db_.reserve(grid_hz_.size());
  const std::span<const trench::core::Biquad> six_sections{
      cascade.data(), trench::core::native::kSections};
  for (const double hz : grid_hz_) {
    response_db_.push_back(finiteDb(trench::core::cascade_response_db(
        six_sections, hz, sample_rate_hz)));
  }
  row_db_.clear();
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    rung_db_[index].clear();
    if (!enabled[index]) continue;
    const std::span<const trench::core::Biquad> rung{&sections[index], 1};
    rung_db_[index].reserve(grid_hz_.size());
    for (const double hz : grid_hz_) {
      rung_db_[index].push_back(
          finiteDb(trench::core::cascade_response_db(rung, hz, sample_rate_hz)));
    }
  }
  if (selected_section < trench::core::native::kSections) row_db_ = rung_db_[selected_section];

  update();
}

const std::vector<double>& CascadePlot::gridHz() const { return grid_hz_; }

double CascadePlot::xForFrequency(double frequency_hz, const QRectF& plot) const {
  const double fraction = std::log(frequency_hz / kLowHz) /
                          std::log(kHighHz / kLowHz);
  return plot.left() + std::clamp(fraction, 0.0, 1.0) * plot.width();
}

double CascadePlot::yForDb(double db, const QRectF& plot, double low_db,
                           double high_db) const {
  const double fraction = (db - low_db) / (high_db - low_db);
  return plot.bottom() - fraction * plot.height();
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

  const QRectF plot = QRectF(rect()).adjusted(62.0, kTopMargin, -22.0, -38.0);

  const double low_db = kLowDb;
  const double high_db = kHighDb;
  const int step_db = kStepDb;

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
  const double step = static_cast<double>(step_db);
  const int first_db = static_cast<int>(std::ceil(low_db / step) * step);
  for (int db = first_db; db <= static_cast<int>(high_db); db += step_db) {
    const double y = yForDb(static_cast<double>(db), plot, low_db, high_db);
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
      const QPointF point{xForFrequency(hz[index], plot),
                          yForDb(db[index], plot, low_db, high_db)};
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

  QPen rung_pen(kGhost, 1.0);
  rung_pen.setCosmetic(true);
  for (const auto& rung : rung_db_) {
    if (rung.size() == grid_hz_.size()) draw_curve(grid_hz_, rung, rung_pen);
  }
  if (row_db_.size() == grid_hz_.size() && !grid_hz_.empty()) {
    QPen row_pen(kAddressed, 2.0);
    row_pen.setCosmetic(true);
    row_pen.setCapStyle(Qt::FlatCap);
    draw_curve(grid_hz_, row_db_, row_pen);
  }

  QPen response_pen(kResponse, 2.5);
  response_pen.setCosmetic(true);
  response_pen.setCapStyle(Qt::FlatCap);
  draw_curve(grid_hz_, response_db_, response_pen);
}
