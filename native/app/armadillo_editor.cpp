#include "armadillo_editor.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QShortcut>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace {

using Resonant = trench::core::native::Resonant;

constexpr QColor kChassis{237, 235, 230};
constexpr QColor kCard{30, 34, 38};
constexpr QColor kHairline{50, 55, 59};
constexpr QColor kGrid{52, 58, 63};
constexpr QColor kText{139, 139, 132};
constexpr QColor kInk{210, 207, 198};
constexpr QColor kAccent{196, 103, 79};
constexpr QColor kOverlay{184, 134, 46};

constexpr double kMinDragBandwidthHz = 4.0;
constexpr double kLowAngle =
    2.0 * std::numbers::pi * EditorState::kLowHz / EditorState::kDatumHz;
constexpr double kHighAngle = std::numbers::pi;
constexpr double kAngleFloor = kLowAngle * (1.0 - 1.0e-9);
const double kMaxRadius = std::exp(
    -std::numbers::pi * EditorState::kMinBandwidthHz / EditorState::kDatumHz);
const double kMinRadius = std::exp(
    -std::numbers::pi * EditorState::kMaxBandwidthHz / EditorState::kDatumHz);

double frequencyForAngle(double angle) {
  return angle / (2.0 * std::numbers::pi) * EditorState::kDatumHz;
}

double bandwidthForRadius(double radius) {
  return -std::log(radius) * EditorState::kDatumHz / std::numbers::pi;
}

const Resonant& rootOf(const EditorState& state, std::size_t section,
                       EditorState::Lane lane) {
  const auto& value = state.section(section);
  return std::get<Resonant>(lane == EditorState::Lane::kPole ? value.pole
                                                             : value.zero);
}

QString shortValue(double value) {
  if (value >= 1'000.0) {
    return QStringLiteral("%1k").arg(value / 1'000.0, 0, 'g', 3);
  }
  return QString::number(value, 'g', 4);
}

QColor faded(QColor color, int alpha) {
  color.setAlpha(alpha);
  return color;
}

}  // namespace

ArmadilloEditor::ArmadilloEditor(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setMinimumHeight(230);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  setCursor(Qt::CrossCursor);
  setFocusPolicy(Qt::ClickFocus);
  setAccessibleName(QStringLiteral("ARMAdillo frequency and bandwidth root editor"));
  connect(state_, &EditorState::changed, this,
          qOverload<>(&ArmadilloEditor::update));
  connect(state_, &EditorState::selectionChanged, this,
          [this] { update(); });
  auto* drop_group = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(drop_group, &QShortcut::activated, this, [this] {
    groupPrimaryOnly();
    update();
  });
  for (const auto key : {Qt::Key_Delete, Qt::Key_Backspace}) {
    auto* drop_zero = new QShortcut(QKeySequence(key), this);
    drop_zero->setContext(Qt::WidgetShortcut);
    connect(drop_zero, &QShortcut::activated, state_, &EditorState::removeZero);
  }
}

QRectF ArmadilloEditor::field() const {
  return QRectF(rect()).adjusted(62.0, 16.0, -22.0, -44.0);
}

QPointF ArmadilloEditor::discCentre() const {
  return QRectF(rect()).center();
}

double ArmadilloEditor::discRadius() const {
  return std::min(width() * 0.5, height() * 0.5) - 22.0;
}

QPointF ArmadilloEditor::pointFor(double frequency_hz,
                                  double bandwidth_hz) const {
  if (projection_ == Projection::kZPlane) {
    const double radius =
        std::exp(-std::numbers::pi * bandwidth_hz / EditorState::kDatumHz);
    const double angle =
        2.0 * std::numbers::pi * frequency_hz / EditorState::kDatumHz;
    const double scale = discRadius();
    return discCentre() + QPointF{radius * std::cos(angle) * scale,
                                  -radius * std::sin(angle) * scale};
  }
  const QRectF bounds = field();
  const double x_fraction =
      std::log(frequency_hz / EditorState::kLowHz) /
      std::log(EditorState::kHighHz / EditorState::kLowHz);
  const double y_fraction =
      std::log(bandwidth_hz / EditorState::kMinBandwidthHz) /
      std::log(EditorState::kMaxBandwidthHz / EditorState::kMinBandwidthHz);
  return {bounds.left() + std::clamp(x_fraction, 0.0, 1.0) * bounds.width(),
          bounds.top() + std::clamp(y_fraction, 0.0, 1.0) * bounds.height()};
}

