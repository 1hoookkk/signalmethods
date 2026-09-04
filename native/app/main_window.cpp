#include "main_window.hpp"

#ifndef TRENCH_AUDITION_SLOT
#define TRENCH_AUDITION_SLOT ""
#endif

#include "body_io.hpp"
#include "gesture_dial.hpp"
#include "keyframe_grid.hpp"
#include "pole_templates.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/body_from_audio.hpp"
#include "trench/core/native_body.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QGuiApplication>
#include <QCloseEvent>
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QTimer>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QToolButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace {

using Resonant = trench::core::native::Resonant;

constexpr int kBottomHeight = 300;
constexpr int kInsideGroup = 6;
constexpr int kBetweenGroups = 12;

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return {};
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

QFont captionFont(const QWidget* base) { return base->font(); }

bool offscreen() {
  return QGuiApplication::platformName() == QLatin1String("offscreen");
}

double poleHzOf(const trench::core::Biquad& section, double sample_rate_hz) {
  const double radius_squared = section[4];
  if (!(radius_squared > 0.0) || !std::isfinite(radius_squared)) return 0.0;
  const double radius = std::sqrt(radius_squared);
  const double cosine = -section[3] / (2.0 * radius);
  if (!(std::abs(cosine) < 1.0)) return 0.0;
  return std::acos(cosine) * sample_rate_hz / (2.0 * std::numbers::pi);
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), audition_(std::make_unique<trench::audio::Audition>()) {
  setWindowTitle(QStringLiteral("TRENCH · 6 × 2P2Z"));
  resize(1560, 860);

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* top = new QHBoxLayout;
  top->setSpacing(kInsideGroup);
  auto* load = new QPushButton(QStringLiteral("OPEN"), central);
  load->setObjectName(QStringLiteral("openBody"));
  auto* reset = new QPushButton(QStringLiteral("RESET"), central);
  reset->setObjectName(QStringLiteral("resetDocument"));
  frames_button_ = new QPushButton(QStringLiteral("FRAMES"), central);
  frames_button_->setObjectName(QStringLiteral("frames"));
  connect(frames_button_, &QPushButton::clicked, this, [this] { openFrames(); });
  from_audio_button_ = new QToolButton(central);
  from_audio_button_->setObjectName(QStringLiteral("fromAudio"));
  from_audio_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  from_audio_button_->setPopupMode(QToolButton::MenuButtonPopup);
  auto* from_audio_menu = new QMenu(from_audio_button_);
  auto* from_audio_modes = new QActionGroup(this);
  from_audio_modes->setExclusive(true);
  six_bells_action_ = from_audio_menu->addAction(QStringLiteral("SIX BELLS"));
  six_bells_action_->setObjectName(QStringLiteral("fromAudioSixBells"));
  speech_action_ = from_audio_menu->addAction(QStringLiteral("SPEECH"));
  speech_action_->setObjectName(QStringLiteral("fromAudioSpeech"));
  for (QAction* mode : {six_bells_action_, speech_action_}) {
    mode->setCheckable(true);
    from_audio_modes->addAction(mode);
  }
  from_audio_button_->setMenu(from_audio_menu);
  connect(six_bells_action_, &QAction::triggered, this, [this] { setFromAudioMode(0); });
  connect(speech_action_, &QAction::triggered, this, [this] { setFromAudioMode(1); });
  setFromAudioMode(0);
  anchor_button_ = new QToolButton(central);
  anchor_button_->setObjectName(QStringLiteral("anchor"));
  anchor_button_->setText(QStringLiteral("ANCHOR"));
  anchor_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  anchor_button_->setPopupMode(QToolButton::MenuButtonPopup);
  anchor_button_->setCheckable(true);
  anchor_button_->setChecked(true);
  auto* anchor_menu = new QMenu(anchor_button_);
  auto* anchor_now = anchor_menu->addAction(QStringLiteral("ANCHOR NOW"));
  anchor_now->setObjectName(QStringLiteral("anchorNow"));
  auto* anchor_square = anchor_menu->addAction(QStringLiteral("ANCHOR SQUARE"));
  anchor_square->setObjectName(QStringLiteral("anchorSquare"));
  anchor_button_->setMenu(anchor_menu);
  auto* save = new QPushButton(QStringLiteral("SAVE"), central);
  save->setObjectName(QStringLiteral("saveBody"));
  auto* export_body = new QPushButton(QStringLiteral("EXPORT .BODY240"), central);
  export_body->setObjectName(QStringLiteral("exportBody240"));
  auto* copy_across = new QPushButton(QStringLiteral("COPY ACROSS"), central);
  copy_across->setObjectName(QStringLiteral("copyAcross"));
  audition_button_ = new QPushButton(QStringLiteral("AUDITION"), central);
  audition_button_->setObjectName(QStringLiteral("auditionSwitch"));
  audition_button_->setCheckable(true);
  solo_button_ = new QPushButton(QStringLiteral("SOLO"), central);
  solo_button_->setObjectName(QStringLiteral("soloStage"));
  solo_button_->setCheckable(true);
  solo_button_->setEnabled(false);
  setAcceptDrops(true);
  for (QWidget* chrome : std::initializer_list<QWidget*>{
           load, reset, frames_button_, from_audio_button_, anchor_button_,
           audition_button_, solo_button_, save, export_body, copy_across}) {
    chrome->setFont(captionFont(chrome));
    chrome->setFocusPolicy(Qt::NoFocus);
  }

  keyframe_grid_ = new KeyframeGrid(this);
  keyframe_grid_->onPick = [this](const KeyframeGrid::Entry& entry, bool partner) {
    landPick(entry, partner);
  };
  keyframe_grid_->onHover = [this](int tile) {
    const KeyframeGrid::Entry* entry = keyframe_grid_->entryAt(tile);
    if (entry == nullptr) return;
    if (cascade_plot_ != nullptr) {
      auto ghost = keyframe_grid_->responseOn(tile, cascade_plot_->gridHz());
      if (ghost.size() == cascade_plot_->gridHz().size()) {
        cascade_plot_->setReference(std::move(ghost));
      }
    }
    pushAuditionView(frameView(*entry));
    if (status_label_ != nullptr && audition_ && audition_->running()) {
      status_label_->setText(QStringLiteral("HEARING · %1").arg(entry->name));
    }
  };
  keyframe_grid_->onHoverFrame = [this](const trench::app::Keyframe& key) {
    if (cascade_plot_ != nullptr) {
      auto ghost = trench::app::responseDb(key, cascade_plot_->gridHz());
      if (ghost.size() == cascade_plot_->gridHz().size()) {
        cascade_plot_->setReference(std::move(ghost));
      }
    }
    pushAuditionView(frameView(key));
    if (status_label_ != nullptr && audition_ && audition_->running()) {
      status_label_->setText(QStringLiteral("HEARING · %1").arg(key.name));
    }
  };
  keyframe_grid_->onPickFrame = [this](const trench::app::Keyframe& key, bool partner) {
    landFrame(key, partner);
  };
  keyframe_grid_->onHoverLeave = [this] {
    restorePlotReference();
    updateAuditionView();
    if (status_label_ != nullptr && !audition_line_.isEmpty()) {
      status_label_->setText(audition_line_);
    }
  };
  keyframe_grid_->onClose = [this] { closeFrames(); };

  slot_box_ = new QCheckBox(QStringLiteral("SLOT \u2192 TRENCH DEV"), central);
  slot_box_->setObjectName(QStringLiteral("slot"));
  slot_timer_ = new QTimer(this);
  slot_timer_->setSingleShot(true);
  slot_timer_->setInterval(120);
  connect(slot_timer_, &QTimer::timeout, this, [this] { pushSlot(); });
  connect(slot_box_, &QCheckBox::toggled, this, [this](bool on) { if (on) pushSlot(); });
  connect(&state_, &EditorState::changed, this, [this] { if (slot_box_->isChecked()) slot_timer_->start(); });

  top->addWidget(load);
  top->addWidget(reset);
  top->addSpacing(kBetweenGroups);
  top->addWidget(frames_button_);
  top->addWidget(from_audio_button_);
  top->addSpacing(kBetweenGroups);
  top->addWidget(anchor_button_);
  top->addSpacing(kBetweenGroups);
  top->addWidget(audition_button_);
  top->addWidget(solo_button_);
  top->addStretch(1);
  top->addWidget(slot_box_);
  top->addWidget(save);
  top->addWidget(export_body);
  layout->addLayout(top);

  cascade_plot_ = new CascadePlot(central);
  cascade_plot_->setObjectName(QStringLiteral("cascadePlot"));
  layout->addWidget(cascade_plot_, 1);

  morph_pad_ = new MorphPad(&state_, central);
  morph_pad_->setObjectName(QStringLiteral("morphPad"));
  path_meter_ = new PathMeter(central);
  path_meter_->setObjectName(QStringLiteral("pathMeter"));
  rows_table_ = new RowsTable(&state_, central);

  auto* transpose = new GestureDial(
      QStringLiteral("TRANSPOSE"), 0.03,
      [](double total) { return QString::asprintf("%+.1f st", total); },
      central);
  transpose->onDelta = [this](double delta) {
    state_.applyAffine(delta, 1.0, 1.0, 1.0);
  };
  transpose->onBegin = [this] { state_.beginUndoGroup(); };
  transpose->onEnd = [this] { state_.endUndoGroup(); };
  auto* posture = new GestureDial(
      QStringLiteral("POSTURE"), 0.03,
      [](double total) { return QString::asprintf("%+.1f st", total); },
      central);
  posture->setObjectName(QStringLiteral("posture"));
  posture->onBegin = [this] {
    state_.beginUndoGroup();
    state_.copyCornerTo(state_.editingCorner() ^ 1u);
  };
  posture->onDelta = [this](double delta) {
    state_.applyAffineAt(state_.editingCorner() ^ 1u, delta, 1.0, 1.0, 1.0);
  };
  posture->onEnd = [this] {
    state_.endUndoGroup();
    status_label_->setText(QStringLiteral("POSTURE \u00b7 CORNER %1 \u2192 CORNER %2")
                               .arg(state_.editingCorner() + 1)
                               .arg((state_.editingCorner() ^ 1u) + 1));
  };
  auto* sharpen = new GestureDial(
      QStringLiteral("SHARPEN"), 0.0002,
      [](double total) { return QString::asprintf("%+.4f r", total); },
      central);
  sharpen->setObjectName(QStringLiteral("sharpen"));
  sharpen->onBegin = [this] {
    state_.beginUndoGroup();
    state_.copyCornerTo(state_.editingCorner() ^ 2u);
  };
  sharpen->onDelta = [this](double delta) {
    state_.sharpenPolesAt(state_.editingCorner() ^ 2u, delta);
  };
  sharpen->onEnd = [this] {
    state_.endUndoGroup();
    status_label_->setText(QStringLiteral("SHARPEN \u00b7 CORNER %1 \u2192 CORNER %2")
                               .arg(state_.editingCorner() + 1)
                               .arg((state_.editingCorner() ^ 2u) + 1));
  };

  status_label_ = new QLabel(QStringLiteral("44,100 Hz DSP"), central);
  status_label_->setObjectName(QStringLiteral("status"));
  status_label_->setFont(captionFont(status_label_));

  auto* bottom = new QWidget(central);
  bottom->setFixedHeight(kBottomHeight);
  auto* bottom_row = new QHBoxLayout(bottom);
  bottom_row->setContentsMargins(0, 0, 0, 0);
  bottom_row->setSpacing(8);
  auto* left = new QVBoxLayout;
  left->setSpacing(6);
  left->addWidget(morph_pad_, 0, Qt::AlignLeft);
  auto* gestures = new QHBoxLayout;
  gestures->setSpacing(6);
  gestures->addWidget(posture);
  gestures->addWidget(sharpen);
  gestures->addWidget(transpose);
  gestures->addWidget(copy_across);
  gestures->addStretch(1);
  left->addLayout(gestures);
  left->addStretch(1);
  bottom_row->addLayout(left, 0);
  auto* right = new QVBoxLayout;
  right->setSpacing(4);
  right->addWidget(rows_table_, 1);
  auto* out_line = new QHBoxLayout;
  out_line->setSpacing(6);
  out_line->addWidget(path_meter_, 0, Qt::AlignVCenter);
  out_line->addWidget(path_meter_->readout(), 0, Qt::AlignVCenter);
  out_line->addWidget(path_meter_->hereReadout(), 0, Qt::AlignVCenter);
  out_line->addStretch(1);
  out_line->addWidget(status_label_, 0, Qt::AlignVCenter);
  right->addLayout(out_line, 0);
  bottom_row->addLayout(right, 1);
  layout->addWidget(bottom, 0);

  setCentralWidget(central);

  rows_table_->onSoFarHover = [this](std::size_t row) {
    if (cascade_plot_ == nullptr) return;
    std::vector<double> ghost;
    ghost.reserve(cascade_plot_->gridHz().size());
    for (const double hz : cascade_plot_->gridHz()) {
      ghost.push_back(rows_table_->soFarDbAt(row, hz));
    }
    cascade_plot_->setReference(std::move(ghost));
  };
  rows_table_->onSoFarLeave = [this] { restorePlotReference(); };

  cascade_plot_->onSelect = [this](std::size_t section) { state_.selectSection(section); };
  cascade_plot_->onDragBegin = [this](std::size_t section) {
    state_.beginUndoGroup();
    rows_table_->handleBegin(section);
  };
  cascade_plot_->onDragEnd = [this] { state_.endUndoGroup(); };
  cascade_plot_->onNote = [this](std::size_t section, double hz) {
    rows_table_->handleNote(section, hz);
  };
  cascade_plot_->onHeight = [this](std::size_t section, double db) {
    rows_table_->handleHeight(section, db);
  };
  cascade_plot_->onRingSteps = [this](std::size_t section, int steps) {
    rows_table_->handleRingSteps(section, steps);
  };
  cascade_plot_->onWheelRing = [this](std::size_t section, int steps) {
    rows_table_->handleWheelRing(section, steps);
  };
  cascade_plot_->onCreate = [this](double hz, double db) {
    rows_table_->handleCreate(hz, db);
  };

  connect(load, &QPushButton::clicked, this, &MainWindow::openFile);
  connect(from_audio_button_, &QToolButton::clicked, this, [this] {
    const QString chosen = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open a recording"), QString(),
        QStringLiteral("Audio (*.wav)"));
    if (chosen.isEmpty()) return;
    seedFromAudioFile(chosen);
  });
  connect(anchor_now, &QAction::triggered, this, [this] {
    const std::size_t corner = state_.editingCorner();
    state_.anchorCornerToSquare(corner);
    status_label_->setText(QStringLiteral("ANCHOR · CORNER %1 → CORNER %2 AND CORNER %3")
                               .arg(corner + 1)
                               .arg((corner ^ 1u) + 1)
                               .arg((corner ^ 2u) + 1));
  });
  connect(anchor_square, &QAction::triggered, this, [this] {
    state_.anchorSquare();
    status_label_->setText(
        QStringLiteral("ANCHOR SQUARE · CORNERS 2 3 4 → CORNER 1"));
  });
  connect(reset, &QPushButton::clicked, this, &MainWindow::resetDocument);
  connect(copy_across, &QPushButton::clicked, this, &MainWindow::copyAcross);
  auto* undo = new QShortcut(QKeySequence::Undo, this);
  connect(undo, &QShortcut::activated, this, [this] { state_.undo(); });
  auto* redo = new QShortcut(QKeySequence::Redo, this);
  connect(redo, &QShortcut::activated, this, [this] { state_.redo(); });
  auto* redo_shift = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), this);
  connect(redo_shift, &QShortcut::activated, this, [this] { state_.redo(); });
  connect(audition_button_, &QPushButton::toggled, this,
          [this](bool open) { setAudition(open); });
  connect(save, &QPushButton::clicked, this, &MainWindow::saveDocument);
  connect(export_body, &QPushButton::clicked, this, &MainWindow::exportBody240);
  auto* toggle_addressed = new QShortcut(QKeySequence(Qt::Key_Space), this);
  connect(toggle_addressed, &QShortcut::activated, this, [this] {
    QWidget* focused = qApp->focusWidget();
    if (qobject_cast<QAbstractSpinBox*>(focused) ||
        qobject_cast<QLineEdit*>(focused)) {
      return;
    }
    state_.toggleSection(state_.selectedSection());
  });

  connect(&state_, &EditorState::changed, this, &MainWindow::refresh);
  connect(solo_button_, &QPushButton::toggled, this, [this](bool) { refresh(); });
  connect(&state_, &EditorState::selectionChanged, this,
          [this] { refresh(); });

  refresh();
  setMinimumSize(sizeHint());
}

