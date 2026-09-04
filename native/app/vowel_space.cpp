#include "vowel_space.hpp"

#include "keyframe_grid.hpp"
#include "number_box.hpp"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

constexpr QColor kInk{0, 0, 0};
constexpr QColor kMuted{140, 140, 140};
constexpr QColor kMint{72, 190, 148};
constexpr QColor kPanel{255, 255, 255};

constexpr double kTopRoom = 16.0;
constexpr double kSideRoom = 12.0;
constexpr double kReadoutRoom = 52.0;
constexpr int kCellWidth = 96;
constexpr int kCellHeight = 22;
constexpr int kLabelWidth = 30;
constexpr int kCellStride = 160;
constexpr int kReadoutDrop = 22;

const QString kGroup = QStringLiteral("VOWEL H95");

double logLerp(double low, double high, double t) {
  return std::exp(std::log(low) + t * (std::log(high) - std::log(low)));
}

}

VowelSpace::VowelSpace(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("vowelSpace"));
  setMouseTracking(true);
  setAutoFillBackground(true);
  setMinimumSize(360, 240);

  for (const trench::app::Keyframe& key : KeyframeGrid::keys()) {
    if (key.group != kGroup || key.rows.size() < 3) continue;
    Mark mark;
    mark.name = key.name;
    const QStringList words = key.name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    mark.label = words.isEmpty() ? key.name : words.front();
    mark.man = key.name.endsWith(QStringLiteral(" man"));
    mark.f1 = key.rows[0].hz;
    mark.f2 = key.rows[1].hz;
    mark.f3 = key.rows[2].hz;
    marks_.push_back(std::move(mark));
  }

  static const std::array<std::pair<int, int>, 4> spans{
      std::pair<int, int>{200, 850}, std::pair<int, int>{400, 2800},
      std::pair<int, int>{1000, 5200}, std::pair<int, int>{1200, 7400}};
  for (int cell = 0; cell < 4; ++cell) {
    auto* box = new NumberBox(this);
    box->setObjectName(QStringLiteral("vowelF%1").arg(cell + 1));
    box->setRange(spans[static_cast<std::size_t>(cell)].first,
                  spans[static_cast<std::size_t>(cell)].second);
    QFont face = box->font();
    face.setPointSizeF(std::max(6.0, font().pointSizeF() - 1.0));
    box->setFont(face);
    connect(box, &NumberBox::valueChanged, this, [this, cell](int value) {
      if (refreshing_) return;
      applyCell(cell, value);
    });
    connect(box, &NumberBox::typed, this, [this, cell](const QString& entry) {
      bool good = false;
      const double wanted = entry.trimmed().toDouble(&good);
      if (!good || wanted <= 0.0) {
        refreshCells();
        return;
      }
      applyCell(cell, wanted);
    });
    cells_[static_cast<std::size_t>(cell)] = box;
  }
  refreshCells();
  placeCells();
}

int VowelSpace::markCount() const { return static_cast<int>(marks_.size()); }

QRectF VowelSpace::chartRect() const {
  const double wide = std::max(120.0, static_cast<double>(width()) - 2.0 * kSideRoom);
  const double tall = std::max(80.0, static_cast<double>(height()) - kTopRoom - kReadoutRoom);
  return {kSideRoom, kTopRoom, wide, tall};
}

double VowelSpace::f2Left(double v) const { return logLerp(kF2TopLeft, kF2BottomLeft, v); }

double VowelSpace::f2Right(double v) const { return logLerp(kF2TopRight, kF2BottomRight, v); }

double VowelSpace::leftEdgeX(double v, const QRectF& chart) const {
  return chart.left() + chart.width() * kSlant * v;
}

QPointF VowelSpace::pointFor(double f1, double f2) const {
  const QRectF chart = chartRect();
  const double v = std::clamp(std::log(std::max(1.0, f1) / kF1Top) / std::log(kF1Bottom / kF1Top),
                              0.0, 1.0);
  const double low = std::log(f2Left(v));
  const double high = std::log(f2Right(v));
  const double u = std::clamp((std::log(std::max(1.0, f2)) - low) / (high - low), 0.0, 1.0);
  const double left = leftEdgeX(v, chart);
  return {left + u * (chart.right() - left), chart.top() + v * chart.height()};
}

