#include "section_desk.hpp"

#include <QDoubleSpinBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <cmath>
#include <variant>

namespace {

using Resonant = trench::core::native::Resonant;

constexpr double kPairedOctaves = 0.02;

const Resonant& resonant(const trench::core::native::Roots& roots) {
  return std::get<Resonant>(roots);
}

bool pairedBell(const EditorState& state, std::size_t index) {
  if (!state.rootPresent(index, EditorState::Lane::kZero)) return false;
  const auto& section = state.section(index);
  return std::abs(std::log2(resonant(section.pole).hz /
                            resonant(section.zero).hz)) < kPairedOctaves;
}

QFont captionFont(const QWidget* base) { return base->font(); }

QFont valueFont(const QWidget* base) { return base->font(); }

QDoubleSpinBox* deskEditor(double low, double high, double step,
                           const QString& suffix, QWidget* parent) {
  auto* editor = new QDoubleSpinBox(parent);
  editor->setRange(low, high);
  editor->setDecimals(1);
  editor->setSingleStep(step);
  editor->setSuffix(suffix);
  editor->setKeyboardTracking(true);
  editor->setFixedSize(110, 26);
  editor->setFont(valueFont(editor));
  return editor;
}

QWidget* column(const QString& caption, QDoubleSpinBox* editor,
                QWidget* parent) {
  auto* widget = new QWidget(parent);
  auto* layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(3);
  auto* name = new QLabel(caption, widget);
  name->setObjectName(QStringLiteral("fieldName"));
  name->setFont(captionFont(name));
  layout->addWidget(name);
  layout->addWidget(editor);
  return widget;
}

}  // namespace

// SECTIONS ARE THEIR OWN PANEL (Tyson 2026-08-29): the parametric voice opens
// beside the app the way the UltraProteus filter page does, one FC / BW / GAIN
// row per section, while the main window keeps the raw pole and zero.
SectionDesk::SectionDesk(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setWindowFlag(Qt::Tool);
  setWindowTitle(QStringLiteral("SECTIONS"));
  setObjectName(QStringLiteral("sectionDesk"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(12);

  for (std::size_t index = 0; index < rows_.size(); ++index) {
    Row& row = rows_[index];
    row.frame = new QWidget(this);
    auto* line = new QHBoxLayout(row.frame);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(10);

    auto* ordinal = new QLabel(QString::number(index + 1), row.frame);
    QFont number = ordinal->font();
    number.setPixelSize(11);
    number.setWeight(QFont::DemiBold);
    ordinal->setFont(number);
    ordinal->setFixedWidth(18);
    line->addWidget(ordinal, 0, Qt::AlignBottom);

    row.centre = deskEditor(EditorState::kLowHz, EditorState::kNyquistHz, 1.0,
                            QStringLiteral(" Hz"), row.frame);
    row.width = deskEditor(EditorState::kMinBandwidthHz,
                           EditorState::kMaxBandwidthHz, 1.0,
                           QStringLiteral(" Hz"), row.frame);
    row.gain = deskEditor(-40.0, 40.0, 0.1, QStringLiteral(" dB"), row.frame);
    line->addWidget(column(QStringLiteral("FC"), row.centre, row.frame));
    line->addWidget(column(QStringLiteral("BW"), row.width, row.frame));
    line->addWidget(column(QStringLiteral("GAIN"), row.gain, row.frame));
    layout->addWidget(row.frame);

    connect(row.centre, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this, index](double value) { writeCentre(index, value); });
    connect(row.width, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this, index](double value) { writeWidth(index, value); });
    connect(row.gain, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this, index](double value) { writeGain(index, value); });
  }

  connect(state_, &EditorState::changed, this, [this] { refreshRows(); });
  connect(state_, &EditorState::selectionChanged, this,
          [this] { refreshRows(); });

  refreshRows();
  layout->activate();
  setFixedSize(sizeHint());
}

void SectionDesk::refreshRows() {
  updating_ = true;
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    const auto& section = state_->section(index);
    const auto& pole = resonant(section.pole);
    const bool bell = pairedBell(*state_, index);
    Row& row = rows_[index];
    if (bell) {
      const auto& zero = resonant(section.zero);
      row.centre->setValue(std::sqrt(pole.hz * zero.hz));
      row.width->setValue(std::sqrt(pole.bw_hz * zero.bw_hz));
      row.gain->setValue(20.0 * std::log10(zero.bw_hz / pole.bw_hz));
    } else {
      row.centre->setValue(pole.hz);
      row.width->setValue(pole.bw_hz);
      row.gain->setValue(0.0);
    }
    row.centre->setEnabled(bell);
    row.width->setEnabled(bell);
    row.gain->setEnabled(bell);
    row.frame->setEnabled(state_->sectionEnabled(index));
  }
  updating_ = false;
}

void SectionDesk::writeCentre(std::size_t index, double value) {
  if (updating_ || !pairedBell(*state_, index)) return;
  const auto& section = state_->section(index);
  const double pole_bandwidth = resonant(section.pole).bw_hz;
  const double zero_bandwidth = resonant(section.zero).bw_hz;
  state_->selectSection(index);
  state_->setRoot(index, EditorState::Lane::kPole, value, pole_bandwidth);
  state_->setRoot(index, EditorState::Lane::kZero, value, zero_bandwidth);
}

void SectionDesk::writeWidth(std::size_t index, double value) {
  if (updating_ || !pairedBell(*state_, index)) return;
  const auto& section = state_->section(index);
  const auto& pole = resonant(section.pole);
  const auto& zero = resonant(section.zero);
  const double centre = std::sqrt(pole.hz * zero.hz);
  const double skirt = std::sqrt(zero.bw_hz / pole.bw_hz);
  state_->selectSection(index);
  state_->setRoot(index, EditorState::Lane::kPole, centre, value / skirt);
  state_->setRoot(index, EditorState::Lane::kZero, centre, value * skirt);
}

void SectionDesk::writeGain(std::size_t index, double value) {
  if (updating_ || !pairedBell(*state_, index)) return;
  const auto& section = state_->section(index);
  const auto& pole = resonant(section.pole);
  const auto& zero = resonant(section.zero);
  const double centre = std::sqrt(pole.hz * zero.hz);
  const double width = std::sqrt(pole.bw_hz * zero.bw_hz);
  const double skirt = std::pow(10.0, value / 40.0);
  state_->selectSection(index);
  state_->setRoot(index, EditorState::Lane::kPole, centre, width / skirt);
  state_->setRoot(index, EditorState::Lane::kZero, centre, width * skirt);
}
