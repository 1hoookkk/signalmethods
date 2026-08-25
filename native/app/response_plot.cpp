#include "response_plot.hpp"

#include "frequency_axis.hpp"

#include "trench/core/p2k.hpp"
#include "trench/core/section_param.hpp"
#include "trench/core/transpose.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {

const QColor kBackground{17, 20, 22};
const QColor kGrid{55, 63, 67};
const QColor kText{174, 186, 190};
const QColor kTrace{87, 222, 205};

constexpr int kTraceOversample = 4;
constexpr int kTraceMinimumBins = 192;
constexpr double kTraceWidthPx = 1.1;
constexpr float kTraceFlatPx = 0.15F;

constexpr double kTokenRadiusPx = 8.0;
constexpr qint64 kFlashHoldMs = 200;
const QColor kTarget{72, 82, 88};
const QColor kResidual{156, 130, 224};
const QColor kPrimitive{174, 186, 190, 60};
const QColor kExposed{174, 186, 190, 200};

constexpr double kResidualBandPx = 34.0;

constexpr double kPrimitiveWidthPx = 1.0;
constexpr double kExposedWidthPx = 1.4;

constexpr double kAxisLowHz = 100.0;
constexpr double kAxisHighHz = 16'000.0;
constexpr double kAxisMaxSpanDb = 144.0;

double x_for_frequency(double frequency_hz, double sample_rate_hz, const QRectF& plot) {
  return plot.left() + trench::app::frequency_axis::fraction(frequency_hz, sample_rate_hz) *
                           plot.width();
}

double frequency_for_x(double x, double sample_rate_hz, const QRectF& plot) {
  return trench::app::frequency_axis::hz((x - plot.left()) / plot.width(), sample_rate_hz);
}

double y_for_db(double db, double low_db, double high_db, const QRectF& plot) {
  return plot.bottom() - (db - low_db) / (high_db - low_db) * plot.height();
}

double db_for_y(double y, double low_db, double high_db, const QRectF& plot) {
  if (plot.height() <= 0.0) return low_db;
  return low_db + (plot.bottom() - y) / plot.height() * (high_db - low_db);
}

constexpr double kOverflowBandPx = 30.0;
constexpr double kOverflowScaleDb = 18.0;

double y_for_contribution(double db, double low_db, double high_db, const QRectF& plot) {
  const auto top = plot.top() + kOverflowBandPx;
  const auto bottom = plot.bottom() - kOverflowBandPx;
  if (db > high_db) {
    const auto excess = db - high_db;
    return plot.top() + kTokenRadiusPx +
           (kOverflowBandPx - kTokenRadiusPx) / (1.0 + excess / kOverflowScaleDb);
  }
  if (db < low_db) {
    const auto excess = low_db - db;
    return plot.bottom() - kTokenRadiusPx -
           (kOverflowBandPx - kTokenRadiusPx) / (1.0 + excess / kOverflowScaleDb);
  }
  return bottom - (db - low_db) / (high_db - low_db) * (bottom - top);
}

std::vector<double> primitive_db(const trench::core::ConjugatePair& pair, bool pole,
                                 const std::vector<double>& frequencies_hz,
                                 double sample_rate_hz) {
  const auto radius = std::clamp(pair.radius, 0.0, 0.99999);
  const auto theta = 2.0 * std::numbers::pi * pair.hz / sample_rate_hz;
  const auto a1 = -2.0 * radius * std::cos(theta);
  const auto a2 = radius * radius;
  const auto dc = std::max(std::abs(1.0 + a1 + a2), 1e-12);
  const trench::core::Biquad section =
      pole ? trench::core::Biquad{dc, 0.0, 0.0, a1, a2}
           : trench::core::Biquad{1.0 / dc, a1 / dc, a2 / dc, 0.0, 0.0};
  std::vector<double> out;
  out.reserve(frequencies_hz.size());
  for (const auto frequency_hz : frequencies_hz) {
    out.push_back(trench::core::section_response_db(section, frequency_hz, sample_rate_hz));
  }
  return out;
}

