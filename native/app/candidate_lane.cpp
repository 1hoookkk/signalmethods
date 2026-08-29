#include "candidate_lane.hpp"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kLowHz = 20.0;
constexpr double kHighHz = 20'000.0;
constexpr double kMinBandwidthHz = 1.0;
constexpr double kMaxBandwidthHz = 20'000.0;
constexpr double kMinProminenceDb = 3.0;
constexpr double kMinSpacingOctaves = 1.0 / 6.0;
constexpr double kFullStemDb = 24.0;
constexpr std::size_t kMaxCandidates = 12;
constexpr double kPickSlackPx = 8.0;

constexpr QColor kPanel{255, 255, 255};
constexpr QColor kPanelEdge{200, 200, 200};
constexpr QColor kInk{128, 128, 128};
constexpr QColor kHoverInk{0, 0, 0};

double prominenceAt(const std::vector<double>& magnitude_db,
                    std::size_t peak) {
  const double top = magnitude_db[peak];
  double left_min = top;
  for (std::size_t index = peak; index-- > 0;) {
    if (magnitude_db[index] > top) break;
    left_min = std::min(left_min, magnitude_db[index]);
  }
  double right_min = top;
  for (std::size_t index = peak + 1; index < magnitude_db.size(); ++index) {
    if (magnitude_db[index] > top) break;
    right_min = std::min(right_min, magnitude_db[index]);
  }
  return top - std::max(left_min, right_min);
}

double bandwidthAt(const std::vector<double>& frequency_hz,
                   const std::vector<double>& magnitude_db,
                   std::size_t peak) {
  const double edge_db = magnitude_db[peak] - 3.0;
  double left_hz = 0.0;
  bool has_left = false;
  for (std::size_t index = peak; index-- > 0;) {
    if (magnitude_db[index] > edge_db) continue;
    const double span = magnitude_db[index + 1] - magnitude_db[index];
    const double fraction = span > 0.0 ? (edge_db - magnitude_db[index]) / span : 0.0;
    left_hz = frequency_hz[index] +
              fraction * (frequency_hz[index + 1] - frequency_hz[index]);
    has_left = true;
    break;
  }
  double right_hz = 0.0;
  bool has_right = false;
  for (std::size_t index = peak + 1; index < magnitude_db.size(); ++index) {
    if (magnitude_db[index] > edge_db) continue;
    const double span = magnitude_db[index - 1] - magnitude_db[index];
    const double fraction = span > 0.0 ? (magnitude_db[index - 1] - edge_db) / span : 0.0;
    right_hz = frequency_hz[index - 1] +
               fraction * (frequency_hz[index] - frequency_hz[index - 1]);
    has_right = true;
    break;
  }
  double width_hz = kMaxBandwidthHz;
  if (has_left && has_right) {
    width_hz = right_hz - left_hz;
  } else if (has_left) {
    width_hz = 2.0 * (frequency_hz[peak] - left_hz);
  } else if (has_right) {
    width_hz = 2.0 * (right_hz - frequency_hz[peak]);
  }
  return std::clamp(width_hz, kMinBandwidthHz, kMaxBandwidthHz);
}

}  // namespace

CandidateLane::CandidateLane(QWidget* parent) : QWidget(parent) {
  setFixedHeight(48);
  setMouseTracking(true);
  setAccessibleName(QStringLiteral("Candidate resonances read off the reference"));
}

