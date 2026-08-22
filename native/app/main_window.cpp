#include "main_window.hpp"

#include "body_document.hpp"
#include "chassis_bar.hpp"
#include "fit_controller.hpp"
#include "morph_strip.hpp"
#include "response_plot.hpp"
#include "section_model.hpp"
#include "space_dock.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QAction>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
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

constexpr int kSectionRowHeight = 18;

bool is_audio(const std::filesystem::path& path) {
  auto ext = path.extension().string();
  for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac";
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
    : QMainWindow(parent), body_path_(body_path) {
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
  response_plot_->setSpace(document_->space());
  column->addWidget(response_plot_, 1);

  morph_strip_ = new MorphStrip(central);
  morph_strip_->setObjectName(QStringLiteral("morphStrip"));
  column->addWidget(morph_strip_, 0);

  section_model_ = new SectionModel(document_, this);
  section_table_ = new QTableView(central);
  section_table_->setObjectName(QStringLiteral("sectionTable"));
  section_table_->setModel(section_model_);
  section_table_->setItemDelegateForColumn(SectionModel::kType, new TypeDelegate(this));
  section_table_->setItemDelegateForColumn(SectionModel::kIntent, new IntentDelegate(this));
  section_table_->setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold));
  section_table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  section_table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  section_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  section_table_->verticalHeader()->setMinimumSectionSize(kSectionRowHeight);
  section_table_->verticalHeader()->setDefaultSectionSize(kSectionRowHeight);
  section_table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  section_table_->verticalHeader()->setFixedWidth(18);
  section_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  section_table_->horizontalHeader()->setFixedHeight(kSectionRowHeight);
  section_table_->setFixedHeight(kSectionRowHeight *
                                 (static_cast<int>(trench::core::kLegacySectionCount) + 1));
  section_table_->setStyleSheet(QStringLiteral(
      "QTableView { background: #1a1f23; color: #aebabe; gridline-color: #373f43; "
      "border: none; selection-background-color: #373f43; selection-color: #aebabe; }"
      "QHeaderView::section { background: #1a1f23; color: #767f83; border: none; "
      "border-bottom: 1px solid #373f43; padding: 0 4px; }"
      "QTableCornerButton::section { background: #1a1f23; border: none; }"));
  column->addWidget(section_table_, 0);

  space_dock_ = new SpaceDock(central);
  space_dock_->setObjectName(QStringLiteral("spaceDock"));
  space_dock_->setSpace(document_->space());
  column->addWidget(space_dock_, 0);

  chassis_bar_ = new ChassisBar(central);
  chassis_bar_->setBodyName(QString::fromStdString(body_path.filename().string()));
  column->addWidget(chassis_bar_, 0);

  setCentralWidget(central);
  setWindowTitle(QStringLiteral("TRENCH — %1").arg(QString::fromStdString(body_path.stem().string())));
  resize(960, 540);

  connect(response_plot_, &ResponsePlotWidget::gestureStarted, this,
          [this](std::size_t section) {
            before_words_ = document_->body().words[document_->corner()][section];
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
  connect(document_, &BodyDocument::bodyChanged, this, [this] {
    response_plot_->refresh();
    updateProbes();
    updateInterior();
  });
  connect(document_, &BodyDocument::cornerChanged, this, [this](std::size_t corner) {
    response_plot_->setCorner(corner);
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q);
    updateProbes();
  });
  connect(document_, &BodyDocument::viewChanged, this, [this] {
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q);
    updateProbes();
  });
  connect(morph_strip_, &MorphStrip::viewEdited, this,
          [this](float morph, float q) { document_->setView(morph, q); });
  connect(space_dock_, &SpaceDock::spaceEdited, this,
          [this](const trench::core::p2k::PerceptualSpace& space) {
            document_->setSpace(space);
          });
  connect(document_, &BodyDocument::spaceChanged, this, [this] {
    space_dock_->setSpace(document_->space());
    response_plot_->setSpace(document_->space());
    updateProbes();
    updateInterior();
  });
  connect(document_, &BodyDocument::freedomMaskChanged, this,
          [this](std::uint32_t mask) {
            response_plot_->setFreedomMask(mask);
            fit_controller_->setMask(mask);
          });
  connect(document_, &BodyDocument::targetChanged, this, [this] {
    response_plot_->setTarget(document_->target() ? &*document_->target() : nullptr);
    updateVerbs();
    updateProbes();
  });

  connect(fit_controller_, &FitController::stepReady, this,
          [this](quint64 generation, quint64 section, const QList<quint16>& words) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            document_->applyFitStep(fit_corner_, unflatten(words));
            response_plot_->flashLane(static_cast<std::size_t>(section));
          });
  connect(fit_controller_, &FitController::finished, this,
          [this](quint64 generation, bool ok, const QList<quint16>& words) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            if (ok) {
              document_->applyFitResult(fit_corner_, unflatten_stored(words));
              document_->commitFit(fit_corner_, pre_fit_);
            } else {
              document_->applyCorner(fit_corner_, pre_fit_);
            }
            endRun();
          });

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

  auto* save_action = new QAction(this);
  save_action->setShortcut(QKeySequence::Save);
  connect(save_action, &QAction::triggered, this, [this] { saveBody(body_path_); });
  addAction(save_action);
  auto* save_as_action = new QAction(this);
  save_as_action->setShortcut(QKeySequence::SaveAs);
  connect(save_as_action, &QAction::triggered, this, &MainWindow::saveBodyAs);
  addAction(save_as_action);

  for (std::size_t corner = 0; corner < trench::core::kLegacyCornerCount; ++corner) {
    auto* corner_action = new QAction(this);
    corner_action->setShortcut(QKeySequence(QString::number(corner + 1)));
    connect(corner_action, &QAction::triggered, this,
            [this, corner] { document_->setCorner(corner); });
    addAction(corner_action);
  }

  undo_action_ = document_->undoStack()->createUndoAction(this);
  undo_action_->setShortcut(QKeySequence::Undo);
  addAction(undo_action_);
  redo_action_ = document_->undoStack()->createRedoAction(this);
  redo_action_->setShortcut(QKeySequence::Redo);
  addAction(redo_action_);

  updateVerbs();
  updateProbes();
  updateInterior();
}