bool root_placement(const trench::core::native::Roots& pair, double sample_rate_hz,
                    double low_hz, double high_hz, double& hz, double& radius) {
  if (const auto* conjugate = std::get_if<trench::core::native::Resonant>(&pair)) {
    hz = conjugate->hz;
    radius = std::exp(-std::numbers::pi * conjugate->bw_hz / sample_rate_hz);
    return true;
  }
  if (const auto* real = std::get_if<trench::core::native::RealRoots>(&pair)) {
    const auto root = [sample_rate_hz](double decay_hz) {
      if (!std::isfinite(decay_hz)) return 0.0;
      const double magnitude =
          std::exp(-2.0 * std::numbers::pi * std::abs(decay_hz) / sample_rate_hz);
      return std::signbit(decay_hz) ? -magnitude : magnitude;
    };
    const auto a = root(real->a_hz);
    const auto b = root(real->b_hz);
    if (a == 0.0 && b == 0.0) {
      hz = low_hz;
      radius = 0.0;
      return false;
    }
    const auto centre = (a + b) * 0.5;
    hz = centre >= 0.0 ? low_hz : high_hz;
    radius = std::clamp(std::max(std::abs(a), std::abs(b)), 0.0, 1.0);
    return false;
  }
  hz = low_hz;
  radius = 0.0;
  return false;
}

trench::core::Cascade corner_cascade(const trench::core::native::Body& body,
                                     std::size_t corner, double sample_rate_hz) {
  return trench::core::native::cascade(
      trench::core::native::design(body.corners[corner], sample_rate_hz),
      body.corners[corner].gain_db);
}

}  // namespace

ResponsePlotWidget::ResponsePlotWidget(QWidget* parent) : QWidget(parent) {
  setAutoFillBackground(false);
  setAccessibleName(QStringLiteral("Complete cascade response"));
  setAccessibleDescription(
      QStringLiteral("Read-only response, aligned target, residual, and fit pins"));
  auto policy = QSizePolicy{QSizePolicy::Preferred, QSizePolicy::Preferred};
  policy.setHeightForWidth(true);
  setSizePolicy(policy);
}

QSize ResponsePlotWidget::sizeHint() const { return {720, 260}; }

QSize ResponsePlotWidget::minimumSizeHint() const { return {480, 190}; }

int ResponsePlotWidget::heightForWidth(int width) const {
  return std::clamp(static_cast<int>(std::lround(width / 2.8)), 190, 320);
}

void ResponsePlotWidget::setBody(const trench::core::native::Body* body,
                                 double sample_rate_hz,
                                 std::string source_label) {
  const auto label = QString::fromStdString(source_label);
  if (body_ == body && sample_rate_hz_ == sample_rate_hz && source_label_ == label) return;
  body_ = body;
  sample_rate_hz_ = sample_rate_hz;
  source_label_ = label;
  refresh();
}

void ResponsePlotWidget::setSpace(const trench::core::p2k::PerceptualSpace& space) {
  if (space == space_) return;
  space_ = space;
  refresh();
}

void ResponsePlotWidget::setCorner(std::size_t corner) {
  if (corner == corner_) return;
  corner_ = corner;
  refresh();
}

void ResponsePlotWidget::setView(float morph, float q, double semitones) {
  if (view_morph_ == morph && view_q_ == q && view_semitones_ == semitones) return;
  view_morph_ = morph;
  view_q_ = q;
  view_semitones_ = semitones;
  at_corner_ = (morph == 0.0F || morph == 1.0F) && (q == 0.0F || q == 1.0F);
  refresh();
}

trench::core::Cascade ResponsePlotWidget::viewCascade() const {
  const auto gain_db =
      trench::core::native::blend_gain_db(*body_, view_morph_, view_q_);
  if (at_corner_ && view_semitones_ == 0.0) {
    return corner_cascade(*body_, corner_, sample_rate_hz_);
  }
  auto transposed = trench::core::unity_dc(trench::core::transpose_cascade(
      trench::core::native::cascade(
          trench::core::native::blend(*body_, view_morph_, view_q_, sample_rate_hz_),
          gain_db),
      trench::core::ratio_of_semitones(view_semitones_), sample_rate_hz_));
  const double gain = std::pow(10.0, gain_db / 20.0);
  for (std::size_t wi = 0; wi < 3; ++wi) transposed[0][wi] *= gain;
  return transposed;
}

double ResponsePlotWidget::viewRatio() const {
  return trench::core::ratio_of_semitones(view_semitones_);
}

double ResponsePlotWidget::viewHz(double hz, double radius, bool zero) const {
  return trench::core::transposed_root_hz(hz, radius, viewRatio(), sample_rate_hz_, zero);
}

