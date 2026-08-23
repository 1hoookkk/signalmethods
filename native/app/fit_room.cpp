#include "fit_room.hpp"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

const QColor kBackground{17, 20, 22};
const QColor kGrid{55, 63, 67};
const QColor kText{174, 186, 190};
const QColor kTrace{87, 222, 205};
const QColor kTarget{72, 82, 88};
const QColor kResidual{156, 130, 224};

constexpr double kTraceWidthPx = 1.1;
constexpr double kPointRadiusPx = 2.5;
constexpr double kControlsPx = 120.0;
constexpr double kBandFraction = 0.28;

double erb_rate(double frequency_hz) {
  return 21.4 * std::log10(1.0 + 0.00437 * frequency_hz);
}

double x_for_frequency(double frequency_hz, double low_hz, double high_hz,
                       const QRectF& plot) {
  const auto span = erb_rate(high_hz) - erb_rate(low_hz);
  if (span <= 0.0) return plot.left();
  return plot.left() + (erb_rate(frequency_hz) - erb_rate(low_hz)) / span * plot.width();
}

double y_for_db(double db, double low_db, double high_db, const QRectF& plot) {
  return plot.bottom() - (db - low_db) / (high_db - low_db) * plot.height();
}

}  // namespace

FitRoom::FitRoom(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::Tool);
  setWindowTitle(QStringLiteral("FIT"));
  setMinimumSize(520, 360);
  setAutoFillBackground(false);
  setStyleSheet(QStringLiteral(
      "QListWidget, QPushButton { background: #1a1f23; color: #aebabe; "
      "border: 1px solid #373f43; border-radius: 2px; padding: 0 4px; }"));

  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(6, 6, 6, 6);
  column->setSpacing(6);
  column->addStretch(1);

  auto* row = new QHBoxLayout();
  row->setSpacing(6);
  list_ = new QListWidget(this);
  list_->setObjectName(QStringLiteral("overlayList"));
  list_->setFixedHeight(static_cast<int>(kControlsPx) - 14);
  list_->installEventFilter(this);
  load_ = new QPushButton(QStringLiteral("LOAD"), this);
  load_->setObjectName(QStringLiteral("loadOverlay"));
  load_->setFixedWidth(72);
  row->addWidget(list_, 1);
  row->addWidget(load_);
  column->addLayout(row);

  connect(list_, &QListWidget::currentRowChanged, this, [this](int index) {
    if (populating_) return;
    selected_ = index;
    update();
    emit overlaySelected(index);
  });
  connect(load_, &QPushButton::clicked, this, [this] { emit loadRequested(); });
}

void FitRoom::setGridHz(std::vector<double> hz) {
  hz_ = std::move(hz);
  update();
}

void FitRoom::setResponse(std::vector<double> db) {
  response_db_ = std::move(db);
  update();
}

void FitRoom::setOverlays(QList<Overlay> overlays, int selected) {
  overlays_ = std::move(overlays);
  selected_ = selected;
  populating_ = true;
  list_->clear();
  for (const auto& overlay : overlays_) {
    list_->addItem(overlay.name);
  }
  list_->setCurrentRow(selected);
  populating_ = false;
  update();
}

void FitRoom::setScoreDb(double rms_db) {
  score_db_ = rms_db;
  update();
}

void FitRoom::setFitRunning(bool running) {
  fit_running_ = running;
  update();
}

int FitRoom::overlayCount() const {
  return static_cast<int>(overlays_.size());
}

int FitRoom::selectedOverlay() const {
  return selected_;
}

std::size_t FitRoom::pointCount() const noexcept {
  return hz_.size();
}

double FitRoom::differenceDbAt(std::size_t index) const {
  if (selected_ < 0 || selected_ >= static_cast<int>(overlays_.size())) return 0.0;
  const auto& overlay = overlays_[selected_].db;
  if (overlay.size() != response_db_.size() || index >= overlay.size()) return 0.0;
  return overlay[index] - response_db_[index];
}

double FitRoom::scoreDb() const noexcept {
  return score_db_;
}

QListWidget* FitRoom::overlayList() const noexcept {
  return list_;
}

QPushButton* FitRoom::loadButton() const noexcept {
  return load_;
}

QRectF FitRoom::plotRect() const {
  return QRectF(rect()).adjusted(54.0, 22.0, -18.0, -(kControlsPx + 24.0));
}

std::pair<double, double> FitRoom::dbRange() const {
  if (response_db_.empty()) return {-48.0, 0.0};
  auto sorted = response_db_;
  const auto middle = sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2);
  std::nth_element(sorted.begin(), middle, sorted.end());
  const auto low_db = std::floor((*middle - 24.0) / 12.0) * 12.0;
  return {low_db, low_db + 48.0};
}

