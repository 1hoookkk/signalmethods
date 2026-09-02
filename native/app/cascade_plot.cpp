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
constexpr QColor kReference{128, 128, 128};
constexpr QColor kAddressed{196, 103, 79};
constexpr QColor kResidual{70, 110, 170};
constexpr QColor kGhost{160, 160, 160};

constexpr double kTopMargin = 14.0;
constexpr double kStripHeight = 84.0;
constexpr double kStripGap = 6.0;
constexpr double kStripDb = 12.0;

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
  double level = 0.0;
  std::size_t counted = 0;
  for (std::size_t index = 0; index < grid_hz_.size(); ++index) {
    if (grid_hz_[index] < 100.0 || grid_hz_[index] > 10'000.0) continue;
    level += response_db_[index];
    ++counted;
  }
  level_db_ = counted > 0 ? level / static_cast<double>(counted) : 0.0;

  without_row_db_.clear();
  if (selected_section < trench::core::native::kSections && enabled[selected_section]) {
    std::vector<trench::core::Biquad> others;
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      if (enabled[index] && index != selected_section) others.push_back(cascade[index]);
    }
    without_row_db_.reserve(grid_hz_.size());
    for (const double hz : grid_hz_) {
      without_row_db_.push_back(finiteDb(trench::core::cascade_response_db(
          std::span<const trench::core::Biquad>{others.data(), others.size()}, hz,
          sample_rate_hz)));
    }
  }

  buildResidual();
  update();
}

void CascadePlot::buildResidual() {
  residual_db_.clear();
  if (reference_hz_.empty() || reference_hz_.size() != reference_db_.size() ||
      grid_hz_.size() < 2 || grid_hz_.size() != response_db_.size()) {
    return;
  }
  residual_db_.reserve(reference_hz_.size());
  std::size_t cursor = 0;
  for (std::size_t index = 0; index < reference_hz_.size(); ++index) {
    const double hz = reference_hz_[index];
    while (cursor + 2 < grid_hz_.size() && grid_hz_[cursor + 1] < hz) ++cursor;
    const double left = grid_hz_[cursor];
    const double right = grid_hz_[cursor + 1];
    double fraction = 0.0;
    if (left > 0.0 && right > left && hz > 0.0) {
      fraction = std::log(hz / left) / std::log(right / left);
    }
    fraction = std::clamp(fraction, 0.0, 1.0);
    const double response =
        std::lerp(response_db_[cursor], response_db_[cursor + 1], fraction);
    residual_db_.push_back(response -
                           (reference_db_[index] + level_db_ - reference_mean_db_));
  }
}

