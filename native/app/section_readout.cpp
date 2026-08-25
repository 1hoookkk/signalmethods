#include "section_readout.hpp"

#include "section_color.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace {

const QColor kChassis{26, 31, 35};
const QColor kInk{174, 186, 190};
const QColor kDim{118, 127, 131};
const QColor kLine{55, 63, 67};

constexpr int kBarHeight = 26;
constexpr int kFieldWidth = 62;

const QString kFieldStyle = QStringLiteral(
    "QLineEdit { background: transparent; color: %1; border: none; "
    "border-bottom: 1px solid #2a3134; padding: 0; }"
    "QLineEdit:hover { border-bottom: 1px solid #57decd; }"
    "QLineEdit:focus { background: #111416; border: 1px solid #373f43; }");

const QString kPinStyle = QStringLiteral(
    "QToolButton { background: transparent; color: #767f83; border: 1px solid #373f43; "
    "border-radius: 2px; padding: 0 5px; }"
    "QToolButton:hover, QToolButton:focus { color: #aebabe; border-color: #59656a; }"
    "QToolButton:checked { color: #f7b85c; border-color: #9a7138; }");

QFont bar_font() { return QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold); }

QLineEdit* make_field(QWidget* parent, const char* name) {
  auto* field = new QLineEdit(parent);
  field->setObjectName(QString::fromLatin1(name));
  field->setFont(bar_font());
  field->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  field->setFixedWidth(kFieldWidth);
  field->setFrame(false);
  field->setStyleSheet(kFieldStyle.arg(kInk.name()));
  return field;
}

}  // namespace

SectionReadout::SectionReadout(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("sectionReadout"));
  setFixedHeight(kBarHeight);
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(8, 3, 8, 3);
  row->setSpacing(6);

  const auto add_caption = [this, row](const QString& text) {
    auto* caption = new QLabel(text, this);
    caption->setFont(bar_font());
    caption->setStyleSheet(QStringLiteral("color: %1;").arg(kDim.name()));
    row->addWidget(caption);
  };

  add_caption(QStringLiteral("P"));

  hz_ = make_field(this, "poleHzField");
  hz_->setAccessibleName(QStringLiteral("Pole frequency in hertz"));
  hz_->installEventFilter(this);
  row->addWidget(hz_);
  add_caption(QStringLiteral("Hz"));

  add_caption(QStringLiteral("ΔF"));

  bw_ = make_field(this, "poleWidthField");
  bw_->setAccessibleName(QStringLiteral("Pole bandwidth in hertz"));
  bw_->installEventFilter(this);
  row->addWidget(bw_);
  add_caption(QStringLiteral("Hz"));

  zero_ = new QLabel(this);
  zero_->setObjectName(QStringLiteral("zeroReadout"));
  zero_->setFont(bar_font());
  zero_->setStyleSheet(QStringLiteral("color: %1;").arg(kDim.name()));
  row->addWidget(zero_, 1);

  pole_pin_ = new QToolButton(this);
  pole_pin_->setObjectName(QStringLiteral("polePinButton"));
  pole_pin_->setAccessibleName(QStringLiteral("Hold pole for fitting"));
  pole_pin_->setToolTip(QStringLiteral("toggle whether FIT may move this pole"));
  pole_pin_->setFont(bar_font());
  pole_pin_->setCheckable(true);
  pole_pin_->setStyleSheet(kPinStyle);
  row->addWidget(pole_pin_);

  zero_pin_ = new QToolButton(this);
  zero_pin_->setObjectName(QStringLiteral("zeroPinButton"));
  zero_pin_->setAccessibleName(QStringLiteral("Hold zero for fitting"));
  zero_pin_->setToolTip(QStringLiteral("toggle whether FIT may move this zero"));
  zero_pin_->setFont(bar_font());
  zero_pin_->setCheckable(true);
  zero_pin_->setStyleSheet(kPinStyle);
  row->addWidget(zero_pin_);

  connect(hz_, &QLineEdit::editingFinished, this, [this] { commit(hz_); });
  connect(bw_, &QLineEdit::editingFinished, this, [this] { commit(bw_); });
  connect(pole_pin_, &QToolButton::clicked, this,
          [this] { emit pinToggled(section_, true); });
  connect(zero_pin_, &QToolButton::clicked, this,
          [this] { emit pinToggled(section_, false); });

  zero_->setAccessibleName(QStringLiteral("Zero offset and bandwidth"));
  setReading(0, std::nullopt, std::nullopt);
  setPins(true, true);
}

