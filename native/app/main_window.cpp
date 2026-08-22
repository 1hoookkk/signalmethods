#include "main_window.hpp"

#include "body_document.hpp"
#include "chassis_bar.hpp"
#include "fit_controller.hpp"
#include "response_plot.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QAction>
#include <QFileDialog>
#include <QKeySequence>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) throw std::runtime_error("cannot open body: " + path.string());
  const auto size = static_cast<std::size_t>(stream.tellg());
  std::vector<std::uint8_t> bytes(size);
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  if (!stream) throw std::runtime_error("cannot read body: " + path.string());
  return bytes;
}

std::vector<double> read_curve(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return {};
  std::vector<double> values;
  double value = 0.0;
  while (stream >> value) values.push_back(value);
  if (!stream.eof() || values.size() != trench::core::p2k::kNpts) return {};
  return values;
}

trench::core::p2k::CornerWords unflatten(const QList<quint16>& words) {
  trench::core::p2k::CornerWords out{};
  for (std::size_t section = 0; section < trench::core::p2k::kStageCount; ++section) {
    for (std::size_t word = 0; word < out[section].size(); ++word) {
      out[section][word] =
          words[static_cast<qsizetype>(section * out[section].size() + word)];
    }
  }
  return out;
}

trench::core::p2k::PackedCorner flatten_corner(const BodyDocument::CornerSnapshot& corner) {
  trench::core::p2k::PackedCorner out{};
  for (std::size_t section = 0; section < trench::core::p2k::kStageCount; ++section) {
    for (std::size_t word = 0; word < trench::core::p2k::kWordCount; ++word) {
      out[section * trench::core::p2k::kWordCount + word] = corner[section][word];
    }
  }
  return out;
}

bool is_audio(const std::filesystem::path& path) {
  auto ext = path.extension().string();
  for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac";
}

QString hex_word(std::uint16_t word) {
  return QString::number(word, 16).toUpper().rightJustified(4, QLatin1Char('0'));
}

trench::core::p2k::StoredCorner unflatten_stored(const QList<quint16>& words) {
  trench::core::p2k::StoredCorner out{};
  for (std::size_t section = 0; section < trench::core::p2k::kStageCount; ++section) {
    for (std::size_t word = 0; word < trench::core::p2k::kWordCount; ++word) {
      out[section][word] = words[static_cast<qsizetype>(
          section * trench::core::p2k::kWordCount + word)];
    }
  }
  return out;
}

}  // namespace

MainWindow::MainWindow(const std::filesystem::path& body_path,
                       double sample_rate_hz,
                       QWidget* parent)
    : QMainWindow(parent) {
  document_ = new BodyDocument(
      trench::core::PackedBody::from_body_bytes(read_bytes(body_path)),
      sample_rate_hz, this);
  fit_controller_ = new FitController(this);

  auto* central = new QWidget(this);
  auto* column = new QVBoxLayout(central);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);

  response_plot_ = new ResponsePlotWidget(central);
  response_plot_->setObjectName(QStringLiteral("responsePlot"));
  response_plot_->setBody(&document_->body(), document_->sampleRateHz(),
                          body_path.filename().string());
  response_plot_->setFreedomMask(document_->freedomMask());
  column->addWidget(response_plot_, 1);

  chassis_bar_ = new ChassisBar(central);
  chassis_bar_->setBodyName(QString::fromStdString(body_path.filename().string()));
  column->addWidget(chassis_bar_, 0);

  setCentralWidget(central);
  setWindowTitle(QStringLiteral("TRENCH — %1").arg(QString::fromStdString(body_path.stem().string())));
  resize(960, 540);

  connect(response_plot_, &ResponsePlotWidget::gestureStarted, this,
          [this](std::size_t section) {
            before_words_ = document_->body().words[0][section];
          });
  connect(response_plot_, &ResponsePlotWidget::sectionEdited, this,
          [this](std::size_t section, const trench::core::PackedSection& words) {
            document_->applySection(section, words);
          });
  connect(response_plot_, &ResponsePlotWidget::gestureFinished, this,
          [this](std::size_t section) {
            document_->commitGesture(section, before_words_);
          });
  connect(response_plot_, &ResponsePlotWidget::pinToggled, this,
          [this](std::size_t section, ResponsePlotWidget::Lane lane) {
            document_->toggleLane(section, lane == ResponsePlotWidget::Lane::kPole);
          });
  connect(response_plot_, &ResponsePlotWidget::tokenSelected, this,
          [this](std::size_t section, ResponsePlotWidget::Lane lane) {
            selected_ = std::make_pair(section, lane);
            updateProbes();
          });

  connect(document_, &BodyDocument::bodyChanged, this, [this] {
    response_plot_->refresh();
    updateProbes();
  });
  connect(document_, &BodyDocument::freedomMaskChanged, this,
          [this](std::uint32_t mask) {
            response_plot_->setFreedomMask(mask);
            fit_controller_->setMask(mask);
          });
  connect(document_, &BodyDocument::targetChanged, this, [this] {
    response_plot_->setTarget(document_->target() ? &*document_->target() : nullptr);
    updateVerbs();
  });

  connect(fit_controller_, &FitController::stepReady, this,
          [this](quint64 generation, quint64 section, const QList<quint16>& words) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            document_->applyFitStep(unflatten(words));
            response_plot_->flashLane(static_cast<std::size_t>(section));
          });
  connect(fit_controller_, &FitController::finished, this,
          [this](quint64 generation, bool ok, const QList<quint16>& words) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            if (ok) {
              document_->applyFitResult(unflatten_stored(words));
              document_->commitFit(pre_fit_);
            } else {
              document_->applyCorner(pre_fit_);
            }
            endRun();
          });

  connect(chassis_bar_, &ChassisBar::rootTyped, this,
          [this](const QString& text) { applyTypedRoot(text); });
  connect(chassis_bar_, &ChassisBar::verbClicked, this, [this](ChassisBar::Verb verb) {
    switch (verb) {
      case ChassisBar::Verb::kUnity:
        renormalizeDc();
        break;
      case ChassisBar::Verb::kSource:
        setSourceModel(source_model_ == trench::core::measure::Source::kFlat
                           ? trench::core::measure::Source::kSawtooth
                           : trench::core::measure::Source::kFlat);
        break;
      case ChassisBar::Verb::kTarget:
        chooseTarget();
        break;
      case ChassisBar::Verb::kFit:
        startFit();
        break;
      case ChassisBar::Verb::kKeep:
        stopAndKeep();
        break;
      case ChassisBar::Verb::kDiscard:
        discardFit();
        break;
    }
  });

  auto* target_action = new QAction(this);
  target_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
  connect(target_action, &QAction::triggered, this, &MainWindow::chooseTarget);
  addAction(target_action);

  undo_action_ = document_->undoStack()->createUndoAction(this);
  undo_action_->setShortcut(QKeySequence::Undo);
  addAction(undo_action_);
  redo_action_ = document_->undoStack()->createRedoAction(this);
  redo_action_->setShortcut(QKeySequence::Redo);
  addAction(redo_action_);

  updateVerbs();
  updateProbes();
}

