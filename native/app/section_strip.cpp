#include "section_strip.hpp"

#include "section_color.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

namespace p2k = trench::core::p2k;

const QColor kChassis{26, 31, 35};
const QColor kInk{174, 186, 190};
const QColor kDim{118, 127, 131};
const QColor kLine{55, 63, 67};

constexpr int kGainSteps = 10;
constexpr int kGainSpanDb = 72;
constexpr int kRowHeight = 18;
constexpr double kSeedFcHz = 1000.0;
constexpr double kSeedBwOct = 0.5;

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
  for (int index = 0; index < 4; ++index) {
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

  bw_ = new QDoubleSpinBox(this);
  bw_->setObjectName(QStringLiteral("bwControl"));
  bw_->setFont(strip_font());
  bw_->setRange(0.02, 4.0);
  bw_->setSingleStep(0.02);
  bw_->setDecimals(2);
  bw_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  bw_->setAlignment(Qt::AlignHCenter);
  bw_->setFixedHeight(kRowHeight);
  bw_->setStyleSheet(QStringLiteral(
      "QDoubleSpinBox { background: #111416; color: #aebabe; border: 1px solid #373f43; }"));
  retain_space(bw_);
  column->addWidget(bw_);

  fc_ = make_label(this, "fcValue", kDim);
  column->addWidget(fc_);

  connect(type_, &QComboBox::currentIndexChanged, this, [this](int) {
    if (updating_) return;
    emit selectRequested(section_);
    relay(p2k::SectionEdit::kType);
  });
  connect(bw_, &QDoubleSpinBox::valueChanged, this, [this](double) {
    if (updating_) return;
    emit selectRequested(section_);
    relay(p2k::SectionEdit::kBw);
  });
  connect(gain_, &QSlider::sliderPressed, this, [this] {
    emit selectRequested(section_);
    emit gestureStarted(section_);
  });
  connect(gain_, &QSlider::sliderReleased, this,
          [this] { emit gestureFinished(section_); });
  connect(gain_, &QSlider::valueChanged, this, [this](int) {
    if (updating_) return;
    relay(p2k::SectionEdit::kGain);
  });

  setSelected(false);
}

std::size_t SectionStrip::section() const noexcept { return section_; }

bool SectionStrip::selected() const noexcept { return selected_; }

int SectionStrip::trenchFaderValue(const p2k::SectionParam& param) const {
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

QString SectionStrip::typeName(p2k::SectionType type) {
  switch (type) {
    case p2k::SectionType::kOff: return QStringLiteral("off");
    case p2k::SectionType::kLowPass: return QStringLiteral("LP");
    case p2k::SectionType::kHighPass: return QStringLiteral("HP");
    case p2k::SectionType::kEq: return QStringLiteral("EQ");
  }
  return {};
}

p2k::SectionType SectionStrip::typeFromIndex(int index) {
  switch (index) {
    case 1: return p2k::SectionType::kLowPass;
    case 2: return p2k::SectionType::kHighPass;
    case 3: return p2k::SectionType::kEq;
    default: return p2k::SectionType::kOff;
  }
}

int SectionStrip::typeIndex(p2k::SectionType type) {
  switch (type) {
    case p2k::SectionType::kLowPass: return 1;
    case p2k::SectionType::kHighPass: return 2;
    case p2k::SectionType::kEq: return 3;
    case p2k::SectionType::kOff: return 0;
  }
  return 0;
}

void SectionStrip::setParam(const p2k::SectionParam& param) {
  param_ = param;
  const bool live = param.type != p2k::SectionType::kOff;
  updating_ = true;
  type_->setCurrentIndex(typeIndex(param.type));
  const bool trench = param.type == p2k::SectionType::kLowPass;
  if (!gain_->isSliderDown()) {
    gain_->setValue(trench ? trenchFaderValue(param)
                           : static_cast<int>(std::lround(param.gain_db * kGainSteps)));
  }
  gain_->setEnabled(live);
  bw_->setValue(live ? param.bw_oct : bw_->minimum());
  bw_->setEnabled(live);
  updating_ = false;
  gain_value_->setText(!live    ? QStringLiteral("—")
                       : trench ? QString::number(param.trench_hz, 'f', 0)
                                : QString::number(param.gain_db, 'f', 1));
  fc_->setText(live ? QString::number(param.fc_hz, 'f', param.fc_hz < 100.0 ? 1 : 0)
                    : QStringLiteral("—"));
  update();
}

void SectionStrip::setSelected(bool selected) {
  selected_ = selected;
  type_->setVisible(selected);
  bw_->setVisible(selected);
  update();
}

void SectionStrip::relay(p2k::SectionEdit edit) {
  auto param = param_;
  if (edit == p2k::SectionEdit::kType) {
    const auto type = typeFromIndex(type_->currentIndex());
    if (type == param.type) return;
    if (param.type == p2k::SectionType::kOff) {
      param.fc_hz = param.fc_hz > 0.0 ? std::min(param.fc_hz, 8000.0) : kSeedFcHz;
      param.bw_oct = kSeedBwOct;
      param.gain_db = 0.0;
    }
    param.type = type;
  } else if (edit == p2k::SectionEdit::kBw) {
    param.bw_oct = bw_->value();
  } else if (edit == p2k::SectionEdit::kGain) {
    if (param.type == p2k::SectionType::kLowPass) {
      param.trench_hz = trenchHzOfFader(param.fc_hz);
    } else {
      param.gain_db = faderDb();
    }
  }
  emit paramEdited(section_, edit, param);
}

void SectionStrip::mousePressEvent(QMouseEvent* event) {
  emit selectRequested(section_);
  event->accept();
}

void SectionStrip::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), kChassis);
  const auto hue = section_color(section_);
  painter.fillRect(QRectF(0.0, 0.0, width(), 2.0), hue);
  painter.setPen(selected_ ? hue : kLine);
  painter.drawLine(width() - 1, 2, width() - 1, height());
}