std::optional<std::pair<double, double>> ArmadilloEditor::placementAt(
    const QPointF& position) const {
  if (projection_ != Projection::kZPlane) return dragTargetAt(position);
  const QPointF offset = (position - discCentre()) / discRadius();
  const double radius = std::hypot(offset.x(), offset.y());
  const double angle = std::atan2(0.0 - offset.y(), offset.x());
  if (radius > 1.0 || angle < kAngleFloor || angle > kHighAngle) {
    return std::nullopt;
  }
  return std::pair<double, double>{
      frequencyForAngle(angle),
      bandwidthForRadius(std::clamp(radius, kMinRadius, kMaxRadius))};
}

std::pair<double, double> ArmadilloEditor::dragTargetAt(
    const QPointF& position) const {
  if (projection_ == Projection::kZPlane) {
    const QPointF offset = (position - discCentre()) / discRadius();
    const double angle =
        std::clamp(std::atan2(std::abs(offset.y()), offset.x()), kLowAngle,
                   kHighAngle);
    const double radius = std::clamp(std::hypot(offset.x(), offset.y()),
                                     kMinRadius, kMaxRadius);
    return {frequencyForAngle(angle), bandwidthForRadius(radius)};
  }
  const QRectF bounds = field();
  const double x_fraction = std::clamp(
      (position.x() - bounds.left()) / bounds.width(), 0.0, 1.0);
  const double y_fraction = std::clamp(
      (position.y() - bounds.top()) / bounds.height(), 0.0, 1.0);
  return {EditorState::kLowHz *
              std::pow(EditorState::kHighHz / EditorState::kLowHz, x_fraction),
          EditorState::kMinBandwidthHz *
              std::pow(
                  EditorState::kMaxBandwidthHz / EditorState::kMinBandwidthHz,
                  y_fraction)};
}

// THE WHOLE CORNER IS ON THE PLANE (Tyson 2026-08-28 "direct armadillo
// editor"): every live root of the editing corner is drawn and grabbable, so
// what is seen is what is picked.
std::vector<ArmadilloEditor::Handle> ArmadilloEditor::handles() const {
  std::vector<Handle> result;
  result.reserve(2 * trench::core::native::kSections);
  for (std::size_t section = 0; section < trench::core::native::kSections;
       ++section) {
    if (!state_->sectionEnabled(section)) continue;
    const auto& pole = rootOf(*state_, section, EditorState::Lane::kPole);
    result.push_back(Handle{section, EditorState::Lane::kPole,
                            pointFor(pole.hz, pole.bw_hz)});
    if (!state_->rootPresent(section, EditorState::Lane::kZero)) continue;
    const auto& zero = rootOf(*state_, section, EditorState::Lane::kZero);
    result.push_back(
        Handle{section, EditorState::Lane::kZero,
               pointFor(zero.hz, zero.bw_hz)});
  }
  return result;
}

// THE OVERLAY LIVES WHERE THE EDITING LIVES (Tyson 2026-08-29): the compared
// posture is drawn on the same plane, under the live roots, and cannot be
// grabbed - it is a target to move onto, not a thing to move.
void ArmadilloEditor::setGhost(std::vector<std::pair<double, double>> poles) {
  ghost_ = std::move(poles);
  update();
}

void ArmadilloEditor::clearGhost() {
  ghost_.clear();
  update();
}

void ArmadilloEditor::setProjection(Projection projection) {
  if (projection_ == projection) return;
  projection_ = projection;
  update();
}

std::optional<ArmadilloEditor::Handle> ArmadilloEditor::hitHandle(
    const QPointF& position) const {
  std::optional<Handle> closest;
  double closest_distance = 14.0;
  for (const Handle& handle : handles()) {
    const double distance = QLineF{position, handle.position}.length();
    if (distance <= closest_distance) {
      closest = handle;
      closest_distance = distance;
    }
  }
  return closest;
}

