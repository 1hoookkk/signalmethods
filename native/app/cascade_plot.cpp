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

namespace {

constexpr double kLowHz = 20.0;
constexpr double kHighHz = 20'000.0;
constexpr double kNyquistHz = 22'050.0;

constexpr QColor kChassis{237, 235, 230};
constexpr QColor kCard{30, 34, 38};
constexpr QColor kHairline{50, 55, 59};
constexpr QColor kGrid{52, 58, 63, 140};
constexpr QColor kGridUnity{52, 58, 63};
constexpr QColor kText{139, 139, 132};
constexpr QColor kResponse{210, 207, 198};
constexpr QColor kAddressed{196, 103, 79};
constexpr QColor kReference{184, 134, 46};

struct Frame {
  double low_db;
  double high_db;
  int step_db;
};

constexpr Frame kNormalFrame{-30.0, 30.0, 10};
constexpr Frame kTallFrame{-120.0, 96.0, 24};

double finiteDb(double value) {
  if (!std::isfinite(value)) return value < 0.0 ? -120.0 : 120.0;
  return std::clamp(value, -120.0, 120.0);
}

}  // namespace

CascadePlot::CascadePlot(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(160);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
  selected_section_ = selected_section;
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
  for (std::size_t section = 0; section < section_db_.size(); ++section) {
    section_db_[section].clear();
    if (!enabled_[section]) continue;
    section_db_[section].reserve(grid_hz_.size());
    const std::span<const trench::core::Biquad> one{&sections[section], 1};
    for (const double hz : grid_hz_) {
      section_db_[section].push_back(finiteDb(
          trench::core::cascade_response_db(one, hz, sample_rate_hz)));
    }
  }
  update();
}

void CascadePlot::setReference(std::vector<double> frequency_hz,
                               std::vector<double> magnitude_db) {
  reference_hz_ = std::move(frequency_hz);
  reference_db_ = std::move(magnitude_db);
  for (double& db : reference_db_) db = finiteDb(db);
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

void CascadePlot::clearReference() {
  reference_hz_.clear();
  reference_db_.clear();
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
  return plot.bottom() - std::clamp(fraction, 0.0, 1.0) * plot.height();
}

void CascadePlot::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kChassis);
  const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kCard);
  painter.drawRoundedRect(card, 6.0, 6.0);
  painter.setPen(QPen(kHairline, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(card, 6.0, 6.0);

  const QRectF plot = QRectF(rect()).adjusted(62.0, 14.0, -22.0, -38.0);

  const Frame frame = tall_frame_ ? kTallFrame : kNormalFrame;
  const double low_db = frame.low_db;
  const double high_db = frame.high_db;

  QFont scale_font = painter.font();
  scale_font.setPixelSize(9);
  scale_font.setWeight(QFont::Normal);
  painter.setFont(scale_font);

  painter.setPen(QPen(kGrid, 1.0));
  // OCTAVE GRID FROM 20 Hz (US 10,514,883's own display law).
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
  const double step = static_cast<double>(frame.step_db);
  const int first_db = static_cast<int>(std::ceil(low_db / step) * step);
  for (int db = first_db; db <= static_cast<int>(high_db);
       db += frame.step_db) {
    const double y = yForDb(static_cast<double>(db), plot, low_db, high_db);
    const bool unity = db == 0;
    painter.setPen(QPen(unity ? kGridUnity : kGrid, unity ? 1.5 : 1.0));
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
    painter.setPen(pen);
    painter.drawPath(path);
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

  QPen reference_pen(kReference, 1.0, Qt::DashLine);
  reference_pen.setCosmetic(true);
  reference_pen.setCapStyle(Qt::FlatCap);
  draw_curve(reference_hz_, reference_db_, reference_pen);
  // ONE LINE, plus the addressed stage (Tyson 2026-08-28 "dont add more than
  // one line other than when you select a stage"): the complete cascade is
  // the plot; selecting a stage overlays exactly that stage's own curve.
  if (enabled_[selected_section_]) {
    QPen section_pen(kAddressed, 1.0);
    section_pen.setCosmetic(true);
    section_pen.setCapStyle(Qt::FlatCap);
    draw_curve(grid_hz_, section_db_[selected_section_], section_pen);
  }
  QPen response_pen(kResponse, 1.5);
  response_pen.setCosmetic(true);
  response_pen.setCapStyle(Qt::FlatCap);
  draw_curve(grid_hz_, response_db_, response_pen);
}

void CascadePlot::wheelEvent(QWheelEvent* event) {
  if (!event->modifiers().testFlag(Qt::ControlModifier)) {
    QWidget::wheelEvent(event);
    return;
  }
  tall_frame_ = !tall_frame_;
  event->accept();
  update();
}