void CandidateLane::setReference(const std::vector<double>& frequency_hz,
                                 const std::vector<double>& magnitude_db) {
  if (frequency_hz.size() != magnitude_db.size() || frequency_hz.size() < 3) {
    clearReference();
    return;
  }
  std::vector<std::size_t> peaks;
  for (std::size_t index = 1; index + 1 < magnitude_db.size(); ++index) {
    if (!std::isfinite(magnitude_db[index]) ||
        !(frequency_hz[index] >= kLowHz && frequency_hz[index] <= kHighHz)) {
      continue;
    }
    if (magnitude_db[index] > magnitude_db[index - 1] &&
        magnitude_db[index] > magnitude_db[index + 1]) {
      peaks.push_back(index);
    }
  }
  std::vector<Candidate> found;
  found.reserve(peaks.size());
  for (const std::size_t peak : peaks) {
    const double prominence = prominenceAt(magnitude_db, peak);
    if (!(prominence >= kMinProminenceDb)) continue;
    found.push_back(Candidate{frequency_hz[peak],
                              bandwidthAt(frequency_hz, magnitude_db, peak),
                              prominence});
  }
  std::sort(found.begin(), found.end(),
            [](const Candidate& a, const Candidate& b) {
              return a.prominence_db > b.prominence_db;
            });
  candidates_.clear();
  for (const Candidate& candidate : found) {
    if (candidates_.size() == kMaxCandidates) break;
    const bool crowded = std::any_of(
        candidates_.begin(), candidates_.end(), [&](const Candidate& kept) {
          return std::abs(std::log2(candidate.hz / kept.hz)) < kMinSpacingOctaves;
        });
    if (!crowded) candidates_.push_back(candidate);
  }
  std::sort(candidates_.begin(), candidates_.end(),
            [](const Candidate& a, const Candidate& b) { return a.hz < b.hz; });
  hovered_ = -1;
  update();
}

void CandidateLane::clearReference() {
  candidates_.clear();
  hovered_ = -1;
  update();
}

QRectF CandidateLane::lane() const {
  return QRectF(rect()).adjusted(62.0, 8.0, -22.0, -8.0);
}

double CandidateLane::xForFrequency(double frequency_hz,
                                    const QRectF& bounds) const {
  const double fraction = std::log(frequency_hz / kLowHz) /
                          std::log(kHighHz / kLowHz);
  return bounds.left() + std::clamp(fraction, 0.0, 1.0) * bounds.width();
}

int CandidateLane::candidateAt(double x) const {
  const QRectF bounds = lane();
  int closest = -1;
  double closest_distance = kPickSlackPx;
  for (std::size_t index = 0; index < candidates_.size(); ++index) {
    const double distance =
        std::abs(xForFrequency(candidates_[index].hz, bounds) - x);
    if (distance > closest_distance) continue;
    closest_distance = distance;
    closest = static_cast<int>(index);
  }
  return closest;
}

void CandidateLane::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), palette().window().color());
  const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kPanel);
  painter.drawRect(card);
  painter.setPen(QPen(kPanelEdge, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(card);

  const QRectF bounds = lane();

  for (std::size_t index = 0; index < candidates_.size(); ++index) {
    const Candidate& candidate = candidates_[index];
    const bool hovered = static_cast<int>(index) == hovered_;
    const QColor ink = hovered ? kHoverInk : kInk;
    const double x = xForFrequency(candidate.hz, bounds);
    const double height = std::clamp(
        candidate.prominence_db / kFullStemDb * bounds.height(), 6.0,
        bounds.height());
    QPen stem(ink, 1.0);
    stem.setCosmetic(true);
    painter.setPen(stem);
    painter.drawLine(QPointF{x, bounds.bottom()},
                     QPointF{x, bounds.bottom() - height});
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawEllipse(QPointF{x, bounds.bottom() - height}, 4.5, 4.5);
  }
}

void CandidateLane::mouseMoveEvent(QMouseEvent* event) {
  const int hovered = candidateAt(event->position().x());
  if (hovered == hovered_) return;
  hovered_ = hovered;
  update();
}

void CandidateLane::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const int picked = candidateAt(event->position().x());
  if (picked < 0 || !onPick) return;
  const Candidate& candidate = candidates_[static_cast<std::size_t>(picked)];
  onPick(candidate.hz, candidate.bw_hz);
}

void CandidateLane::leaveEvent(QEvent*) {
  if (hovered_ < 0) return;
  hovered_ = -1;
  update();
}
