#include "path_meter.hpp"

#include "editor_state.hpp"
#include "trench/audio/audition.hpp"
#include "trench/core/native_body.hpp"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace {

constexpr QColor kQuiet{128, 128, 128};
constexpr QColor kWarn{214, 150, 38};
constexpr QColor kHot{196, 60, 48};
constexpr QColor kPanel{255, 255, 255};
constexpr QColor kPanelEdge{200, 200, 200};
constexpr int kBarHeight = 8;

const std::vector<double>& auditGrid() {
  static const std::vector<double> grid =
      trench::core::logarithmic_frequency_grid(20.0, 20'000.0, 96);
  return grid;
}

double peakDb(const trench::core::PackedBody& packed, double morph, double q) {
  const trench::core::Cascade cascade = trench::audio::design_audition(
      {packed, static_cast<float>(morph), static_cast<float>(q), 0.0, 0.0},
      EditorState::kDatumHz);
  const std::span<const trench::core::Biquad> sections{cascade.data(),
                                                       trench::core::native::kSections};
  double peak = -1.0e9;
  for (const double hz : auditGrid()) {
    peak = std::max(peak,
                    trench::core::cascade_response_db(sections, hz, EditorState::kDatumHz));
  }
  return peak;
}

QColor toneFor(double db) {
  if (db >= PathMeter::kFrameDb) return kHot;
  if (db >= PathMeter::kWarnDb) return kWarn;
  return kQuiet;
}

}

PathMeter::PathMeter(QWidget* parent) : QWidget(parent) {
  setFixedSize(170, 44);
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(4, 2, 4, kBarHeight + 6);
  column->setSpacing(0);
  readout_ = new QLabel(this);
  readout_->setObjectName(QStringLiteral("pathReadout"));
  readout_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  column->addWidget(readout_);
  setBody({}, 0.0, 0.0);
}

void PathMeter::setBody(const trench::core::PackedBody& packed, double morph, double q) {
  worst_db_ = -1.0e9;
  for (int m = 0; m < kSteps; ++m) {
    for (int k = 0; k < kSteps; ++k) {
      const double at_m = static_cast<double>(m) / (kSteps - 1);
      const double at_q = static_cast<double>(k) / (kSteps - 1);
      const double db = peakDb(packed, at_m, at_q);
      if (db > worst_db_) {
        worst_db_ = db;
        worst_morph_ = at_m;
        worst_q_ = at_q;
      }
    }
  }
  here_db_ = peakDb(packed, morph, q);
  readout_->setText(QString::asprintf("PATH %+.1f dB  M%.2f Q%.2f   HERE %+.1f dB",
                                      worst_db_, worst_morph_, worst_q_, here_db_));
  update();
}

void PathMeter::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), palette().window().color());
  const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setPen(QPen(kPanelEdge, 1.0));
  painter.setBrush(kPanel);
  painter.drawRect(card);
  const QRectF lane(card.left() + 4.0, card.bottom() - kBarHeight - 3.0, card.width() - 8.0,
                    static_cast<double>(kBarHeight));
  painter.setPen(QPen(kPanelEdge, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(lane);
  const double span = std::clamp(worst_db_, 0.0, kFrameDb) / kFrameDb;
  painter.setPen(Qt::NoPen);
  painter.setBrush(toneFor(worst_db_));
  painter.drawRect(QRectF(lane.left(), lane.top(), lane.width() * span, lane.height()));
  const double warn_x = lane.left() + lane.width() * (kWarnDb / kFrameDb);
  painter.setPen(QPen(kWarn, 1.0));
  painter.drawLine(QPointF(warn_x, lane.top() - 2.0), QPointF(warn_x, lane.bottom() + 2.0));
  const double here = std::clamp(here_db_, 0.0, kFrameDb) / kFrameDb;
  const double here_x = lane.left() + lane.width() * here;
  painter.setPen(QPen(QColor(64, 64, 64), 1.5));
  painter.drawLine(QPointF(here_x, lane.top()), QPointF(here_x, lane.bottom()));
}
