#include "armadillo_view.hpp"

#include "frequency_axis.hpp"

#include "trench/core/formants.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/transpose.hpp"

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
const QColor kTransposed{116, 174, 255};
const QColor kLpc{156, 130, 224};

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
  setAccessibleName(QStringLiteral("Pole and zero plot"));
  setAccessibleDescription(
      QStringLiteral("Edit pole and zero frequency and bandwidth; references are read-only"));
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

  body_overlay_menu_ = overlay_menu_->addMenu(QStringLiteral("POLES + ZEROS"));
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
  overlay_button_->setAccessibleName(QStringLiteral("Reference poles and zeros"));
  overlay_button_->setToolTip(QStringLiteral("read-only reference"));
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
    connect(body_menu->addAction(QStringLiteral("C%1").arg(corner + 1)),
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
    button_text.replace(0, 3, QStringLiteral("P2K"));
  }
  overlay_button_->setText(button_text.isEmpty() ? QStringLiteral("OVERLAY") : button_text);
  overlay_button_->setToolTip(
      overlay_name_.isEmpty()
          ? QStringLiteral("read-only reference")
          : QStringLiteral("%1 · reference · hover for Hz and BW")
                .arg(overlay_name_));
  overlay_button_->setAccessibleDescription(
      overlay_name_.isEmpty() ? QStringLiteral("No pole and zero reference selected")
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

double ArmadilloView::transposedHz(double hz, double radius, bool zero) const {
  return trench::core::transposed_root_hz(
      hz, radius, trench::core::ratio_of_semitones(transpose_semitones_),
      sample_rate_hz_, zero);
}

std::optional<QPointF> ArmadilloView::transposedPosition(std::size_t section,
                                                        bool zero) const {
  if (transpose_semitones_ == 0.0) return std::nullopt;
  const auto found = std::find_if(markers_.begin(), markers_.end(),
                                  [section, zero](const Marker& marker) {
                                    return marker.section == section && marker.zero == zero &&
                                           !marker.real && !marker.ghost;
                                  });
  if (found == markers_.end()) return std::nullopt;
  const double moved_hz = transposedHz(found->hz, found->radius, found->zero);
  if (moved_hz == found->hz) return std::nullopt;
  return QPointF{xForFrequency(moved_hz), found->position.y()};
}

std::size_t ArmadilloView::lpcFormantCount() const noexcept {
  return lpc_formants_hz_.size();
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

void ArmadilloView::setTranspose(double semitones) {
  if (transpose_semitones_ == semitones) return;
  transpose_semitones_ = semitones;
  update();
}

void ArmadilloView::setLpcFormants(const QString& source, std::vector<double> hz) {
  const double low = trench::app::frequency_axis::low_hz(sample_rate_hz_);
  const double high = trench::app::frequency_axis::high_hz(sample_rate_hz_);
  std::erase_if(hz, [low, high](double value) {
    return !std::isfinite(value) || value < low || value > high;
  });
  std::sort(hz.begin(), hz.end());
  hz.erase(std::unique(hz.begin(), hz.end()), hz.end());
  if (lpc_source_ == source && lpc_formants_hz_ == hz) return;
  lpc_source_ = source;
  lpc_formants_hz_ = std::move(hz);
  update();
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
  return QRectF(54.0, 14.0, std::max(40.0, width() - 72.0),
                std::max(30.0, height() - 30.0));
}

double ArmadilloView::xForFrequency(double hz) const {
  const auto area = plane();
  return area.left() + area.width() *
                           trench::app::frequency_axis::fraction(hz, sample_rate_hz_);
}

double ArmadilloView::yForRadius(double radius) const {
  const auto area = plane();
  return area.bottom() - area.height() * std::clamp(rimward_db(radius) / kRimDb, 0.0, 1.0);
}

double ArmadilloView::frequencyForX(double x) const {
  const auto area = plane();
  const auto t = std::clamp((x - area.left()) / area.width(), 0.0, 1.0);
  return trench::app::frequency_axis::hz(t, sample_rate_hz_);
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
  painter.drawText(QPointF{area.left() + 2.0, area.top() - 3.0},
                   QStringLiteral("NARROW BW"));

  paintLpcFormants(painter);
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
    const auto label = QStringLiteral("%1%2")
                           .arg(marker.zero ? QLatin1Char('z') : QLatin1Char('p'))
                           .arg(marker.section + 1);
    painter.setPen(selected ? kAccent : ink);
    const auto text_width = metrics.horizontalAdvance(label);
    painter.drawText(QPointF{marker.position.x() - text_width * 0.5,
                             marker.position.y() + metrics.ascent() * 0.35},
                     label);
    const auto moved = transposedPosition(marker.section, marker.zero);
    if (moved) {
      const auto moved_label = label + QStringLiteral("′");
      painter.setPen(kTransposed);
      painter.drawText(QPointF{moved->x() - metrics.horizontalAdvance(moved_label) * 0.5,
                               moved->y() + metrics.ascent() * 0.35},
                       moved_label);
    }
  }
}

void ArmadilloView::paintLpcFormants(QPainter& painter) const {
  if (lpc_formants_hz_.empty()) return;
  const auto area = plane();
  painter.save();
  painter.setFont(letter_font());
  for (std::size_t index = 0; index < lpc_formants_hz_.size(); ++index) {
    const auto x = xForFrequency(lpc_formants_hz_[index]);
    auto guide = kLpc;
    guide.setAlpha(78);
    painter.setPen(QPen(guide, 1.0, Qt::DashDotLine));
    painter.drawLine(QPointF{x, area.top()}, QPointF{x, area.bottom()});
    painter.setPen(kLpc);
    painter.drawText(QPointF{x + 3.0, area.top() + 14.0},
                     QStringLiteral("F%1").arg(index + 1));
  }
  painter.restore();
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
    const auto label = QStringLiteral("%1%2")
                           .arg(marker.zero ? QLatin1Char('Z') : QLatin1Char('P'))
                           .arg(marker.section + 1);
    const auto text_width = metrics.horizontalAdvance(label);
    const auto label_y = at.y() < area.top() + 18.0
                             ? at.y() + metrics.ascent() + 10.0
                             : at.y() - 9.0;
    painter.drawText(QPointF{at.x() - text_width * 0.5,
                             label_y},
                     label);
    if (transpose_semitones_ != 0.0) {
      const double moved_hz = transposedHz(geometry->hz, geometry->radius, marker.zero);
      if (moved_hz != geometry->hz) {
        const auto moved_label = label + QStringLiteral("′");
        const auto moved_x = xForFrequency(moved_hz);
        painter.setPen(kTransposed);
        painter.drawText(QPointF{moved_x - metrics.horizontalAdvance(moved_label) * 0.5,
                                 label_y},
                         moved_label);
      }
    }
  }
  painter.restore();
}

