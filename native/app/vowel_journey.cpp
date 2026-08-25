#include "vowel_journey.hpp"

#include "trench/core/formants.hpp"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

#include <algorithm>
#include <cmath>

namespace {

QString display_name(std::string_view symbol) {
  if (symbol == "aa") return QStringLiteral("AH");
  if (symbol == "eh") return QStringLiteral("EH");
  if (symbol == "iy") return QStringLiteral("EE");
  if (symbol == "ey") return QStringLiteral("AY");
  if (symbol == "ao") return QStringLiteral("AW");
  if (symbol == "ow") return QStringLiteral("OH");
  if (symbol == "uw") return QStringLiteral("OO");
  if (symbol == "er") return QStringLiteral("ER");
  return QString::fromUtf8(symbol.data(), static_cast<qsizetype>(symbol.size())).toUpper();
}

QLabel* caption(const QString& text, QWidget* parent) {
  auto* label = new QLabel(text, parent);
  label->setStyleSheet(QStringLiteral("color: #767f83;"));
  return label;
}

}  // namespace

VowelJourney::VowelJourney(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("vowelJourney"));
  setFixedHeight(76);
  setStyleSheet(QStringLiteral(
      "VowelJourney { background: #111416; border-left: 1px solid #373f43; "
      "border-bottom: 1px solid #373f43; }"
      "QComboBox { background: #1a1f23; color: #aebabe; border: 1px solid #373f43; "
      "padding: 1px 4px; }"));

  auto* layout = new QGridLayout(this);
  layout->setContentsMargins(6, 5, 6, 5);
  layout->setHorizontalSpacing(5);
  layout->setVerticalSpacing(3);

  from_ = new QComboBox(this);
  from_->setObjectName(QStringLiteral("vowelFrom"));
  from_->setAccessibleName(QStringLiteral("Vowel journey from"));
  to_ = new QComboBox(this);
  to_->setObjectName(QStringLiteral("vowelTo"));
  to_->setAccessibleName(QStringLiteral("Vowel journey to"));
  for (const auto& vowel : trench::core::p2k::klatt_vowels()) {
    const auto symbol = QString::fromUtf8(vowel.symbol.data(),
                                          static_cast<qsizetype>(vowel.symbol.size()));
    if (vowel.symbol == "ah") continue;
    from_->addItem(display_name(vowel.symbol), symbol);
    to_->addItem(display_name(vowel.symbol), symbol);
  }

  q_ = new QSlider(Qt::Horizontal, this);
  q_->setObjectName(QStringLiteral("vowelQBandwidth"));
  q_->setAccessibleName(QStringLiteral("High Q bandwidth scale"));
  q_->setToolTip(QStringLiteral("high-Q bandwidth"));
  q_->setRange(15, 100);
  q_->setValue(50);

  layout->addWidget(caption(QStringLiteral("FROM"), this), 0, 0);
  layout->addWidget(from_, 0, 1);
  layout->addWidget(caption(QStringLiteral("TO"), this), 1, 0);
  layout->addWidget(to_, 1, 1);
  layout->addWidget(caption(QStringLiteral("Q"), this), 2, 0);
  layout->addWidget(q_, 2, 1);

  setEndpoints(QStringLiteral("aa"), QStringLiteral("eh"), 0.5);
  connect(from_, &QComboBox::textActivated, this, [this] { emitJourney(); });
  connect(to_, &QComboBox::textActivated, this, [this] { emitJourney(); });
  connect(q_, &QSlider::sliderReleased, this, &VowelJourney::emitJourney);
}

QString VowelJourney::fromSymbol() const { return from_->currentData().toString(); }

QString VowelJourney::toSymbol() const { return to_->currentData().toString(); }

double VowelJourney::highQBandwidthScale() const noexcept {
  return static_cast<double>(q_->value()) / 100.0;
}

QComboBox* VowelJourney::fromBox() const noexcept { return from_; }

QComboBox* VowelJourney::toBox() const noexcept { return to_; }

QSlider* VowelJourney::qSlider() const noexcept { return q_; }

bool VowelJourney::setEndpoints(const QString& from, const QString& to,
                                double high_q_bandwidth_scale) {
  const int from_index = from_->findData(from);
  const int to_index = to_->findData(to);
  if (from_index < 0 || to_index < 0) return false;
  from_->setCurrentIndex(from_index);
  to_->setCurrentIndex(to_index);
  q_->setValue(std::clamp(static_cast<int>(std::lround(high_q_bandwidth_scale * 100.0)),
                          q_->minimum(), q_->maximum()));
  return true;
}

void VowelJourney::emitJourney() {
  emit journeyChosen(fromSymbol(), toSymbol(), highQBandwidthScale());
}
