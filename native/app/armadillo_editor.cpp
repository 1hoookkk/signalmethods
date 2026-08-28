#include "armadillo_editor.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {

using Resonant = trench::core::native::Resonant;

constexpr std::array<QColor, trench::core::native::kSections> kSectionColors{
    QColor{66, 224, 207}, QColor{231, 158, 76}, QColor{226, 210, 90},
    QColor{224, 99, 151}, QColor{92, 170, 238}, QColor{155, 213, 96}};

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
  setMinimumHeight(290);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  setCursor(Qt::CrossCursor);
  setAccessibleName(QStringLiteral("ARMAdillo frequency and bandwidth root editor"));
  connect(state_, &EditorState::changed, this,
          qOverload<>(&ArmadilloEditor::update));
  connect(state_, &EditorState::selectionChanged, this,
          [this] { update(); });
}

QRectF ArmadilloEditor::field() const {
  return QRectF(rect()).adjusted(68.0, 48.0, -24.0, -38.0);
}

QPointF ArmadilloEditor::pointFor(double frequency_hz,
                                  double bandwidth_hz) const {
  const QRectF bounds = field();
  const double x_fraction =
      std::log(frequency_hz / EditorState::kLowHz) /
      std::log(EditorState::kHighHz / EditorState::kLowHz);
  const double y_fraction =
      std::log(bandwidth_hz / EditorState::kMinBandwidthHz) /
      std::log(EditorState::kMaxBandwidthHz / EditorState::kMinBandwidthHz);
  return {bounds.left() + std::clamp(x_fraction, 0.0, 1.0) * bounds.width(),
          bounds.bottom() - std::clamp(y_fraction, 0.0, 1.0) * bounds.height()};
}