ResponsePlotWidget* MainWindow::responsePlot() const noexcept { return response_plot_; }

BodyDocument* MainWindow::document() const noexcept { return document_; }

FitController* MainWindow::fitController() const noexcept { return fit_controller_; }

const trench::core::PackedBody& MainWindow::body() const noexcept {
  return document_->body();
}

QUndoStack* MainWindow::undoStack() noexcept { return document_->undoStack(); }

std::uint32_t MainWindow::freedomMask() const noexcept {
  return document_->freedomMask();
}

bool MainWindow::fitRunning() const noexcept { return fit_active_; }

double MainWindow::dcDriftDb() const noexcept { return dc_drift_db_; }

QString MainWindow::readoutText() const { return chassis_bar_->readoutText(); }

void MainWindow::applySection(std::size_t section,
                              const trench::core::PackedSection& words) {
  document_->applySection(section, words);
}

bool MainWindow::applyTypedRoot(const QString& text) {
  namespace p2k = trench::core::p2k;
  if (!selected_ || fit_active_) return false;
  const auto parts = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.isEmpty() || parts.size() > 2) return false;
  bool ok = false;
  const double hz = parts[0].toDouble(&ok);
  if (!ok || !(hz > 0.0)) return false;
  const auto [section, lane] = *selected_;
  const bool pole = lane == ResponsePlotWidget::Lane::kPole;
  const auto before = document_->body().words[0][section];
  const auto geometry = trench::core::geometry_from_words(before, trench::core::kP2kDatumHz);
  const auto& pair = pole ? geometry.pole : geometry.zero;
  double radius = 0.9;
  if (const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&pair)) {
    radius = conjugate->radius;
  }
  if (parts.size() == 2) {
    const double q = parts[1].toDouble(&ok);
    if (!ok || !(q > 0.0)) return false;
    radius = std::exp(-std::numbers::pi * hz / (q * trench::core::kP2kDatumHz));
  }
  radius = pole ? std::clamp(radius, 0.0, p2k::kPoleRMax) : std::clamp(radius, 0.0, 1.0);
  const double clamped_hz = std::clamp(hz, 20.0, p2k::kRootHiHz);
  const auto [word_mag, word_rsq] = p2k::words_from_root(clamped_hz, radius);
  const auto [p, q] = p2k::pq(word_mag, word_rsq);
  if (!p2k::is_legal(p, q, pole) ||
      !p2k::magnitude_admissible(p2k::nearest_lattice_word(word_mag), pole)) {
    return false;
  }
  auto candidate = before;
  candidate[pole ? 2 : 0] = word_mag;
  candidate[pole ? 3 : 1] = word_rsq;
  if (candidate == before) return true;
  document_->applySection(section, candidate);
  document_->commitGesture(section, before);
  return true;
}

void MainWindow::setSourceModel(trench::core::measure::Source source) {
  source_model_ = source;
  chassis_bar_->setSourceSawtooth(source == trench::core::measure::Source::kSawtooth);
}