MainWindow::~MainWindow() {
  if (audition_) audition_->stop();
}

void MainWindow::openFile() {
  const QString chosen = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open body"), QString(),
      QStringLiteral("TRENCH document (*.trenchbody);;"
                     "Packed body (*.body240 *.bin);;All files (*)"));
  if (chosen.isEmpty()) return;
  openPath(chosen);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
  for (const QUrl& url : event->mimeData()->urls()) {
    if (!url.isLocalFile()) continue;
    const QString chosen = url.toLocalFile();
    if (chosen.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
      seedFromAudioFile(chosen);
    } else {
      openPath(chosen);
    }
    event->acceptProposedAction();
    return;
  }
}

void MainWindow::seedFromAudioFile(const QString& path) {
  const auto clip = trench::core::audio::read_wav_mono(
      std::filesystem::path(path.toStdWString()));
  if (!clip) {
    if (auto* bar = findChild<QStatusBar*>()) {
      bar->showMessage(QStringLiteral("Could not read that WAV"), 4000);
    }
    return;
  }
  seedFromAudio(clip->samples, clip->sample_rate_hz);
}

void MainWindow::setFromAudioMode(int mode) {
  const bool speech = mode == 1;
  if (six_bells_action_ != nullptr) six_bells_action_->setChecked(!speech);
  if (speech_action_ != nullptr) speech_action_->setChecked(speech);
  if (from_audio_button_ == nullptr) return;
  from_audio_button_->setText(speech
                                  ? QStringLiteral("FROM AUDIO · SPEECH")
                                  : QStringLiteral("FROM AUDIO · SIX BELLS"));
}