std::pair<double, double> VowelSpace::formantsAt(QPointF point) const {
  const QRectF chart = chartRect();
  const double v = std::clamp((point.y() - chart.top()) / chart.height(), 0.0, 1.0);
  const double left = leftEdgeX(v, chart);
  const double u = std::clamp((point.x() - left) / (chart.right() - left), 0.0, 1.0);
  const double f1 = kF1Top * std::pow(kF1Bottom / kF1Top, v);
  const double f2 = logLerp(f2Left(v), f2Right(v), u);
  return {f1, f2};
}

double VowelSpace::f3For(double f1, double f2) const {
  const double note_f1 = std::log(std::max(1.0, f1));
  const double note_f2 = std::log(std::max(1.0, f2));
  double weight_sum = 0.0;
  double value_sum = 0.0;
  for (const Mark& mark : marks_) {
    if (!mark.man) continue;
    const double dx = note_f1 - std::log(mark.f1);
    const double dy = note_f2 - std::log(mark.f2);
    const double reach = std::sqrt(dx * dx + dy * dy);
    if (reach < 1e-9) return mark.f3;
    const double weight = 1.0 / reach;
    weight_sum += weight;
    value_sum += weight * mark.f3;
  }
  if (weight_sum <= 0.0) return 2500.0;
  return value_sum / weight_sum;
}

std::array<double, 4> VowelSpace::formants() const {
  const double f3 = f3_override_ > 0.0 ? f3_override_ : f3For(f1_, f2_);
  const double f4 = f4_override_ > 0.0 ? f4_override_ : 1.4 * f3;
  return {f1_, f2_, f3, f4};
}

trench::app::Keyframe VowelSpace::frame() const {
  const auto reading = formants();
  trench::app::Keyframe key;
  key.group = QStringLiteral("VOWEL SPACE");
  key.name = cursor_name_.isEmpty()
                 ? QStringLiteral("F1 %1 F2 %2")
                       .arg(static_cast<int>(std::lround(reading[0])))
                       .arg(static_cast<int>(std::lround(reading[1])))
                 : cursor_name_;
  key.source = QStringLiteral("vowel space");
  key.mode = QStringLiteral("poles");
  key.rows.reserve(reading.size());
  for (const double hz : reading) {
    key.rows.push_back({trench::app::RowType::kPole, hz, 50.0 + hz / 20.0, 0.0});
  }
  return key;
}

int VowelSpace::markNear(const QPointF& point) const {
  int found = -1;
  double best = kSnapPixels;
  for (std::size_t i = 0; i < marks_.size(); ++i) {
    const QPointF seat = pointFor(marks_[i].f1, marks_[i].f2);
    const double reach = std::hypot(seat.x() - point.x(), seat.y() - point.y());
    if (reach <= best) {
      best = reach;
      found = static_cast<int>(i);
    }
  }
  return found;
}

void VowelSpace::moveTo(const QPointF& point, bool snap) {
  QPointF landed = point;
  cursor_name_.clear();
  if (snap) {
    const int mark = markNear(point);
    if (mark >= 0) {
      const Mark& held = marks_[static_cast<std::size_t>(mark)];
      landed = pointFor(held.f1, held.f2);
      cursor_name_ = held.name;
    }
  }
  const auto reading = formantsAt(landed);
  f1_ = reading.first;
  f2_ = reading.second;
  f3_override_ = 0.0;
  f4_override_ = 0.0;
  refreshCells();
  update();
}

void VowelSpace::setFormants(double f1, double f2) { moveTo(pointFor(f1, f2), false); }

void VowelSpace::applyCell(int cell, double value) {
  switch (cell) {
    case 0:
      setFormants(value, f2_);
      break;
    case 1:
      setFormants(f1_, value);
      break;
    case 2:
      f3_override_ = value;
      refreshCells();
      update();
      break;
    default:
      f4_override_ = value;
      refreshCells();
      update();
      break;
  }
  hear();
}

void VowelSpace::refreshCells() {
  const auto reading = formants();
  refreshing_ = true;
  for (std::size_t cell = 0; cell < cells_.size(); ++cell) {
    NumberBox* box = cells_[cell];
    if (box == nullptr) continue;
    const int shown = static_cast<int>(std::lround(reading[cell]));
    const QSignalBlocker blocker(box);
    box->setValue(shown);
    box->setText(QString::number(shown));
  }
  refreshing_ = false;
}

void VowelSpace::hear() {
  if (onHover) onHover(frame());
}

void VowelSpace::pick(bool partner) {
  if (onPick) onPick(frame(), partner);
}

void VowelSpace::pickForTest(bool partner) { pick(partner); }

