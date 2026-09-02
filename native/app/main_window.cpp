#include "main_window.hpp"

#include "body_io.hpp"
#include "gesture_dial.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/native_body.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace {

using Resonant = trench::core::native::Resonant;

double frequencyOf(const trench::core::native::Roots& roots) {
  if (const auto* tone = std::get_if<Resonant>(&roots)) return tone->hz;
  return std::get<trench::core::native::RealRoots>(roots).a_hz;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return {};
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

QFont captionFont(const QWidget* base) { return base->font(); }

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), audition_(std::make_unique<trench::audio::Audition>()) {
  setWindowTitle(QStringLiteral("TRENCH · 6 × 2P2Z"));
  resize(1400, 820);

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* top = new QHBoxLayout;
  top->setSpacing(10);
  auto* load = new QPushButton(QStringLiteral("OPEN"), central);
  auto* reset = new QPushButton(QStringLiteral("RESET"), central);
  reset->setObjectName(QStringLiteral("resetDocument"));
  auto* save = new QPushButton(QStringLiteral("SAVE"), central);
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
  load->setFont(captionFont(load));
  reset->setFont(captionFont(reset));
  save->setFont(captionFont(save));
  export_body->setFont(captionFont(export_body));
  audition_button_->setFont(captionFont(audition_button_));
  solo_button_->setFont(captionFont(solo_button_));
  for (QWidget* chrome : std::initializer_list<QWidget*>{
           load, reset, save, export_body,
           copy_across, audition_button_, solo_button_}) {
    chrome->setFocusPolicy(Qt::NoFocus);
  }
  top->addWidget(load);
  top->addWidget(reset);
  top->addStretch(1);
  layout->addLayout(top);
  auto* actions = new QHBoxLayout;
  actions->setSpacing(10);
  actions->addWidget(audition_button_);
  actions->addWidget(solo_button_);
  actions->addWidget(copy_across);
  actions->addStretch(1);
  actions->addWidget(export_body);
  actions->addWidget(save);
  layout->addLayout(actions);

  cascade_plot_ = new CascadePlot(central);
  cascade_plot_->setObjectName(QStringLiteral("cascadePlot"));
  morph_pad_ = new MorphPad(&state_, central);
  morph_pad_->setObjectName(QStringLiteral("morphPad"));
  row_table_ = new RowTable(&state_, central);
  row_table_->setObjectName(QStringLiteral("rowTable"));

  auto* body = new QHBoxLayout;
  body->setSpacing(8);
  auto* stack = new QVBoxLayout;
  stack->setSpacing(8);
  stack->addWidget(cascade_plot_, 1);
  stack->addWidget(row_table_, 0);
  body->addLayout(stack, 1);
  auto* side = new QVBoxLayout;
  side->setSpacing(8);
  body->addLayout(side, 0);
  layout->addLayout(body, 1);

  auto* gestures = new QHBoxLayout;
  gestures->setSpacing(10);
  auto* transpose = new GestureDial(
      QStringLiteral("TRANSPOSE"), 0.03,
      [](double total) { return QString::asprintf("%+.1f st", total); },
      central);
  transpose->onDelta = [this](double delta) {
    state_.applyAffine(delta, 1.0, 1.0, 1.0);
  };
  transpose->onBegin = [this] { state_.beginUndoGroup(); };
  transpose->onEnd = [this] { state_.endUndoGroup(); };
  auto* inspector = new QVBoxLayout;
  inspector->setSpacing(8);
  inspector->addWidget(transpose);
  gestures->addWidget(morph_pad_, 0, Qt::AlignTop);
  gestures->addLayout(inspector);
  side->addLayout(gestures);

  status_label_ = new QLabel(QStringLiteral("44,100 Hz DSP"), central);
  status_label_->setObjectName(QStringLiteral("status"));
  status_label_->setFont(captionFont(status_label_));
  side->addStretch(1);
  side->addWidget(status_label_, 0, Qt::AlignRight | Qt::AlignBottom);

  setCentralWidget(central);

  connect(load, &QPushButton::clicked, this, &MainWindow::openFile);
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
    openPath(url.toLocalFile());
    event->acceptProposedAction();
    return;
  }
}

void MainWindow::openPath(const QString& chosen) {
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

void MainWindow::applyAuditionSource() {
  audition_->setSaw(73.42, 0.18F);
}

void MainWindow::refresh() {
  std::array<trench::core::Biquad, trench::core::native::kSections> sections{};
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
  for (std::size_t index = 0; index < sections.size(); ++index) {
    sections[index] = state_.sectionBiquad(index);
    enabled[index] = state_.sectionEnabled(index);
    if (!enabled[index]) continue;
    seed_root(state_.section(index).pole);
    if (state_.rootPresent(index, EditorState::Lane::kZero)) {
      seed_root(state_.section(index).zero);
    }
  }
  const std::size_t selected = state_.selectedSection();
  const auto& selected_section = state_.section(selected);
  const bool selected_zero =
      state_.selectedLane() == EditorState::Lane::kZero &&
      state_.rootPresent(selected, EditorState::Lane::kZero);
  const double selected_frequency =
      frequencyOf(selected_zero ? selected_section.zero : selected_section.pole);
  cascade_plot_->setCascade(
      trench::audio::design_audition(plotView(), EditorState::kDatumHz),
      sections, enabled, seed_hz,
                            selected, selected_frequency,
                            EditorState::kDatumHz);
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
  if (!audition_) return;
  audition_->setView(heardView());
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
    status_label_->setText(QStringLiteral("AUDITION CLOSED"));
    return;
  }
  const std::string error = audition_->start();
  if (!error.empty()) {
    syncAuditionButton(false);
    status_label_->setText(QStringLiteral("AUDIO DEVICE · %1")
                               .arg(QString::fromStdString(error)));
    return;
  }
  updateAuditionView();
  applyAuditionSource();
  audition_->setGate(true);
  syncAuditionButton(true);
  solo_button_->setEnabled(true);
  status_label_->setText(QStringLiteral("AUDITION OPEN · %1 · %2 Hz")
                             .arg(QString::fromStdString(audition_->deviceName()).toUpper())
                             .arg(audition_->sampleRateHz(), 0, 'f', 0));
}

void MainWindow::closeEvent(QCloseEvent* event) {
  audition_->setGate(false);
  audition_->stop();
  syncAuditionButton(false);
  QMainWindow::closeEvent(event);
}
