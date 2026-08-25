#include "armadillo_view.hpp"

#include "trench/core/p2k.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QToolButton>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <variant>

namespace {

const QColor kGround{16, 20, 24};
const QColor kLine{40, 47, 51};
const QColor kRim{88, 98, 102};
const QColor kInk{174, 186, 190};
const QColor kDim{118, 127, 131};
const QColor kAccent{87, 222, 205};
const QColor kOverlayGuide{174, 186, 190, 44};
const QColor kOverlayPole{174, 186, 190, 148};
const QColor kOverlayZero{247, 184, 92, 210};

constexpr double kRimDb = 96.0;
constexpr double kHitRadius = 9.0;
constexpr double kMaxDragRadius = 0.99998;

struct RootGeometry {
  double hz{};
  double radius{};
};

std::optional<RootGeometry> root_geometry(const trench::core::native::Roots& roots,
                                          double sample_rate_hz) {
  if (const auto* resonant = std::get_if<trench::core::native::Resonant>(&roots)) {
    return RootGeometry{
        resonant->hz,
        std::exp(-std::numbers::pi * resonant->bw_hz / sample_rate_hz)};
  }
  const auto& real = std::get<trench::core::native::RealRoots>(roots);
  if (!std::isfinite(real.a_hz) && !std::isfinite(real.b_hz)) return std::nullopt;
  const auto root = [sample_rate_hz](double decay_hz) {
    if (!std::isfinite(decay_hz)) return 0.0;
    const auto magnitude = std::exp(-2.0 * std::numbers::pi *
                                    std::abs(decay_hz) / sample_rate_hz);
    return std::signbit(decay_hz) ? -magnitude : magnitude;
  };
  const auto a = root(real.a_hz);
  const auto b = root(real.b_hz);
  return RootGeometry{a + b >= 0.0 ? sample_rate_hz / 2048.0
                                   : sample_rate_hz / 2.0,
                      std::max(std::abs(a), std::abs(b))};
}

double rimward_db(double radius) {
  const auto distance = std::clamp(1.0 - radius, std::pow(10.0, -kRimDb / 20.0), 1.0);
  return -20.0 * std::log10(distance);
}

QFont letter_font() { return QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold); }

}  // namespace

ArmadilloView::ArmadilloView(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setAccessibleName(QStringLiteral("Pole-zero authoring plane"));
  setAccessibleDescription(
      QStringLiteral("Pole and zero frequency and bandwidth editor with read-only references"));
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  buildOverlayPicker();
}

QSize ArmadilloView::sizeHint() const { return {720, 260}; }

QSize ArmadilloView::minimumSizeHint() const { return {480, 190}; }

void ArmadilloView::buildOverlayPicker() {
  overlay_menu_ = new QMenu(this);
  overlay_menu_->setFont(QFont(QStringLiteral("Segoe UI"), 8));
  overlay_menu_->setStyleSheet(QStringLiteral(
      "QMenu { background: #1a1f23; color: #aebabe; border: 1px solid #373f43; }"
      "QMenu::item { padding: 2px 16px 2px 12px; }"
      "QMenu::item:selected { background: #373f43; }"));
  connect(overlay_menu_->addAction(QStringLiteral("none")), &QAction::triggered, this,
          [this] { setOverlay(QString()); });

  body_overlay_menu_ = overlay_menu_->addMenu(QStringLiteral("FULL P/Z"));
  body_overlay_menu_->setFont(overlay_menu_->font());
  body_overlay_menu_->setStyleSheet(overlay_menu_->styleSheet());
  body_overlay_menu_->setEnabled(false);

  QMenu* group = nullptr;
  QString type;
  for (const auto& skeleton : trench::core::p2k::templates()) {
    const auto skeleton_type = QString::fromUtf8(skeleton.type.data(),
                                                 static_cast<int>(skeleton.type.size()));
    if (group == nullptr || skeleton_type != type) {
      type = skeleton_type;
      group = overlay_menu_->addMenu(type);
      group->setFont(overlay_menu_->font());
      group->setStyleSheet(overlay_menu_->styleSheet());
    }
    const auto name = QString::fromUtf8(skeleton.name.data(),
                                       static_cast<int>(skeleton.name.size()));
    connect(group->addAction(name), &QAction::triggered, this,
            [this, name] { setOverlay(name); });
  }

  overlay_button_ = new QToolButton(this);
  overlay_button_->setObjectName(QStringLiteral("overlayPicker"));
  overlay_button_->setAccessibleName(QStringLiteral("Pole-zero reference"));
  overlay_button_->setToolTip(
      QStringLiteral("display a read-only pole-zero reference"));
  overlay_button_->setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold));
  overlay_button_->setPopupMode(QToolButton::InstantPopup);
  overlay_button_->setMenu(overlay_menu_);
  overlay_button_->setMinimumWidth(156);
  overlay_button_->setStyleSheet(QStringLiteral(
      "QToolButton { background: #1a1f23; color: #767f83; border: 1px solid #373f43; "
      "border-radius: 2px; padding: 1px 8px; }"
      "QToolButton:hover, QToolButton:focus { color: #aebabe; border-color: #59656a; }"
      "QToolButton::menu-indicator { image: none; }"));
  overlay_button_->setText(QStringLiteral("OVERLAY"));

  auto* tools = new QHBoxLayout(this);
  tools->setContentsMargins(0, 4, 12, 0);
  tools->addStretch(1);
  tools->addWidget(overlay_button_, 0, Qt::AlignTop);
}