double ResponsePlotWidget::authorHzForX(double x, const QRectF& plot) const {
  const auto view_hz = frequency_for_x(x, sample_rate_hz_, plot);
  return std::clamp(view_hz / viewRatio(), 20.0, trench::core::p2k::kRootHiHz);
}

void ResponsePlotWidget::setFreedomMask(std::uint32_t mask) {
  if (freedom_mask_ == mask) return;
  freedom_mask_ = mask;
  update();
}

void ResponsePlotWidget::setSelectedSection(std::size_t section, Lane lane) {
  if (selected_section_ == section && selected_lane_ == lane) return;
  selected_section_ = section;
  selected_lane_ = lane;
  rebuildExposed();
  update();
}

void ResponsePlotWidget::setHighlightedSection(std::optional<std::size_t> section) {
  if (highlight_section_ == section) return;
  highlight_section_ = section;
  update();
}

double ResponsePlotWidget::responseDbAtHz(double hz) const {
  if (response_db_.empty() || response_db_.size() != frequencies_hz_.size()) return 0.0;
  const auto upper_it = std::lower_bound(frequencies_hz_.begin(), frequencies_hz_.end(), hz);
  if (upper_it == frequencies_hz_.begin()) return response_db_.front();
  if (upper_it == frequencies_hz_.end()) return response_db_.back();
  const auto upper = static_cast<std::size_t>(upper_it - frequencies_hz_.begin());
  const auto lower = upper - 1;
  const auto span = frequencies_hz_[upper] - frequencies_hz_[lower];
  const auto blend = span > 0.0 ? (hz - frequencies_hz_[lower]) / span : 0.0;
  return response_db_[lower] * (1.0 - blend) + response_db_[upper] * blend;
}

void ResponsePlotWidget::setTarget(const std::vector<double>* target) {
  const auto wanted = target == nullptr ? std::vector<double>{} : *target;
  if (target_db_ == wanted) return;
  target_db_ = wanted;
  rebuildResidual();
  update();
}

void ResponsePlotWidget::setFitRunning(bool running) {
  if (fit_running_ == running) return;
  fit_running_ = running;
  update();
}

void ResponsePlotWidget::flashLane(std::size_t section) {
  flash_section_ = section;
  flash_age_.start();
  QTimer::singleShot(kFlashHoldMs + 10, this, [this] { update(); });
  update();
}

void ResponsePlotWidget::refresh() {
  if (body_ == nullptr) return;
  frequencies_hz_ = trench::core::p2k::make_grid(space_).hz;
  const auto cascade = viewCascade();
  response_db_.clear();
  response_db_.reserve(frequencies_hz_.size());
  for (const auto frequency_hz : frequencies_hz_) {
    response_db_.push_back(
        trench::core::cascade_response_db(cascade, frequency_hz, sample_rate_hz_));
  }
  contributions_ = trench::core::marginal_contributions_db(cascade, frequencies_hz_,
                                                           sample_rate_hz_);
  std::vector<double> cumulative(frequencies_hz_.size(), 0.0);
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    double peak = -1.0e9;
    for (std::size_t index = 0; index < frequencies_hz_.size(); ++index) {
      cumulative[index] += trench::core::section_response_db(
          cascade[section], frequencies_hz_[index], sample_rate_hz_);
      peak = std::max(peak, cumulative[index]);
    }
    running_peak_db_[section] = peak;
  }
  rebuildResidual();
  rebuildExposed();
  ++body_revision_;
  update();
}

void ResponsePlotWidget::rebuildResidual() {
  residual_db_.clear();
  aligned_target_db_.clear();
  residual_span_db_ = 1.0;
  if (target_db_.size() != response_db_.size() || response_db_.empty()) return;
  const auto grid = trench::core::p2k::make_grid(space_);
  if (grid.weight.size() != response_db_.size() || grid.weight_sum <= 0.0) return;
  double mean = 0.0;
  for (std::size_t index = 0; index < response_db_.size(); ++index) {
    mean += grid.weight[index] * (target_db_[index] - response_db_[index]);
  }
  mean /= grid.weight_sum;
  residual_db_.reserve(response_db_.size());
  aligned_target_db_.reserve(response_db_.size());
  double largest = 0.0;
  for (std::size_t index = 0; index < response_db_.size(); ++index) {
    const auto aligned = target_db_[index] - mean;
    const auto value = aligned - response_db_[index];
    aligned_target_db_.push_back(aligned);
    residual_db_.push_back(value);
    largest = std::max(largest, std::abs(value));
  }
  residual_span_db_ = std::max(1.0, std::ceil(largest));
}

