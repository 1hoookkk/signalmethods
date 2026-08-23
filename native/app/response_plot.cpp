#include "response_plot.hpp"

#include "section_color.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/transpose.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace {

const QColor kBackground{17, 20, 22};
const QColor kGrid{55, 63, 67};
const QColor kText{174, 186, 190};
const QColor kTrace{87, 222, 205};
const QColor kRefusal{226, 78, 74};

constexpr int kTraceOversample = 4;
constexpr int kTraceMinimumBins = 192;
constexpr double kTraceWidthPx = 1.1;
constexpr float kTraceFlatPx = 0.15F;

constexpr double kRadiusWarpMax = 60.0;
constexpr double kHitRadiusPx = 11.0;
constexpr double kTokenRadiusPx = 8.0;
constexpr qint64 kRefusalHoldMs = 600;
constexpr qint64 kFlashHoldMs = 200;
const QColor kTarget{72, 82, 88};
const QColor kResidual{156, 130, 224};

constexpr double kResidualBandPx = 34.0;

double erb_rate(double frequency_hz) {
  return 21.4 * std::log10(1.0 + 0.00437 * frequency_hz);
}

double frequency_for_erb_rate(double rate) {
  return (std::pow(10.0, rate / 21.4) - 1.0) / 0.00437;
}

double frequency_for_fraction(double fraction, double low_hz, double high_hz) {
  return frequency_for_erb_rate(erb_rate(low_hz) +
                                fraction * (erb_rate(high_hz) - erb_rate(low_hz)));
}

double x_for_frequency(double frequency_hz, double low_hz, double high_hz,
                       const QRectF& plot) {
  const auto fraction = (erb_rate(frequency_hz) - erb_rate(low_hz)) /
                        (erb_rate(high_hz) - erb_rate(low_hz));
  return plot.left() + fraction * plot.width();
}

double frequency_for_x(double x, double low_hz, double high_hz, const QRectF& plot) {
  return frequency_for_fraction((x - plot.left()) / plot.width(), low_hz, high_hz);
}

double y_for_db(double db, double low_db, double high_db, const QRectF& plot) {
  return plot.bottom() - (db - low_db) / (high_db - low_db) * plot.height();
}

double warp_of_radius(double radius) {
  if (radius <= 0.0) return 0.0;
  if (radius >= 1.0) return kRadiusWarpMax;
  return std::clamp(20.0 * std::log10(1.0 / (1.0 - radius)), 0.0, kRadiusWarpMax);
}

double radius_of_warp(double warp) {
  return 1.0 - std::pow(10.0, -std::max(warp, 0.0) / 20.0);
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

bool root_placement(const trench::core::RootPair& pair, double low_hz, double high_hz,
                    double& hz, double& radius) {
  if (const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&pair)) {
    hz = std::clamp(conjugate->hz, low_hz, high_hz);
    radius = conjugate->radius;
    return true;
  }
  if (const auto* real = std::get_if<trench::core::RealPair>(&pair)) {
    const auto centre = (real->root_a + real->root_b) * 0.5;
    hz = centre >= 0.0 ? low_hz : high_hz;
    radius = std::clamp(std::max(std::abs(real->root_a), std::abs(real->root_b)), 0.0, 1.0);
    return false;
  }
  hz = low_hz;
  radius = 0.0;
  return false;
}

trench::core::Cascade corner_cascade(const trench::core::PackedBody& body,
                                     std::size_t corner) {
  trench::core::Cascade out{};
  for (std::size_t section = 0; section < trench::core::kSectionCount; ++section) {
    out[section] = trench::core::section_words_to_biquad(body.words[corner][section]);
  }
  return out;
}

}  // namespace

ResponsePlotWidget::ResponsePlotWidget(QWidget* parent) : QWidget(parent) {
  setMinimumSize(480, 280);
  setAutoFillBackground(false);
  setMouseTracking(true);
}