int MainWindow::fromAudioMode() const noexcept {
  return speech_action_ != nullptr && speech_action_->isChecked() ? 1 : 0;
}

void MainWindow::seedFromAudio(const std::vector<float>& mono, double sample_rate_hz) {
  constexpr std::size_t kSections = trench::core::native::kSections;
  constexpr double kSpeechRateHz = 11'025.0;
  constexpr std::size_t kSpeechOrder = 12;
  const bool speech = fromAudioMode() == 1;
  const std::vector<double>& grid = cascade_plot_->gridHz();
  std::vector<trench::app::TypeRow> rows;
  std::vector<double> ghost;

  if (speech) {
    const auto poles = trench::core::audio::speech_poles(
        mono, sample_rate_hz, kSections, kSpeechRateHz, kSpeechOrder);
    if (poles.empty()) return;
    rows.reserve(poles.size());
    for (const auto& one : poles) {
      rows.push_back({trench::app::RowType::kPole, one.hz, one.bw_hz, 0.0});
    }
    const auto voiced =
        trench::core::audio::resample(mono, sample_rate_hz, kSpeechRateHz);
    const auto model =
        trench::core::audio::all_pole_model(voiced, kSpeechRateHz, kSpeechOrder);
    std::vector<double> modelled;
    modelled.reserve(grid.size());
    for (const double hz : grid) {
      if (hz < 0.45 * kSpeechRateHz) modelled.push_back(hz);
    }
    const auto head = trench::core::audio::envelope_db(model, modelled);
    ghost.assign(grid.size(), std::numeric_limits<double>::quiet_NaN());
    for (std::size_t i = 0; i < head.size() && i < ghost.size(); ++i) ghost[i] = head[i];
  } else {
    const auto model = trench::core::audio::all_pole_model(
        mono, sample_rate_hz, trench::core::audio::model_order(kSections));
    const auto found = trench::core::audio::neutral_rows(model, kSections);
    if (found.empty()) return;
    rows.reserve(found.size());
    for (const auto& one : found) {
      rows.push_back({trench::app::RowType::kEq, one.hz, one.bw_hz, one.gain_db});
    }
    ghost = trench::core::audio::envelope_db(model, grid);
  }

  landOnEditingCorner([this, &rows](std::size_t corner, bool anchor) {
    trench::app::applyTypeRows(state_, rows, corner, anchor);
  });

  std::vector<double> finite;
  finite.reserve(ghost.size());
  for (const double db : ghost) {
    if (std::isfinite(db)) finite.push_back(db);
  }
  if (!finite.empty()) {
    std::nth_element(finite.begin(),
                     finite.begin() + static_cast<std::ptrdiff_t>(finite.size() / 2),
                     finite.end());
    const double centre = finite[finite.size() / 2];
    for (double& db : ghost) db -= centre;
  }
  setPlotReference(std::move(ghost));
  status_label_->setText(
      QStringLiteral("FROM AUDIO · %1 · %2 ROWS → CORNER %3")
          .arg(speech ? QStringLiteral("SPEECH") : QStringLiteral("SIX BELLS"))
          .arg(rows.size())
          .arg(state_.editingCorner() + 1));
}