void ResponsePlotWidget::rebuildExposed() {
  exposed_db_.clear();
  primitives_.clear();
  if (body_ == nullptr || !at_corner_ || frequencies_hz_.empty()) return;
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    const auto& authored = body_->corners[corner_].sections[section];
    const auto* pole = std::get_if<trench::core::native::Resonant>(&authored.pole);
    if (pole == nullptr) continue;
    trench::core::ConjugatePair viewed{
        pole->hz, std::exp(-std::numbers::pi * pole->bw_hz / sample_rate_hz_)};
    viewed.hz = viewHz(pole->hz, viewed.radius, false);
    auto curve = primitive_db(viewed, true, frequencies_hz_, sample_rate_hz_);
    if (section == selected_section_) {
      if (selected_lane_ == Lane::kPole) {
        exposed_db_ = curve;
      } else if (const auto* zero =
                     std::get_if<trench::core::native::Resonant>(&authored.zero)) {
        trench::core::ConjugatePair viewed_zero{
            zero->hz, std::exp(-std::numbers::pi * zero->bw_hz / sample_rate_hz_)};
        viewed_zero.hz = viewHz(zero->hz, viewed_zero.radius, true);
        exposed_db_ = primitive_db(viewed_zero, false, frequencies_hz_, sample_rate_hz_);
      }
    }
    primitives_.emplace_back(section, std::move(curve));
  }
}

void ResponsePlotWidget::ensureTrace(const QRectF& plot, double low_db, double high_db) {
  const auto key = std::make_tuple(plot.width(), plot.height(), low_db, high_db,
                                   body_revision_);
  if (trace_key_ && *trace_key_ == key) return;
  trace_key_ = key;
  trace_path_ = QPainterPath();
  trace_image_ = QImage();
  if (body_ == nullptr || plot.width() <= 0.0 || plot.height() <= 0.0) return;

  if (frequencies_hz_.empty()) return;
  const auto cascade = viewCascade();
  const auto bins =
      std::max(kTraceMinimumBins, static_cast<int>(std::lround(plot.width())));
  const auto points = bins * kTraceOversample;

  std::vector<float> raw_x;
  std::vector<float> raw_y;
  std::vector<float> locked_x;
  raw_x.reserve(static_cast<std::size_t>(bins));
  raw_y.reserve(static_cast<std::size_t>(bins));
  locked_x.reserve(static_cast<std::size_t>(bins));

  for (int bin = 0; bin < bins; ++bin) {
    double peak_db = 0.0;
    bool have = false;
    for (int step = 0; step < kTraceOversample; ++step) {
      const auto index = bin * kTraceOversample + step;
      const auto fraction =
          static_cast<double>(index) / static_cast<double>(points - 1);
      const auto frequency_hz =
          trench::app::frequency_axis::hz(fraction, sample_rate_hz_);
      const auto omega = 2.0 * std::numbers::pi * frequency_hz / sample_rate_hz_;
      const auto cw = static_cast<float>(std::cos(omega));
      const auto sw = static_cast<float>(std::sin(omega));
      const auto c2 = cw * cw - sw * sw;
      const auto s2 = 2.0F * sw * cw;
      auto power = 1.0F;
      for (const auto& section : cascade) {
        const auto b0 = static_cast<float>(section[0]);
        const auto b1 = static_cast<float>(section[1]);
        const auto b2 = static_cast<float>(section[2]);
        const auto a1 = static_cast<float>(section[3]);
        const auto a2 = static_cast<float>(section[4]);
        const auto nr = b2 * c2 + b1 * cw + b0;
        const auto ni = b2 * s2 + b1 * sw;
        const auto dr = a2 * c2 + a1 * cw + 1.0F;
        const auto di = a2 * s2 + a1 * sw;
        power *= (nr * nr + ni * ni) / (dr * dr + di * di);
      }
      if (std::isnan(power)) continue;
      const auto db = 10.0 * std::log10(static_cast<double>(power));
      if (!have || std::abs(peak_db) < std::abs(db)) {
        peak_db = db;
        have = true;
      }
    }
    if (!have) continue;
    const auto fraction = static_cast<double>(bin) / static_cast<double>(bins - 1);
    const auto yt =
        std::clamp((high_db - peak_db) / (high_db - low_db), -0.25, 1.25);
    const auto x = static_cast<float>(plot.left() + fraction * plot.width());
    raw_x.push_back(x);
    locked_x.push_back(std::floor(x) + 0.5F);
    raw_y.push_back(static_cast<float>(plot.top() + yt * plot.height()));
  }

  const auto count = static_cast<int>(raw_y.size());
  if (count == 0) return;
  const auto per_pixel = std::max(
      1, static_cast<int>(std::lround(static_cast<double>(bins) / plot.width())));
  for (int index = 0; index < count; ++index) {
    const auto before = raw_y[static_cast<std::size_t>(std::max(0, index - per_pixel))];
    const auto after =
        raw_y[static_cast<std::size_t>(std::min(count - 1, index + per_pixel))];
    const auto flat = std::abs(after - before) < kTraceFlatPx;
    const auto y = flat ? std::floor(raw_y[static_cast<std::size_t>(index)]) + 0.5F
                        : raw_y[static_cast<std::size_t>(index)];
    const auto x = flat ? locked_x[static_cast<std::size_t>(index)]
                        : raw_x[static_cast<std::size_t>(index)];
    if (index == 0) {
      trace_path_.moveTo(x, y);
    } else {
      trace_path_.lineTo(x, y);
    }
  }
}