void SectionReadout::setPins(bool pole_free, bool zero_free) {
  pole_pin_->setChecked(!pole_free);
  zero_pin_->setChecked(!zero_free);
  pole_pin_->setText(pole_free ? QStringLiteral("P FREE") : QStringLiteral("P HOLD"));
  zero_pin_->setText(zero_free ? QStringLiteral("Z FREE") : QStringLiteral("Z HOLD"));
  pole_pin_->setAccessibleDescription(
      pole_free ? QStringLiteral("FIT may move the selected pole")
                : QStringLiteral("The selected pole is held"));
  zero_pin_->setAccessibleDescription(
      zero_free ? QStringLiteral("FIT may move the selected zero")
                : QStringLiteral("The selected zero is held"));
}

void SectionReadout::setReading(std::size_t section,
                                const std::optional<trench::core::native::Resonant>& pole,
                                const std::optional<trench::core::native::Resonant>& zero) {
  section_ = section;
  live_ = pole.has_value();
  pole_hz_ = live_ ? pole->hz : 0.0;
  pole_bw_hz_ = live_ ? pole->bw_hz : 0.0;
  hz_->setReadOnly(!live_);
  bw_->setReadOnly(!live_);
  const auto hue = (live_ ? section_color(section_) : kDim).name();
  hz_->setStyleSheet(kFieldStyle.arg(hue));
  bw_->setStyleSheet(kFieldStyle.arg(hue));
  if (!hz_->hasFocus() && !bw_->hasFocus()) showPole();
  zero_->setText(live_ && zero
                     ? QStringLiteral("Z  %1 oct · ΔF %2 Hz")
                           .arg(std::log2(zero->hz / pole->hz), 0, 'f', 2)
                           .arg(zero->bw_hz, 0, 'f', 0)
                     : QStringLiteral("Z  —"));
  update();
}

void SectionReadout::showPole() {
  hz_->setText(live_ ? QString::number(pole_hz_, 'f', 0) : QStringLiteral("—"));
  bw_->setText(live_ ? QString::number(pole_bw_hz_, 'f', 0) : QStringLiteral("—"));
}

void SectionReadout::commit(QLineEdit* field) {
  if (committing_ || !live_) return;
  committing_ = true;
  const auto text = field->text();
  field->clearFocus();
  showPole();
  committing_ = false;
  bool ok = false;
  const auto typed = text.toDouble(&ok);
  if (!ok || typed <= 0.0) return;
  const auto hz = field == hz_ ? std::max(typed, 20.0) : pole_hz_;
  const auto bw_hz = field == bw_ ? typed : pole_bw_hz_;
  if (std::abs(hz - pole_hz_) < 0.5 && std::abs(bw_hz - pole_bw_hz_) < 0.5) return;
  emit poleEdited(section_, hz, bw_hz);
}

bool SectionReadout::eventFilter(QObject* watched, QEvent* event) {
  if ((watched == hz_ || watched == bw_) && live_) {
    if (event->type() == QEvent::FocusIn) {
      static_cast<QLineEdit*>(watched)->selectAll();
    } else if (event->type() == QEvent::KeyPress &&
               static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
      committing_ = true;
      showPole();
      static_cast<QLineEdit*>(watched)->clearFocus();
      committing_ = false;
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void SectionReadout::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), kChassis);
  painter.setPen(live_ ? section_color(section_) : kLine);
  painter.drawLine(0, 0, width(), 0);
}