void ResponsePlotWidget::setBody(const trench::core::PackedBody* body,
                                 double sample_rate_hz,
                                 std::string source_label) {
  body_ = body;
  sample_rate_hz_ = sample_rate_hz;
  source_label_ = QString::fromStdString(std::move(source_label));
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

void ResponsePlotWidget::setView(float morph, float q, int semitones) {
  view_morph_ = morph;
  view_q_ = q;
  view_semitones_ = semitones;
  at_corner_ = (morph == 0.0F || morph == 1.0F) && (q == 0.0F || q == 1.0F) && semitones == 0;
  refresh();
}

trench::core::Cascade ResponsePlotWidget::viewCascade() const {
  if (at_corner_) return corner_cascade(*body_, corner_);
  return trench::core::transpose_cascade(
      body_->interpolate_biquads(view_morph_, view_q_, 0.0F),
      trench::core::ratio_of_semitones(view_semitones_), sample_rate_hz_);
}

void ResponsePlotWidget::setFreedomMask(std::uint32_t mask) {
  freedom_mask_ = mask;
  update();
}

void ResponsePlotWidget::setSelectedSection(std::size_t section) {
  selected_section_ = section;
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
  target_db_ = target == nullptr ? std::vector<double>{} : *target;
  rebuildResidual();
  update();
}

void ResponsePlotWidget::setFitRunning(bool running) {
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
  ++body_revision_;
  update();
}

void ResponsePlotWidget::rebuildResidual() {
  residual_db_.clear();
  residual_span_db_ = 1.0;
  if (target_db_.size() != response_db_.size() || response_db_.empty()) return;
  const auto& weight = trench::core::p2k::grid().weight;
  if (weight.size() != response_db_.size()) return;
  double mean = 0.0;
  for (std::size_t index = 0; index < response_db_.size(); ++index) {
    mean += weight[index] * (target_db_[index] - response_db_[index]);
  }
  mean /= trench::core::p2k::grid().weight_sum;
  residual_db_.reserve(response_db_.size());
  double largest = 0.0;
  for (std::size_t index = 0; index < response_db_.size(); ++index) {
    const auto value = target_db_[index] - response_db_[index] - mean;
    residual_db_.push_back(value);
    largest = std::max(largest, std::abs(value));
  }
  residual_span_db_ = std::max(1.0, std::ceil(largest));
}

double ResponsePlotWidget::contributionAt(std::size_t section, double hz) const {
  if (section >= contributions_.size() || frequencies_hz_.empty()) return 0.0;
  const auto& curve = contributions_[section];
  const auto low = frequencies_hz_.front();
  const auto high = frequencies_hz_.back();
  const auto clamped = std::clamp(hz, low, high);
  const auto position = std::log(clamped / low) / std::log(high / low) *
                        static_cast<double>(curve.size() - 1);
  const auto lower = static_cast<std::size_t>(std::floor(position));
  const auto upper = std::min(lower + 1, curve.size() - 1);
  const auto blend = position - static_cast<double>(lower);
  return curve[lower] * (1.0 - blend) + curve[upper] * blend;
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
  const auto axis_low_hz = frequencies_hz_.front();
  const auto axis_high_hz = frequencies_hz_.back();
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
      const auto frequency_hz = frequency_for_fraction(fraction, axis_low_hz, axis_high_hz);
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
  if (trace_image_.size() != target || trace_image_color_ != colour) {
    trace_image_ = QImage(target, QImage::Format_ARGB32_Premultiplied);
    trace_image_.fill(Qt::transparent);
    trace_image_color_ = colour;
    QPainter buffer(&trace_image_);
    buffer.setRenderHint(QPainter::Antialiasing, true);
    buffer.scale(supersample, supersample);
    QPen pen(colour, kTraceWidthPx);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::RoundJoin);
    buffer.setPen(pen);
    buffer.setBrush(Qt::NoBrush);
    buffer.drawPath(trace_path_);
  }
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.drawImage(QRectF(rect()), trace_image_);
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

bool ResponsePlotWidget::refusalVisible() const noexcept {
  return refusal_active_ && refusal_age_.isValid() &&
         refusal_age_.elapsed() <= kRefusalHoldMs;
}

std::optional<std::size_t> ResponsePlotWidget::highlightedSection() const noexcept {
  return highlight_section_;
}

QRectF ResponsePlotWidget::plotRect() const {
  const auto band = target_db_.empty() ? 0.0 : kResidualBandPx;
  return QRectF(rect()).adjusted(54.0, 28.0, -18.0, -(36.0 + band));
}

std::pair<double, double> ResponsePlotWidget::dbRange() const {
  if (response_db_.empty()) return {-48.0, 0.0};
  const auto [minimum, maximum] =
      std::minmax_element(response_db_.begin(), response_db_.end());
  double low_db = std::floor((*minimum - 3.0) / 12.0) * 12.0;
  double high_db = std::ceil((*maximum + 3.0) / 12.0) * 12.0;
  if (high_db - low_db < 48.0) {
    const auto middle = (high_db + low_db) * 0.5;
    low_db = std::floor((middle - 24.0) / 12.0) * 12.0;
    high_db = low_db + 48.0;
  }
  return {low_db, high_db};
}

std::vector<ResponsePlotWidget::TokenInfo> ResponsePlotWidget::tokens() const {
  std::vector<TokenInfo> out;
  if (body_ == nullptr || !at_corner_ || frequencies_hz_.empty()) return out;
  const auto plot = plotRect();
  if (plot.width() <= 0.0 || plot.height() <= 0.0) return out;
  const auto low_hz = frequencies_hz_.front();
  const auto high_hz = frequencies_hz_.back();

  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    const auto geometry = trench::core::geometry_from_words(
        body_->words[corner_][section], trench::core::kP2kDatumHz);
    for (const auto lane : {Lane::kPole, Lane::kZero}) {
      if (lane == Lane::kZero && section != selected_section_) continue;
      const auto& pair = lane == Lane::kPole ? geometry.pole : geometry.zero;
      double hz = low_hz;
      double radius = 0.0;
      if (!root_placement(pair, low_hz, high_hz, hz, radius)) continue;
      const auto bit = lane == Lane::kPole ? trench::core::p2k::pole_bit(section)
                                           : trench::core::p2k::zero_bit(section);
      const auto [low_db, high_db] = latched_db_.value_or(dbRange());
      TokenInfo token;
      token.section = section;
      token.lane = lane;
      token.position = QPointF{
          std::clamp(x_for_frequency(hz, low_hz, high_hz, plot),
                     plot.left() + kTokenRadiusPx, plot.right() - kTokenRadiusPx),
          y_for_contribution(responseDbAtHz(hz), low_db, high_db, plot)};
      token.live = true;
      token.pinned = (freedom_mask_ & bit) == 0U;
      out.push_back(token);
    }
  }
  return out;
}