bool FitRoom::eventFilter(QObject* watched, QEvent* event) {
  if (watched == list_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Delete) {
      emit overlayRemoved(list_->currentRow());
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void FitRoom::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kBackground);

  const auto area = plotRect();
  if (area.width() <= 0.0 || area.height() <= 0.0 || hz_.size() < 2) return;

  const auto band_height = area.height() * kBandFraction;
  const QRectF plot = area.adjusted(0.0, 0.0, 0.0, -band_height);
  const QRectF band{area.left(), plot.bottom() + 10.0, area.width(), band_height - 14.0};
  const auto low_hz = hz_.front();
  const auto high_hz = hz_.back();
  const auto [low_db, high_db] = dbRange();

  painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
  painter.setPen(QPen(kGrid, 1.0));
  for (const auto frequency : {100.0, 1000.0, 10'000.0}) {
    if (frequency < low_hz || frequency > high_hz) continue;
    const auto x = x_for_frequency(frequency, low_hz, high_hz, plot);
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
    painter.setPen(kText);
    const auto label =
        frequency >= 1000.0
            ? QString::number(frequency / 1000.0, 'g', 2) + QStringLiteral(" kHz")
            : QString::number(frequency, 'f', 0) + QStringLiteral(" Hz");
    painter.drawText(QRectF{x - 32.0, band.bottom() + 2.0, 64.0, 18.0},
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

  const auto have_overlay = selected_ >= 0 &&
                            selected_ < static_cast<int>(overlays_.size()) &&
                            overlays_[selected_].db.size() == hz_.size();

  painter.save();
  painter.setClipRect(plot);
  if (have_overlay) {
    const auto& overlay = overlays_[selected_].db;
    painter.setPen(QPen(kTarget, 1.0));
    painter.setBrush(Qt::NoBrush);
    for (std::size_t index = 0; index < overlay.size(); ++index) {
      const auto x = x_for_frequency(hz_[index], low_hz, high_hz, plot);
      const auto y = y_for_db(overlay[index], low_db, high_db, plot);
      painter.drawEllipse(QPointF{x, y}, kPointRadiusPx, kPointRadiusPx);
    }
    painter.setPen(QPen(kTarget.lighter(160), 1.0, Qt::DashLine));
    for (const double hz : overlays_[selected_].marks_hz) {
      if (hz < low_hz || hz > high_hz) continue;
      const auto x = x_for_frequency(hz, low_hz, high_hz, plot);
      painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
    }
  }

  if (response_db_.size() == hz_.size()) {
    QPainterPath trace;
    for (std::size_t index = 0; index < response_db_.size(); ++index) {
      const auto x = x_for_frequency(hz_[index], low_hz, high_hz, plot);
      const auto y = y_for_db(response_db_[index], low_db, high_db, plot);
      if (index == 0) {
        trace.moveTo(x, y);
      } else {
        trace.lineTo(x, y);
      }
    }
    painter.setPen(QPen(fit_running_ ? kTrace.lighter(130) : kTrace, kTraceWidthPx));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(trace);
  }
  painter.restore();

  if (have_overlay && band.height() > 0.0) {
    double largest = 0.0;
    for (std::size_t index = 0; index < hz_.size(); ++index) {
      largest = std::max(largest, std::abs(differenceDbAt(index)));
    }
    const auto span = std::max(6.0, largest);
    const auto middle = band.center().y();
    painter.setPen(QPen(kGrid, 1.0));
    painter.drawLine(QPointF{band.left(), middle}, QPointF{band.right(), middle});
    painter.setPen(kText);
    painter.drawText(QRectF{2.0, middle - 9.0, 46.0, 18.0},
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("±%1 dB").arg(span, 0, 'f', 0));
    QPainterPath difference;
    for (std::size_t index = 0; index < hz_.size(); ++index) {
      const auto x = x_for_frequency(hz_[index], low_hz, high_hz, plot);
      const auto y = middle - differenceDbAt(index) / span * band.height() * 0.5;
      if (index == 0) {
        difference.moveTo(x, y);
      } else {
        difference.lineTo(x, y);
      }
    }
    painter.save();
    painter.setClipRect(band.adjusted(0.0, -2.0, 0.0, 2.0));
    painter.setPen(QPen(kResidual, kTraceWidthPx));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(difference);
    painter.restore();
  }

  painter.setPen(kText);
  painter.drawText(QRectF{plot.right() - 90.0, plot.top() - 18.0, 90.0, 16.0},
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(score_db_, 'f', 1) + QStringLiteral(" dB"));
}