void ArmadilloEditor::paintEvent(QPaintEvent*) {
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
  const QRectF bounds = field();
  const bool z_plane = projection_ == Projection::kZPlane;

  QFont scale_font = painter.font();
  scale_font.setPixelSize(9);
  scale_font.setWeight(QFont::Normal);
  painter.setFont(scale_font);

  painter.setPen(QPen(kGrid, 1.0));
  if (z_plane) {
    const QPointF centre = discCentre();
    const double scale = discRadius();
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(centre, scale, scale);
    painter.drawLine(QPointF{centre.x() - scale, centre.y()},
                     QPointF{centre.x() + scale, centre.y()});
    painter.drawLine(QPointF{centre.x(), centre.y() - scale},
                     QPointF{centre.x(), centre.y() + scale});
    painter.setPen(kText);
    painter.drawText(
        QRectF{centre.x() + scale + 5.0, centre.y() - 9.0, 24.0, 18.0},
        Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Re"));
    painter.drawText(
        QRectF{centre.x() - 12.0, centre.y() - scale - 20.0, 24.0, 16.0},
        Qt::AlignHCenter | Qt::AlignBottom, QStringLiteral("Im"));
  } else {
    constexpr std::array<double, 10> frequency_lines{
        20.0, 50.0, 100.0, 200.0, 500.0, 1'000.0, 2'000.0,
        5'000.0, 10'000.0, EditorState::kNyquistHz};
    for (const double hz : frequency_lines) {
      const double x = pointFor(hz, EditorState::kMinBandwidthHz).x();
      painter.drawLine(QPointF{x, bounds.top()}, QPointF{x, bounds.bottom()});
      painter.setPen(kText);
      painter.drawText(
          QRectF{x - 26.0, bounds.bottom() + 8.0, 52.0, 17.0},
          Qt::AlignHCenter | Qt::AlignTop,
          hz == EditorState::kNyquistHz ? QStringLiteral("NYQ")
                                        : shortValue(hz));
      painter.setPen(QPen(kGrid, 1.0));
    }

    constexpr std::array<double, 6> bandwidth_lines{
        1.0, 10.0, 100.0, 1'000.0, 10'000.0, 20'000.0};
    for (const double bandwidth : bandwidth_lines) {
      const double y = pointFor(EditorState::kLowHz, bandwidth).y();
      painter.drawLine(QPointF{bounds.left(), y}, QPointF{bounds.right(), y});
      painter.setPen(kText);
      painter.drawText(QRectF{8.0, y - 9.0, 46.0, 18.0},
                       Qt::AlignRight | Qt::AlignVCenter,
                       shortValue(bandwidth));
      painter.setPen(QPen(kGrid, 1.0));
    }
  }

  painter.setBrush(Qt::NoBrush);

  painter.setPen(kText);
  QFont axis_font = painter.font();
  axis_font.setCapitalization(QFont::AllUppercase);
  axis_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  axis_font.setWeight(QFont::DemiBold);
  axis_font.setPixelSize(10);
  if (!z_plane) {
    painter.setFont(axis_font);
    painter.drawText(QRectF{bounds.left(), bounds.bottom() + 25.0,
                            bounds.width(), 13.0},
                     Qt::AlignCenter, QStringLiteral("frequency"));
    painter.save();
    painter.translate(15.0, bounds.center().y());
    painter.rotate(-90.0);
    painter.drawText(QRectF{-bounds.height() * 0.5, -8.0,
                            bounds.height(), 16.0},
                     Qt::AlignCenter, QStringLiteral("bandwidth"));
    painter.restore();
  }

  painter.setPen(QPen(faded(kOverlay, 165), 1.1));
  painter.setBrush(Qt::NoBrush);
  for (const auto& ghost : ghost_) {
    painter.drawEllipse(pointFor(ghost.first, ghost.second), 4.0, 4.0);
  }

  QFont marker_font = painter.font();
  marker_font.setCapitalization(QFont::MixedCase);
  marker_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
  marker_font.setWeight(QFont::DemiBold);
  marker_font.setPixelSize(9);
  // DORMANT INK (Tyson 2026-08-29 "why are they on the plot if theres no
  // current curve"): a bell whose pole still sits on its zero claims no gain,
  // so its marks go faint until the pair separates.
  const auto dormant = [this](std::size_t section) {
    if (!state_->rootPresent(section, EditorState::Lane::kZero)) return false;
    const auto& pole = rootOf(*state_, section, EditorState::Lane::kPole);
    const auto& zero = rootOf(*state_, section, EditorState::Lane::kZero);
    return std::abs(std::log2(pole.hz / zero.hz)) < 0.01 &&
           std::abs(std::log2(pole.bw_hz / zero.bw_hz)) < 0.01;
  };
  const auto drawMark = [&](EditorState::Lane lane, const QPointF& at,
                            bool addressed, const QColor& color,
                            double radius) {
    if (z_plane) {
      painter.setPen(QPen(color, addressed ? 1.6 : 1.2));
      painter.setBrush(Qt::NoBrush);
      if (lane == EditorState::Lane::kPole) {
        constexpr double arm = 3.5;
        painter.drawLine(at + QPointF{-arm, -arm}, at + QPointF{arm, arm});
        painter.drawLine(at + QPointF{-arm, arm}, at + QPointF{arm, -arm});
      } else {
        painter.drawEllipse(at, radius, radius);
      }
      return;
    }
    painter.setPen(addressed ? QPen(Qt::NoPen) : QPen(color, 1.2));
    painter.setBrush(addressed ? QBrush(kAccent) : QBrush(Qt::NoBrush));
    if (lane == EditorState::Lane::kPole) {
      painter.drawEllipse(at, radius, radius);
    } else {
      QPainterPath diamond;
      diamond.moveTo(at + QPointF{0.0, -radius});
      diamond.lineTo(at + QPointF{radius, 0.0});
      diamond.lineTo(at + QPointF{0.0, radius});
      diamond.lineTo(at + QPointF{-radius, 0.0});
      diamond.closeSubpath();
      painter.drawPath(diamond);
    }
  };

  for (const Handle& handle : handles()) {
    const bool addressed = handle.section == state_->selectedSection();
    const bool selected_root =
        addressed && handle.lane == state_->selectedLane();
    const QColor color = addressed ? kAccent
                                   : faded(kInk, dormant(handle.section) ? 70 : 255);
    if (selected_root && drag_) {
      painter.setPen(QPen(faded(kAccent, 90), 1.0));
      painter.drawLine(QPointF{handle.position.x(), bounds.top()},
                       QPointF{handle.position.x(), bounds.bottom()});
      painter.drawLine(QPointF{bounds.left(), handle.position.y()},
                       QPointF{bounds.right(), handle.position.y()});
    }

    const double radius = addressed ? 6.5 : 4.5;
    if (z_plane) {
      drawMark(handle.lane,
               QPointF{handle.position.x(),
                       2.0 * discCentre().y() - handle.position.y()},
               false, faded(color, color.alpha() * 45 / 100), radius);
    }
    drawMark(handle.lane, handle.position, addressed, color, radius);

    painter.setBrush(Qt::NoBrush);
    if (group_.count(Key{handle.section, handle.lane}) > 0) {
      painter.setPen(QPen(kAccent, 1.2));
      painter.drawEllipse(handle.position, radius + 3.0, radius + 3.0);
    }

    painter.setPen(color);
    painter.setFont(marker_font);
    const QString tag = handle.lane == EditorState::Lane::kPole
                            ? QStringLiteral("P")
                            : QStringLiteral("Z");
    const bool parked = handle.position.x() > bounds.right() - 24.0;
    const double label_y = handle.position.y() - 9.0;
    const double label_gap = radius + 4.0;
    const QRectF label_bounds =
        parked
            ? QRectF{handle.position.x() - label_gap - 16.0, label_y, 16.0, 18.0}
            : QRectF{handle.position.x() + label_gap, label_y, 16.0, 18.0};
    painter.drawText(label_bounds,
                     (parked ? Qt::AlignRight : Qt::AlignLeft) |
                         Qt::AlignVCenter,
                     tag);
  }

  if (!state_->sectionEnabled(state_->selectedSection())) {
    painter.setFont(axis_font);
    painter.setPen(kText);
    painter.drawText(bounds, Qt::AlignCenter,
                     QStringLiteral("SECTION OFF"));
  }
}

void ArmadilloEditor::groupPrimaryOnly() {
  group_.clear();
  group_.insert(Key{state_->selectedSection(), state_->selectedLane()});
}

// A GROUP MOVES AS ONE (Tyson 2026-08-28 "ctrl click to select multiple and
// drag multiple"): every member is carried by the same log-plane delta, so the
// shape held between the roots survives the drag.
void ArmadilloEditor::beginDrag(const QPointF& position) {
  state_->beginUndoGroup();
  members_.clear();
  for (const Key& key : group_) {
    if (!state_->sectionEnabled(key.first)) continue;
    if (!state_->rootPresent(key.first, key.second)) continue;
    const auto& root = rootOf(*state_, key.first, key.second);
    members_.push_back(Member{key.first, key.second, root.hz, root.bw_hz});
  }
  const auto press = dragTargetAt(position);
  press_hz_ = press.first;
  press_bw_hz_ = press.second;
}

void ArmadilloEditor::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const auto hit = hitHandle(event->position());
  if (!hit) {
    drag_.reset();
    groupPrimaryOnly();
    update();
    return;
  }

  const Key key{hit->section, hit->lane};
  if (event->modifiers().testFlag(Qt::ControlModifier)) {
    if (group_.count(key) > 0 && group_.size() > 1) {
      group_.erase(key);
    } else {
      group_.insert(key);
    }
  } else {
    group_.clear();
    group_.insert(key);
  }
  state_->selectRoot(hit->section, hit->lane);

  if (group_.count(key) > 0) {
    drag_ = hit;
    beginDrag(event->position());
    grabMouse();
  } else {
    drag_.reset();
  }
  update();
}

void ArmadilloEditor::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || hitHandle(event->position())) return;
  const auto placement = placementAt(event->position());
  if (!placement) return;
  state_->addZeroAt(placement->first,
                    std::max(kMinDragBandwidthHz, placement->second));
}