std::optional<ResponsePlotWidget::TokenInfo> ResponsePlotWidget::hit(
    const QPointF& at) const {
  std::optional<TokenInfo> best;
  double best_distance = kHitRadiusPx;
  for (const auto& token : tokens()) {
    const auto dx = token.position.x() - at.x();
    const auto dy = token.position.y() - at.y();
    const auto distance = std::hypot(dx, dy);
    if (distance < best_distance) {
      best_distance = distance;
      best = token;
    }
  }
  return best;
}

void ResponsePlotWidget::refuse(double frequency_hz) {
  refusal_active_ = true;
  refusal_hz_ = frequency_hz;
  refusal_age_.start();
  QTimer::singleShot(kRefusalHoldMs + 10, this, [this] { update(); });
  update();
}

void ResponsePlotWidget::moveTo(const QPointF& at) {
  namespace p2k = trench::core::p2k;
  const auto plot = plotRect();
  if (plot.width() <= 0.0 || plot.height() <= 0.0 || frequencies_hz_.empty()) return;
  const auto low_hz = frequencies_hz_.front();
  const auto high_hz = frequencies_hz_.back();
  const auto is_pole = press_lane_ == Lane::kPole;
  const auto s6_zero = press_section_ == 5 && !is_pole;

  const auto hz = std::clamp(frequency_for_x(at.x(), low_hz, high_hz, plot), 20.0,
                             p2k::kRootHiHz);
  const auto warp_delta =
      (press_position_.y() - at.y()) / plot.height() * kRadiusWarpMax;
  double radius = s6_zero ? p2k::s6_zero_radius()
                          : radius_of_warp(warp_of_radius(press_radius_) + warp_delta);
  radius = is_pole ? std::clamp(radius, 0.0, p2k::kPoleRMax)
                   : std::clamp(radius, 0.0, 1.0);

  const auto [word_mag, snapped_rsq] = p2k::words_from_root(hz, radius);
  const auto word_rsq = s6_zero ? p2k::kS6ZeroRsqWord : snapped_rsq;
  const auto [p, q] = p2k::pq(word_mag, word_rsq);
  if (!p2k::is_legal(p, q, is_pole) ||
      !p2k::magnitude_admissible(p2k::nearest_lattice_word(word_mag), is_pole)) {
    refuse(hz);
    return;
  }

  auto candidate = origin_words_;
  candidate[is_pole ? 2 : 0] = word_mag;
  candidate[is_pole ? 3 : 1] = word_rsq;
  refusal_active_ = false;
  if (candidate == body_->words[corner_][press_section_]) {
    update();
    return;
  }
  emit sectionEdited(press_section_, candidate);
}

void ResponsePlotWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || body_ == nullptr) {
    QWidget::mousePressEvent(event);
    return;
  }
  const auto token = hit(event->position());
  if (!token) {
    QWidget::mousePressEvent(event);
    return;
  }
  pressed_ = true;
  moved_ = false;
  dragging_ = false;
  press_section_ = token->section;
  press_lane_ = token->lane;
  press_position_ = event->position();
  origin_words_ = body_->words[corner_][token->section];
  const auto geometry = trench::core::geometry_from_words(
      origin_words_, trench::core::kP2kDatumHz);
  const auto& pair = token->lane == Lane::kPole ? geometry.pole : geometry.zero;
  if (const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&pair)) {
    press_radius_ = conjugate->radius;
  } else {
    press_radius_ = 0.0;
  }
  emit tokenSelected(token->section, token->lane);
  event->accept();
}

void ResponsePlotWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!pressed_) {
    const auto token = hit(event->position());
    const auto hovered =
        token ? std::optional<std::size_t>{token->section} : std::nullopt;
    if (hovered != last_hover_section_) {
      last_hover_section_ = hovered;
      emit tokenHovered(hovered);
    }
    QWidget::mouseMoveEvent(event);
    return;
  }
  const auto at = event->position();
  if (!dragging_) {
    const auto travel = std::hypot(at.x() - press_position_.x(), at.y() - press_position_.y());
    if (travel < QApplication::startDragDistance()) {
      event->accept();
      return;
    }
    moved_ = true;
    const auto token = hit(press_position_);
    if (fit_running_ || !token || !token->live || token->pinned) {
      event->accept();
      return;
    }
    dragging_ = true;
    latched_db_ = dbRange();
    emit gestureStarted(press_section_);
  }
  moveTo(at);
  event->accept();
}

void ResponsePlotWidget::leaveEvent(QEvent* event) {
  if (last_hover_section_) {
    last_hover_section_.reset();
    emit tokenHovered(std::nullopt);
  }
  QWidget::leaveEvent(event);
}

void ResponsePlotWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (!pressed_) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  const auto section = press_section_;
  const auto lane = press_lane_;
  const auto dragged = dragging_;
  const auto moved = moved_;
  pressed_ = false;
  moved_ = false;
  dragging_ = false;
  latched_db_.reset();
  refusal_active_ = false;
  if (dragged) {
    emit gestureFinished(section);
  } else if (!moved) {
    emit pinToggled(section, lane);
  }
  update();
  event->accept();
}

void ResponsePlotWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kBackground);

  const QRectF plot = plotRect();
  if (plot.width() <= 0.0 || plot.height() <= 0.0 || response_db_.empty()) return;

  const auto [low_db, high_db] = latched_db_.value_or(dbRange());

  painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
  painter.setPen(QPen(kGrid, 1.0));
  const auto low_hz = frequencies_hz_.front();
  const auto high_hz = frequencies_hz_.back();
  for (const auto frequency : {100.0, 1000.0, 10'000.0}) {
    if (frequency < low_hz || frequency > high_hz) continue;
    const auto x = x_for_frequency(frequency, low_hz, high_hz, plot);
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
      const auto x = x_for_frequency(frequencies_hz_[index], low_hz, high_hz, plot);
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

  if (target_db_.size() == frequencies_hz_.size()) {
    QPainterPath target_path;
    for (std::size_t index = 0; index < target_db_.size(); ++index) {
      const auto x = x_for_frequency(frequencies_hz_[index], low_hz, high_hz, plot);
      const auto y = y_for_db(target_db_[index], low_db, high_db, plot);
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

  if (refusalVisible()) {
    const auto hz = std::clamp(refusal_hz_, low_hz, high_hz);
    const auto x = x_for_frequency(hz, low_hz, high_hz, plot);
    const auto fraction = std::log(hz / low_hz) / std::log(high_hz / low_hz);
    const auto position = fraction * static_cast<double>(response_db_.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = std::min(lower + 1, response_db_.size() - 1);
    const auto blend = position - static_cast<double>(lower);
    const auto db = response_db_[lower] * (1.0 - blend) + response_db_[upper] * blend;
    const auto y = y_for_db(db, low_db, high_db, plot);
    painter.setPen(QPen(kRefusal, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF{x, y}, 5.0, 5.0);
    painter.drawLine(QPointF{x, y - 11.0}, QPointF{x, y - 6.0});
  }

  const auto flashing = flash_section_ && flash_age_.isValid() &&
                        flash_age_.elapsed() <= kFlashHoldMs;
  for (const auto& token : tokens()) {
    auto ink = section_color(token.section);
    const auto highlighted = highlight_section_ && token.section == *highlight_section_;
    if ((flashing && token.section == *flash_section_) || highlighted) {
      ink = ink.lighter(160);
    }
    const auto selected = token.section == selected_section_;
    const auto radius = kTokenRadiusPx * (selected || highlighted ? 0.8 : 0.6);
    if (token.lane == Lane::kPole) {
      painter.setPen(QPen(kBackground, 1.5));
      painter.setBrush(token.pinned ? QBrush(kBackground) : QBrush(ink));
      painter.drawEllipse(token.position, radius, radius);
      if (token.pinned) {
        painter.setPen(QPen(ink, 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(token.position, radius - 1.0, radius - 1.0);
      }
      if (selected) {
        painter.setPen(QPen(ink, 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(token.position, radius + 3.5, radius + 3.5);
      }
    } else {
      painter.setPen(QPen(ink, token.pinned ? 2.4 : 1.4));
      painter.setBrush(QBrush(kBackground));
      painter.drawEllipse(token.position, radius, radius);
    }
  }
  painter.setClipping(false);
}
