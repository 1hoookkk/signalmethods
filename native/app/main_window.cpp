#include "main_window.hpp"

#include "armadillo_view.hpp"
#include "body_document.hpp"
#include "chassis_bar.hpp"
#include "fit_controller.hpp"
#include "morph_strip.hpp"
#include "posture_list.hpp"
#include "response_plot.hpp"
#include "section_readout.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/formants.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr std::array<const char*, 12> kFilterTypes{"LPF", "HPF", "BPF", "EQ+", "EQ-", "VOW",
                                                   "PHA", "FLG", "REZ", "WAH", "DST", "SFX"};

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

trench::core::p2k::CornerWords roots_of_corner(
    const BodyDocument::CornerSnapshot& corner) {
  trench::core::p2k::CornerWords out{};
  for (std::size_t section = 0; section < trench::core::p2k::kStageCount; ++section) {
    for (std::size_t word = 0; word < out[section].size(); ++word) {
      out[section][word] = corner[section][word];
    }
  }
  return out;
}

bool is_audio(const std::filesystem::path& path) {
  auto ext = path.extension().string();
  for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac";
}

trench::core::PackedBody empty_body() {
  trench::core::PackedBody body;
  for (auto& corner : body.words) corner.fill(trench::core::kIdentitySection);
  return body;
}

}  // namespace