void MainWindow::openPath(const QString& chosen) {
  plot_reference_.clear();
  cascade_plot_->clearReference();
  const std::filesystem::path path(chosen.toStdWString());
  const QString name = QFileInfo(chosen).fileName().toUpper();
  const QString extension = QFileInfo(chosen).suffix().toLower();
  if (extension == QStringLiteral("trenchbody")) {
    openDocument(chosen);
    return;
  }
  if (extension == QStringLiteral("body240") || extension == QStringLiteral("bin")) {
    const auto bytes = readBytes(path);
    if (bytes.size() != trench::core::kLegacyBodyBytes) {
      status_label_->setText(QStringLiteral("PACKED BODY MUST BE 240 BYTES · %1").arg(name));
      return;
    }
    state_.setDocument(
        EditorState::documentFrom(trench::core::native::import_p2k(bytes)));
    state_.setSourceWords(trench::core::PackedBody::from_legacy_bytes(bytes));
    document_path_.clear();
    setWindowTitle(QStringLiteral("TRENCH · %1").arg(QFileInfo(chosen).fileName()));
    status_label_->setText(QStringLiteral("BODY · %1 · FOUR CORNERS").arg(name));
    return;
  }
  status_label_->setText(QStringLiteral("UNSUPPORTED · %1").arg(extension.toUpper()));
}

