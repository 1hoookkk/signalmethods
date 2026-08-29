#include "main_window.hpp"

#include "body_io.hpp"
#include "gesture_dial.hpp"
#include "template_shelf.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/measure.hpp"

#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
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

using NumericRow = std::array<double, 2>;

std::vector<NumericRow> readNumericRows(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return {};
  std::vector<NumericRow> rows;
  std::string line;
  while (std::getline(stream, line)) {
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) continue;
    if (line[first] == '#' || line[first] == ';') continue;
    std::replace(line.begin(), line.end(), ',', ' ');
    std::stringstream row(line);
    double column_one = 0.0;
    double column_two = 0.0;
    if (!(row >> column_one >> column_two)) continue;
    if (!std::isfinite(column_one) || !std::isfinite(column_two)) return {};
    rows.push_back({column_one, column_two});
  }
  return rows;
}

bool isResponseTable(const std::vector<NumericRow>& rows) {
  if (rows.size() < 2) return false;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    if (!(rows[index][0] > 0.0)) return false;
    if (index > 0 && !(rows[index][0] > rows[index - 1][0])) return false;
  }
  return std::any_of(rows.begin(), rows.end(),
                     [](const NumericRow& row) { return row[1] < 0.0; });
}

bool isPoleMaterial(const std::vector<NumericRow>& rows) {
  return !rows.empty() &&
         std::all_of(rows.begin(), rows.end(), [](const NumericRow& row) {
           return row[0] > 0.0 && row[1] > 0.0;
         });
}

QFont captionFont(const QWidget* base) {
  QFont font = base->font();
  font.setPixelSize(10);
  font.setWeight(QFont::DemiBold);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  return font;
}

QFont valueFont(const QWidget* base) {
  QFont font = base->font();
  font.setPixelSize(13);
  font.setWeight(QFont::Normal);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
  return font;
}