MainWindow::MainWindow(const std::filesystem::path& body_path,
                       double sample_rate_hz,
                       QWidget* parent)
    : QMainWindow(parent), body_path_(body_path) {
  document_ = new BodyDocument(
      body_path.empty() ? empty_body()
                        : trench::core::PackedBody::from_body_bytes(read_bytes(body_path)),
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
  response_plot_->setToolTip(
      QStringLiteral("the sounding cascade — click a token to pin or free it for FIT — "
                     "space auditions"));

  auto* plot_row = new QWidget(central);
  auto* plot_layout = new QHBoxLayout(plot_row);
  plot_layout->setContentsMargins(0, 0, 0, 0);
  plot_layout->setSpacing(0);
  auto* surface_column = new QWidget(plot_row);
  auto* surface_layout = new QVBoxLayout(surface_column);
  surface_layout->setContentsMargins(0, 0, 0, 0);
  surface_layout->setSpacing(0);
  armadillo_ = new ArmadilloView(surface_column);
  armadillo_->setObjectName(QStringLiteral("armadilloView"));
  armadillo_->setToolTip(
      QStringLiteral("the armadillo plane: log frequency across, depth to the rim up — "
                     "drag a p or z — double-click places a pole pair — drag off the "
                     "right edge to park it"));
  armadillo_->setBody(&document_->body(), document_->sampleRateHz());
  surface_layout->addWidget(armadillo_, 3);
  response_plot_->setParent(surface_column);
  surface_layout->addWidget(response_plot_, 2);
  plot_layout->addWidget(surface_column, 1);
  auto* posture_column = new QWidget(plot_row);
  auto* posture_layout = new QVBoxLayout(posture_column);
  posture_layout->setContentsMargins(0, 0, 0, 0);
  posture_layout->setSpacing(0);
  keep_posture_ = new QPushButton(QStringLiteral("KEEP"), posture_column);
  keep_posture_->setObjectName(QStringLiteral("keepPosture"));
  keep_posture_->setFocusPolicy(Qt::NoFocus);
  keep_posture_->setCursor(Qt::PointingHandCursor);
  keep_posture_->setToolTip(QStringLiteral("keep the current pole posture under MINE"));
  keep_posture_->setFixedHeight(20);
  keep_posture_->setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold));
  keep_posture_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1a1f23; color: #e8c14a; border: none; "
      "border-left: 1px solid #373f43; border-bottom: 1px solid #373f43; text-align: left; "
      "padding-left: 6px; }"
      "QPushButton:disabled { color: #4a5257; }"
      "QPushButton:pressed { background: #232a2f; }"));
  posture_layout->addWidget(keep_posture_, 0);
  posture_list_ = new PostureList(posture_column);
  posture_layout->addWidget(posture_list_, 1);
  plot_layout->addWidget(posture_column, 0);
  connect(keep_posture_, &QPushButton::clicked, this, &MainWindow::keepPosture);
  column->addWidget(plot_row, 1);

  morph_strip_ = new MorphStrip(central);
  morph_strip_->setObjectName(QStringLiteral("morphStrip"));
  column->addWidget(morph_strip_, 0);

  section_readout_ = new SectionReadout(central);
  column->addWidget(section_readout_, 0);

  chassis_bar_ = new ChassisBar(central);
  chassis_bar_->setBodyName(QString::fromStdString(body_path.filename().string()));
  column->addWidget(chassis_bar_, 0);

  setCentralWidget(central);
  setWindowTitle(body_path.empty()
                     ? QStringLiteral("TRENCH")
                     : QStringLiteral("TRENCH — %1")
                           .arg(QString::fromStdString(body_path.stem().string())));
  resize(960, 540);

  connect(response_plot_, &ResponsePlotWidget::pinToggled, this,
          [this](std::size_t section, ResponsePlotWidget::Lane lane) {
            document_->toggleLane(section, lane == ResponsePlotWidget::Lane::kPole);
          });
  connect(response_plot_, &ResponsePlotWidget::tokenSelected, this,
          [this](std::size_t section, ResponsePlotWidget::Lane lane) {
            selectSection(section, lane);
          });
  connect(response_plot_, &ResponsePlotWidget::tokenHovered, this,
          [this](std::optional<std::size_t> section) {
            response_plot_->setHighlightedSection(section);
          });
  connect(section_readout_, &SectionReadout::poleEdited, this, &MainWindow::applyPole);
  connect(armadillo_, &ArmadilloView::rootPressed, this, [this](std::size_t section, bool zero) {
    armadillo_before_ = document_->cornerSnapshot();
    selectSection(section, zero ? ResponsePlotWidget::Lane::kZero
                                : ResponsePlotWidget::Lane::kPole);
  });
  connect(armadillo_, &ArmadilloView::rootDragged, this,
          [this](std::size_t section, bool zero, double hz, double radius) {
            if (fit_active_) return;
            namespace p2k = trench::core::p2k;
            const auto free_bit = zero ? p2k::zero_bit(section) : p2k::pole_bit(section);
            if ((document_->freedomMask() & free_bit) == 0U) return;
            auto words = document_->cornerSnapshot()[section];
            const auto [mag, rsq] = p2k::words_from_root(
                std::clamp(hz, 20.0, p2k::kRootHiHz), std::clamp(radius, 0.0, 0.99998));
            const auto [pw, qw] = p2k::pq(mag, rsq);
            if (!p2k::is_legal(pw, qw, !zero) ||
                !p2k::magnitude_admissible(p2k::nearest_lattice_word(mag), !zero)) {
              return;
            }
            if (zero) {
              words[0] = mag;
              words[1] = rsq;
            } else {
              words[2] = mag;
              words[3] = rsq;
            }
            document_->editSection(section, words);
          });
  connect(armadillo_, &ArmadilloView::rootReleased, this, [this] {
    if (fit_active_) return;
    document_->commitGesture(armadillo_before_);
  });
  connect(armadillo_, &ArmadilloView::zeroParked, this, [this](std::size_t section) {
    if (fit_active_) return;
    if ((document_->freedomMask() & trench::core::p2k::zero_bit(section)) == 0U) return;
    auto words = document_->cornerSnapshot()[section];
    words[0] = trench::core::kIdentitySection[0];
    words[1] = trench::core::kIdentitySection[1];
    document_->editSection(section, words);
  });
  connect(armadillo_, &ArmadilloView::poleParked, this, [this](std::size_t section) {
    if (fit_active_) return;
    if ((document_->freedomMask() & trench::core::p2k::pole_bit(section)) == 0U) return;
    if (document_->cornerSnapshot()[section] == trench::core::kIdentitySection) return;
    document_->editSection(section, trench::core::kIdentitySection);
  });
  connect(armadillo_, &ArmadilloView::placeRequested, this,
          [this](double hz, double radius) { placeResonanceAt(hz, radius); });
  connect(document_, &BodyDocument::bodyChanged, this, [this] {
    response_plot_->refresh();
    armadillo_->refresh();
    updateReadout();
    updateProbes();
    updateInterior();
    updatePostureMatch();
  });
  connect(morph_strip_, &MorphStrip::cornerPicked, this, [this](int corner) {
    document_->setCorner(static_cast<std::size_t>(corner));
  });
  connect(morph_strip_, &MorphStrip::cornerCopyRequested, this, [this](int corner) {
    copyCornerFromCurrent(static_cast<std::size_t>(corner));
  });
  connect(morph_strip_, &MorphStrip::cornerMenuRequested, this,
          [this](int corner, const QPoint& at) {
            const auto to = static_cast<std::size_t>(corner);
            QMenu menu(this);
            auto* copy = menu.addAction(
                QStringLiteral("COPY FROM CORNER %1").arg(document_->corner() + 1));
            copy->setEnabled(to != document_->corner() && !fit_active_);
            auto* save = menu.addAction(QStringLiteral("SAVE CORNER…"));
            auto* load = menu.addAction(QStringLiteral("LOAD CORNER…"));
            const auto* chosen = menu.exec(at);
            if (chosen == copy) {
              copyCornerFromCurrent(to);
            } else if (chosen == save) {
              document_->setCorner(to);
              saveCornerAs();
            } else if (chosen == load) {
              document_->setCorner(to);
              chooseCorner();
            }
          });
  connect(document_, &BodyDocument::cornerChanged, this, [this](std::size_t corner) {
    morph_strip_->setCorner(corner);
    response_plot_->setCorner(corner);
    armadillo_->setCorner(corner);
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q, view.semitones);
    morph_strip_->setTranspose(static_cast<int>(std::lround(view.semitones)));
    updateProbes();
    updatePostureMatch();
  });
  connect(document_, &BodyDocument::viewChanged, this, [this] {
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q, view.semitones);
    morph_strip_->setTranspose(static_cast<int>(std::lround(view.semitones)));
    updateReadout();
    updateProbes();
  });
  connect(morph_strip_, &MorphStrip::viewEdited, this,
          [this](float morph, float q) { document_->setView(morph, q); });
  connect(morph_strip_, &MorphStrip::transposeEdited, this,
          [this](int semitones) { document_->setTranspose(semitones); });
  connect(morph_strip_, &MorphStrip::characterGestureStarted, this, [this] {
    character_before_ = {document_->cornerSnapshot(2), document_->cornerSnapshot(3)};
    character_gesture_ = true;
  });
  connect(morph_strip_, &MorphStrip::characterGestureFinished, this, [this] {
    character_gesture_ = false;
    document_->commitCharacter(character_before_[0], character_before_[1]);
  });
  connect(morph_strip_, &MorphStrip::characterEdited, this, &MainWindow::applyCharacter);
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

  connect(fit_controller_, &FitController::previewReady, this,
          [this](quint64 generation, const QList<quint16>& words) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            document_->applyFitStep(fit_corner_, unflatten(words));
          });
  connect(fit_controller_, &FitController::finished, this,
          [this](quint64 generation, bool ok, const QList<quint16>& words) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            if (ok) {
              document_->applyFitStep(fit_corner_, unflatten(words));
              document_->commitFit(fit_corner_, pre_fit_);
            } else {
              document_->applyCorner(fit_corner_, pre_fit_);
            }
            endRun();
          });

  connect(chassis_bar_, &ChassisBar::verbClicked, this, [this](ChassisBar::Verb verb) {
    switch (verb) {
      case ChassisBar::Verb::kSave:
        if (body_path_.empty()) {
          saveBodyAs();
        } else {
          saveBody(body_path_);
        }
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
  rebuildPostureGroups();
  connect(posture_list_, &PostureList::postureChosen, this, &MainWindow::applyVowel);
  connect(fit_room_, &FitRoom::overlaySelected, this, &MainWindow::selectOverlay);
  connect(fit_room_, &FitRoom::overlayRemoved, this, &MainWindow::removeOverlay);
  connect(fit_room_, &FitRoom::loadRequested, this, &MainWindow::chooseTarget);
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
  updatePostureMatch();
  updateVerbs();
  updateProbes();
  updateInterior();

  qApp->installEventFilter(this);
}

ResponsePlotWidget* MainWindow::responsePlot() const noexcept { return response_plot_; }

ArmadilloView* MainWindow::armadilloView() const noexcept { return armadillo_; }

ChassisBar* MainWindow::chassisBar() const noexcept { return chassis_bar_; }

MorphStrip* MainWindow::morphStrip() const noexcept { return morph_strip_; }

PostureList* MainWindow::postureList() const noexcept { return posture_list_; }

SectionReadout* MainWindow::sectionReadout() const noexcept {
  return section_readout_;
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

void MainWindow::placeResonanceAt(double frequency_hz, double radius) {
  namespace p2k = trench::core::p2k;
  if (fit_active_) return;
  const auto before_corner = document_->cornerSnapshot();
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    if (p2k::param_of(before_corner[section], document_->sampleRateHz()).type !=
        p2k::SectionType::kOff) {
      continue;
    }
    auto candidate = before_corner[section];
    const auto [mag, rsq] = p2k::words_from_root(
        std::clamp(frequency_hz, 20.0, p2k::kRootHiHz), std::clamp(radius, 0.0, 0.99998));
    candidate[2] = mag;
    candidate[3] = rsq;
    if (candidate == before_corner[section]) return;
    document_->editSection(section, candidate);
    document_->commitGesture(before_corner);
    selectSection(section);
    return;
  }
}

void MainWindow::copyCornerFromCurrent(std::size_t to) {
  if (fit_active_ || to >= trench::core::kLegacyCornerCount || to == document_->corner()) {
    return;
  }
  const auto source = document_->cornerSnapshot();
  document_->setCorner(to);
  const auto before = document_->cornerSnapshot();
  if (before == source) return;
  document_->applyCorner(source);
  document_->commitFit(document_->corner(), before);
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

void MainWindow::applyPole(std::size_t section, double frequency_hz, double bw_hz) {
  namespace p2k = trench::core::p2k;
  if (section >= trench::core::kLegacySectionCount || fit_active_) return;
  const auto before_corner = document_->cornerSnapshot();
  const auto before = before_corner[section];
  if (!p2k::pole_of(before, document_->sampleRateHz())) return;
  const std::array<std::uint16_t, 4> current{before[0], before[1], before[2], before[3]};
  const auto roots = p2k::words_from_pole(frequency_hz, bw_hz, current, section,
                                          document_->sampleRateHz());
  auto candidate = before;
  for (std::size_t word = 0; word < roots.size(); ++word) {
    candidate[word] = roots[word];
  }
  if (candidate == before) return;
  document_->editSection(section, candidate);
  document_->commitGesture(before_corner);
}

void MainWindow::clearSection(std::size_t section) {
  if (section >= trench::core::kLegacySectionCount || fit_active_) return;
  const auto before_corner = document_->cornerSnapshot();
  if (before_corner[section] == trench::core::kIdentitySection) return;
  document_->editSection(section, trench::core::kIdentitySection);
  document_->commitGesture(before_corner);
}

void MainWindow::selectSection(std::size_t section, ResponsePlotWidget::Lane lane) {
  if (section >= trench::core::kLegacySectionCount) return;
  selected_section_ = section;
  response_plot_->setSelectedSection(section, lane);
  armadillo_->setSelected(section, lane == ResponsePlotWidget::Lane::kZero);
  updateReadout();
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

void MainWindow::rebuildPostureGroups() {
  QList<PostureList::Group> groups;
  PostureList::Group mine{QStringLiteral("MINE"), {}};
  for (const auto& entry : user_postures_.entries()) mine.names.push_back(entry.name);
  groups.push_back(mine);
  for (const auto* type : kFilterTypes) {
    PostureList::Group group{QString::fromLatin1(type), {}};
    for (const auto& skeleton : trench::core::p2k::postures()) {
      if (skeleton.type != type) continue;
      group.names.push_back(
          QString::fromUtf8(skeleton.name.data(), static_cast<int>(skeleton.name.size())));
    }
    for (const auto& vowel : trench::core::p2k::compiled_vowels()) {
      if (vowel.type != type) continue;
      group.names.push_back(
          QString::fromUtf8(vowel.name.data(), static_cast<int>(vowel.name.size())));
    }
    groups.push_back(group);
  }
  posture_list_->setGroups(groups);
}

std::vector<UserPostures::Pole> MainWindow::currentPolePosture() const {
  std::vector<UserPostures::Pole> poles;
  const auto current = document_->cornerSnapshot();
  for (std::size_t section = 0; section < current.size(); ++section) {
    if (!trench::core::p2k::pole_of(current[section], document_->sampleRateHz())) continue;
    poles.push_back({section, current[section][2], current[section][3]});
  }
  return poles;
}

void MainWindow::keepPosture() {
  if (fit_active_) return;
  const auto name = user_postures_.keep(currentPolePosture());
  if (name.isEmpty()) return;
  rebuildPostureGroups();
  updatePostureMatch();
}

std::optional<BodyDocument::CornerSnapshot> MainWindow::cornerWithPosture(
    const QString& symbol) const {
  namespace p2k = trench::core::p2k;
  if (const auto* mine = user_postures_.find(symbol)) {
    BodyDocument::CornerSnapshot after;
    after.fill(trench::core::kIdentitySection);
    for (const auto& pole : mine->poles) {
      auto& row = after[pole.row];
      const auto roots = p2k::words_with_parked_zero({row[0], row[1], pole.mag, pole.rsq},
                                                     pole.row, document_->sampleRateHz());
      for (std::size_t word = 0; word < roots.size(); ++word) row[word] = roots[word];
    }
    p2k::write_dc_unity_scales(after);
    return after;
  }
  const auto name = symbol.toStdString();
  const auto* skeleton = p2k::posture(name);
  const auto* vowel = p2k::klatt_vowel(name);
  const auto* manual = p2k::manual_recipe(name);
  if (skeleton == nullptr && vowel == nullptr && manual == nullptr) return std::nullopt;
  BodyDocument::CornerSnapshot after;
  after.fill(trench::core::kIdentitySection);
  if (skeleton != nullptr) {
    for (const auto& pole : p2k::pole_words_from_posture(*skeleton)) {
      auto& row = after[pole.row];
      const auto roots = p2k::words_with_parked_zero({row[0], row[1], pole.mag, pole.rsq},
                                                     pole.row, document_->sampleRateHz());
      for (std::size_t word = 0; word < roots.size(); ++word) row[word] = roots[word];
    }
  } else {
    const auto words = p2k::words_from_recipe(vowel != nullptr ? p2k::rows_from_formants(vowel->f)
                                                               : manual->recipe);
    for (std::size_t section = 0; section < p2k::kStageCount; ++section) {
      for (std::size_t word = 0; word < 4; ++word) after[section][word] = words[section][word];
    }
  }
  p2k::write_dc_unity_scales(after);
  return after;
}

bool MainWindow::posturePolesHeld(const QString& symbol) const {
  namespace p2k = trench::core::p2k;
  const auto current = document_->cornerSnapshot();
  if (const auto* mine = user_postures_.find(symbol)) {
    for (const auto& pole : mine->poles) {
      if (current[pole.row][2] != pole.mag || current[pole.row][3] != pole.rsq) return false;
    }
    return true;
  }
  const auto* skeleton = p2k::posture(symbol.toStdString());
  if (skeleton != nullptr) {
    for (const auto& pole : p2k::pole_words_from_posture(*skeleton)) {
      if (current[pole.row][2] != pole.mag || current[pole.row][3] != pole.rsq) return false;
    }
    return true;
  }
  const auto after = cornerWithPosture(symbol);
  return after && *after == current;
}

void MainWindow::updatePostureMatch() {
  if (keep_posture_ != nullptr) {
    keep_posture_->setEnabled(!fit_active_ && !currentPolePosture().empty());
  }
  if (posture_list_ == nullptr) return;
  QString matched;
  for (int row = 0; row < posture_list_->count(); ++row) {
    const auto name = posture_list_->item(row)->data(Qt::UserRole).toString();
    if (name.isEmpty()) continue;
    if (posturePolesHeld(name)) {
      matched = name;
      break;
    }
  }
  posture_list_->setMatched(matched);
}

void MainWindow::applyVowel(const QString& symbol) {
  if (fit_active_) return;
  const auto after = cornerWithPosture(symbol);
  if (!after) return;
  const auto before = document_->cornerSnapshot();
  if (*after == before) {
    updatePostureMatch();
    return;
  }
  document_->applyCorner(*after);
  document_->commitFit(document_->corner(), before);
  updatePostureMatch();
}

void MainWindow::applyCharacter(double amount) {
  if (fit_active_) return;
  if (!character_gesture_) {
    character_before_ = {document_->cornerSnapshot(2), document_->cornerSnapshot(3)};
  }
  document_->applyCharacter(amount);
  if (!character_gesture_) {
    document_->commitCharacter(character_before_[0], character_before_[1]);
  }
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
  const auto roots = roots_of_corner(pre_fit_);
  auto ceiling = *document_->target();
  const auto model = document_->viewResponseDb();
  const auto& grid = document_->grid();
  double offset = 0.0;
  if (ceiling.size() == model.size() && ceiling.size() == grid.weight.size() &&
      grid.weight_sum > 0.0) {
    for (std::size_t index = 0; index < ceiling.size(); ++index) {
      offset += grid.weight[index] * (ceiling[index] - model[index]);
    }
    offset /= grid.weight_sum;
    for (auto& db : ceiling) db -= offset;
  }
  fit_controller_->startZeros(std::move(ceiling), roots, document_->freedomMask(), grid);
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

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Space && !key->isAutoRepeat()) {
      setAuditionGate(event->type() == QEvent::KeyPress);
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateProbes() {
  chassis_bar_->setPowerDb(document_->viewPowerDb());
  chassis_bar_->setScoreDb(document_->targetScoreDb());
  refreshFitRoom();
  updateAudition();
}

void MainWindow::updateReadout() {
  const auto& words = document_->body().words[document_->corner()][selected_section_];
  const auto geometry =
      trench::core::geometry_from_words(words, document_->sampleRateHz());
  section_readout_->setReading(
      selected_section_, trench::core::p2k::pole_of(words, document_->sampleRateHz()),
      trench::core::p2k::mask_of(words, selected_section_, document_->sampleRateHz()),
      std::holds_alternative<trench::core::ConjugatePair>(geometry.zero));
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