void MainWindow::saveDocument() {
  QString start = document_path_;
  if (start.isEmpty()) {
    const QString folder =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
        QStringLiteral("/TRENCH/bodies");
    QDir().mkpath(folder);
    start = folder + QStringLiteral("/untitled.trenchbody");
  }
  QString chosen = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save TRENCH document"), start,
      QStringLiteral("TRENCH document (*.trenchbody)"));
  if (chosen.isEmpty()) return;
  if (!chosen.endsWith(QStringLiteral(".trenchbody"), Qt::CaseInsensitive)) {
    chosen += QStringLiteral(".trenchbody");
  }
  const QString refusal = trench::app::saveDocument(state_.document(), chosen);
  if (!refusal.isEmpty()) {
    status_label_->setText(refusal);
    return;
  }
  document_path_ = chosen;
  setWindowTitle(
      QStringLiteral("TRENCH · %1").arg(QFileInfo(chosen).fileName()));
  status_label_->setText(
      QStringLiteral("SAVED · %1").arg(QFileInfo(chosen).fileName().toUpper()));
}

void MainWindow::resetDocument() {
  cascade_plot_->clearReference();
  state_.setDocument(EditorState::blank());
  document_path_.clear();
  setWindowTitle(QStringLiteral("TRENCH · 6 × 2P2Z"));
  status_label_->setText(QStringLiteral("RESET · SIX SECTIONS OFF"));
}

void MainWindow::exportBody240() {
  const QString folder =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
      QStringLiteral("/TRENCH/bodies");
  QDir().mkpath(folder);
  QString chosen = QFileDialog::getSaveFileName(
      this, QStringLiteral("Export packed body (legacy .body240)"),
      folder + QStringLiteral("/untitled.body240"),
      QStringLiteral("Packed body (*.body240)"));
  if (chosen.isEmpty()) return;
  if (!chosen.endsWith(QStringLiteral(".body240"), Qt::CaseInsensitive)) {
    chosen += QStringLiteral(".body240");
  }
  const QString refusal = trench::app::saveBody240(state_, chosen);
  status_label_->setText(
      refusal.isEmpty()
          ? QStringLiteral("EXPORTED · %1")
                .arg(QFileInfo(chosen).fileName().toUpper())
          : refusal);
}

