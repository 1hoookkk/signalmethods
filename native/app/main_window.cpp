#include "main_window.hpp"

#include "body_document.hpp"
#include "chassis_bar.hpp"
#include "fit_controller.hpp"
#include "morph_strip.hpp"
#include "response_plot.hpp"
#include "section_strip.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/formants.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QAction>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QVBoxLayout>
#include <QWidget>

#include <cctype>
#include <fstream>
#include <optional>
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

constexpr int kStripBankHeight = 148;

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

  auto* bank = new QWidget(central);
  bank->setObjectName(QStringLiteral("sectionStrips"));
  bank->setFixedHeight(kStripBankHeight);
  auto* row = new QHBoxLayout(bank);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(0);
  for (std::size_t section = 0; section < strips_.size(); ++section) {
    strips_[section] = new SectionStrip(section, bank);
    row->addWidget(strips_[section], 1);
  }
  column->addWidget(bank, 0);

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
  connect(response_plot_, &ResponsePlotWidget::tokenSelected, this,
          [this](std::size_t section, ResponsePlotWidget::Lane) {
            selectSection(section);
          });
  connect(response_plot_, &ResponsePlotWidget::tokenHovered, this,
          [this](std::optional<std::size_t> section) {
            for (auto* strip : strips_) {
              strip->setHighlighted(section && *section == strip->section());
            }
          });
  for (auto* strip : strips_) {
    connect(strip, &SectionStrip::selectRequested, this, &MainWindow::selectSection);
    connect(strip, &SectionStrip::gestureStarted, this, [this](std::size_t section) {
      before_words_ = document_->body().words[document_->corner()][section];
      strip_gesture_ = true;
    });
    connect(strip, &SectionStrip::gestureFinished, this, [this](std::size_t section) {
      strip_gesture_ = false;
      document_->commitGesture(section, before_words_);
    });
    connect(strip, &SectionStrip::paramEdited, this, &MainWindow::applyParam);
    connect(strip, &SectionStrip::hoverChanged, this,
            [this](std::size_t section, bool inside) {
              response_plot_->setHighlightedSection(
                  inside ? std::optional<std::size_t>{section} : std::nullopt);
            });
  }
  connect(document_, &BodyDocument::bodyChanged, this, [this] {
    response_plot_->refresh();
    updateStrips();
    updateProbes();
    updateInterior();
  });
  connect(document_, &BodyDocument::cornerChanged, this, [this](std::size_t corner) {
    response_plot_->setCorner(corner);
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q, view.semitones);
    morph_strip_->setTranspose(view.semitones);
    updateProbes();
  });
  connect(document_, &BodyDocument::viewChanged, this, [this] {
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q, view.semitones);
    morph_strip_->setTranspose(view.semitones);
    updateStrips();
    updateProbes();
  });
  connect(morph_strip_, &MorphStrip::viewEdited, this,
          [this](float morph, float q) { document_->setView(morph, q); });
  connect(morph_strip_, &MorphStrip::transposeEdited, this,
          [this](int semitones) { document_->setTranspose(semitones); });
  connect(document_, &BodyDocument::spaceChanged, this, [this] {
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
      case ChassisBar::Verb::kSource:
        setSourceModel(source_model_ == trench::core::measure::Source::kFlat
                           ? trench::core::measure::Source::kSawtooth
                           : trench::core::measure::Source::kFlat);
        break;
      case ChassisBar::Verb::kTarget:
        chooseTarget();
        break;
      case ChassisBar::Verb::kFit:
        openFitRoom();
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

  fit_room_ = new FitRoom(this);
  QStringList vowels;
  for (const auto& vowel : trench::core::p2k::klatt_vowels()) {
    vowels.push_back(QString::fromUtf8(vowel.symbol.data(), static_cast<int>(vowel.symbol.size())));
  }
  for (const auto& recipe : trench::core::p2k::manual_recipes()) {
    vowels.push_back(QString::fromUtf8(recipe.name.data(), static_cast<int>(recipe.name.size())));
  }
  fit_room_->setVowels(vowels);
  connect(fit_room_, &FitRoom::overlaySelected, this, &MainWindow::selectOverlay);
  connect(fit_room_, &FitRoom::overlayRemoved, this, &MainWindow::removeOverlay);
  connect(fit_room_, &FitRoom::loadRequested, this, &MainWindow::chooseTarget);
  connect(fit_room_, &FitRoom::vowelRequested, this, &MainWindow::applyVowel);
  auto* fit_room_action = new QAction(this);
  fit_room_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
  connect(fit_room_action, &QAction::triggered, this, &MainWindow::openFitRoom);
  addAction(fit_room_action);

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

  auto* save_corner_action = new QAction(this);
  save_corner_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
  connect(save_corner_action, &QAction::triggered, this, &MainWindow::saveCornerAs);
  addAction(save_corner_action);
  auto* load_corner_action = new QAction(this);
  load_corner_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
  connect(load_corner_action, &QAction::triggered, this, &MainWindow::chooseCorner);
  addAction(load_corner_action);

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

  selectSection(0);
  updateStrips();
  updateVerbs();
  updateProbes();
  updateInterior();
}

ResponsePlotWidget* MainWindow::responsePlot() const noexcept { return response_plot_; }

ChassisBar* MainWindow::chassisBar() const noexcept { return chassis_bar_; }

MorphStrip* MainWindow::morphStrip() const noexcept { return morph_strip_; }

SectionStrip* MainWindow::sectionStrip(std::size_t section) const noexcept {
  return section < strips_.size() ? strips_[section] : nullptr;
}

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

bool MainWindow::saveCorner(const std::filesystem::path& path) {
  if (fit_active_) return false;
  const auto snapshot = document_->cornerSnapshot();
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  for (const auto& section : snapshot) {
    for (const auto word : section) {
      const unsigned char bytes[2] = {static_cast<unsigned char>(word & 0xFFU),
                                      static_cast<unsigned char>(word >> 8U)};
      stream.write(reinterpret_cast<const char*>(bytes), 2);
    }
  }
  return static_cast<bool>(stream);
}

bool MainWindow::loadCorner(const std::filesystem::path& path) {
  if (fit_active_) return false;
  std::vector<std::uint8_t> bytes;
  try {
    bytes = read_bytes(path);
  } catch (const std::exception&) {
    return false;
  }
  constexpr std::size_t kCornerBytes =
      trench::core::kLegacySectionCount * trench::core::p2k::kWordCount * 2;
  if (bytes.size() != kCornerBytes) return false;
  const auto before = document_->cornerSnapshot();
  auto after = before;
  std::size_t at = 0;
  for (auto& section : after) {
    for (auto& word : section) {
      word = static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1] << 8U));
      at += 2;
    }
  }
  if (after == before) return true;
  document_->applyCorner(after);
  document_->commitFit(document_->corner(), before);
  return true;
}