trench::core::measure::Source MainWindow::sourceModel() const noexcept {
  return source_model_;
}

bool MainWindow::loadTarget(const std::filesystem::path& path) {
  if (is_audio(path)) {
    const auto clip = trench::audio::decode_mono(path);
    if (!clip) return false;
    std::vector<double> target;
    try {
      const auto envelope = trench::core::measure::harmonic_envelope(
          clip->samples, clip->sample_rate_hz, source_model_);
      target = trench::core::measure::target_on_grid(envelope, trench::core::p2k::grid().hz);
    } catch (const std::exception&) {
      return false;
    }
    document_->setTarget(std::move(target));
    chassis_bar_->setTargetName(QString::fromStdString(path.filename().string()));
    return true;
  }
  if (path.extension() == ".txt") {
    auto curve = read_curve(path);
    if (curve.size() != trench::core::p2k::kNpts) return false;
    document_->setTarget(std::move(curve));
    chassis_bar_->setTargetName(QString::fromStdString(path.filename().string()));
    return true;
  }
  std::vector<std::uint8_t> bytes;
  try {
    bytes = read_bytes(path);
  } catch (const std::exception&) {
    return false;
  }
  if (bytes.size() != trench::core::kLegacyBodyBytes) return false;
  document_->setTarget(
      trench::core::p2k::corner_response_db(trench::core::p2k::rom_corner_words(bytes, 0)));
  chassis_bar_->setTargetName(QString::fromStdString(path.filename().string()));
  return true;
}

void MainWindow::chooseTarget() {
  const auto chosen = QFileDialog::getOpenFileName(
      this, QStringLiteral("TARGET"), QString(),
      QStringLiteral("Target (*.body240 *.bin *.txt *.wav *.aif *.aiff *.flac)"));
  if (chosen.isEmpty()) return;
  loadTarget(std::filesystem::path(chosen.toStdWString()));
}

void MainWindow::startFit() {
  if (fit_active_ || !document_->target()) return;
  pre_fit_ = document_->cornerSnapshot();
  fit_active_ = true;
  response_plot_->setFitRunning(true);
  updateVerbs();
  fit_controller_->start(*document_->target(), document_->seedWords(),
                         document_->freedomMask());
}

void MainWindow::stopAndKeep() {
  if (!fit_active_) return;
  fit_controller_->requestStop();
}

void MainWindow::discardFit() {
  if (!fit_active_) return;
  fit_controller_->abandon();
  document_->applyCorner(pre_fit_);
  endRun();
}

void MainWindow::renormalizeDc() {
  namespace p2k = trench::core::p2k;
  if (fit_active_) return;
  const auto before = document_->cornerSnapshot();
  const auto corner = p2k::Corner::from_words(document_->seedWords());
  const auto scales = p2k::stage_gain_pass_held(corner, document_->freedomMask(),
                                                flatten_corner(before));
  const auto packed = p2k::pack_corner(corner, scales);
  auto after = before;
  for (std::size_t section = 0; section < p2k::kStageCount; ++section) {
    after[section][4] = packed[section * p2k::kWordCount + 4];
  }
  if (after == before) return;
  document_->applyCorner(after);
  document_->commitFit(before);
}

void MainWindow::updateProbes() {
  namespace p2k = trench::core::p2k;
  dc_drift_db_ = p2k::dc_gain_db(flatten_corner(document_->cornerSnapshot()));
  chassis_bar_->setDcDriftDb(dc_drift_db_);
  if (!selected_) {
    chassis_bar_->setReadout(std::nullopt);
    return;
  }
  const auto [section, lane] = *selected_;
  const auto pole = lane == ResponsePlotWidget::Lane::kPole;
  const auto& words = document_->body().words[0][section];
  const auto mag = words[pole ? 2 : 0];
  const auto rsq = words[pole ? 3 : 1];
  const auto geometry =
      trench::core::geometry_from_words(words, trench::core::kP2kDatumHz);
  const auto& pair = pole ? geometry.pole : geometry.zero;
  auto text = QString::number(section + 1);
  if (const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&pair)) {
    text += QStringLiteral("  %1 Hz  r %2")
                .arg(conjugate->hz, 0, 'f', conjugate->hz < 100.0 ? 1 : 0)
                .arg(conjugate->radius, 0, 'f', 5);
  }
  text += QStringLiteral("  %1·%2  #%3")
              .arg(hex_word(mag), hex_word(rsq))
              .arg(p2k::nearest_lattice_word(mag));
  chassis_bar_->setReadout(ChassisBar::Readout{section, pole, text});
}

void MainWindow::endRun() {
  fit_active_ = false;
  response_plot_->setFitRunning(false);
  updateVerbs();
}

void MainWindow::updateVerbs() {
  const auto has_target = document_->target().has_value();
  chassis_bar_->setState(has_target, fit_active_);
  if (undo_action_ != nullptr) {
    undo_action_->setEnabled(!fit_active_ && document_->undoStack()->canUndo());
  }
  if (redo_action_ != nullptr) {
    redo_action_->setEnabled(!fit_active_ && document_->undoStack()->canRedo());
  }
}
