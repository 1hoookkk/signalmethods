#include "space_dock.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

namespace {

namespace p2k = trench::core::p2k;

QDoubleSpinBox* spin(QWidget* parent, const char* name, double lo, double hi, double step,
                     int decimals, const QString& suffix) {
  auto* box = new QDoubleSpinBox(parent);
  box->setObjectName(QString::fromLatin1(name));
  box->setRange(lo, hi);
  box->setSingleStep(step);
  box->setDecimals(decimals);
  box->setSuffix(suffix);
  box->setKeyboardTracking(false);
  box->setFixedWidth(92);
  return box;
}

QLabel* tag(QWidget* parent, const QString& text) {
  auto* label = new QLabel(text, parent);
  label->setStyleSheet(QStringLiteral("color: #767f83;"));
  return label;
}

}  // namespace

SpaceDock::SpaceDock(QWidget* parent) : QWidget(parent) {
  setFixedHeight(30);
  setStyleSheet(QStringLiteral(
      "QDoubleSpinBox, QComboBox { background: #1a1f23; color: #aebabe; border: 1px solid #373f43; "
      "border-radius: 2px; padding: 0 4px; }"));
  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(12, 2, 12, 2);
  row->setSpacing(6);
  lo_ = spin(this, "spaceLo", 10.0, 20000.0, 10.0, 0, QStringLiteral(" Hz"));
  hi_ = spin(this, "spaceHi", 100.0, 24000.0, 100.0, 0, QStringLiteral(" Hz"));
  weight_ = new QComboBox(this);
  weight_->setObjectName(QStringLiteral("spaceWeight"));
  weight_->addItems({QStringLiteral("ERB"), QStringLiteral("FLAT")});
  smooth_ = spin(this, "spaceSmooth", 0.0, 2.0, 0.05, 2, QStringLiteral(" oct"));
  band_lo_ = spin(this, "bandLo", 10.0, 22000.0, 10.0, 0, QStringLiteral(" Hz"));
  band_hi_ = spin(this, "bandHi", 10.0, 22000.0, 10.0, 0, QStringLiteral(" Hz"));
  band_gain_ = spin(this, "bandGain", 0.0, 16.0, 0.5, 1, QStringLiteral("×"));
  row->addWidget(lo_);
  row->addWidget(tag(this, QStringLiteral("–")));
  row->addWidget(hi_);
  row->addWidget(weight_);
  row->addWidget(smooth_);
  row->addSpacing(10);
  row->addWidget(band_lo_);
  row->addWidget(tag(this, QStringLiteral("–")));
  row->addWidget(band_hi_);
  row->addWidget(band_gain_);
  row->addStretch(1);
  for (auto* box : {lo_, hi_, smooth_, band_lo_, band_hi_, band_gain_}) {
    connect(box, &QDoubleSpinBox::valueChanged, this, [this](double) { relay(); });
  }
  connect(weight_, &QComboBox::currentIndexChanged, this, [this](int) { relay(); });
  setSpace(p2k::PerceptualSpace{});
}

void SpaceDock::setSpace(const p2k::PerceptualSpace& space) {
  updating_ = true;
  lo_->setValue(space.lo_hz);
  hi_->setValue(space.hi_hz);
  weight_->setCurrentIndex(space.weight == p2k::PerceptualSpace::Weight::kErb ? 0 : 1);
  smooth_->setValue(space.smooth_octaves);
  if (space.emphasis.empty()) {
    band_lo_->setValue(10.0);
    band_hi_->setValue(10.0);
    band_gain_->setValue(1.0);
  } else {
    band_lo_->setValue(space.emphasis.front().lo_hz);
    band_hi_->setValue(space.emphasis.front().hi_hz);
    band_gain_->setValue(space.emphasis.front().gain);
  }
  updating_ = false;
}

p2k::PerceptualSpace SpaceDock::space() const {
  p2k::PerceptualSpace out;
  out.lo_hz = lo_->value();
  out.hi_hz = hi_->value();
  out.weight = weight_->currentIndex() == 0 ? p2k::PerceptualSpace::Weight::kErb
                                            : p2k::PerceptualSpace::Weight::kFlat;
  out.smooth_octaves = smooth_->value();
  if (band_hi_->value() > band_lo_->value() && band_gain_->value() != 1.0) {
    out.emphasis.push_back({band_lo_->value(), band_hi_->value(), band_gain_->value()});
  }
  return out;
}

void SpaceDock::relay() {
  if (updating_) return;
  emit spaceEdited(space());
}
