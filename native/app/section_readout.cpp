#include "section_readout.hpp"

#include "section_color.hpp"

#include "trench/core/p2k.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

namespace p2k = trench::core::p2k;

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
  row->setSpacing(12);

  hz_ = make_field(this, "poleHzField");
  hz_->installEventFilter(this);
  row->addWidget(hz_);

  bw_ = make_field(this, "poleWidthField");
  bw_->installEventFilter(this);
  row->addWidget(bw_);

  zero_ = new QLabel(this);
  zero_->setObjectName(QStringLiteral("zeroReadout"));
  zero_->setFont(bar_font());
  zero_->setStyleSheet(QStringLiteral("color: %1;").arg(kDim.name()));
  row->addWidget(zero_, 1);

  connect(hz_, &QLineEdit::editingFinished, this, [this] { commit(hz_); });
  connect(bw_, &QLineEdit::editingFinished, this, [this] { commit(bw_); });

  setReading(0, std::nullopt, p2k::MaskParam{}, false);
}

void SectionReadout::setReading(std::size_t section,
                                const std::optional<p2k::PoleReading>& pole,
                                const p2k::MaskParam& mask, bool zero_live) {
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
  zero_->setText(live_ && zero_live
                     ? QStringLiteral("%1 oct  %2 Hz")
                           .arg(mask.offset_oct, 0, 'f', 2)
                           .arg(mask.zero_bw_hz, 0, 'f', 0)
                     : QStringLiteral("—"));
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
  const auto hz = field == hz_ ? std::clamp(typed, 20.0, p2k::kRootHiHz) : pole_hz_;
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