ResponsePlotWidget* MainWindow::responsePlot() const noexcept { return response_plot_; }

ChassisBar* MainWindow::chassisBar() const noexcept { return chassis_bar_; }

MorphStrip* MainWindow::morphStrip() const noexcept { return morph_strip_; }

SpaceDock* MainWindow::spaceDock() const noexcept { return space_dock_; }

SectionModel* MainWindow::sectionModel() const noexcept { return section_model_; }

const std::filesystem::path& MainWindow::bodyPath() const noexcept { return body_path_; }

void MainWindow::setCorner(std::size_t corner) { document_->setCorner(corner); }

bool MainWindow::saveBody(const std::filesystem::path& path) {
  if (fit_active_) return false;
  std::array<std::uint8_t, trench::core::kLegacyBodyBytes> bytes{};
  try {
    bytes = document_->body().legacy_bytes();
  } catch (const std::exception&) {
    return false;
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!stream) return false;
  body_path_ = path;
  chassis_bar_->setBodyName(QString::fromStdString(path.filename().string()));
  setWindowTitle(QStringLiteral("TRENCH — %1").arg(QString::fromStdString(path.stem().string())));
  return true;
}

void MainWindow::saveBodyAs() {
  const auto chosen = QFileDialog::getSaveFileName(
      this, QStringLiteral("SAVE"), QString::fromStdWString(body_path_.wstring()),
      QStringLiteral("Body (*.body240)"));
  if (chosen.isEmpty()) return;
  saveBody(std::filesystem::path(chosen.toStdWString()));
}

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

void MainWindow::applySection(std::size_t section,
                              const trench::core::PackedSection& words) {
  document_->applySection(section, words);
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
      target = trench::core::measure::target_on_grid(envelope, document_->grid().hz);
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
  document_->setTarget(trench::core::p2k::corner_response_db(
      trench::core::p2k::rom_corner_words(bytes, document_->corner()), document_->grid()));
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
  fit_corner_ = document_->corner();
  fit_active_ = true;
  response_plot_->setFitRunning(true);
  updateVerbs();
  fit_controller_->start(*document_->target(), document_->seedWords(),
                         document_->freedomMask(), document_->grid(),
                         document_->intent(), document_->seedIsInherited());
}

void MainWindow::stopAndKeep() {
  if (!fit_active_) return;
  fit_controller_->requestStop();
}

void MainWindow::discardFit() {
  if (!fit_active_) return;
  fit_controller_->abandon();
  document_->applyCorner(fit_corner_, pre_fit_);
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
  document_->commitFit(document_->corner(), before);
}

void MainWindow::updateProbes() {
  namespace p2k = trench::core::p2k;
  dc_drift_db_ = p2k::dc_gain_db(flatten_corner(document_->cornerSnapshot()));
  chassis_bar_->setDcDriftDb(dc_drift_db_);
  chassis_bar_->setScoreDb(document_->targetScoreDb());
}

void MainWindow::updateInterior() {
  std::array<std::uint8_t, trench::core::kLegacyBodyBytes> bytes{};
  try {
    bytes = document_->body().legacy_bytes();
  } catch (const std::exception&) {
    return;
  }
  const auto audit = trench::core::p2k::interior_audit(bytes, document_->grid());
  morph_strip_->setWorstStepDb(audit.max_step_db);
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