std::vector<ArmadilloEditor::Handle> ArmadilloEditor::handles() const {
  std::vector<Handle> result;
  result.reserve(state_->activeSections() * 2);
  for (std::size_t section = 0; section < state_->activeSections();
       ++section) {
    const auto& pole = rootOf(*state_, section, EditorState::Lane::kPole);
    const auto& zero = rootOf(*state_, section, EditorState::Lane::kZero);
    QPointF pole_point = pointFor(pole.hz, pole.bw_hz);
    QPointF zero_point = pointFor(zero.hz, zero.bw_hz);
    if (QLineF{pole_point, zero_point}.length() < 15.0) {
      if (pole_point.x() > field().right() - 12.0) {
        pole_point.ry() -= 5.5;
        zero_point.ry() += 5.5;
      } else {
        pole_point.rx() -= 5.5;
        zero_point.rx() += 5.5;
      }
    }
    result.push_back(
        Handle{section, EditorState::Lane::kPole, pole_point});
    result.push_back(
        Handle{section, EditorState::Lane::kZero, zero_point});
  }
  return result;
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
  painter.fillRect(rect(), QColor{9, 12, 14});
  const QRectF bounds = field();
  painter.fillRect(bounds, QColor{15, 20, 22});

  constexpr std::array<double, 10> frequency_lines{
      20.0, 50.0, 100.0, 200.0, 500.0, 1'000.0, 2'000.0,
      5'000.0, 10'000.0, EditorState::kNyquistHz};
  painter.setPen(QPen(QColor{43, 53, 56}, 1.0));
  for (const double hz : frequency_lines) {
    const double x = pointFor(hz, EditorState::kMinBandwidthHz).x();
    painter.drawLine(QPointF{x, bounds.top()}, QPointF{x, bounds.bottom()});
    painter.setPen(QColor{112, 127, 130});
    painter.drawText(
        QRectF{x - 26.0, bounds.bottom() + 8.0, 52.0, 17.0},
        Qt::AlignHCenter | Qt::AlignTop,
        hz == EditorState::kNyquistHz ? QStringLiteral("NYQ")
                                      : shortValue(hz));
    painter.setPen(QPen(QColor{43, 53, 56}, 1.0));
  }

  constexpr std::array<double, 6> bandwidth_lines{
      1.0, 10.0, 100.0, 1'000.0, 10'000.0, 20'000.0};
  for (const double bandwidth : bandwidth_lines) {
    const double y = pointFor(EditorState::kLowHz, bandwidth).y();
    painter.drawLine(QPointF{bounds.left(), y}, QPointF{bounds.right(), y});
    painter.setPen(QColor{112, 127, 130});
    painter.drawText(QRectF{4.0, y - 9.0, 56.0, 18.0},
                     Qt::AlignRight | Qt::AlignVCenter,
                     shortValue(bandwidth));
    painter.setPen(QPen(QColor{43, 53, 56}, 1.0));
  }

  painter.setPen(QColor{143, 157, 160});
  QFont axis_font = painter.font();
  axis_font.setCapitalization(QFont::AllUppercase);
  axis_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  axis_font.setPointSizeF(8.0);
  painter.setFont(axis_font);
  painter.drawText(QRectF{bounds.left(), bounds.bottom() + 25.0,
                          bounds.width(), 13.0},
                   Qt::AlignCenter, QStringLiteral("frequency  →"));
  painter.save();
  painter.translate(15.0, bounds.center().y());
  painter.rotate(-90.0);
  painter.drawText(QRectF{-bounds.height() * 0.5, -8.0,
                          bounds.height(), 16.0},
                   Qt::AlignCenter, QStringLiteral("bandwidth  →"));
  painter.restore();

  const auto all_handles = handles();
  for (std::size_t section = 0; section < state_->activeSections();
       ++section) {
    const bool selected = section == state_->selectedSection();
    const QColor color = kSectionColors[section];
    painter.setPen(QPen(faded(color, selected ? 160 : 62),
                        selected ? 1.4 : 1.0));
    painter.drawLine(all_handles[section * 2].position,
                     all_handles[section * 2 + 1].position);
  }

  for (const Handle& handle : all_handles) {
    const bool selected_section = handle.section == state_->selectedSection();
    const bool selected_root =
        selected_section && handle.lane == state_->selectedLane();
    const QColor color = kSectionColors[handle.section];
    if (selected_root) {
      painter.setPen(QPen(QColor{69, 82, 79}, 1.0));
      painter.drawLine(QPointF{handle.position.x(), bounds.top()},
                       QPointF{handle.position.x(), bounds.bottom()});
      painter.drawLine(QPointF{bounds.left(), handle.position.y()},
                       QPointF{bounds.right(), handle.position.y()});
    }

    painter.setPen(QPen(faded(color, selected_section ? 255 : 145),
                        selected_root ? 2.4 : 1.5));
    painter.setBrush(selected_root ? color : QColor{15, 20, 22});
    if (handle.lane == EditorState::Lane::kPole) {
      painter.drawEllipse(handle.position, selected_root ? 8.0 : 6.5,
                          selected_root ? 8.0 : 6.5);
    } else {
      const double radius = selected_root ? 9.0 : 7.5;
      QPainterPath diamond;
      diamond.moveTo(handle.position + QPointF{0.0, -radius});
      diamond.lineTo(handle.position + QPointF{radius, 0.0});
      diamond.lineTo(handle.position + QPointF{0.0, radius});
      diamond.lineTo(handle.position + QPointF{-radius, 0.0});
      diamond.closeSubpath();
      painter.drawPath(diamond);
    }

    painter.setPen(faded(color, selected_section ? 255 : 150));
    QFont marker_font = painter.font();
    marker_font.setBold(true);
    marker_font.setPointSizeF(8.0);
    painter.setFont(marker_font);
    const QString tag = QStringLiteral("%1%2")
                            .arg(handle.section + 1)
                            .arg(handle.lane == EditorState::Lane::kPole
                                     ? QStringLiteral("P")
                                     : QStringLiteral("Z"));
    const bool parked = handle.position.x() > bounds.right() - 42.0;
    if (parked && !selected_section) continue;
    const double label_y = parked
                               ? handle.position.y() +
                                     (handle.lane == EditorState::Lane::kPole
                                          ? -22.0
                                          : 4.0)
                               : handle.position.y() - 9.0;
    const QRectF label_bounds =
        parked ? QRectF{handle.position.x() - 42.0, label_y, 32.0, 18.0}
               : QRectF{handle.position.x() + 10.0, label_y, 28.0, 18.0};
    painter.drawText(label_bounds,
                     (parked ? Qt::AlignRight : Qt::AlignLeft) |
                         Qt::AlignVCenter,
                     tag);
  }

  const QColor selected_color = kSectionColors[state_->selectedSection()];
  QFont title_font = painter.font();
  title_font.setBold(true);
  title_font.setPointSizeF(10.0);
  title_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
  painter.setFont(title_font);
  painter.setPen(selected_color);
  painter.drawText(QRectF{bounds.left(), 12.0, 250.0, 24.0},
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("ARMADILLO  ·  SECTION %1")
                       .arg(state_->selectedSection() + 1));
}

void ArmadilloEditor::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  drag_ = hitHandle(event->position());
  if (!drag_) return;
  state_->selectRoot(drag_->section, drag_->lane);
  grabMouse();
  update();
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
  drag_.reset();
  releaseMouse();
  setCursor(Qt::CrossCursor);
}

void ArmadilloEditor::applyPointer(const QPointF& position) {
  if (!drag_) return;
  const QRectF bounds = field();
  const double x_fraction = std::clamp(
      (position.x() - bounds.left()) / bounds.width(), 0.0, 1.0);
  const double y_fraction = std::clamp(
      (bounds.bottom() - position.y()) / bounds.height(), 0.0, 1.0);
  const double frequency =
      EditorState::kLowHz *
      std::pow(EditorState::kHighHz / EditorState::kLowHz, x_fraction);
  const double bandwidth =
      EditorState::kMinBandwidthHz *
      std::pow(EditorState::kMaxBandwidthHz / EditorState::kMinBandwidthHz,
               y_fraction);
  state_->setRoot(drag_->section, drag_->lane, frequency, bandwidth);
}