void VowelSpace::hoverForTest(double f1, double f2) {
  moveTo(pointFor(f1, f2), true);
  hear();
}

void VowelSpace::placeCells() {
  const int row = height() - static_cast<int>(kReadoutRoom) + kReadoutDrop;
  for (std::size_t cell = 0; cell < cells_.size(); ++cell) {
    NumberBox* box = cells_[cell];
    if (box == nullptr) continue;
    const int left = static_cast<int>(kSideRoom) + static_cast<int>(cell) * kCellStride;
    box->setGeometry(left + kLabelWidth, row, kCellWidth, kCellHeight);
  }
}

void VowelSpace::resizeEvent(QResizeEvent* event) {
  placeCells();
  QWidget::resizeEvent(event);
}

void VowelSpace::mouseMoveEvent(QMouseEvent* event) {
  const QPointF where = event->position();
  if (!chartRect().contains(where)) {
    QWidget::mouseMoveEvent(event);
    return;
  }
  moveTo(where, true);
  hear();
  event->accept();
}

void VowelSpace::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !chartRect().contains(event->position())) {
    QWidget::mousePressEvent(event);
    return;
  }
  moveTo(event->position(), true);
  pick((event->modifiers() & Qt::ShiftModifier) != 0);
  event->accept();
}

void VowelSpace::leaveEvent(QEvent* event) {
  if (onHoverLeave) onHoverLeave();
  QWidget::leaveEvent(event);
}

void VowelSpace::paintEvent(QPaintEvent*) {
  const QRectF chart = chartRect();
  QPainter painter(this);
  painter.fillRect(rect(), kPanel);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QPolygonF shape;
  shape << QPointF(chart.left(), chart.top()) << QPointF(chart.right(), chart.top())
        << QPointF(chart.right(), chart.bottom())
        << QPointF(leftEdgeX(1.0, chart), chart.bottom());
  painter.setPen(QPen(kInk, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawPolygon(shape);

  QFont small = font();
  small.setPointSizeF(std::max(6.0, font().pointSizeF() - 1.5));
  painter.setFont(small);
  const QFontMetrics metrics(small);
  painter.setPen(kMuted);
  const QString top_left = QStringLiteral("200 · 2600");
  const QString top_right = QStringLiteral("200 · 500");
  const QString bottom_left = QStringLiteral("850 · 1800");
  const QString bottom_right = QStringLiteral("850 · 880");
  painter.drawText(QPointF(chart.left() + 4.0, chart.top() - 4.0), top_left);
  painter.drawText(
      QPointF(chart.right() - metrics.horizontalAdvance(top_right) - 4.0, chart.top() - 4.0),
      top_right);
  painter.drawText(QPointF(leftEdgeX(1.0, chart) + 4.0, chart.bottom() + metrics.ascent() + 2.0),
                   bottom_left);
  painter.drawText(QPointF(chart.right() - metrics.horizontalAdvance(bottom_right) - 4.0,
                           chart.bottom() + metrics.ascent() + 2.0),
                   bottom_right);

  for (const Mark& mark : marks_) {
    const QPointF seat = pointFor(mark.f1, mark.f2);
    if (mark.man) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(kInk);
      painter.drawEllipse(seat, 3.0, 3.0);
      painter.setPen(kInk);
      painter.drawText(QPointF(seat.x() + 5.0, seat.y() - 4.0), mark.label);
    } else {
      painter.setPen(QPen(kMuted, 1.0));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(seat, 2.0, 2.0);
    }
  }

  const QPointF cursor = pointFor(f1_, f2_);
  painter.setPen(QPen(kMint, 2.0));
  painter.drawLine(QPointF(cursor.x() - 7.0, cursor.y()), QPointF(cursor.x() + 7.0, cursor.y()));
  painter.drawLine(QPointF(cursor.x(), cursor.y() - 7.0), QPointF(cursor.x(), cursor.y() + 7.0));

  painter.setPen(kMuted);
  static const std::array<QString, 4> names{QStringLiteral("F1"), QStringLiteral("F2"),
                                            QStringLiteral("F3"), QStringLiteral("F4")};
  const int row = height() - static_cast<int>(kReadoutRoom) + kReadoutDrop;
  for (std::size_t cell = 0; cell < names.size(); ++cell) {
    const int left = static_cast<int>(kSideRoom) + static_cast<int>(cell) * kCellStride;
    painter.drawText(QRect(left, row, kLabelWidth - 4, kCellHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, names[cell]);
  }
}
