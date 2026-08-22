#include "morph_strip.hpp"

#include <QHBoxLayout>
#include <QPainter>
#include <QSlider>

#include <cmath>

namespace {

constexpr int kSteps = 1000;
const QColor kChassis{26, 31, 35};
const QColor kInk{174, 186, 190};

QSlider* make_slider(QWidget* parent, const char* name) {
  auto* slider = new QSlider(Qt::Horizontal, parent);
  slider->setObjectName(QString::fromLatin1(name));
  slider->setRange(0, kSteps);
  slider->setSingleStep(1);
  slider->setPageStep(kSteps / 10);
  slider->setTickPosition(QSlider::TicksBelow);
  slider->setTickInterval(kSteps);
  slider->setStyleSheet(QStringLiteral(
      "QSlider::groove:horizontal { height: 4px; background: #373f43; border-radius: 2px; }"
      "QSlider::handle:horizontal { width: 12px; margin: -6px 0; background: #aebabe; "
      "border-radius: 3px; }"
      "QSlider::sub-page:horizontal { background: #57decd; border-radius: 2px; }"));
  return slider;
}

}  // namespace

MorphStrip::MorphStrip(QWidget* parent) : QWidget(parent) {
  setFixedHeight(30);
  setAutoFillBackground(false);
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(12, 4, 12, 4);
  row->setSpacing(18);
  morph_ = make_slider(this, "morphSlider");
  q_ = make_slider(this, "qSlider");
  row->addWidget(morph_, 3);
  row->addWidget(q_, 1);
  row->addSpacing(72);
  const auto relay = [this] {
    if (updating_) return;
    emit viewEdited(morph(), q());
  };
  connect(morph_, &QSlider::valueChanged, this, relay);
  connect(q_, &QSlider::valueChanged, this, relay);
}

void MorphStrip::setView(float morph, float q) {
  updating_ = true;
  morph_->setValue(static_cast<int>(std::lround(morph * kSteps)));
  q_->setValue(static_cast<int>(std::lround(q * kSteps)));
  updating_ = false;
  update();
}

float MorphStrip::morph() const {
  return static_cast<float>(morph_->value()) / static_cast<float>(kSteps);
}

float MorphStrip::q() const { return static_cast<float>(q_->value()) / static_cast<float>(kSteps); }

void MorphStrip::setWorstStepDb(double db) {
  worst_step_db_ = db;
  update();
}

void MorphStrip::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), kChassis);
  painter.setPen(kInk);
  painter.setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold));
  const auto text = std::isfinite(worst_step_db_) && worst_step_db_ > 0.0
                        ? QStringLiteral("%1 dB").arg(worst_step_db_, 0, 'f', 1)
                        : QString();
  painter.drawText(QRectF(width() - 76.0, 0.0, 64.0, height()),
                   Qt::AlignRight | Qt::AlignVCenter, text);
}