void ArmadilloView::addBodyOverlay(const QString& name,
                                   const trench::core::native::Body& body) {
  auto* body_menu = body_overlay_menu_->addMenu(name);
  body_menu->setFont(overlay_menu_->font());
  body_menu->setStyleSheet(overlay_menu_->styleSheet());
  for (std::size_t corner = 0; corner < body.corners.size(); ++corner) {
    const auto full_name = QStringLiteral("%1 · C%2").arg(name).arg(corner + 1);
    corner_overlays_.push_back({full_name, body.corners[corner]});
    connect(body_menu->addAction(QStringLiteral("C%1 · poles + zeros").arg(corner + 1)),
            &QAction::triggered, this, [this, full_name] { setOverlay(full_name); });
  }
  body_overlay_menu_->setEnabled(true);
}

bool ArmadilloView::setOverlay(const QString& name) {
  overlay_roots_.clear();
  overlay_name_.clear();
  bool found = name.isEmpty();
  if (!name.isEmpty()) {
    const auto exact = std::find_if(
        corner_overlays_.begin(), corner_overlays_.end(),
        [&name](const CornerOverlay& candidate) { return candidate.name == name; });
    if (exact != corner_overlays_.end()) {
      overlay_name_ = name;
      for (std::size_t section = 0; section < exact->corner.sections.size(); ++section) {
        const auto& roots = exact->corner.sections[section];
        overlay_roots_.push_back({section, false, roots.pole});
        overlay_roots_.push_back({section, true, roots.zero});
      }
      found = true;
    } else if (const auto* skeleton = trench::core::p2k::posture(name.toStdString())) {
      overlay_name_ = name;
      for (std::size_t index = 0; index < skeleton->pole_count; ++index) {
        overlay_roots_.push_back(
            {skeleton->rows[index], false,
             trench::core::native::Resonant{skeleton->poles[index].hz,
                                             skeleton->poles[index].bw_hz}});
      }
      found = true;
    }
  }
  auto button_text = overlay_name_;
  if (button_text.startsWith(QStringLiteral("P2k "), Qt::CaseInsensitive)) {
    button_text.remove(0, 4);
  }
  overlay_button_->setText(button_text.isEmpty() ? QStringLiteral("OVERLAY") : button_text);
  overlay_button_->setToolTip(
      overlay_name_.isEmpty()
          ? QStringLiteral("display a read-only pole-zero reference")
          : QStringLiteral("%1 · hover a numbered root for exact Hz and bandwidth")
                .arg(overlay_name_));
  overlay_button_->setAccessibleDescription(
      overlay_name_.isEmpty() ? QStringLiteral("No pole-zero reference selected")
                              : QStringLiteral("Read-only overlay %1, %2 poles and %3 zeros")
                                    .arg(overlay_name_)
                                    .arg(overlayPoleCount())
                                    .arg(overlayZeroCount()));
  update();
  return found;
}

QMenu* ArmadilloView::overlayMenu() const noexcept { return overlay_menu_; }

QToolButton* ArmadilloView::overlayPicker() const noexcept { return overlay_button_; }

QString ArmadilloView::overlay() const { return overlay_name_; }

std::size_t ArmadilloView::overlayGhostCount() const noexcept {
  return overlay_roots_.size();
}

std::size_t ArmadilloView::overlayPoleCount() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      overlay_roots_.begin(), overlay_roots_.end(),
      [](const OverlayRoot& root) { return !root.zero; }));
}

std::size_t ArmadilloView::overlayZeroCount() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      overlay_roots_.begin(), overlay_roots_.end(),
      [](const OverlayRoot& root) { return root.zero; }));
}

std::optional<trench::core::native::Roots> ArmadilloView::overlayRoot(
    std::size_t section, bool zero) const {
  const auto found = std::find_if(
      overlay_roots_.begin(), overlay_roots_.end(),
      [section, zero](const OverlayRoot& root) {
        return root.section == section && root.zero == zero;
      });
  return found == overlay_roots_.end()
             ? std::nullopt
             : std::optional<trench::core::native::Roots>{found->roots};
}

void ArmadilloView::setBody(const trench::core::native::Body* body,
                            double sample_rate_hz) {
  if (body_ == body && sample_rate_hz_ == sample_rate_hz) return;
  body_ = body;
  sample_rate_hz_ = sample_rate_hz;
  refresh();
}

void ArmadilloView::setCorner(std::size_t corner) {
  if (corner_ == corner) return;
  corner_ = corner;
  refresh();
}