void MainWindow::openDocument(const QString& chosen) {
  const QString name = QFileInfo(chosen).fileName().toUpper();
  QString error;
  const auto document = trench::app::loadDocument(chosen, &error);
  if (!document) {
    status_label_->setText(
        QStringLiteral("DOCUMENT REJECTED · %1 · %2").arg(name, error));
    return;
  }
  state_.setDocument(*document);
  document_path_ = chosen;
  setWindowTitle(
      QStringLiteral("TRENCH · %1").arg(QFileInfo(chosen).fileName()));
  status_label_->setText(QStringLiteral("OPENED · %1").arg(name));
}

bool MainWindow::anchoring() const {
  return anchor_button_ != nullptr && anchor_button_->isChecked();
}

void MainWindow::landOnEditingCorner(
    const std::function<void(std::size_t, bool)>& land) {
  landOnCorner(state_.editingCorner(), land);
}

void MainWindow::landOnCorner(std::size_t corner,
                              const std::function<void(std::size_t, bool)>& land) {
  const bool anchor = anchoring();
  state_.beginUndoGroup();
  land(corner, false);
  if (anchor) state_.anchorCornerToSquare(corner);
  state_.endUndoGroup();
}

void MainWindow::setPlotReference(std::vector<double> db_on_grid) {
  plot_reference_ = db_on_grid;
  cascade_plot_->setReference(std::move(db_on_grid));
}

void MainWindow::restorePlotReference() {
  if (cascade_plot_ == nullptr) return;
  if (!plot_reference_.empty() && plot_reference_.size() == cascade_plot_->gridHz().size()) {
    cascade_plot_->setReference(plot_reference_);
    return;
  }
  cascade_plot_->clearReference();
}

void MainWindow::openFrames() {
  if (keyframe_grid_ == nullptr || frames_button_ == nullptr) return;
  if (audition_ && !audition_->running() && !offscreen() && audition_->start().empty()) {
    grid_started_audition_ = true;
    updateAuditionView();
    applyAuditionSource();
    audition_->setGate(true);
    audition_line_ = QStringLiteral("AUDITION · FRAMES");
    if (status_label_ != nullptr) status_label_->setText(audition_line_);
  }
  keyframe_grid_->move(frames_button_->mapToGlobal(QPoint(0, frames_button_->height() + 2)));
  keyframe_grid_->show();
  keyframe_grid_->raise();
}

void MainWindow::closeFrames() {
  updateAuditionView();
  if (!grid_started_audition_) return;
  grid_started_audition_ = false;
  audition_line_.clear();
  if (!audition_) return;
  audition_->setGate(false);
  audition_->stop();
}

trench::audio::AuditionView MainWindow::frameView(const KeyframeGrid::Entry& entry) const {
  EditorState scratch;
  scratch.setDocument(EditorState::blank());
  scratch.setPadPosition(state_.morphPos(), state_.qPos());
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    switch (entry.kind) {
      case KeyframeGrid::Kind::kBank:
        trench::app::applyFrame(scratch, KeyframeGrid::bank()[entry.index], corner, false);
        break;
      case KeyframeGrid::Kind::kType:
        trench::app::applyPoleTemplate(scratch, KeyframeGrid::types()[entry.index], corner, false);
        break;
      case KeyframeGrid::Kind::kKeyframe:
        trench::app::applyKeyframe(scratch, KeyframeGrid::keys()[entry.index], corner, false);
        break;
    }
  }
  trench::audio::AuditionView view = scratch.view();
  view.trim_db = trench::audio::level_trim_db(view, EditorState::kDatumHz);
  return view;
}

trench::audio::AuditionView MainWindow::frameView(const trench::app::Keyframe& key) const {
  EditorState scratch;
  scratch.setDocument(EditorState::blank());
  scratch.setPadPosition(state_.morphPos(), state_.qPos());
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    trench::app::applyKeyframe(scratch, key, corner, false);
  }
  trench::audio::AuditionView view = scratch.view();
  view.trim_db = trench::audio::level_trim_db(view, EditorState::kDatumHz);
  return view;
}

void MainWindow::landFrame(const trench::app::Keyframe& key, bool partner) {
  const std::size_t corner = partner ? (state_.editingCorner() ^ 1u) : state_.editingCorner();
  landOnCorner(corner, [this, &key](std::size_t seat, bool anchor) {
    trench::app::applyKeyframe(state_, key, seat, anchor);
  });
  if (status_label_ == nullptr) return;
  status_label_->setText(QStringLiteral("KEYFRAME · %1 %2 · %3 rows → CORNER %4")
                             .arg(key.group, key.name)
                             .arg(key.rows.size())
                             .arg(corner + 1));
}