void ResponsePlotWidget::strokeTrace(QPainter& painter, const QColor& colour) {
  if (trace_path_.isEmpty() || rect().isEmpty()) return;
  const auto supersample =
      std::max(3, static_cast<int>(std::ceil(devicePixelRatioF())));
  const auto target = rect().size() * supersample;
  if (trace_image_.size() != target || trace_image_color_ != colour ||
      trace_image_scale_ != supersample) {
    trace_image_ = QImage(target, QImage::Format_ARGB32_Premultiplied);
    trace_image_.fill(Qt::transparent);
    trace_image_color_ = colour;
    trace_image_scale_ = supersample;
    QPainter buffer(&trace_image_);
    buffer.setRenderHint(QPainter::Antialiasing, true);
    buffer.scale(supersample, supersample);
    QPen pen(colour, kTraceWidthPx);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::RoundJoin);
    buffer.setPen(pen);
    buffer.setBrush(Qt::NoBrush);
    buffer.drawPath(trace_path_);
    buffer.end();
    trace_image_.setDevicePixelRatio(supersample);
  }
  painter.drawImage(QPointF{0.0, 0.0}, trace_image_);
}

std::size_t ResponsePlotWidget::responsePointCount() const noexcept {
  return response_db_.size();
}

double ResponsePlotWidget::frequencyAt(std::size_t index) const {
  return frequencies_hz_.at(index);
}

double ResponsePlotWidget::responseDbAt(std::size_t index) const {
  return response_db_.at(index);
}

double ResponsePlotWidget::runningPeakDb(std::size_t section) const {
  return running_peak_db_.at(section);
}

std::size_t ResponsePlotWidget::residualPointCount() const noexcept {
  return residual_db_.size();
}

double ResponsePlotWidget::residualDbAt(std::size_t index) const {
  return residual_db_.at(index);
}

double ResponsePlotWidget::alignedTargetDbAt(std::size_t index) const {
  return aligned_target_db_.at(index);
}

std::optional<std::size_t> ResponsePlotWidget::highlightedSection() const noexcept {
  return highlight_section_;
}

QRectF ResponsePlotWidget::plotRect() const {
  const auto band = target_db_.empty() ? 0.0 : kResidualBandPx;
  return QRectF(rect()).adjusted(54.0, 28.0, -18.0, -(36.0 + band));
}

std::pair<double, double> ResponsePlotWidget::dbRange() const {
  if (response_db_.empty() || response_db_.size() != frequencies_hz_.size()) {
    return {-48.0, 0.0};
  }
  auto minimum = std::numeric_limits<double>::infinity();
  auto maximum = -std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < response_db_.size(); ++index) {
    if (frequencies_hz_[index] < kAxisLowHz || frequencies_hz_[index] > kAxisHighHz) {
      continue;
    }
    minimum = std::min(minimum, response_db_[index]);
    maximum = std::max(maximum, response_db_[index]);
  }
  if (minimum > maximum) return {-48.0, 0.0};
  double low_db = std::floor((minimum - 3.0) / 12.0) * 12.0;
  double high_db = std::ceil((maximum + 3.0) / 12.0) * 12.0;
  if (high_db - low_db < 48.0) {
    const auto middle = (high_db + low_db) * 0.5;
    low_db = std::floor((middle - 24.0) / 12.0) * 12.0;
    high_db = low_db + 48.0;
  }
  if (high_db - low_db > kAxisMaxSpanDb) low_db = high_db - kAxisMaxSpanDb;
  return {low_db, high_db};
}