double CascadePlot::maxResidualDb() const {
  double worst = 0.0;
  for (std::size_t index = 0; index < residual_db_.size(); ++index) {
    if (reference_hz_[index] < 40.0 || reference_hz_[index] > 16'000.0) continue;
    worst = std::max(worst, std::abs(residual_db_[index]));
  }
  return worst;
}

void CascadePlot::setReference(std::vector<double> frequency_hz,
                               std::vector<double> magnitude_db) {
  reference_hz_ = std::move(frequency_hz);
  reference_db_ = std::move(magnitude_db);
  for (double& db : reference_db_) db = finiteDb(db);
  double mean = 0.0;
  std::size_t counted = 0;
  for (std::size_t index = 0; index < reference_hz_.size() && index < reference_db_.size(); ++index) {
    if (reference_hz_[index] < 100.0 || reference_hz_[index] > 10'000.0) continue;
    mean += reference_db_[index];
    ++counted;
  }
  reference_mean_db_ = counted > 0 ? mean / static_cast<double>(counted) : 0.0;
  buildResidual();
  update();
}

void CascadePlot::setFormantMarks(std::vector<double> frequency_hz) {
  formant_hz_ = std::move(frequency_hz);
  update();
}

void CascadePlot::clearFormantMarks() {
  formant_hz_.clear();
  update();
}

const std::vector<double>& CascadePlot::gridHz() const { return grid_hz_; }

void CascadePlot::clearReference() {
  reference_hz_.clear();
  reference_db_.clear();
  residual_db_.clear();
  update();
}

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

  const QRectF plot = QRectF(rect()).adjusted(62.0, kTopMargin, -22.0,
                                              -(38.0 + kStripGap + kStripHeight));
  const QRectF strip{plot.left(), rect().bottom() - kStripHeight - 4.0, plot.width(),
                     kStripHeight};

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

  for (const double hz : formant_hz_) {
    if (hz < kLowHz || hz > kHighHz) continue;
    const double x = xForFrequency(hz, plot);
    painter.setPen(QPen(kReference, 1.0));
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.top() + 9.0});
    painter.setBrush(kReference);
    painter.drawEllipse(QPointF{x, plot.top() + 11.5}, 2.2, 2.2);
    painter.setBrush(Qt::NoBrush);
  }

  QPen reference_pen(kReference, 1.5, Qt::DashLine);
  reference_pen.setCosmetic(true);
  reference_pen.setCapStyle(Qt::FlatCap);
  std::vector<double> aligned = reference_db_;
  for (double& db : aligned) db += level_db_ - reference_mean_db_;
  draw_curve(reference_hz_, aligned, reference_pen);

  if (without_row_db_.size() == grid_hz_.size() && !grid_hz_.empty()) {
    QPainterPath band;
    bool started = false;
    for (std::size_t index = 0; index < grid_hz_.size(); ++index) {
      if (!(grid_hz_[index] >= kLowHz && grid_hz_[index] <= kHighHz)) continue;
      const QPointF point{xForFrequency(grid_hz_[index], plot),
                          yForDb(response_db_[index], plot, low_db, high_db)};
      if (!started) {
        band.moveTo(point);
        started = true;
      } else {
        band.lineTo(point);
      }
    }
    for (std::size_t back = grid_hz_.size(); back-- > 0;) {
      if (!(grid_hz_[back] >= kLowHz && grid_hz_[back] <= kHighHz)) continue;
      band.lineTo(QPointF{xForFrequency(grid_hz_[back], plot),
                          yForDb(without_row_db_[back], plot, low_db, high_db)});
    }
    band.closeSubpath();
    painter.save();
    painter.setClipRect(plot);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(kAddressed.red(), kAddressed.green(), kAddressed.blue(), 48));
    painter.drawPath(band);
    painter.restore();
    QPen edge_pen(kAddressed, 1.0);
    edge_pen.setCosmetic(true);
    draw_curve(grid_hz_, without_row_db_, edge_pen);
  }

  QPen response_pen(kResponse, 2.5);
  response_pen.setCosmetic(true);
  response_pen.setCapStyle(Qt::FlatCap);
  draw_curve(grid_hz_, response_db_, response_pen);

  painter.setPen(QPen(kPanelEdge, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(strip);
  painter.setPen(QPen(kGrid, 1.0));
  for (const double hz : frequency_lines) {
    const double x = xForFrequency(hz, plot);
    painter.drawLine(QPointF{x, strip.top()}, QPointF{x, strip.bottom()});
  }
  const auto strip_y = [&](double db) {
    return strip.bottom() - (db + kStripDb) / (2.0 * kStripDb) * strip.height();
  };
  for (const double db : {-kStripDb, 0.0, kStripDb}) {
    const double y = strip_y(db);
    painter.setPen(QPen(db == 0.0 ? kResponse : kGrid, 1.0));
    painter.drawLine(QPointF{strip.left(), y}, QPointF{strip.right(), y});
    painter.setPen(kText);
    painter.drawText(QRectF{8.0, y - 9.0, 46.0, 18.0}, Qt::AlignRight | Qt::AlignVCenter,
                     db == 0.0 ? QStringLiteral("0 dB") : QString::asprintf("%+.0f", db));
  }
  painter.setPen(kText);
  painter.drawText(QRectF{strip.left() + 6.0, strip.top() + 2.0, 120.0, 16.0},
                   Qt::AlignLeft | Qt::AlignTop, QStringLiteral("RESIDUAL"));
  if (!residual_db_.empty()) {
    QPainterPath path;
    bool started = false;
    for (std::size_t index = 0; index < reference_hz_.size(); ++index) {
      const double hz = reference_hz_[index];
      if (!(hz >= kLowHz && hz <= kHighHz) || !std::isfinite(residual_db_[index])) continue;
      const QPointF point{xForFrequency(hz, plot),
                          std::clamp(strip_y(residual_db_[index]), strip.top(), strip.bottom())};
      if (!started) {
        path.moveTo(point);
        started = true;
      } else {
        path.lineTo(point);
      }
    }
    painter.save();
    painter.setClipRect(strip);
    QPen residual_pen(kResidual, 1.5);
    residual_pen.setCosmetic(true);
    painter.setPen(residual_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.restore();
  }
}
