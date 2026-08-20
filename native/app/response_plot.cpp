#include "response_plot.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

const QColor kBackground{17, 20, 22};
const QColor kGrid{55, 63, 67};
const QColor kText{174, 186, 190};
const QColor kTrace{87, 222, 205};

double x_for_frequency(double frequency_hz, double low_hz, double high_hz,
                       const QRectF& plot) {
  const auto fraction = std::log(frequency_hz / low_hz) / std::log(high_hz / low_hz);
  return plot.left() + fraction * plot.width();
}

double y_for_db(double db, double low_db, double high_db, const QRectF& plot) {
  return plot.bottom() - (db - low_db) / (high_db - low_db) * plot.height();
}

}  // namespace

ResponsePlotWidget::ResponsePlotWidget(QWidget* parent) : QWidget(parent) {
  setMinimumSize(480, 280);
  setAutoFillBackground(false);
}

void ResponsePlotWidget::setBody(const trench::core::PackedBody& body,
                                 double sample_rate_hz,
                                 std::string source_label) {
  frequencies_hz_ = trench::core::logarithmic_frequency_grid(
      20.0, sample_rate_hz * 0.499, 512);
  const auto cascade = body.interpolate_biquads(0.0F, 0.0F, 0.0F);
  response_db_.clear();
  response_db_.reserve(frequencies_hz_.size());
  for (const auto frequency_hz : frequencies_hz_) {
    response_db_.push_back(
        trench::core::cascade_response_db(cascade, frequency_hz, sample_rate_hz));
  }
  source_label_ = QString::fromStdString(std::move(source_label));
  update();
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

void ResponsePlotWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kBackground);

  const QRectF plot = QRectF(rect()).adjusted(54.0, 28.0, -18.0, -36.0);
  if (plot.width() <= 0.0 || plot.height() <= 0.0 || response_db_.empty()) return;

  const auto [minimum, maximum] = std::minmax_element(response_db_.begin(), response_db_.end());
  double low_db = std::floor((*minimum - 3.0) / 12.0) * 12.0;
  double high_db = std::ceil((*maximum + 3.0) / 12.0) * 12.0;
  if (high_db - low_db < 48.0) {
    const auto middle = (high_db + low_db) * 0.5;
    low_db = std::floor((middle - 24.0) / 12.0) * 12.0;
    high_db = low_db + 48.0;
  }

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
    painter.drawText(QRectF{x - 32.0, plot.bottom() + 7.0, 64.0, 18.0},
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

  QPainterPath path;
  for (std::size_t index = 0; index < response_db_.size(); ++index) {
    const QPointF point{x_for_frequency(frequencies_hz_[index], low_hz, high_hz, plot),
                        y_for_db(response_db_[index], low_db, high_db, plot)};
    if (index == 0) path.moveTo(point);
    else path.lineTo(point);
  }
  painter.setClipRect(plot);
  painter.setPen(QPen(kTrace, 1.8));
  painter.drawPath(path);
  painter.setClipping(false);

  painter.setPen(kText);
  painter.drawText(QRectF{plot.left(), 5.0, plot.width(), 18.0},
                   Qt::AlignLeft | Qt::AlignVCenter, source_label_);
}