double ResponsePlotWidget::dbForY(double y) const {
  const auto plot = plotRect();
  const auto [low_db, high_db] = dbRange();
  return db_for_y(y, low_db, high_db, plot);
}

double ResponsePlotWidget::xForFrequency(double frequency_hz) const {
  const auto plot = plotRect();
  if (frequencies_hz_.empty() || plot.width() <= 0.0) return 0.0;
  return x_for_frequency(frequency_hz, sample_rate_hz_, plot);
}

std::vector<ResponsePlotWidget::TokenInfo> ResponsePlotWidget::tokens() const {
  std::vector<TokenInfo> out;
  if (body_ == nullptr || !at_corner_ || frequencies_hz_.empty()) return out;
  const auto plot = plotRect();
  if (plot.width() <= 0.0 || plot.height() <= 0.0) return out;
  const auto low_hz = trench::app::frequency_axis::low_hz(sample_rate_hz_);
  const auto high_hz = trench::app::frequency_axis::high_hz(sample_rate_hz_);

  for (const auto lane : {Lane::kPole, Lane::kZero}) {
    for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
      const auto& authored = body_->corners[corner_].sections[section];
      if (!std::holds_alternative<trench::core::native::Resonant>(authored.pole)) continue;
      const auto& pair = lane == Lane::kPole ? authored.pole : authored.zero;
      double hz = low_hz;
      double radius = 0.0;
      auto zero_root = lane == Lane::kZero;
      if (!root_placement(pair, sample_rate_hz_, low_hz, high_hz, hz, radius)) {
        if (lane != Lane::kZero ||
            !root_placement(authored.pole, sample_rate_hz_, low_hz, high_hz, hz,
                            radius)) {
          continue;
        }
        zero_root = false;
      } else {
        hz = viewHz(hz, radius, zero_root);
      }
      const auto bit = lane == Lane::kPole ? trench::core::p2k::pole_bit(section)
                                           : trench::core::p2k::zero_bit(section);
      const auto [low_db, high_db] = dbRange();
      const auto lit = section == selected_section_ ||
                       (highlight_section_ && section == *highlight_section_);
      TokenInfo token;
      token.section = section;
      token.lane = lane;
      auto y = y_for_contribution(responseDbAtHz(hz), low_db, high_db, plot);
      if (lane == Lane::kPole) {
        for (const auto& [owner, curve] : primitives_) {
          if (owner != section || curve.empty()) continue;
          const auto position = trench::app::frequency_axis::fraction(hz, sample_rate_hz_) *
                                static_cast<double>(curve.size() - 1);
          const auto index = std::clamp<std::size_t>(
              static_cast<std::size_t>(std::lround(position)), 0, curve.size() - 1);
          y = y_for_db(curve[index], low_db, high_db, plot);
          break;
        }
      }
      token.position = QPointF{
          std::clamp(x_for_frequency(std::clamp(hz, low_hz, high_hz), sample_rate_hz_, plot),
                     plot.left() + kTokenRadiusPx, plot.right() - kTokenRadiusPx),
          y};
      token.radius = lane == Lane::kPole ? kTokenRadiusPx * (lit ? 0.8 : 0.6)
                                         : kTokenRadiusPx * (lit ? 1.15 : 0.95);
      token.live = true;
      token.pinned = (freedom_mask_ & bit) == 0U;
      out.push_back(token);
    }
  }
  return out;
}

std::vector<double> ResponsePlotWidget::exposedDb() const { return exposed_db_; }

std::size_t ResponsePlotWidget::primitiveCount() const noexcept {
  return primitives_.size();
}

void ResponsePlotWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kBackground);

  const QRectF plot = plotRect();
  if (plot.width() <= 0.0 || plot.height() <= 0.0 || response_db_.empty()) return;

  const auto [low_db, high_db] = dbRange();

  painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
  painter.setPen(QPen(kGrid, 1.0));
  const auto low_hz = trench::app::frequency_axis::low_hz(sample_rate_hz_);
  const auto high_hz = trench::app::frequency_axis::high_hz(sample_rate_hz_);
  for (const auto frequency : {100.0, 1000.0, 10'000.0}) {
    if (frequency < low_hz || frequency > high_hz) continue;
    const auto x = x_for_frequency(frequency, sample_rate_hz_, plot);
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
    painter.setPen(kText);
    const auto label = frequency >= 1000.0
                           ? QString::number(frequency / 1000.0, 'g', 2) + QStringLiteral(" kHz")
                           : QString::number(frequency, 'f', 0) + QStringLiteral(" Hz");
    painter.drawText(QRectF{x - 32.0, height() - 29.0, 64.0, 18.0},
                     Qt::AlignHCenter | Qt::AlignTop, label);
    painter.setPen(QPen(kGrid, 1.0));
  }

  for (double db = low_db; db <= high_db + 0.1; db += 12.0) {
    const auto y = y_for_db(db, low_db, high_db, plot);
    painter.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
    painter.setPen(kText);
    painter.drawText(QRectF{2.0, y - 9.0, 46.0, 18.0}, Qt::AlignRight | Qt::AlignVCenter,
                     QString::number(db, 'f', 0) + QStringLiteral(" dB"));
    painter.setPen(QPen(kGrid, 1.0));
  }

  if (!residual_db_.empty() && residual_db_.size() == frequencies_hz_.size()) {
    const QRectF band{plot.left(), plot.bottom() + 6.0, plot.width(),
                      kResidualBandPx - 12.0};
    const auto middle = band.center().y();
    painter.setPen(QPen(kGrid, 1.0));
    painter.drawLine(QPointF{band.left(), middle}, QPointF{band.right(), middle});
    painter.setPen(kText);
    painter.drawText(QRectF{2.0, middle - 9.0, 46.0, 18.0},
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("±%1 dB").arg(residual_span_db_, 0, 'f', 0));
    QPainterPath residual_path;
    for (std::size_t index = 0; index < residual_db_.size(); ++index) {
      const auto x = x_for_frequency(frequencies_hz_[index], sample_rate_hz_, plot);
      const auto y = middle - residual_db_[index] / residual_span_db_ * band.height() * 0.5;
      if (index == 0) {
        residual_path.moveTo(x, y);
      } else {
        residual_path.lineTo(x, y);
      }
    }
    painter.save();
    painter.setClipRect(band.adjusted(0.0, -2.0, 0.0, 2.0));
    painter.setPen(QPen(kResidual, kTraceWidthPx));
    painter.drawPath(residual_path);
    painter.restore();
  }

  ensureTrace(plot, low_db, high_db);
  painter.setClipRect(plot);

  const auto stroke_primitive = [&](const std::vector<double>& curve, const QColor& ink,
                                   double width) {
    if (curve.size() != frequencies_hz_.size()) return;
    QPainterPath path;
    for (std::size_t index = 0; index < curve.size(); ++index) {
      const auto x = x_for_frequency(frequencies_hz_[index], sample_rate_hz_, plot);
      const auto y = y_for_db(curve[index], low_db, high_db, plot);
      if (index == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    painter.setPen(QPen(ink, width));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
  };
  const auto flashing = flash_section_ && flash_age_.isValid() &&
                        flash_age_.elapsed() <= kFlashHoldMs;
  if (highlight_section_) {
    const auto highlighted = std::find_if(
        primitives_.begin(), primitives_.end(), [this](const auto& primitive) {
          return primitive.first == *highlight_section_;
        });
    if (highlighted != primitives_.end()) {
      stroke_primitive(highlighted->second, kPrimitive.lighter(160),
                       kPrimitiveWidthPx);
    }
  }
  if (fit_running_ || flashing) {
    stroke_primitive(exposed_db_, kExposed, kExposedWidthPx);
  }

  if (aligned_target_db_.size() == frequencies_hz_.size()) {
    QPainterPath target_path;
    for (std::size_t index = 0; index < aligned_target_db_.size(); ++index) {
      const auto x = x_for_frequency(frequencies_hz_[index], sample_rate_hz_, plot);
      const auto y = y_for_db(aligned_target_db_[index], low_db, high_db, plot);
      if (index == 0) {
        target_path.moveTo(x, y);
      } else {
        target_path.lineTo(x, y);
      }
    }
    painter.setPen(QPen(kTarget, kTraceWidthPx));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(target_path);
  }

  strokeTrace(painter, kTrace);
  painter.setClipping(false);
}