void ArmadilloView::setSelected(std::optional<std::size_t> section, bool zero) {
  if (selected_section_ == section && selected_zero_ == zero) return;
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
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    const auto& authored = body_->corners[corner_].sections[section];
    const auto place = [this, section](const trench::core::native::Roots& pair, bool zero) {
      if (const auto* conjugate = std::get_if<trench::core::native::Resonant>(&pair)) {
        const auto radius =
            std::exp(-std::numbers::pi * conjugate->bw_hz / sample_rate_hz_);
        markers_.push_back({section, zero,
                            QPointF{xForFrequency(conjugate->hz), yForRadius(radius)},
                            conjugate->hz, radius, false, false});
        return true;
      }
      if (const auto* real = std::get_if<trench::core::native::RealRoots>(&pair)) {
        if (!std::isfinite(real->a_hz) && !std::isfinite(real->b_hz)) return false;
        const auto root = [this](double decay_hz) {
          if (!std::isfinite(decay_hz)) return 0.0;
          const double magnitude = std::exp(-2.0 * std::numbers::pi *
                                            std::abs(decay_hz) / sample_rate_hz_);
          return std::signbit(decay_hz) ? -magnitude : magnitude;
        };
        const auto a = root(real->a_hz);
        const auto b = root(real->b_hz);
        const auto radius = std::max(std::abs(a), std::abs(b));
        const auto positive = a + b >= 0.0;
        const auto hz = positive ? sample_rate_hz_ / 2048.0 : sample_rate_hz_ / 2.0;
        markers_.push_back({section, zero, QPointF{xForFrequency(hz), yForRadius(radius)},
                            hz, radius, true, false});
        return true;
      }
      return false;
    };
    const auto pole_live = place(authored.pole, false);
    if (!place(authored.zero, true) && pole_live) {
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
  painter.drawText(QPointF{area.left() + 2.0, area.top() - 3.0}, QStringLiteral("R → 1"));

  paintOverlay(painter);

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

void ArmadilloView::paintOverlay(QPainter& painter) const {
  if (overlay_roots_.empty()) return;
  const auto area = plane();
  const QFontMetricsF metrics{letter_font()};
  painter.save();
  painter.setFont(letter_font());
  for (const auto& marker : overlay_roots_) {
    const auto geometry = root_geometry(marker.roots, sample_rate_hz_);
    if (!geometry.has_value()) continue;
    const QPointF at{xForFrequency(geometry->hz), yForRadius(geometry->radius)};
    auto guide = marker.zero ? kOverlayZero : kOverlayGuide;
    guide.setAlpha(marker.zero ? 72 : 32);
    painter.setPen(QPen(guide, marker.zero ? 1.15 : 1.0, Qt::DashLine));
    painter.drawLine(QPointF{at.x(), area.top()}, QPointF{at.x(), area.bottom()});
    const auto ink = marker.zero ? kOverlayZero : kOverlayPole;
    painter.setPen(QPen(ink, marker.zero ? 1.4 : 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(at, 7.0, 7.0);
    if (marker.zero) {
      painter.drawLine(at + QPointF{-4.0, -4.0}, at + QPointF{4.0, 4.0});
      painter.drawLine(at + QPointF{-4.0, 4.0}, at + QPointF{4.0, -4.0});
    }
    const auto label = QStringLiteral("%1%2")
                           .arg(marker.zero ? QLatin1Char('z') : QLatin1Char('p'))
                           .arg(marker.section + 1);
    const auto text_width = metrics.horizontalAdvance(label);
    const auto label_y = at.y() < area.top() + 18.0
                             ? at.y() + metrics.ascent() + 10.0
                             : at.y() - 9.0;
    painter.drawText(QPointF{at.x() - text_width * 0.5,
                             label_y},
                     label);
  }
  painter.restore();
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
  if (!drag_.has_value()) {
    for (const auto& marker : overlay_roots_) {
      const auto geometry = root_geometry(marker.roots, sample_rate_hz_);
      if (!geometry.has_value()) continue;
      const QPointF at{xForFrequency(geometry->hz), yForRadius(geometry->radius)};
      if (std::hypot(at.x() - event->position().x(), at.y() - event->position().y()) >
          kHitRadius) {
        continue;
      }
      const auto root_name = QStringLiteral("%1%2")
                                 .arg(marker.zero ? QLatin1Char('z') : QLatin1Char('p'))
                                 .arg(marker.section + 1);
      QString detail;
      if (const auto* resonant =
              std::get_if<trench::core::native::Resonant>(&marker.roots)) {
        detail = QStringLiteral("%1 Hz · ΔF %2 Hz")
                     .arg(resonant->hz, 0, 'f', 2)
                     .arg(resonant->bw_hz, 0, 'f', 2);
      } else {
        const auto& real = std::get<trench::core::native::RealRoots>(marker.roots);
        detail = QStringLiteral("real roots · %1 Hz / %2 Hz")
                     .arg(real.a_hz, 0, 'f', 2)
                     .arg(real.b_hz, 0, 'f', 2);
      }
      QToolTip::showText(event->globalPosition().toPoint(),
                         QStringLiteral("%1 · %2 · %3")
                             .arg(overlay_name_, root_name, detail),
                         this);
      return;
    }
    QToolTip::hideText();
    return;
  }
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
