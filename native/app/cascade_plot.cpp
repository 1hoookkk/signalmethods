#include "cascade_plot.hpp"

#include "trench/core/native_body.hpp"

#include <QFontMetrics>
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

QColor kBackground{12, 15, 17};
QColor kPanel{20, 23, 26};
QColor kGrid{45, 52, 56};
QColor kText{151, 163, 166};
QColor kResponse{185, 236, 224};
QColor kReference{184, 134, 46};
constexpr std::array<QColor, trench::core::native::kSections> kSectionColors{
    QColor{66, 224, 207}, QColor{231, 158, 76}, QColor{226, 210, 90},
    QColor{224, 99, 151}, QColor{92, 170, 238}, QColor{155, 213, 96}};

double finiteDb(double value) {
  if (!std::isfinite(value)) return value < 0.0 ? -120.0 : 120.0;
  return std::clamp(value, -120.0, 120.0);
}

}  // namespace

CascadePlot::CascadePlot(QWidget* parent) : QWidget(parent) {
  setMinimumHeight(320);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  grid_hz_ = trench::core::logarithmic_frequency_grid(kLowHz, kHighHz, 640);
}

void CascadePlot::setCascade(
    const trench::core::Cascade& cascade,
    const std::array<trench::core::Biquad,
                     trench::core::native::kSections>& sections,
    const std::array<bool, trench::core::native::kSections>& enabled,
    std::size_t selected_section, double selected_frequency_hz,
    double sample_rate_hz) {
  enabled_ = enabled;
  selected_section_ = selected_section;
  selected_frequency_hz_ = selected_frequency_hz;
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

void CascadePlot::setReference(QString name, std::vector<double> frequency_hz,
                               std::vector<double> magnitude_db) {
  reference_name_ = std::move(name);
  reference_hz_ = std::move(frequency_hz);
  reference_db_ = std::move(magnitude_db);
  for (double& db : reference_db_) db = finiteDb(db);
  update();
}

void CascadePlot::clearReference() {
  reference_name_.clear();
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
  painter.fillRect(rect(), kBackground);

  const QRectF plot = QRectF(rect()).adjusted(62.0, 42.0, -22.0, -38.0);
  painter.fillRect(plot, kPanel);

  double low_db = -24.0;
  double high_db = 24.0;
  const auto include = [&](const std::vector<double>& values) {
    for (const double value : values) {
      if (!std::isfinite(value)) continue;
      low_db = std::min(low_db, value);
      high_db = std::max(high_db, value);
    }
  };
  include(response_db_);
  include(reference_db_);
  for (const auto& section : section_db_) include(section);
  const double centre = 0.5 * (low_db + high_db);
  const double half_span = std::max(24.0, 0.5 * (high_db - low_db) + 4.0);
  low_db = std::floor((centre - half_span) / 6.0) * 6.0;
  high_db = std::ceil((centre + half_span) / 6.0) * 6.0;

  painter.setPen(QPen(kGrid, 1.0));
  constexpr std::array<double, 10> frequency_lines{
      20.0, 50.0, 100.0, 200.0, 500.0, 1'000.0, 2'000.0,
      5'000.0, 10'000.0, 20'000.0};
  for (const double hz : frequency_lines) {
    const double x = xForFrequency(hz, plot);
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
    const QString label = hz >= 1000.0
                              ? QStringLiteral("%1k").arg(hz / 1000.0, 0, 'g', 2)
                              : QString::number(static_cast<int>(hz));
    painter.setPen(kText);
    painter.drawText(QRectF{x - 28.0, plot.bottom() + 8.0, 56.0, 18.0},
                     Qt::AlignHCenter | Qt::AlignTop, label);
    painter.setPen(QPen(kGrid, 1.0));
  }
  const int first_db = static_cast<int>(std::ceil(low_db / 12.0) * 12.0);
  for (int db = first_db; db <= static_cast<int>(high_db); db += 12) {
    const double y = yForDb(static_cast<double>(db), plot, low_db, high_db);
    painter.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
    painter.setPen(kText);
    painter.drawText(QRectF{4.0, y - 9.0, 50.0, 18.0},
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

  if (enabled_[selected_section_] && selected_frequency_hz_ >= kLowHz &&
      selected_frequency_hz_ <= kHighHz) {
    QColor guide = kSectionColors[selected_section_];
    guide.setAlpha(115);
    painter.setPen(QPen(guide, 1.0));
    const double x = xForFrequency(selected_frequency_hz_, plot);
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
  }

  QPen reference_pen(kReference, 1.0, Qt::DashLine);
  reference_pen.setCosmetic(true);
  reference_pen.setCapStyle(Qt::FlatCap);
  draw_curve(reference_hz_, reference_db_, reference_pen);
  for (std::size_t section = 0; section < section_db_.size(); ++section) {
    if (!enabled_[section]) continue;
    QColor color = kSectionColors[section];
    color.setAlpha(section == selected_section_ ? 205 : 72);
    QPen section_pen(color, section == selected_section_ ? 1.2 : 1.0);
    section_pen.setCosmetic(true);
    section_pen.setCapStyle(Qt::FlatCap);
    draw_curve(grid_hz_, section_db_[section], section_pen);
  }
  QPen response_pen(kResponse, 1.0);
  response_pen.setCosmetic(true);
  response_pen.setCapStyle(Qt::FlatCap);
  draw_curve(grid_hz_, response_db_, response_pen);

  painter.setPen(QColor{220, 229, 231});
  QFont title_font = painter.font();
  title_font.setBold(true);
  title_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  painter.setFont(title_font);
  painter.drawText(QRectF{plot.left(), 12.0, plot.width(), 22.0},
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("COMPLETE CASCADE"));

  QFont legend_font = painter.font();
  legend_font.setBold(false);
  legend_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
  painter.setFont(legend_font);
  if (!reference_name_.isEmpty()) {
    painter.setPen(kReference);
    painter.drawText(QRectF{plot.right() - 150.0, 12.0, 150.0, 22.0},
                     Qt::AlignRight | Qt::AlignVCenter,
                     reference_name_);
  }
}
