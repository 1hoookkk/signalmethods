#include "section_strip.hpp"

#include "section_color.hpp"

#include <QDoubleSpinBox>
#include <QEnterEvent>
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

constexpr int kOffsetSteps = 100;
constexpr int kRowHeight = 18;
constexpr int kBandPx = 3;
constexpr int kBandHighlightPx = 5;
constexpr int kSelectedTintAlpha = 28;

const QString kSpinStyle = QStringLiteral(
    "QDoubleSpinBox { background: #111416; color: %1; border: 1px solid #373f43; }");

QFont strip_font() { return QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold); }

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

  pole_ = make_label(this, "poleReadout", kInk);
  column->addWidget(pole_);

  offset_ = new QSlider(Qt::Vertical, this);
  offset_->setObjectName(QStringLiteral("offsetFader"));
  offset_->setRange(
      static_cast<int>(std::lround(p2k::mask_offset_min_oct(section_) * kOffsetSteps)),
      static_cast<int>(std::lround(p2k::mask_offset_max_oct(section_) * kOffsetSteps)));
  offset_->setSingleStep(1);
  offset_->setPageStep(kOffsetSteps);
  const auto hue = section_color(section_);
  offset_->setStyleSheet(
      QStringLiteral(
          "QSlider::groove:vertical { width: 4px; background: #373f43; border-radius: 2px; }"
          "QSlider::handle:vertical { height: 9px; margin: 0 -8px; background: %1; "
          "border-radius: 2px; }")
          .arg(hue.name()));
  column->addWidget(offset_, 1, Qt::AlignHCenter);

  offset_value_ = make_label(this, "offsetValue", kInk);
  column->addWidget(offset_value_);

  width_ = new QDoubleSpinBox(this);
  width_->setObjectName(QStringLiteral("widthControl"));
  width_->setFont(strip_font());
  width_->setRange(p2k::mask_width_floor_hz(), p2k::kMaskWidthMaxHz);
  width_->setSingleStep(1.0);
  width_->setDecimals(2);
  width_->setKeyboardTracking(false);
  width_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  width_->setAlignment(Qt::AlignHCenter);
  width_->setFixedHeight(kRowHeight);
  width_->setStyleSheet(kSpinStyle.arg(kInk.name()));
  auto policy = width_->sizePolicy();
  policy.setRetainSizeWhenHidden(true);
  width_->setSizePolicy(policy);
  column->addWidget(width_);

  connect(offset_, &QSlider::sliderPressed, this, [this] {
    emit selectRequested(section_);
    emit gestureStarted(section_);
  });
  connect(offset_, &QSlider::sliderReleased, this,
          [this] { emit gestureFinished(section_); });
  connect(offset_, &QSlider::valueChanged, this, [this](int) {
    if (updating_ || !live_) return;
    emit selectRequested(section_);
    relay();
  });
  connect(width_, &QDoubleSpinBox::valueChanged, this, [this](double) {
    if (updating_ || !live_) return;
    emit selectRequested(section_);
    relay();
  });

  setSelected(false);
}

std::size_t SectionStrip::section() const noexcept { return section_; }

bool SectionStrip::selected() const noexcept { return selected_; }

double SectionStrip::offsetOct() const {
  return static_cast<double>(offset_->value()) / kOffsetSteps;
}

void SectionStrip::relay() {
  emit maskEdited(section_, p2k::MaskParam{offsetOct(), width_->value()});
}

void SectionStrip::setReading(const std::optional<p2k::PoleReading>& pole,
                              const p2k::MaskParam& mask) {
  live_ = pole.has_value();
  updating_ = true;
  if (!offset_->isSliderDown()) {
    offset_->setValue(static_cast<int>(std::lround(mask.offset_oct * kOffsetSteps)));
  }
  width_->setValue(std::clamp(mask.zero_bw_hz, width_->minimum(), width_->maximum()));
  updating_ = false;
  offset_->setEnabled(live_);
  width_->setEnabled(live_ && section_ != 5);
  pole_->setText(live_ ? QStringLiteral("%1 / %2")
                             .arg(pole->hz, 0, 'f', 0)
                             .arg(pole->bw_hz, 0, 'f', 0)
                       : QStringLiteral("—"));
  pole_->setStyleSheet(
      QStringLiteral("color: %1;").arg((live_ ? section_color(section_) : kDim).name()));
  offset_value_->setText(live_ ? QString::number(offsetOct(), 'f', 2)
                               : QStringLiteral("—"));
  update();
}

void SectionStrip::setSelected(bool selected) {
  selected_ = selected;
  width_->setVisible(selected);
  update();
}

void SectionStrip::setHighlighted(bool highlighted) {
  if (highlighted_ == highlighted) return;
  highlighted_ = highlighted;
  update();
}

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