void ArmadilloView::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const auto hit = hitMarker(event->position());
  if (!hit.has_value()) return;
  const auto& marker = markers_[*hit];
  drag_ = {marker.section, marker.zero};
  drag_hz_ = marker.hz;
  emit rootPressed(marker.section, marker.zero);
}

void ArmadilloView::mouseMoveEvent(QMouseEvent* event) {
  if (!drag_.has_value()) {
    for (std::size_t index = 0; index < lpc_formants_hz_.size(); ++index) {
      if (std::abs(xForFrequency(lpc_formants_hz_[index]) - event->position().x()) >
          kHitRadius) {
        continue;
      }
      QToolTip::showText(event->globalPosition().toPoint(),
                         QStringLiteral("%1 · LPC F%2 · %3 Hz")
                             .arg(lpc_source_)
                             .arg(index + 1)
                             .arg(lpc_formants_hz_[index], 0, 'f', 1),
                         this);
      return;
    }
    for (const auto& marker : overlay_roots_) {
      const auto geometry = root_geometry(marker.roots, sample_rate_hz_);
      if (!geometry.has_value()) continue;
      const QPointF at{xForFrequency(geometry->hz), yForRadius(geometry->radius)};
      if (std::hypot(at.x() - event->position().x(), at.y() - event->position().y()) >
          kHitRadius) {
        continue;
      }
      const auto root_name = marker.zero
                                 ? QStringLiteral("reference zero z%1").arg(marker.section + 1)
                                 : QStringLiteral("reference pole p%1").arg(marker.section + 1);
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
  const double hz = event->modifiers().testFlag(Qt::ShiftModifier)
                        ? drag_hz_
                        : frequencyForX(event->position().x());
  emit rootDragged(drag_->first, drag_->second, hz, radiusForY(event->position().y()));
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
