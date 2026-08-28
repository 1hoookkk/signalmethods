#include "root_editor.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

using Resonant = trench::core::native::Resonant;

const Resonant& rootOf(const EditorState& state, EditorState::Lane lane) {
  const auto& section = state.section(state.selectedSection());
  return std::get<Resonant>(lane == EditorState::Lane::kPole ? section.pole
                                                             : section.zero);
}

constexpr double kPanelPaddingX = 20.0;
constexpr double kPanelPaddingTop = 34.0;
constexpr double kPanelPaddingBottom = 28.0;

QRectF activeArea(const QRectF& panel) {
  return panel.adjusted(kPanelPaddingX, kPanelPaddingTop,
                        -kPanelPaddingX, -kPanelPaddingBottom);
}

}  // namespace

RootEditor::RootEditor(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setMinimumHeight(220);
  setCursor(Qt::CrossCursor);
  connect(state_, &EditorState::changed, this,
          qOverload<>(&RootEditor::update));
  connect(state_, &EditorState::selectionChanged, this,
          [this] { update(); });
}

QRectF RootEditor::polePanel() const {
  const QRectF bounds = QRectF(rect()).adjusted(0.0, 0.0, -6.0, 0.0);
  return QRectF{bounds.left(), bounds.top(), bounds.width() * 0.5,
                bounds.height()};
}

QRectF RootEditor::zeroPanel() const {
  const QRectF bounds = QRectF(rect()).adjusted(6.0, 0.0, 0.0, 0.0);
  return QRectF{bounds.left() + bounds.width() * 0.5, bounds.top(),
                bounds.width() * 0.5, bounds.height()};
}

QPointF RootEditor::pointFor(double frequency_hz, double bandwidth_hz,
                             const QRectF& panel) const {
  const QRectF active = activeArea(panel);
  const double x_fraction =
      std::log(frequency_hz / EditorState::kLowHz) /
      std::log(EditorState::kHighHz / EditorState::kLowHz);
  const double y_fraction =
      std::log(bandwidth_hz / EditorState::kMinBandwidthHz) /
      std::log(EditorState::kMaxBandwidthHz / EditorState::kMinBandwidthHz);
  return {active.left() + std::clamp(x_fraction, 0.0, 1.0) * active.width(),
          active.bottom() - std::clamp(y_fraction, 0.0, 1.0) * active.height()};
}

void RootEditor::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QColor{12, 15, 17});

  const auto draw_panel = [&](const QRectF& panel, EditorState::Lane lane,
                              const QString& title, const QColor& accent) {
    painter.fillRect(panel, QColor{17, 22, 25});
    const QRectF active = activeArea(panel);
    painter.setPen(QPen(QColor{47, 57, 61}, 1.0));
    constexpr std::array<double, 4> x_lines{100.0, 1'000.0, 10'000.0,
                                             20'000.0};
    for (const double hz : x_lines) {
      const double fraction = std::log(hz / EditorState::kLowHz) /
                              std::log(EditorState::kHighHz / EditorState::kLowHz);
      const double x = active.left() + fraction * active.width();
      painter.drawLine(QPointF{x, active.top()}, QPointF{x, active.bottom()});
    }
    constexpr std::array<double, 4> y_lines{10.0, 100.0, 1'000.0, 10'000.0};
    for (const double bw : y_lines) {
      const double fraction = std::log(bw / EditorState::kMinBandwidthHz) /
                              std::log(EditorState::kMaxBandwidthHz /
                                       EditorState::kMinBandwidthHz);
      const double y = active.bottom() - fraction * active.height();
      painter.drawLine(QPointF{active.left(), y}, QPointF{active.right(), y});
    }

    const auto& root = rootOf(*state_, lane);
    const QPointF point = pointFor(root.hz, root.bw_hz, panel);
    painter.setPen(QPen(accent, 1.5));
    painter.drawLine(QPointF{point.x(), active.top()},
                     QPointF{point.x(), active.bottom()});
    painter.drawLine(QPointF{active.left(), point.y()},
                     QPointF{active.right(), point.y()});
    painter.setBrush(accent);
    painter.setPen(Qt::NoPen);
    if (lane == EditorState::Lane::kPole) {
      painter.drawEllipse(point, 7.0, 7.0);
    } else {
      QPainterPath diamond;
      diamond.moveTo(point + QPointF{0.0, -8.0});
      diamond.lineTo(point + QPointF{8.0, 0.0});
      diamond.lineTo(point + QPointF{0.0, 8.0});
      diamond.lineTo(point + QPointF{-8.0, 0.0});
      diamond.closeSubpath();
      painter.drawPath(diamond);
    }

    painter.setPen(accent);
    QFont title_font = painter.font();
    title_font.setBold(true);
    title_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    painter.setFont(title_font);
    painter.drawText(panel.adjusted(16.0, 7.0, -16.0, 0.0),
                     Qt::AlignLeft | Qt::AlignTop, title);
    painter.setPen(QColor{178, 190, 193});
    QFont value_font = painter.font();
    value_font.setBold(false);
    value_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
    painter.setFont(value_font);
    painter.drawText(panel.adjusted(16.0, 7.0, -16.0, 0.0),
                     Qt::AlignRight | Qt::AlignTop,
                     QStringLiteral("%1 Hz  ·  %2 Hz BW")
                         .arg(root.hz, 0, 'f', 2)
                         .arg(root.bw_hz, 0, 'f', 2));
    painter.drawText(QRectF{active.left(), active.bottom() + 7.0,
                            active.width(), 18.0},
                     Qt::AlignCenter, QStringLiteral("FREQUENCY  →"));
  };

  draw_panel(polePanel(), EditorState::Lane::kPole,
             QStringLiteral("POLE  F / BW"), QColor{66, 224, 207});
  draw_panel(zeroPanel(), EditorState::Lane::kZero,
             QStringLiteral("ZERO  F / BW"), QColor{221, 142, 85});
}

void RootEditor::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const QPointF position = event->position();
  if (polePanel().contains(position)) {
    drag_lane_ = EditorState::Lane::kPole;
  } else if (zeroPanel().contains(position)) {
    drag_lane_ = EditorState::Lane::kZero;
  } else {
    return;
  }
  dragging_ = true;
  grabMouse();
  applyPointer(position);
}

void RootEditor::mouseMoveEvent(QMouseEvent* event) {
  if (dragging_) applyPointer(event->position());
}

void RootEditor::mouseReleaseEvent(QMouseEvent* event) {
  if (!dragging_ || event->button() != Qt::LeftButton) return;
  applyPointer(event->position());
  dragging_ = false;
  releaseMouse();
}

void RootEditor::applyPointer(const QPointF& position) {
  const QRectF panel = drag_lane_ == EditorState::Lane::kPole ? polePanel()
                                                              : zeroPanel();
  const QRectF active = activeArea(panel);
  const double x_fraction = std::clamp(
      (position.x() - active.left()) / active.width(), 0.0, 1.0);
  const double y_fraction = std::clamp(
      (active.bottom() - position.y()) / active.height(), 0.0, 1.0);
  const double frequency = EditorState::kLowHz *
      std::pow(EditorState::kHighHz / EditorState::kLowHz, x_fraction);
  const double bandwidth = EditorState::kMinBandwidthHz *
      std::pow(EditorState::kMaxBandwidthHz / EditorState::kMinBandwidthHz,
               y_fraction);
  state_->setRoot(state_->selectedSection(), drag_lane_, frequency, bandwidth);
}
