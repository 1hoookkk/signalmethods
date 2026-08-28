#include "section_readout.hpp"

#include "section_color.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

const QColor kChassis{26, 31, 35};
const QColor kInk{174, 186, 190};
const QColor kDim{118, 127, 131};
const QColor kLine{55, 63, 67};

constexpr int kBarHeight = 26;
constexpr int kFieldWidth = 56;

const QString kFieldStyle = QStringLiteral(
    "QLineEdit { background: transparent; color: %1; border: none; "
    "border-bottom: 1px solid #2a3134; padding: 0; }"
    "QLineEdit:hover { border-bottom: 1px solid #57decd; }"
    "QLineEdit:focus { background: #111416; border: 1px solid #373f43; }");

QFont bar_font() { return QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold); }

QLineEdit* make_field(QWidget* parent, const char* name, const QString& accessible) {
  auto* field = new QLineEdit(parent);
  field->setObjectName(QString::fromLatin1(name));
  field->setAccessibleName(accessible);
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
  row->setSpacing(5);

  const auto add_caption = [this, row](const QString& text) {
    auto* caption = new QLabel(text, this);
    caption->setFont(bar_font());
    caption->setStyleSheet(QStringLiteral("color: %1;").arg(kDim.name()));
    row->addWidget(caption);
    return caption;
  };

  head_ = add_caption(QStringLiteral("S1"));
  head_->setObjectName(QStringLiteral("sectionHead"));

  add_caption(QStringLiteral("P"));
  pole_hz_ = make_field(this, "poleHzField", QStringLiteral("Pole frequency in hertz"));
  row->addWidget(pole_hz_);
  add_caption(QStringLiteral("Hz"));
  add_caption(QStringLiteral("·  BW"));
  pole_bw_ = make_field(this, "poleBwField", QStringLiteral("Pole bandwidth in hertz"));
  row->addWidget(pole_bw_);
  add_caption(QStringLiteral("Hz"));

  row->addSpacing(18);

  add_caption(QStringLiteral("Z"));
  zero_hz_ = make_field(this, "zeroHzField", QStringLiteral("Zero frequency in hertz"));
  row->addWidget(zero_hz_);
  add_caption(QStringLiteral("Hz"));
  add_caption(QStringLiteral("·  BW"));
  zero_bw_ = make_field(this, "zeroBwField", QStringLiteral("Zero bandwidth in hertz"));
  row->addWidget(zero_bw_);
  add_caption(QStringLiteral("Hz"));
  row->addStretch(1);

  for (auto* field : {pole_hz_, pole_bw_, zero_hz_, zero_bw_}) {
    field->installEventFilter(this);
    connect(field, &QLineEdit::editingFinished, this, [this, field] { commit(field); });
  }
  setReading(0, std::nullopt, std::nullopt);
}

bool SectionReadout::isZeroField(const QObject* field) const {
  return field == zero_hz_ || field == zero_bw_;
}

void SectionReadout::setReading(std::size_t section,
                                const std::optional<trench::core::native::Resonant>& pole,
                                const std::optional<trench::core::native::Resonant>& zero,
                                bool zero_selected) {
  section_ = section;
  pole_ = pole;
  zero_ = zero;
  zero_selected_ = zero_selected;
  head_->setText(QStringLiteral("S%1").arg(section_ + 1));
  const auto hue = (pole_ ? section_color(section_) : kDim).name();
  pole_hz_->setStyleSheet(kFieldStyle.arg(hue));
  pole_bw_->setStyleSheet(kFieldStyle.arg(hue));
  const auto zero_hue = (zero_ && zero_selected_ ? section_color(section_) : kDim).name();
  zero_hz_->setStyleSheet(kFieldStyle.arg(zero_hue));
  zero_bw_->setStyleSheet(kFieldStyle.arg(zero_hue));
  pole_hz_->setReadOnly(!pole_);
  pole_bw_->setReadOnly(!pole_);
  zero_hz_->setReadOnly(!zero_ || !zero_selected_);
  zero_bw_->setReadOnly(!zero_ || !zero_selected_);
  showValues();
  update();
}

void SectionReadout::showValues() {
  const auto text = [](const std::optional<trench::core::native::Resonant>& value,
                       bool width) {
    if (!value) return QStringLiteral("—");
    return QString::number(width ? value->bw_hz : value->hz, 'f', 0);
  };
  const bool force = committing_;
  if (force || !pole_hz_->hasFocus()) pole_hz_->setText(text(pole_, false));
  if (force || !pole_bw_->hasFocus()) pole_bw_->setText(text(pole_, true));
  if (force || !zero_hz_->hasFocus()) zero_hz_->setText(text(zero_, false));
  if (force || !zero_bw_->hasFocus()) zero_bw_->setText(text(zero_, true));
}

void SectionReadout::commit(QLineEdit* field) {
  if (committing_) return;
  const bool zero_lane = isZeroField(field);
  const auto& value = zero_lane ? zero_ : pole_;
  if (!value || (zero_lane && !zero_selected_)) return;
  committing_ = true;
  const auto text = field->text();
  field->clearFocus();
  showValues();
  committing_ = false;
  bool ok = false;
  const auto typed = text.toDouble(&ok);
  if (!ok || typed <= 0.0) return;
  const bool width = field == pole_bw_ || field == zero_bw_;
  const auto hz = width ? value->hz : std::max(typed, 20.0);
  const auto bw_hz = width ? typed : value->bw_hz;
  if (std::abs(hz - value->hz) < 0.5 && std::abs(bw_hz - value->bw_hz) < 0.5) return;
  if (zero_lane) {
    emit zeroEdited(section_, hz, bw_hz);
  } else {
    emit poleEdited(section_, hz, bw_hz);
  }
}

bool SectionReadout::eventFilter(QObject* watched, QEvent* event) {
  auto* field = qobject_cast<QLineEdit*>(watched);
  if (field != nullptr && !field->isReadOnly()) {
    if (event->type() == QEvent::FocusIn) {
      field->selectAll();
    } else if (event->type() == QEvent::KeyPress &&
               static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
      committing_ = true;
      showValues();
      field->clearFocus();
      committing_ = false;
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void SectionReadout::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), kChassis);
  painter.setPen(pole_ ? section_color(section_) : kLine);
  painter.drawLine(0, 0, width(), 0);
}