void MainWindow::saveCornerAs() {
  const auto chosen = QFileDialog::getSaveFileName(
      this, QStringLiteral("SAVE CORNER"),
      QString::fromStdWString((body_path_.parent_path() / body_path_.stem()).wstring()) +
          QStringLiteral("_c%1.corner").arg(document_->corner() + 1),
      QStringLiteral("Corner (*.corner)"));
  if (chosen.isEmpty()) return;
  saveCorner(std::filesystem::path(chosen.toStdWString()));
}

void MainWindow::chooseCorner() {
  const auto chosen = QFileDialog::getOpenFileName(
      this, QStringLiteral("LOAD CORNER"), QString::fromStdWString(body_path_.parent_path().wstring()),
      QStringLiteral("Corner (*.corner)"));
  if (chosen.isEmpty()) return;
  loadCorner(std::filesystem::path(chosen.toStdWString()));
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

void MainWindow::applySection(std::size_t section,
                              const trench::core::PackedSection& words) {
  document_->applySection(section, words);
}

void MainWindow::applyParam(std::size_t section, trench::core::p2k::SectionEdit edit,
                            const trench::core::p2k::SectionParam& param) {
  if (section >= trench::core::kLegacySectionCount) return;
  const auto before = document_->body().words[document_->corner()][section];
  const std::array<std::uint16_t, 4> current{before[0], before[1], before[2], before[3]};
  const auto roots = trench::core::p2k::words_from_param_keeping_offset(
      param, edit, current, section, trench::core::kP2kDatumHz);
  auto candidate = before;
  for (std::size_t word = 0; word < roots.size(); ++word) {
    candidate[word] = roots[word];
  }
  if (candidate == before) return;
  document_->applySection(section, candidate);
  if (!strip_gesture_) {
    document_->commitGesture(section, before);
  }
}

void MainWindow::selectSection(std::size_t section) {
  if (section >= strips_.size()) return;
  for (auto* strip : strips_) {
    strip->setSelected(strip->section() == section);
  }
  response_plot_->setSelectedSection(section);
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
    std::vector<double> lpc_target;
    std::vector<double> marks;
    try {
      const auto envelope = trench::core::measure::harmonic_envelope(
          clip->samples, clip->sample_rate_hz, source_model_);
      target = trench::core::measure::target_on_grid(envelope, document_->grid().hz);
      const auto lpc = trench::core::measure::lpc_envelope(clip->samples, clip->sample_rate_hz);
      lpc_target = trench::core::measure::target_on_grid(lpc, document_->grid().hz);
      for (const auto& formant : lpc.formants) marks.push_back(formant.hz);
    } catch (const std::exception&) {
      return false;
    }
    const auto name = QString::fromStdString(path.filename().string());
    addOverlay(name + QStringLiteral(" LPC"), std::move(lpc_target), std::move(marks));
    addOverlay(name, std::move(target));
    audition_clip_ = *clip;
    if (audition_) audition_->setClip(*clip);
    return true;
  }
  if (path.extension() == ".txt") {
    auto curve = read_curve(path);
    if (curve.size() != trench::core::p2k::kNpts) return false;
    addOverlay(QString::fromStdString(path.filename().string()), std::move(curve));
    return true;
  }
  std::vector<std::uint8_t> bytes;
  try {
    bytes = read_bytes(path);
  } catch (const std::exception&) {
    return false;
  }
  if (bytes.size() != trench::core::kLegacyBodyBytes) return false;
  addOverlay(QString::fromStdString(path.filename().string()),
             trench::core::p2k::corner_response_db(
                 trench::core::p2k::rom_corner_words(bytes, document_->corner()),
                 document_->grid()));
  return true;
}

