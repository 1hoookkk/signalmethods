#include "main_window.hpp"

#include "trench/audio/audio_boundary.hpp"
#include "trench/core/measure.hpp"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using Resonant = trench::core::native::Resonant;

const Resonant& resonant(const trench::core::native::Roots& roots) {
  return std::get<Resonant>(roots);
}

std::string lowerExtension(const std::filesystem::path& path) {
  std::string result = path.extension().string();
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

bool isAudio(const std::string& extension) {
  return extension == ".wav" || extension == ".aif" ||
         extension == ".aiff" || extension == ".flac";
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return {};
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

struct ResponseCurve {
  std::vector<double> frequency_hz;
  std::vector<double> magnitude_db;
};

std::optional<ResponseCurve> readResponseCurve(
    const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return std::nullopt;
  ResponseCurve result;
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::replace(line.begin(), line.end(), ',', ' ');
    std::stringstream row(line);
    double frequency = 0.0;
    double magnitude = 0.0;
    if (!(row >> frequency >> magnitude)) continue;
    if (!(frequency > 0.0) || !std::isfinite(frequency) ||
        !std::isfinite(magnitude)) {
      return std::nullopt;
    }
    result.frequency_hz.push_back(frequency);
    result.magnitude_db.push_back(magnitude);
  }
  if (result.frequency_hz.size() < 2 ||
      !std::is_sorted(result.frequency_hz.begin(),
                      result.frequency_hz.end()) ||
      std::adjacent_find(result.frequency_hz.begin(),
                         result.frequency_hz.end()) !=
          result.frequency_hz.end()) {
    return std::nullopt;
  }
  return result;
}

QDoubleSpinBox* physicalEditor(double low, double high, QWidget* parent) {
  auto* editor = new QDoubleSpinBox(parent);
  editor->setRange(low, high);
  editor->setDecimals(2);
  editor->setSingleStep(1.0);
  editor->setSuffix(QStringLiteral(" Hz"));
  editor->setKeyboardTracking(true);
  editor->setMinimumWidth(150);
  return editor;
}

QWidget* labelledEditor(const QString& label, QDoubleSpinBox* editor,
                        QWidget* parent) {
  auto* widget = new QWidget(parent);
  auto* layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  auto* name = new QLabel(label, widget);
  name->setObjectName(QStringLiteral("fieldName"));
  layout->addWidget(name);
  layout->addWidget(editor);
  return widget;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), audition_(std::make_unique<trench::audio::Audition>()) {
  setWindowTitle(QStringLiteral("TRENCH · 6 × 2P2Z"));
  resize(1180, 860);
  setMinimumSize(900, 700);

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(18, 16, 18, 16);
  layout->setSpacing(12);

  auto* top = new QHBoxLayout;
  top->setSpacing(10);
  auto* load = new QPushButton(QStringLiteral("LOAD REFERENCE"), central);
  reference_label_ = new QLabel(QStringLiteral("NO REFERENCE"), central);
  reference_label_->setObjectName(QStringLiteral("referenceName"));
  audition_button_ = new QPushButton(QStringLiteral("AUDITION"), central);
  audition_button_->setCheckable(true);
  audition_button_->setObjectName(QStringLiteral("audition"));
  top->addWidget(load);
  top->addWidget(reference_label_);
  top->addStretch(1);
  top->addWidget(audition_button_);
  layout->addLayout(top);

  cascade_plot_ = new CascadePlot(central);
  layout->addWidget(cascade_plot_, 1);

  auto* section_row = new QHBoxLayout;
  section_row->setSpacing(7);
  section_label_ = new QLabel(QStringLiteral("SECTION 1 / 6"), central);
  section_label_->setObjectName(QStringLiteral("sectionTitle"));
  section_row->addWidget(section_label_);
  section_row->addStretch(1);
  auto* section_group = new QButtonGroup(this);
  section_group->setExclusive(true);
  for (std::size_t index = 0; index < section_buttons_.size(); ++index) {
    auto* button = new QPushButton(QString::number(index + 1), central);
    button->setCheckable(true);
    button->setObjectName(QStringLiteral("sectionButton"));
    button->setFixedSize(42, 30);
    section_group->addButton(button, static_cast<int>(index));
    section_buttons_[index] = button;
    section_row->addWidget(button);
    connect(button, &QPushButton::clicked, this,
            [this, index] { state_.selectSection(index); });
  }
  section_buttons_.front()->setChecked(true);
  layout->addLayout(section_row);

  armadillo_editor_ = new ArmadilloEditor(&state_, central);
  layout->addWidget(armadillo_editor_, 1);

  auto* inspector = new QHBoxLayout;
  inspector->setSpacing(12);
  pole_frequency_ = physicalEditor(EditorState::kLowHz, EditorState::kHighHz, central);
  pole_bandwidth_ = physicalEditor(EditorState::kMinBandwidthHz,
                                   EditorState::kMaxBandwidthHz, central);
  zero_frequency_ = physicalEditor(EditorState::kLowHz, EditorState::kHighHz, central);
  zero_bandwidth_ = physicalEditor(EditorState::kMinBandwidthHz,
                                   EditorState::kMaxBandwidthHz, central);
  inspector->addWidget(labelledEditor(QStringLiteral("POLE FREQUENCY"),
                                      pole_frequency_, central));
  inspector->addWidget(labelledEditor(QStringLiteral("POLE BANDWIDTH"),
                                      pole_bandwidth_, central));
  inspector->addWidget(labelledEditor(QStringLiteral("ZERO FREQUENCY"),
                                      zero_frequency_, central));
  inspector->addWidget(labelledEditor(QStringLiteral("ZERO BANDWIDTH"),
                                      zero_bandwidth_, central));
  status_label_ = new QLabel(QStringLiteral("44,100 Hz DSP"), central);
  status_label_->setObjectName(QStringLiteral("status"));
  inspector->addStretch(1);
  inspector->addWidget(status_label_, 0, Qt::AlignBottom);
  layout->addLayout(inspector);

  setCentralWidget(central);
  setStyleSheet(QStringLiteral(R"(
    QMainWindow, QWidget { background: #0c0f11; color: #b2bec1; }
    QPushButton { background: #171d20; border: 1px solid #354044;
                  border-radius: 2px; padding: 7px 13px; color: #dce5e7; }
    QPushButton:hover { border-color: #42e0cf; }
    QPushButton:checked { background: #173c3a; border-color: #42e0cf;
                          color: #42e0cf; }
    QPushButton#audition:checked { background: #5b3423; border-color: #dd8e55;
                                   color: #dd8e55; }
    QLabel#referenceName { color: #dd8e55; }
    QLabel#sectionTitle { color: #dce5e7; font-weight: 600; letter-spacing: 1px; }
    QLabel#fieldName { color: #758286; font-size: 10px; letter-spacing: 1px; }
    QLabel#status { color: #758286; }
    QDoubleSpinBox { background: #111619; border: 1px solid #354044;
                     padding: 6px; color: #e5edef; selection-background-color: #245b56; }
  )"));

  connect(load, &QPushButton::clicked, this, &MainWindow::chooseReference);
  connect(audition_button_, &QPushButton::toggled, this,
          &MainWindow::setAudition);
  connect(&state_, &EditorState::changed, this, &MainWindow::refresh);
  connect(&state_, &EditorState::selectionChanged, this,
          [this] { refreshInspector(); });

  connect(pole_frequency_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            const auto& root = resonant(state_.section(state_.selectedSection()).pole);
            state_.setRoot(state_.selectedSection(), EditorState::Lane::kPole,
                           value, root.bw_hz);
          });
  connect(pole_bandwidth_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            const auto& root = resonant(state_.section(state_.selectedSection()).pole);
            state_.setRoot(state_.selectedSection(), EditorState::Lane::kPole,
                           root.hz, value);
          });
  connect(zero_frequency_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            const auto& root = resonant(state_.section(state_.selectedSection()).zero);
            state_.setRoot(state_.selectedSection(), EditorState::Lane::kZero,
                           value, root.bw_hz);
          });
  connect(zero_bandwidth_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          [this](double value) {
            const auto& root = resonant(state_.section(state_.selectedSection()).zero);
            state_.setRoot(state_.selectedSection(), EditorState::Lane::kZero,
                           root.hz, value);
          });

  refresh();
}

MainWindow::~MainWindow() {
  if (audition_) audition_->stop();
}

void MainWindow::chooseReference() {
  const QString chosen = QFileDialog::getOpenFileName(
      this, QStringLiteral("Load one response reference"), QString(),
      QStringLiteral("Response reference (*.csv *.txt *.wav *.aif *.aiff *.flac *.body240 *.bin)"));
  if (!chosen.isEmpty()) {
    loadReference(std::filesystem::path(chosen.toStdWString()));
  }
}

bool MainWindow::loadReference(const std::filesystem::path& path) {
  try {
    Reference reference;
    reference.name = QString::fromStdWString(path.filename().wstring());
    const std::string extension = lowerExtension(path);
    if (extension == ".csv" || extension == ".txt") {
      const auto curve = readResponseCurve(path);
      if (!curve) throw std::runtime_error("response file is not readable");
      reference.frequency_hz = curve->frequency_hz;
      reference.magnitude_db = curve->magnitude_db;
    } else if (isAudio(extension)) {
      const auto clip = trench::audio::decode_mono(path);
      if (!clip) throw std::runtime_error("audio file is not readable");
      const std::size_t analysis_frames = std::min(
          clip->samples.size(), static_cast<std::size_t>(
                                    std::llround(2.0 * clip->sample_rate_hz)));
      const std::size_t analysis_start =
          (clip->samples.size() - analysis_frames) / 2;
      const auto envelope = trench::core::measure::spectral_envelope(
          std::span<const float>(clip->samples).subspan(analysis_start,
                                                        analysis_frames),
          clip->sample_rate_hz);
      reference.frequency_hz = trench::core::logarithmic_frequency_grid(
          EditorState::kLowHz, EditorState::kHighHz, 640);
      reference.magnitude_db = trench::core::measure::target_on_grid(
          envelope, reference.frequency_hz);
      reference.clip = *clip;
    } else if (extension == ".body240" || extension == ".bin") {
      const auto bytes = readBytes(path);
      if (bytes.size() != trench::core::kLegacyBodyBytes) {
        throw std::runtime_error("packed reference must be 240 bytes");
      }
      const auto body = trench::core::native::import_p2k(bytes);
      const auto cascade = trench::core::native::cascade(
          trench::core::native::design(body.corners.front(), EditorState::kDatumHz),
          body.corners.front().gain_db);
      reference.frequency_hz = trench::core::logarithmic_frequency_grid(
          EditorState::kLowHz, EditorState::kHighHz, 640);
      reference.magnitude_db.reserve(reference.frequency_hz.size());
      const std::span<const trench::core::Biquad> six_sections{
          cascade.data(), trench::core::native::kSections};
      for (const double hz : reference.frequency_hz) {
        reference.magnitude_db.push_back(trench::core::cascade_response_db(
            six_sections, hz, EditorState::kDatumHz));
      }
    } else {
      throw std::runtime_error("unsupported reference type");
    }
    setReference(std::move(reference));
    return true;
  } catch (const std::exception& error) {
    status_label_->setText(QStringLiteral("REFERENCE ERROR · %1")
                               .arg(QString::fromUtf8(error.what())));
    return false;
  }
}

void MainWindow::setReference(Reference reference) {
  reference_ = std::move(reference);
  reference_label_->setText(reference_->name.toUpper());
  cascade_plot_->setReference(reference_->name, reference_->frequency_hz,
                              reference_->magnitude_db);
  status_label_->setText(QStringLiteral("REFERENCE LOADED · %1 POINTS")
                             .arg(reference_->frequency_hz.size()));
  if (audition_button_->isChecked()) {
    if (reference_->clip) {
      audition_->setClip(clipForDevice(*reference_->clip));
    } else {
      audition_->setSaw(73.42, 0.18F);
    }
  }
}

void MainWindow::refresh() {
  cascade_plot_->setCascade(state_.cascade(), EditorState::kDatumHz);
  refreshInspector();
  updateAuditionView();
}

void MainWindow::refreshInspector() {
  const std::size_t selected = state_.selectedSection();
  const auto& section = state_.section(selected);
  const auto& pole = resonant(section.pole);
  const auto& zero = resonant(section.zero);
  const QSignalBlocker pole_frequency_blocker(pole_frequency_);
  const QSignalBlocker pole_bandwidth_blocker(pole_bandwidth_);
  const QSignalBlocker zero_frequency_blocker(zero_frequency_);
  const QSignalBlocker zero_bandwidth_blocker(zero_bandwidth_);
  pole_frequency_->setValue(pole.hz);
  pole_bandwidth_->setValue(pole.bw_hz);
  zero_frequency_->setValue(zero.hz);
  zero_bandwidth_->setValue(zero.bw_hz);
  section_label_->setText(QStringLiteral("SECTION %1 / 6").arg(selected + 1));
  section_buttons_[selected]->setChecked(true);
  armadillo_editor_->update();
}

void MainWindow::updateAuditionView() {
  if (!audition_) return;
  audition_->setCorner(state_.corner());
}

trench::audio::MonoClip MainWindow::clipForDevice(
    const trench::audio::MonoClip& clip) const {
  const double target_rate = audition_->sampleRateHz();
  if (!(clip.sample_rate_hz > 0.0) || !(target_rate > 0.0) ||
      clip.samples.empty() || std::abs(clip.sample_rate_hz - target_rate) < 0.5) {
    return clip;
  }
  trench::audio::MonoClip result;
  result.sample_rate_hz = target_rate;
  const std::size_t frames = std::max<std::size_t>(
      1, static_cast<std::size_t>(std::llround(
             static_cast<double>(clip.samples.size()) * target_rate /
             clip.sample_rate_hz)));
  result.samples.resize(frames);
  const double source_step = clip.sample_rate_hz / target_rate;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const double position = static_cast<double>(frame) * source_step;
    const std::size_t first = std::min(
        static_cast<std::size_t>(position), clip.samples.size() - 1);
    const std::size_t second = std::min(first + 1, clip.samples.size() - 1);
    const float fraction = static_cast<float>(position - static_cast<double>(first));
    result.samples[frame] = std::lerp(clip.samples[first], clip.samples[second], fraction);
  }
  return result;
}

void MainWindow::setAudition(bool enabled) {
  if (!enabled) {
    audition_->setGate(false);
    audition_->stop();
    status_label_->setText(QStringLiteral("AUDITION CLOSED"));
    return;
  }
  const std::string error = audition_->start();
  if (!error.empty()) {
    const QSignalBlocker blocker(audition_button_);
    audition_button_->setChecked(false);
    status_label_->setText(QStringLiteral("AUDIO DEVICE · %1")
                               .arg(QString::fromStdString(error)));
    return;
  }
  updateAuditionView();
  if (reference_ && reference_->clip) {
    audition_->setClip(clipForDevice(*reference_->clip));
  } else {
    audition_->setSaw(73.42, 0.18F);
  }
  audition_->setGate(true);
  status_label_->setText(QStringLiteral("AUDITION OPEN · %1 Hz")
                             .arg(audition_->sampleRateHz(), 0, 'f', 0));
}

void MainWindow::closeEvent(QCloseEvent* event) {
  audition_->setGate(false);
  audition_->stop();
  QMainWindow::closeEvent(event);
}
