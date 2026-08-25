#include "armadillo_view.hpp"

#include "trench/core/p2k.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <variant>

namespace {

const QColor kGround{16, 20, 24};
const QColor kLine{40, 47, 51};
const QColor kRim{88, 98, 102};
const QColor kInk{174, 186, 190};
const QColor kDim{118, 127, 131};
const QColor kAccent{87, 222, 205};

constexpr double kRimDb = 96.0;
constexpr double kHitRadius = 9.0;
constexpr double kMaxDragRadius = 0.99998;

double rimward_db(double radius) {
  const auto distance = std::clamp(1.0 - radius, std::pow(10.0, -kRimDb / 20.0), 1.0);
  return -20.0 * std::log10(distance);
}

QFont letter_font() { return QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold); }

}  // namespace

ArmadilloView::ArmadilloView(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setMinimumHeight(120);
}

void ArmadilloView::setBody(const trench::core::PackedBody* body, double sample_rate_hz) {
  body_ = body;
  sample_rate_hz_ = sample_rate_hz;
  refresh();
}

void ArmadilloView::setCorner(std::size_t corner) {
  corner_ = corner;
  refresh();
}

void ArmadilloView::setSelected(std::optional<std::size_t> section, bool zero) {
  selected_section_ = section;
  selected_zero_ = zero;
  update();
}

void ArmadilloView::refresh() {
  rebuildMarkers();
  update();
}

const std::vector<ArmadilloView::Marker>& ArmadilloView::markers() const noexcept {
  return markers_;
}

QRectF ArmadilloView::plane() const {
  return QRectF(10.0, 14.0, std::max(40.0, width() - 20.0),
                std::max(30.0, height() - 30.0));
}

double ArmadilloView::xForFrequency(double hz) const {
  const auto area = plane();
  const auto lo = sample_rate_hz_ / 2048.0;
  const auto hi = sample_rate_hz_ / 2.0;
  const auto clamped = std::clamp(hz, lo, hi);
  return area.left() + area.width() * std::log2(clamped / lo) / std::log2(hi / lo);
}

double ArmadilloView::yForRadius(double radius) const {
  const auto area = plane();
  return area.bottom() - area.height() * std::clamp(rimward_db(radius) / kRimDb, 0.0, 1.0);
}

double ArmadilloView::frequencyForX(double x) const {
  const auto area = plane();
  const auto lo = sample_rate_hz_ / 2048.0;
  const auto hi = sample_rate_hz_ / 2.0;
  const auto t = std::clamp((x - area.left()) / area.width(), 0.0, 1.0);
  return lo * std::pow(hi / lo, t);
}

double ArmadilloView::radiusForY(double y) const {
  const auto area = plane();
  const auto t = std::clamp((area.bottom() - y) / area.height(), 0.0, 1.0);
  return std::min(kMaxDragRadius, 1.0 - std::pow(10.0, -t * kRimDb / 20.0));
}

void ArmadilloView::rebuildMarkers() {
  markers_.clear();
  if (body_ == nullptr) return;
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    const auto geometry =
        trench::core::geometry_from_words(body_->words[corner_][section], sample_rate_hz_);
    const auto place = [this, section](const trench::core::RootPair& pair, bool zero) {
      if (const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&pair)) {
        markers_.push_back({section, zero,
                            QPointF{xForFrequency(conjugate->hz), yForRadius(conjugate->radius)},
                            conjugate->hz, conjugate->radius, false, false});
        return true;
      }
      if (const auto* real = std::get_if<trench::core::RealPair>(&pair)) {
        const auto radius = std::max(std::abs(real->root_a), std::abs(real->root_b));
        const auto positive = real->root_a + real->root_b >= 0.0;
        const auto hz = positive ? sample_rate_hz_ / 2048.0 : sample_rate_hz_ / 2.0;
        markers_.push_back({section, zero, QPointF{xForFrequency(hz), yForRadius(radius)},
                            hz, radius, true, false});
        return true;
      }
      return false;
    };
    const auto pole_live = place(geometry.pole, false);
    if (!place(geometry.zero, true) && pole_live) {
      const auto area = plane();
      markers_.push_back({section, true,
                          QPointF{area.right() - 8.0,
                                  area.top() + 12.0 + static_cast<double>(section) * 11.0},
                          sample_rate_hz_ / 2.0, 0.9, false, true});
    }
  }
}