void MainWindow::addOverlay(const QString& name, std::vector<double> curve,
                            std::vector<double> marks_hz) {
  overlays_.push_back(FitRoom::Overlay{name, std::move(curve), std::move(marks_hz)});
  selectOverlay(static_cast<int>(overlays_.size()) - 1);
}

void MainWindow::selectOverlay(int index) {
  if (index < 0 || index >= overlays_.size()) return;
  selected_overlay_ = index;
  chassis_bar_->setTargetName(overlays_[index].name);
  document_->setTarget(overlays_[index].db);
}

void MainWindow::removeOverlay(int index) {
  if (index < 0 || index >= overlays_.size()) return;
  overlays_.removeAt(index);
  if (overlays_.isEmpty()) {
    selected_overlay_ = -1;
    chassis_bar_->setTargetName(QString());
    document_->clearTarget();
    return;
  }
  selectOverlay(std::min(index, static_cast<int>(overlays_.size()) - 1));
}

int MainWindow::overlayCount() const noexcept { return static_cast<int>(overlays_.size()); }

FitRoom* MainWindow::fitRoom() const noexcept { return fit_room_; }

void MainWindow::openFitRoom() {
  refreshFitRoom();
  fit_room_->show();
  fit_room_->raise();
}

void MainWindow::refreshFitRoom() {
  if (fit_room_ == nullptr) return;
  fit_room_->setGridHz(document_->grid().hz);
  fit_room_->setResponse(document_->viewResponseDb());
  fit_room_->setOverlays(overlays_, selected_overlay_);
  fit_room_->setScoreDb(document_->targetScoreDb());
  fit_room_->setFitRunning(fit_active_);
}

void MainWindow::applyVowel(const QString& symbol) {
  namespace p2k = trench::core::p2k;
  if (fit_active_) return;
  const auto* vowel = p2k::klatt_vowel(symbol.toStdString());
  const auto* manual = p2k::manual_recipe(symbol.toStdString());
  if (vowel == nullptr && manual == nullptr) return;
  const auto words = p2k::words_from_recipe(vowel != nullptr ? p2k::rows_from_formants(vowel->f)
                                                             : manual->recipe);
  const auto before = document_->cornerSnapshot();
  auto after = before;
  for (std::size_t section = 0; section < p2k::kStageCount; ++section) {
    for (std::size_t word = 0; word < 4; ++word) after[section][word] = words[section][word];
  }
  if (after == before) return;
  document_->applyCorner(after);
  document_->commitFit(document_->corner(), before);
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
  namespace p2k = trench::core::p2k;
  const auto held = document_->seedWords();
  auto seed = p2k::rows_of_corner(held);
  bool typed = false;
  for (const auto& row : seed) typed |= row.type != p2k::SectionType::kOff;
  if (!typed) seed = p2k::seed_rows_from_target(*document_->target(), document_->grid());
  fit_controller_->startRows(*document_->target(), seed, held, flatten_corner(pre_fit_),
                             document_->freedomMask(), document_->grid());
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

void MainWindow::setAuditionGate(bool open) {
  if (open && !audition_) {
    audition_ = std::make_unique<trench::audio::Audition>();
    if (!audition_->start().empty()) {
      audition_.reset();
      return;
    }
    if (audition_clip_) audition_->setClip(*audition_clip_);
    updateAudition();
  }
  audition_open_ = open && audition_ != nullptr;
  if (audition_) audition_->setGate(audition_open_);
}

bool MainWindow::auditionOpen() const noexcept { return audition_open_; }

void MainWindow::updateAudition() {
  if (!audition_) return;
  audition_->setCascade(document_->viewCascade());
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
    setAuditionGate(true);
    event->accept();
    return;
  }
  QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
    setAuditionGate(false);
    event->accept();
    return;
  }
  QMainWindow::keyReleaseEvent(event);
}

void MainWindow::updateProbes() {
  chassis_bar_->setScoreDb(document_->targetScoreDb());
  refreshFitRoom();
  updateAudition();
}

void MainWindow::updateStrips() {
  for (auto* strip : strips_) {
    strip->setParam(trench::core::p2k::param_of(
        document_->body().words[document_->corner()][strip->section()],
        trench::core::kP2kDatumHz));
  }
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
  refreshFitRoom();
  if (undo_action_ != nullptr) {
    undo_action_->setEnabled(!fit_active_ && document_->undoStack()->canUndo());
  }
  if (redo_action_ != nullptr) {
    redo_action_->setEnabled(!fit_active_ && document_->undoStack()->canRedo());
  }
}