void MainWindow::landPick(const KeyframeGrid::Entry& entry, bool partner) {
  const std::size_t corner = partner ? (state_.editingCorner() ^ 1u) : state_.editingCorner();
  switch (entry.kind) {
    case KeyframeGrid::Kind::kBank: {
      const auto& frame = KeyframeGrid::bank()[entry.index];
      landOnCorner(corner, [this, &frame](std::size_t seat, bool anchor) {
        trench::app::applyFrame(state_, frame, seat, anchor);
      });
      status_label_->setText(QStringLiteral("FRAME · %1 %2 → CORNER %3")
                                 .arg(frame.family, frame.type)
                                 .arg(corner + 1));
      break;
    }
    case KeyframeGrid::Kind::kType: {
      const auto& tpl = KeyframeGrid::types()[entry.index];
      landOnCorner(corner, [this, &tpl](std::size_t seat, bool anchor) {
        trench::app::applyPoleTemplate(state_, tpl, seat, anchor);
      });
      status_label_->setText(
          QStringLiteral("TEMPLATE · %1 · %2 poles from %3 bodies → CORNER %4")
              .arg(tpl.label())
              .arg(tpl.ladder ? tpl.ratios.size()
                              : trench::app::pickPoles(tpl, trench::core::native::kSections).size())
              .arg(tpl.bodies.size())
              .arg(corner + 1));
      break;
    }
    case KeyframeGrid::Kind::kKeyframe: {
      const auto& key = KeyframeGrid::keys()[entry.index];
      landOnCorner(corner, [this, &key](std::size_t seat, bool anchor) {
        trench::app::applyKeyframe(state_, key, seat, anchor);
      });
      status_label_->setText(QStringLiteral("KEYFRAME · %1 %2 · %3 rows → CORNER %4")
                                 .arg(key.group, key.name)
                                 .arg(key.rows.size())
                                 .arg(corner + 1));
      break;
    }
  }
}

void MainWindow::setAxisNamesForTest(const QString& morph, const QString& q) {
  state_.setAxisNames(morph, q);
  if (status_label_ == nullptr) return;
  status_label_->setText(
      QStringLiteral("AXES · MORPH %1 · Q %2")
          .arg(morph.isEmpty() ? QStringLiteral("MORPH") : morph.toUpper(),
               q.isEmpty() ? QStringLiteral("Q") : q.toUpper()));
}

void MainWindow::applyAuditionSource() {
  audition_->setSaw(73.42, 0.18F);
}

