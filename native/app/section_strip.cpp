#include "section_strip.hpp"

#include "section_color.hpp"

#include "trench/core/rbj.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

namespace p2k = trench::core::p2k;
namespace rbj = trench::core::rbj;

const QColor kChassis{26, 31, 35};
const QColor kInk{174, 186, 190};
const QColor kDim{118, 127, 131};
const QColor kLine{55, 63, 67};

constexpr int kGainSteps = 10;
constexpr int kGainSpanDb = 72;
constexpr int kRowHeight = 18;
constexpr int kBandPx = 3;
constexpr int kBandHighlightPx = 5;
constexpr int kSelectedTintAlpha = 28;
constexpr double kSeedFcHz = 1000.0;
constexpr double kSeedQ = 2.0;
constexpr int kShapeCount = 6;

const QString kSpinStyle = QStringLiteral(
    "QDoubleSpinBox { background: #111416; color: %1; border: 1px solid #373f43; }");

QFont strip_font() { return QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold); }

void retain_space(QWidget* widget) {
  auto policy = widget->sizePolicy();
  policy.setRetainSizeWhenHidden(true);
  widget->setSizePolicy(policy);
}

QLabel* make_label(QWidget* parent, const char* name, const QColor& ink) {
  auto* label = new QLabel(parent);
  label->setObjectName(QString::fromLatin1(name));
  label->setFont(strip_font());
  label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  label->setFixedHeight(13);
  label->setStyleSheet(QStringLiteral("color: %1;").arg(ink.name()));
  return label;
}

QDoubleSpinBox* make_spin(QWidget* parent, const char* name, double low, double high,
                          double step, int decimals) {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setObjectName(QString::fromLatin1(name));
  spin->setFont(strip_font());
  spin->setRange(low, high);
  spin->setSingleStep(step);
  spin->setDecimals(decimals);
  spin->setKeyboardTracking(false);
  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setAlignment(Qt::AlignHCenter);
  spin->setFixedHeight(kRowHeight);
  spin->setStyleSheet(kSpinStyle.arg(kInk.name()));
  retain_space(spin);
  return spin;
}

}  // namespace