QDoubleSpinBox* physicalEditor(double low, double high, QWidget* parent) {
  auto* editor = new QDoubleSpinBox(parent);
  editor->setRange(low, high);
  editor->setDecimals(2);
  editor->setSingleStep(1.0);
  editor->setSuffix(QStringLiteral(" Hz"));
  editor->setKeyboardTracking(true);
  editor->setMinimumWidth(150);
  editor->setFont(valueFont(editor));
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
  name->setFont(captionFont(name));
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
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  auto* top = new QHBoxLayout;
  top->setSpacing(10);
  auto* load = new QPushButton(QStringLiteral("OPEN"), central);
  auto* save = new QPushButton(QStringLiteral("SAVE"), central);
  reference_label_ = new QLabel(QStringLiteral("NO REFERENCE"), central);
  reference_label_->setObjectName(QStringLiteral("referenceName"));
  tilt_button_ = new QPushButton(QStringLiteral("TILT"), central);
  tilt_button_->setCheckable(true);
  tilt_button_->setObjectName(QStringLiteral("tiltSwitch"));
  tilt_button_->setEnabled(false);
  template_shelf_ = new QComboBox(central);
  template_shelf_->setObjectName(QStringLiteral("templateShelf"));
  template_shelf_->addItem(QStringLiteral("TEMPLATE"));
  for (const auto& entry : trench::app::kTemplateShelf) {
    template_shelf_->addItem(QString::fromUtf8(entry.name));
  }
  overlay_shelf_ = new QComboBox(central);
  overlay_shelf_->setObjectName(QStringLiteral("overlayShelf"));
  overlay_shelf_->addItem(QStringLiteral("OVERLAY"));
  for (const auto& entry : trench::app::kTemplateShelf) {
    overlay_shelf_->addItem(QString::fromUtf8(entry.name));
  }
  load->setFont(captionFont(load));
  save->setFont(captionFont(save));
  tilt_button_->setFont(captionFont(tilt_button_));
  template_shelf_->setFont(captionFont(template_shelf_));
  overlay_shelf_->setFont(captionFont(overlay_shelf_));
  reference_label_->setFont(valueFont(reference_label_));
  top->addWidget(load);
  top->addWidget(template_shelf_);
  top->addWidget(reference_label_);
  top->addWidget(tilt_button_);
  top->addWidget(overlay_shelf_);
  top->addStretch(1);
  top->addWidget(save);
  layout->addLayout(top);

  // THE EDITOR IS THE SURFACE (Tyson 2026-08-28 "direct armadillo editor"):
  // the cascade is a fixed monitor above, the plane below owns the height.
  cascade_plot_ = new CascadePlot(central);
  cascade_plot_->setFixedHeight(170);
  morph_pad_ = new MorphPad(&state_, central);
  auto* interior = new QHBoxLayout;
  interior->setSpacing(12);
  interior->addWidget(cascade_plot_, 3);
  interior->addWidget(morph_pad_, 0, Qt::AlignTop);
  layout->addLayout(interior);

  candidate_lane_ = new CandidateLane(central);
  layout->addWidget(candidate_lane_);

  armadillo_editor_ = new ArmadilloEditor(&state_, central);
  layout->addWidget(armadillo_editor_, 1);

  auto* gestures = new QHBoxLayout;
  gestures->setSpacing(10);
  auto* transpose = new GestureDial(
      QStringLiteral("TRANSPOSE"), 0.03,
      [](double total) { return QString::asprintf("%+.1f st", total); },
      central);
  transpose->onDelta = [this](double delta) {
    state_.applyAffine(delta, 1.0, 1.0, 1.0);
  };
  gestures->addWidget(transpose);
  const auto addRatioDial = [central, gestures](
                                const QString& label, double units_per_pixel,
                                int decimals, std::function<void(double)> apply) {
    auto product = std::make_shared<double>(1.0);
    auto* dial = new GestureDial(
        label, units_per_pixel,
        [product, decimals](double total) {
          if (total == 0.0) *product = 1.0;
          return QStringLiteral("x") + QString::number(*product, 'f', decimals);
        },
        central);
    dial->onDelta = [product, apply = std::move(apply)](double delta) {
      *product *= 1.0 + delta;
      apply(delta);
    };
    gestures->addWidget(dial);
  };
  addRatioDial(QStringLiteral("TRACT"), 0.0015, 3, [this](double delta) {
    state_.applyAffine(0.0, 1.0 + delta, 1.0, 1.0);
  });
  addRatioDial(QStringLiteral("CHARACTER"), 0.002, 2, [this](double delta) {
    state_.applyAffine(0.0, 1.0, 1.0 + delta, 1.0);
  });
  addRatioDial(QStringLiteral("EXAGGERATE"), 0.002, 2, [this](double delta) {
    state_.applyAffine(0.0, 1.0, 1.0, 1.0 + delta);
  });
  gestures->addStretch(1);
  layout->addLayout(gestures);

  section_strip_ = new SectionStrip(&state_, central);
  layout->addWidget(section_strip_);

  auto* inspector = new QHBoxLayout;
  inspector->setSpacing(12);
  pole_frequency_ = physicalEditor(EditorState::kLowHz, EditorState::kNyquistHz, central);
  pole_bandwidth_ = physicalEditor(EditorState::kMinBandwidthHz,
                                   EditorState::kMaxBandwidthHz, central);
  zero_frequency_ = physicalEditor(EditorState::kLowHz, EditorState::kNyquistHz, central);
  zero_bandwidth_ = physicalEditor(EditorState::kMinBandwidthHz,
                                   EditorState::kMaxBandwidthHz, central);
  inspector->addWidget(labelledEditor(QStringLiteral("POLE FREQUENCY"),
                                      pole_frequency_, central));
  inspector->addWidget(labelledEditor(QStringLiteral("POLE BANDWIDTH"),
                                      pole_bandwidth_, central));
  zero_frequency_group_ = labelledEditor(QStringLiteral("ZERO FREQUENCY"),
                                         zero_frequency_, central);
  zero_bandwidth_group_ = labelledEditor(QStringLiteral("ZERO BANDWIDTH"),
                                         zero_bandwidth_, central);
  inspector->addWidget(zero_frequency_group_);
  inspector->addWidget(zero_bandwidth_group_);
  status_label_ = new QLabel(QStringLiteral("44,100 Hz DSP"), central);
  status_label_->setObjectName(QStringLiteral("status"));
  status_label_->setFont(captionFont(status_label_));
  inspector->addStretch(1);
  inspector->addWidget(status_label_, 0, Qt::AlignBottom);
  layout->addLayout(inspector);

  setCentralWidget(central);
  setStyleSheet(QStringLiteral(R"(
    QMainWindow, QWidget { background: #edebe6; color: #26241f; }
    QPushButton, QComboBox, QDoubleSpinBox {
        background: #f6f4ef; border: 1px solid #c9c4b8; border-radius: 4px;
        padding: 0px 12px; min-height: 26px; color: #26241f; }
    QPushButton:hover, QComboBox:hover, QDoubleSpinBox:hover {
        border-color: #a8a296; }
    QPushButton:focus, QComboBox:focus, QDoubleSpinBox:focus {
        border-color: #c4674f; }
    QPushButton:disabled, QDoubleSpinBox:disabled {
        color: #a8a296; border-color: #ddd9cf; }
    QPushButton:checked { border-color: #c4674f; color: #c4674f; }
    QPushButton#tiltSwitch:checked { background: #f6f4ef;
                                     border-color: #c4674f; color: #c4674f; }
    QComboBox::drop-down { border: none; width: 18px; }
    QComboBox QAbstractItemView { background: #f6f4ef; border: 1px solid #c9c4b8;
                                  color: #26241f; outline: none;
                                  selection-background-color: #c4674f;
                                  selection-color: #ffffff; }
    QDoubleSpinBox { padding-right: 20px;
                     selection-background-color: #c4674f;
                     selection-color: #f6f4ef; }
    QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
        width: 16px; border: none; background: transparent; }
    QLabel#referenceName { color: #26241f; }
    QLabel#fieldName { color: #8b877c; }
    QLabel#status { color: #8b877c; }
  )"));

  connect(load, &QPushButton::clicked, this, &MainWindow::openFile);
  connect(save, &QPushButton::clicked, this, &MainWindow::saveBody);
  connect(tilt_button_, &QPushButton::toggled, this,
          [this] { applyReferenceView(); });
  connect(overlay_shelf_, &QComboBox::activated, this, [this](int index) {
    if (index <= 0) {
      cascade_plot_->clearFormantMarks();
      return;
    }
    const auto& entry = trench::app::kTemplateShelf[std::size_t(index) - 1];
    std::vector<double> marks;
    for (const auto& pole : entry.poles) {
      if (pole.present) marks.push_back(pole.hz);
    }
    cascade_plot_->setFormantMarks(std::move(marks));
  });
  connect(template_shelf_, &QComboBox::activated, this, [this](int index) {
    if (index < 1) return;
    state_.loadTemplate(trench::app::kTemplateShelf[static_cast<std::size_t>(index - 1)]);
    template_shelf_->setCurrentIndex(0);
  });
  candidate_lane_->onPick = [this](double hz, double bw_hz) {
    const std::size_t section = state_.selectedSection();
    if (!state_.sectionEnabled(section)) state_.toggleSection(section);
    state_.setRoot(section, EditorState::Lane::kPole, hz, bw_hz);
  };
  connect(&state_, &EditorState::changed, this, &MainWindow::refresh);
  connect(&state_, &EditorState::selectionChanged, this,
          [this] { refresh(); });

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

void MainWindow::openFile() {
  const QString chosen = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open sound or data"), QString(),
      QStringLiteral("Sound or data (*.wav *.txt *.csv *.fbw);;All files (*)"));
  if (chosen.isEmpty()) return;
  const std::filesystem::path path(chosen.toStdWString());
  if (isAudio(lowerExtension(path))) {
    loadReference(path);
    return;
  }
  const auto rows = readNumericRows(path);
  if (isResponseTable(rows)) {
    Reference reference;
    reference.name = QString::fromStdWString(path.filename().wstring());
    for (const auto& row : rows) {
      reference.frequency_hz.push_back(row[0]);
      reference.magnitude_db.push_back(row[1]);
    }
    setReference(std::move(reference));
    status_label_->setText(
        QStringLiteral("RESPONSE · %1 POINTS").arg(rows.size()));
    return;
  }
  if (isPoleMaterial(rows)) {
    std::vector<std::pair<double, double>> poles;
    poles.reserve(rows.size());
    for (const auto& row : rows) poles.emplace_back(row[0], row[1]);
    state_.loadPoles(poles);
    status_label_->setText(
        QStringLiteral("POLES · %1 SECTIONS")
            .arg(std::min(poles.size(), trench::core::native::kSections)));
    return;
  }
  loadReference(path);
}

void MainWindow::saveBody() {
  const QString folder =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
      QStringLiteral("/TRENCH/bodies");
  QDir().mkpath(folder);
  QString chosen = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save one packed body"),
      folder + QStringLiteral("/untitled.body240"),
      QStringLiteral("Packed body (*.body240)"));
  if (chosen.isEmpty()) return;
  if (!chosen.endsWith(QStringLiteral(".body240"), Qt::CaseInsensitive)) {
    chosen += QStringLiteral(".body240");
  }
  const QString refusal = trench::app::saveBody240(state_, chosen);
  status_label_->setText(
      refusal.isEmpty()
          ? QStringLiteral("SAVED · %1").arg(QFileInfo(chosen).fileName().toUpper())
          : refusal);
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
  tilt_button_->setEnabled(true);
  applyReferenceView();
  status_label_->setText(QStringLiteral("REFERENCE LOADED · %1 POINTS")
                             .arg(reference_->frequency_hz.size()));
  if (audition_->running()) {
    if (reference_->clip) {
      audition_->setClip(clipForDevice(*reference_->clip));
    } else {
      audition_->setSaw(73.42, 0.18F);
    }
  }
}

// TILT (Tyson 2026-08-28): whitening lives in the response domain - the
// overlay and the candidates switch together, so what is seen is what is
// picked. The stored reference stays true.
void MainWindow::applyReferenceView() {
  if (!reference_) return;
  const auto& hz = reference_->frequency_hz;
  std::vector<double> view = reference_->magnitude_db;
  if (tilt_button_->isChecked() && hz.size() > 2) {
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < hz.size(); ++i) {
      if (hz[i] <= 0.0) continue;
      const double lx = std::log2(hz[i]);
      sx += lx;
      sy += view[i];
      sxx += lx * lx;
      sxy += lx * view[i];
      ++count;
    }
    const double denom = double(count) * sxx - sx * sx;
    if (count > 2 && std::abs(denom) > 1e-12) {
      const double slope = (double(count) * sxy - sx * sy) / denom;
      const double mean_lx = sx / double(count);
      for (std::size_t i = 0; i < hz.size(); ++i) {
        if (hz[i] > 0.0) view[i] -= slope * (std::log2(hz[i]) - mean_lx);
      }
    }
  }
  cascade_plot_->setReference(hz, view);
  candidate_lane_->setReference(hz, view);
}

void MainWindow::refresh() {
  std::array<trench::core::Biquad, trench::core::native::kSections> sections{};
  std::array<bool, trench::core::native::kSections> enabled{};
  for (std::size_t index = 0; index < sections.size(); ++index) {
    sections[index] = state_.sectionBiquad(index);
    enabled[index] = state_.sectionEnabled(index);
  }
  const std::size_t selected = state_.selectedSection();
  const auto& selected_section = state_.section(selected);
  const bool selected_zero =
      state_.selectedLane() == EditorState::Lane::kZero &&
      state_.rootPresent(selected, EditorState::Lane::kZero);
  const double selected_frequency =
      resonant(selected_zero ? selected_section.zero : selected_section.pole).hz;
  cascade_plot_->setCascade(state_.cascade(), sections, enabled, selected,
                            selected_frequency, EditorState::kDatumHz);
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
  const bool enabled = state_.sectionEnabled(selected);
  const bool zero_present =
      state_.rootPresent(selected, EditorState::Lane::kZero);
  pole_frequency_->setEnabled(enabled);
  pole_bandwidth_->setEnabled(enabled);
  zero_frequency_group_->setVisible(zero_present);
  zero_bandwidth_group_->setVisible(zero_present);
  zero_frequency_->setEnabled(enabled && zero_present);
  zero_bandwidth_->setEnabled(enabled && zero_present);
  section_strip_->update();
  armadillo_editor_->update();
}

void MainWindow::updateAuditionView() {
  if (!audition_) return;
  audition_->setCascade(state_.cascade(audition_->sampleRateHz()));
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