void ArmadilloEditor::mouseMoveEvent(QMouseEvent* event) {
  if (drag_) {
    applyPointer(event->position());
    return;
  }
  const auto hit = hitHandle(event->position());
  setCursor(hit ? Qt::OpenHandCursor : Qt::CrossCursor);
  if (hit) {
    const auto& root = rootOf(*state_, hit->section, hit->lane);
    QToolTip::showText(
        event->globalPosition().toPoint(),
        QStringLiteral("S%1 %2  ·  %3 Hz  ·  %4 Hz BW")
            .arg(hit->section + 1)
            .arg(hit->lane == EditorState::Lane::kPole ? QStringLiteral("POLE")
                                                       : QStringLiteral("ZERO"))
            .arg(root.hz, 0, 'f', 2)
            .arg(root.bw_hz, 0, 'f', 2),
        this);
  }
}

void ArmadilloEditor::mouseReleaseEvent(QMouseEvent* event) {
  if (!drag_ || event->button() != Qt::LeftButton) return;
  applyPointer(event->position());
  state_->endUndoGroup();
  drag_.reset();
  members_.clear();
  releaseMouse();
  setCursor(Qt::CrossCursor);
}

void ArmadilloEditor::applyPointer(const QPointF& position) {
  if (!drag_) return;
  const auto now = dragTargetAt(position);
  const double frequency_ratio = now.first / press_hz_;
  const double bandwidth_ratio = now.second / press_bw_hz_;
  for (const Member& member : members_) {
    state_->setRoot(
        member.section, member.lane, member.hz * frequency_ratio,
        std::max(kMinDragBandwidthHz, member.bw_hz * bandwidth_ratio));
  }
}