void MainWindow::refresh() {
  std::array<bool, trench::core::native::kSections> enabled{};
  std::vector<double> seed_hz;
  const auto seed_root = [&seed_hz](const trench::core::native::Roots& roots) {
    const auto* found = std::get_if<Resonant>(&roots);
    if (found == nullptr) return;
    const Resonant& root = *found;
    if (!std::isfinite(root.hz) || !std::isfinite(root.bw_hz)) return;
    if (!(root.hz > 20.0) || !(root.hz < 20'000.0)) return;
    seed_hz.push_back(root.hz);
    for (const double share : {0.25, 0.5, 1.0, 2.0}) {
      seed_hz.push_back(
          std::clamp(root.hz - share * root.bw_hz, 20.0, 20'000.0));
      seed_hz.push_back(
          std::clamp(root.hz + share * root.bw_hz, 20.0, 20'000.0));
    }
  };
  for (std::size_t index = 0; index < enabled.size(); ++index) {
    enabled[index] = state_.sectionEnabled(index);
    if (!enabled[index]) continue;
    seed_root(state_.section(index).pole);
    if (state_.rootPresent(index, EditorState::Lane::kZero)) {
      seed_root(state_.section(index).zero);
    }
  }
  const std::size_t selected = state_.selectedSection();
  CascadePlot::View view;
  view.pad = trench::audio::design_audition(plotView(), EditorState::kDatumHz);
  const std::size_t editing = state_.editingCorner();
  view.enabled = enabled;
  for (std::size_t index = 0; index < enabled.size(); ++index) {
    const auto& pole = state_.sectionAt(editing, index).pole;
    const auto* tone = std::get_if<Resonant>(&pole);
    view.pole_hz[index] = tone != nullptr ? tone->hz : 0.0;
    const RowsTable::Type type =
        rows_table_ != nullptr ? rows_table_->typeAt(editing, index) : RowsTable::Type::kOff;
    view.peaked[index] = type == RowsTable::Type::kEq || type == RowsTable::Type::kNotch;
  }
  const std::size_t base = editing & ~static_cast<std::size_t>(1u);
  const trench::core::Cascade at_pad = state_.cascade(EditorState::kDatumHz);
  for (std::size_t index = 0; index < enabled.size(); ++index) {
    const auto lo = state_.poleHzAt(base, index);
    const auto hi = state_.poleHzAt(base + 1u, index);
    view.lo_pole_hz[index] = lo ? *lo : 0.0;
    view.hi_pole_hz[index] = hi ? *hi : 0.0;
    view.now_pole_hz[index] =
        lo && hi ? poleHzOf(at_pad[index], EditorState::kDatumHz) : 0.0;
  }
  view.seed_hz = seed_hz;
  view.selected = selected;
  const double morph = state_.morphPos();
  const double q = state_.qPos();
  view.pad_at_corner =
      (morph < 0.12 || morph > 0.88) && (q < 0.12 || q > 0.88);
  view.sample_rate_hz = EditorState::kDatumHz;
  cascade_plot_->setView(view);
  QString reading;
  if (const auto& words = state_.sourceWords()) {
    const trench::core::Cascade raw = trench::audio::design_audition(
        {*words, static_cast<float>(state_.morphPos()),
         static_cast<float>(state_.qPos()), 0.0, 0.0},
        EditorState::kDatumHz);
    const trench::core::Cascade mine = state_.cascade(EditorState::kDatumHz);
    const std::span<const trench::core::Biquad> raw_six{
        raw.data(), trench::core::native::kSections};
    const std::span<const trench::core::Biquad> mine_six{
        mine.data(), trench::core::native::kSections};
    double worst = 0.0;
    for (const double hz :
         trench::core::logarithmic_frequency_grid(40.0, 16'000.0, 256)) {
      worst = std::max(
          worst, std::abs(trench::core::cascade_response_db(raw_six, hz,
                                                            EditorState::kDatumHz) -
                          trench::core::cascade_response_db(mine_six, hz,
                                                            EditorState::kDatumHz)));
    }
    const QString bytes = QStringLiteral("BYTES ±%1 dB").arg(worst, 0, 'f', 2);
    reading = reading.isEmpty() ? bytes : reading + QStringLiteral(" · ") + bytes;
  }
  if (!reading.isEmpty()) status_label_->setText(reading);

  path_meter_->setBody(state_.packed(), state_.morphPos(), state_.qPos());
  morph_pad_->setWorst(path_meter_->worstMorph(), path_meter_->worstQ(),
                       path_meter_->worstDb());

  updateAuditionView();
}

void MainWindow::copyAcross() {
  const std::size_t from = state_.editingCorner();
  const std::size_t to = from ^ 1u;
  state_.copyCornerTo(to);
  status_label_->setText(QStringLiteral("COPIED · CORNER %1 → CORNER %2").arg(from + 1).arg(to + 1));
}

trench::audio::AuditionView MainWindow::heardView() const {
  trench::audio::AuditionView view = state_.view();
  const double trim = trench::audio::level_trim_db(view, EditorState::kDatumHz);
  if (solo_button_ != nullptr && solo_button_->isChecked()) {
    view = state_.soloView(state_.selectedSection());
  }
  view.trim_db = trim;
  return view;
}

trench::audio::AuditionView MainWindow::plotView() const {
  trench::audio::AuditionView view = heardView();
  view.trim_db = 0.0;
  return view;
}

void MainWindow::updateAuditionView() {
  pushAuditionView(heardView());
}

void MainWindow::pushAuditionView(trench::audio::AuditionView view) {
  if (auditionTap) auditionTap(view);
  if (!audition_) return;
  audition_->setView(std::move(view));
}

bool MainWindow::auditionRunning() const noexcept {
  return audition_ && audition_->running();
}

void MainWindow::syncAuditionButton(bool checked) {
  const QSignalBlocker blocker(audition_button_);
  audition_button_->setChecked(checked);
}

void MainWindow::setAudition(bool enabled) {
  if (!enabled) {
    audition_->setGate(false);
    audition_->stop();
    syncAuditionButton(false);
    solo_button_->setEnabled(false);
    grid_started_audition_ = false;
    audition_line_.clear();
    status_label_->setText(QStringLiteral("AUDITION CLOSED"));
    return;
  }
  const std::string error = audition_->start();
  if (!error.empty()) {
    syncAuditionButton(false);
    audition_line_.clear();
    status_label_->setText(QStringLiteral("AUDIO DEVICE · %1")
                               .arg(QString::fromStdString(error)));
    return;
  }
  grid_started_audition_ = false;
  updateAuditionView();
  applyAuditionSource();
  audition_->setGate(true);
  syncAuditionButton(true);
  solo_button_->setEnabled(true);
  audition_line_ = QStringLiteral("AUDITION OPEN · %1 · %2 Hz")
                       .arg(QString::fromStdString(audition_->deviceName()).toUpper())
                       .arg(audition_->sampleRateHz(), 0, 'f', 0);
  status_label_->setText(audition_line_);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  audition_->setGate(false);
  audition_->stop();
  syncAuditionButton(false);
  QMainWindow::closeEvent(event);
}

void MainWindow::pushSlot() {
  const QString path = QString::fromUtf8(TRENCH_AUDITION_SLOT);
  const QString err = trench::app::saveBody240(state_, path);
  if (!err.isEmpty()) status_label_->setText(QStringLiteral("SLOT · %1").arg(err));
}