SectionStrip::SectionStrip(std::size_t section, QWidget* parent)
    : QWidget(parent), section_(section) {
  setObjectName(QStringLiteral("sectionStrip%1").arg(section));
  setAutoFillBackground(false);
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(5, 6, 5, 4);
  column->setSpacing(3);

  type_ = new QComboBox(this);
  type_->setObjectName(QStringLiteral("typeCombo"));
  type_->setFont(strip_font());
  for (int index = 0; index < kShapeCount; ++index) {
    type_->addItem(typeName(typeFromIndex(index)));
  }
  type_->setFixedHeight(kRowHeight);
  type_->setStyleSheet(QStringLiteral(
      "QComboBox { background: #111416; color: #aebabe; border: 1px solid #373f43; "
      "padding: 0 3px; } QComboBox::drop-down { border: none; width: 10px; }"
      "QComboBox QAbstractItemView { background: #111416; color: #aebabe; "
      "selection-background-color: #373f43; }"));
  retain_space(type_);
  column->addWidget(type_);

  gain_ = new QSlider(Qt::Vertical, this);
  gain_->setObjectName(QStringLiteral("gainFader"));
  gain_->setRange(-kGainSpanDb * kGainSteps, kGainSpanDb * kGainSteps);
  gain_->setSingleStep(1);
  gain_->setPageStep(kGainSteps * 6);
  const auto hue = section_color(section_);
  gain_->setStyleSheet(
      QStringLiteral(
          "QSlider::groove:vertical { width: 4px; background: #373f43; border-radius: 2px; }"
          "QSlider::handle:vertical { height: 9px; margin: 0 -8px; background: %1; "
          "border-radius: 2px; }")
          .arg(hue.name()));
  column->addWidget(gain_, 1, Qt::AlignHCenter);

  gain_value_ = make_label(this, "gainValue", kInk);
  column->addWidget(gain_value_);

  q_ = make_spin(this, "qControl", p2k::kShapeQMin, p2k::kShapeQMax, 0.1, 2);
  bw_ = make_spin(this, "bwControl", rbj::bandwidth_oct_from_q(p2k::kShapeQMax),
                  rbj::bandwidth_oct_from_q(p2k::kShapeQMin), 0.02, 2);
  auto* width = new QHBoxLayout;
  width->setContentsMargins(0, 0, 0, 0);
  width->setSpacing(3);
  width->addWidget(q_);
  width->addWidget(bw_);
  column->addLayout(width);

  fc_ = make_spin(this, "fcControl", 20.0, 20000.0, 10.0, 0);
  column->addWidget(fc_);

  connect(type_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (updating_) return;
    emit selectRequested(section_);
    const auto shape = typeFromIndex(index);
    if (shape == param_.shape) return;
    relay(shape);
  });
  connect(fc_, &QDoubleSpinBox::valueChanged, this, [this](double) {
    if (updating_ || param_.shape == p2k::Shape::kOff) return;
    emit selectRequested(section_);
    relay(param_.shape);
  });
  connect(q_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    if (updating_) return;
    updating_ = true;
    bw_->setValue(rbj::bandwidth_oct_from_q(value));
    updating_ = false;
    if (param_.shape == p2k::Shape::kOff) return;
    emit selectRequested(section_);
    relay(param_.shape);
  });
  connect(bw_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    if (updating_) return;
    updating_ = true;
    q_->setValue(rbj::q_from_bandwidth_oct(value));
    updating_ = false;
    if (param_.shape == p2k::Shape::kOff) return;
    emit selectRequested(section_);
    relay(param_.shape);
  });
  connect(gain_, &QSlider::sliderPressed, this, [this] {
    emit selectRequested(section_);
    emit gestureStarted(section_);
  });
  connect(gain_, &QSlider::sliderReleased, this,
          [this] { emit gestureFinished(section_); });
  connect(gain_, &QSlider::valueChanged, this, [this](int) {
    if (updating_) return;
    if (param_.shape == p2k::Shape::kLow) {
      p2k::SectionParam trench;
      trench.type = p2k::SectionType::kLowPass;
      trench.fc_hz = param_.fc_hz;
      trench.bw_oct = rbj::bandwidth_oct_from_q(q_->value());
      trench.trench_hz = trenchHzOfFader(param_.fc_hz);
      emit paramEdited(section_, p2k::SectionEdit::kGain, trench);
      return;
    }
    if (param_.shape == p2k::Shape::kOff || param_.shape == p2k::Shape::kHigh) return;
    relay(param_.shape);
  });

  setSelected(false);
}

std::size_t SectionStrip::section() const noexcept { return section_; }

bool SectionStrip::selected() const noexcept { return selected_; }

int SectionStrip::trenchFaderValue(const p2k::ShapeParam& param) const {
  const double oct = std::log2(std::max(param.trench_hz, 1.0) / std::max(param.fc_hz, 1.0));
  const double unit = (oct - p2k::kTrenchMinOct) / (p2k::kTrenchMaxOct - p2k::kTrenchMinOct);
  return static_cast<int>(std::lround((unit * 2.0 - 1.0) * kGainSpanDb * kGainSteps));
}

double SectionStrip::trenchHzOfFader(double fc_hz) const {
  const double unit = (static_cast<double>(gain_->value()) / (kGainSpanDb * kGainSteps) + 1.0) * 0.5;
  const double oct = p2k::kTrenchMinOct + unit * (p2k::kTrenchMaxOct - p2k::kTrenchMinOct);
  return fc_hz * std::pow(2.0, oct);
}

double SectionStrip::faderDb() const {
  return static_cast<double>(gain_->value()) / kGainSteps;
}

QString SectionStrip::typeName(p2k::Shape shape) {
  switch (shape) {
    case p2k::Shape::kOff: return QStringLiteral("off");
    case p2k::Shape::kLow: return QStringLiteral("low");
    case p2k::Shape::kHigh: return QStringLiteral("high");
    case p2k::Shape::kPeak: return QStringLiteral("peak");
    case p2k::Shape::kLowShelf: return QStringLiteral("low shelf");
    case p2k::Shape::kHighShelf: return QStringLiteral("high shelf");
  }
  return {};
}