std::optional<std::size_t> ArmadilloView::hitMarker(const QPointF& at) const {
  std::optional<std::size_t> best;
  double best_distance = kHitRadius;
  for (std::size_t index = 0; index < markers_.size(); ++index) {
    const auto delta = markers_[index].position - at;
    const auto distance = std::hypot(delta.x(), delta.y());
    if (distance <= best_distance) {
      best_distance = distance;
      best = index;
    }
  }
  return best;
}

void ArmadilloView::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kGround);
  const auto area = plane();

  painter.setPen(QPen(kLine, 1.0));
  const QFontMetricsF metrics{letter_font()};
  painter.setFont(letter_font());
  for (const double hz : {100.0, 1000.0, 10000.0}) {
    const auto x = xForFrequency(hz);
    painter.setPen(QPen(kLine, 1.0));
    painter.drawLine(QPointF{x, area.top()}, QPointF{x, area.bottom()});
    painter.setPen(kDim);
    painter.drawText(QPointF{x + 4.0, area.bottom() + 12.0},
                     hz < 1000.0 ? QStringLiteral("100 Hz")
                                 : (hz < 10000.0 ? QStringLiteral("1 kHz")
                                                 : QStringLiteral("10 kHz")));
  }

  painter.setPen(QPen(kRim, 1.4));
  painter.drawLine(QPointF{area.left(), area.top()}, QPointF{area.right(), area.top()});
  painter.setPen(kDim);
  painter.drawText(QPointF{area.left() + 2.0, area.top() - 3.0}, QStringLiteral("RIM"));

  for (const auto& marker : markers_) {
    const bool selected = selected_section_.has_value() &&
                          *selected_section_ == marker.section &&
                          selected_zero_ == marker.zero;
    auto ink = marker.real ? kDim : kInk;
    if (marker.ghost) {
      ink = kDim;
      ink.setAlphaF(0.55F);
    }
    painter.setPen(selected ? kAccent : ink);
    const auto letter = marker.zero ? QStringLiteral("z") : QStringLiteral("p");
    const auto text_width = metrics.horizontalAdvance(letter);
    painter.drawText(QPointF{marker.position.x() - text_width * 0.5,
                             marker.position.y() + metrics.ascent() * 0.35},
                     letter);
    if (selected) {
      painter.setPen(QPen(kAccent, 1.0));
      painter.drawEllipse(marker.position, 7.0, 7.0);
    }
  }
}

void ArmadilloView::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const auto hit = hitMarker(event->position());
  if (!hit.has_value()) return;
  const auto& marker = markers_[*hit];
  drag_ = {marker.section, marker.zero};
  emit rootPressed(marker.section, marker.zero);
}

void ArmadilloView::mouseMoveEvent(QMouseEvent* event) {
  if (!drag_.has_value()) return;
  if (event->position().x() > plane().right() + 6.0) {
    if (drag_->second) {
      emit zeroParked(drag_->first);
    } else {
      emit poleParked(drag_->first);
    }
    return;
  }
  emit rootDragged(drag_->first, drag_->second, frequencyForX(event->position().x()),
                   radiusForY(event->position().y()));
}

void ArmadilloView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !drag_.has_value()) return;
  drag_.reset();
  emit rootReleased();
}

void ArmadilloView::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  if (hitMarker(event->position()).has_value()) return;
  emit placeRequested(frequencyForX(event->position().x()),
                      radiusForY(event->position().y()));
}

void ArmadilloView::resizeEvent(QResizeEvent*) { rebuildMarkers(); }