p2k::Shape SectionStrip::typeFromIndex(int index) {
  switch (index) {
    case 1: return p2k::Shape::kLow;
    case 2: return p2k::Shape::kHigh;
    case 3: return p2k::Shape::kPeak;
    case 4: return p2k::Shape::kLowShelf;
    case 5: return p2k::Shape::kHighShelf;
    default: return p2k::Shape::kOff;
  }
}

int SectionStrip::typeIndex(p2k::Shape shape) {
  switch (shape) {
    case p2k::Shape::kLow: return 1;
    case p2k::Shape::kHigh: return 2;
    case p2k::Shape::kPeak: return 3;
    case p2k::Shape::kLowShelf: return 4;
    case p2k::Shape::kHighShelf: return 5;
    case p2k::Shape::kOff: return 0;
  }
  return 0;
}

void SectionStrip::setParam(const p2k::ShapeParam& param) {
  param_ = param;
  const bool live = param.shape != p2k::Shape::kOff;
  const bool trench = param.shape == p2k::Shape::kLow;
  const bool levelless = param.shape == p2k::Shape::kHigh;
  updating_ = true;
  type_->setCurrentIndex(typeIndex(param.shape));
  if (!gain_->isSliderDown()) {
    gain_->setValue(trench ? trenchFaderValue(param)
                           : static_cast<int>(std::lround(param.gain_db * kGainSteps)));
  }
  gain_->setEnabled(live && !levelless);
  q_->setValue(live ? std::clamp(param.q, q_->minimum(), q_->maximum()) : q_->minimum());
  bw_->setValue(live ? std::clamp(rbj::bandwidth_oct_from_q(q_->value()), bw_->minimum(),
                                  bw_->maximum())
                     : bw_->minimum());
  fc_->setValue(live ? std::clamp(param.fc_hz, fc_->minimum(), fc_->maximum())
                     : fc_->minimum());
  q_->setEnabled(live);
  bw_->setEnabled(live);
  fc_->setEnabled(live);
  updating_ = false;
  gain_value_->setText(!live || levelless ? QStringLiteral("—")
                       : trench           ? QString::number(param.trench_hz, 'f', 0)
                                          : QString::number(param.gain_db, 'f', 1));
  fc_->setStyleSheet(kSpinStyle.arg((live ? section_color(section_) : kDim).name()));
  update();
}

void SectionStrip::setSelected(bool selected) {
  selected_ = selected;
  type_->setVisible(selected);
  q_->setVisible(selected);
  bw_->setVisible(selected);
  update();
}

void SectionStrip::setHighlighted(bool highlighted) {
  if (highlighted_ == highlighted) return;
  highlighted_ = highlighted;
  update();
}

p2k::ShapeParam SectionStrip::typed(p2k::Shape shape) const {
  auto param = param_;
  param.shape = shape;
  if (param_.shape == p2k::Shape::kOff) {
    param.fc_hz = param_.fc_hz > 0.0 ? std::min(param_.fc_hz, 8000.0) : kSeedFcHz;
    param.q = kSeedQ;
    param.gain_db = 0.0;
    return param;
  }
  param.fc_hz = fc_->value();
  param.q = q_->value();
  param.gain_db = faderDb();
  return param;
}

void SectionStrip::relay(p2k::Shape shape) { emit shapeEdited(section_, typed(shape)); }

void SectionStrip::mousePressEvent(QMouseEvent* event) {
  emit selectRequested(section_);
  event->accept();
}

void SectionStrip::enterEvent(QEnterEvent* event) {
  emit hoverChanged(section_, true);
  QWidget::enterEvent(event);
}

void SectionStrip::leaveEvent(QEvent* event) {
  emit hoverChanged(section_, false);
  QWidget::leaveEvent(event);
}

void SectionStrip::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), kChassis);
  const auto hue = section_color(section_);
  if (selected_) {
    auto tint = hue;
    tint.setAlpha(kSelectedTintAlpha);
    painter.fillRect(rect(), tint);
  }
  const auto band = highlighted_ ? kBandHighlightPx : kBandPx;
  painter.fillRect(QRectF(0.0, 0.0, width(), band), hue);
  painter.setPen(selected_ ? hue : kLine);
  painter.drawLine(width() - 1, band, width() - 1, height());
}
