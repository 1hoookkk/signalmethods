#include "main_window.hpp"

#include "analyze.hpp"
#include "body_io.hpp"
#include "gesture_dial.hpp"
#include "import_routing.hpp"
#include "section_desk.hpp"
#include "template_shelf.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/measure.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QToolButton>
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
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using Resonant = trench::core::native::Resonant;

const Resonant& resonant(const trench::core::native::Roots& roots) {
  return std::get<Resonant>(roots);
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return {};
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

QString templatesFolder() {
  return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
         QStringLiteral("/TRENCH/templates");
}

QString templateStem(const QString& name) {
  QString stem;
  for (const QChar letter : name) {
    stem.append(letter.isLetterOrNumber() ? letter : QChar('_'));
  }
  return stem.isEmpty() ? QStringLiteral("untitled") : stem;
}

QString templateName(const std::filesystem::path& path) {
  std::ifstream stream(path);
  std::string line;
  while (std::getline(stream, line)) {
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) continue;
    if (line[first] != '#') break;
    const QString named =
        QString::fromStdString(line.substr(first + 1)).trimmed();
    if (!named.isEmpty()) return named;
    break;
  }
  return QString::fromStdWString(path.stem().wstring());
}

QFont captionFont(const QWidget* base) { return base->font(); }

QFont valueFont(const QWidget* base) { return base->font(); }

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

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), audition_(std::make_unique<trench::audio::Audition>()) {
  setWindowTitle(QStringLiteral("TRENCH · 6 × 2P2Z"));
  resize(1060, 940);
  loadUserShelf();

  auto* frame = new QWidget(this);
  auto* frame_layout = new QHBoxLayout(frame);
  frame_layout->setContentsMargins(0, 0, 0, 0);
  auto* central = new QWidget(frame);
  central->setMaximumWidth(1040);
  frame_layout->addStretch(1);
  frame_layout->addWidget(central, 0);
  frame_layout->addStretch(1);
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  auto* top = new QHBoxLayout;
  top->setSpacing(10);
  auto* load = new QPushButton(QStringLiteral("OPEN"), central);
  auto* reset = new QPushButton(QStringLiteral("RESET"), central);
  reset->setObjectName(QStringLiteral("resetDocument"));
  auto* save = new QPushButton(QStringLiteral("SAVE"), central);
  auto* export_body = new QPushButton(QStringLiteral("EXPORT .BODY240"), central);
  export_body->setObjectName(QStringLiteral("exportBody240"));
  analyze_button_ = new QPushButton(QStringLiteral("ANALYZE"), central);
  analyze_button_->setObjectName(QStringLiteral("analyze"));
  sections_button_ = new QPushButton(QStringLiteral("SECTIONS"), central);
  auto* zeros = new QPushButton(QStringLiteral("ZEROS"), central);
  zeros->setObjectName(QStringLiteral("zeroHabits"));
  audition_button_ = new QPushButton(QStringLiteral("AUDITION"), central);
  audition_button_->setObjectName(QStringLiteral("auditionSwitch"));
  audition_button_->setCheckable(true);
  clip_button_ = new QPushButton(QStringLiteral("CLIP"), central);
  clip_button_->setObjectName(QStringLiteral("clipSwitch"));
  clip_button_->setCheckable(true);
  clip_button_->setEnabled(false);
  reference_label_ = new QLabel(QStringLiteral("NO REFERENCE"), central);
  reference_label_->setObjectName(QStringLiteral("referenceName"));
  reference_label_->setMinimumWidth(
      QFontMetrics(valueFont(reference_label_)).horizontalAdvance(QStringLiteral("NO REFERENCE")) + 12);
  reference_label_->setMaximumWidth(280);
  tilt_button_ = new QPushButton(QStringLiteral("TILT"), central);
  tilt_button_->setCheckable(true);
  tilt_button_->setObjectName(QStringLiteral("tiltSwitch"));
  tilt_button_->setEnabled(false);
  const std::vector<trench::app::ShelfGroup> groups = trench::app::buildShelf();
  for (const auto& group : groups) {
    for (const auto& entry : group.entries) shelf_.push_back(entry);
  }
  template_shelf_ = new QPushButton(QStringLiteral("EQ"), central);
  template_shelf_->setObjectName(QStringLiteral("eqTemplate"));
  overlay_shelf_ = new QToolButton(central);
  overlay_shelf_->setObjectName(QStringLiteral("overlayShelf"));
  overlay_shelf_->setText(QStringLiteral("OVERLAY"));
  overlay_shelf_->setPopupMode(QToolButton::InstantPopup);
  auto* overlay_menu = new QMenu(overlay_shelf_);
  overlay_menu->addAction(QStringLiteral("NONE"), this, [this] { chooseOverlay(std::nullopt); });
  overlay_menu->addSeparator();
  std::size_t flat = 0;
  for (const auto& group : groups) {
    QMenu* sub = overlay_menu->addMenu(group.title);
    for (const auto& entry : group.entries) {
      const std::size_t slot = flat++;
      sub->addAction(entry.name, this, [this, slot] { chooseOverlay(slot); });
    }
  }
  overlay_mine_ = overlay_menu->addMenu(QStringLiteral("MINE"));
  for (const auto& kept : user_shelf_) {
    const std::size_t slot = flat++;
    overlay_mine_->addAction(kept.name, this, [this, slot] { chooseOverlay(slot); });
  }
  overlay_shelf_->setMenu(overlay_menu);
  connect(template_shelf_, &QPushButton::clicked, this,
          [this] { state_.loadTemplate(trench::app::kEqTemplate); });
  auto* keep = new QPushButton(QStringLiteral("+"), central);
  keep->setObjectName(QStringLiteral("keepTemplate"));
  keep->setFixedSize(28, 28);
  keep->setFont(captionFont(keep));
  load->setFont(captionFont(load));
  reset->setFont(captionFont(reset));
  save->setFont(captionFont(save));
  export_body->setFont(captionFont(export_body));
  analyze_button_->setFont(captionFont(analyze_button_));
  sections_button_->setFont(captionFont(sections_button_));
  zeros->setFont(captionFont(zeros));
  audition_button_->setFont(captionFont(audition_button_));
  clip_button_->setFont(captionFont(clip_button_));
  tilt_button_->setFont(captionFont(tilt_button_));
  template_shelf_->setFont(captionFont(template_shelf_));
  overlay_shelf_->setFont(captionFont(overlay_shelf_));
  reference_label_->setFont(valueFont(reference_label_));
  for (QWidget* chrome : std::initializer_list<QWidget*>{
           load, reset, save, export_body, analyze_button_, sections_button_, zeros,
           tilt_button_, keep, template_shelf_, overlay_shelf_,
           audition_button_, clip_button_}) {
    chrome->setFocusPolicy(Qt::NoFocus);
  }
  top->addWidget(load);
  top->addWidget(reset);
  top->addWidget(template_shelf_);
  top->addWidget(keep);
  top->addWidget(reference_label_);
  top->addWidget(tilt_button_);
  top->addWidget(overlay_shelf_);
  top->addStretch(1);
  layout->addLayout(top);
  auto* actions = new QHBoxLayout;
  actions->setSpacing(10);
  actions->addWidget(audition_button_);
  actions->addWidget(clip_button_);
  actions->addWidget(sections_button_);
  actions->addWidget(analyze_button_);
  actions->addWidget(zeros);
  actions->addStretch(1);
  actions->addWidget(export_body);
  actions->addWidget(save);
  layout->addLayout(actions);

  cascade_plot_ = new CascadePlot(central);
  cascade_plot_->setObjectName(QStringLiteral("cascadePlot"));
  morph_pad_ = new MorphPad(&state_, central);
  auto* interior = new QHBoxLayout;
  interior->setSpacing(12);
  interior->addWidget(cascade_plot_, 3);
  interior->addWidget(morph_pad_, 0, Qt::AlignTop);
  layout->addLayout(interior, 1);

  armadillo_editor_ = new ArmadilloEditor(&state_, central);
  armadillo_editor_->setObjectName(QStringLiteral("armadilloEditor"));
  armadillo_editor_->setFixedHeight(230);
  layout->addWidget(armadillo_editor_);

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
  gestures->addWidget(transpose);
  const auto addRatioDial = [this, central, gestures](
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
    dial->onBegin = [this] { state_.beginUndoGroup(); };
    dial->onEnd = [this] { state_.endUndoGroup(); };
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
  section_strip_->setObjectName(QStringLiteral("sectionStrip"));
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

  setCentralWidget(frame);

  connect(load, &QPushButton::clicked, this, &MainWindow::openFile);
  connect(reset, &QPushButton::clicked, this, &MainWindow::resetDocument);
  connect(zeros, &QPushButton::clicked, this, &MainWindow::applyZeroHabits);
  auto* undo = new QShortcut(QKeySequence::Undo, this);
  connect(undo, &QShortcut::activated, this, [this] { state_.undo(); });
  auto* redo = new QShortcut(QKeySequence::Redo, this);
  connect(redo, &QShortcut::activated, this, [this] { state_.redo(); });
  auto* redo_shift = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), this);
  connect(redo_shift, &QShortcut::activated, this, [this] { state_.redo(); });
  connect(sections_button_, &QPushButton::clicked, this,
          &MainWindow::toggleSectionDesk);
  connect(clip_button_, &QPushButton::toggled, this, [this] {
    if (audition_->running()) applyAuditionSource();
  });
  connect(audition_button_, &QPushButton::toggled, this,
          [this](bool open) { setAudition(open); });
  connect(save, &QPushButton::clicked, this, &MainWindow::saveDocument);
  connect(export_body, &QPushButton::clicked, this, &MainWindow::exportBody240);
  connect(analyze_button_, &QPushButton::clicked, this,
          &MainWindow::analyzeReference);
  connect(tilt_button_, &QPushButton::toggled, this,
          [this] { applyReferenceView(); });
  connect(keep, &QPushButton::clicked, this, &MainWindow::keepTemplate);
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
      QStringLiteral("Sound (*.wav *.aif *.aiff *.flac);;Poles (*.fbw);;"
                     "Response table (*.csv *.txt);;"
                     "Praat formant table (*.Table);;"
                     "TRENCH document (*.trenchbody);;"
                     "Packed body (*.body240 *.bin);;All files (*)"));
  if (chosen.isEmpty()) return;
  clearProposal();
  const std::filesystem::path path(chosen.toStdWString());
  const QString name = QFileInfo(chosen).fileName().toUpper();
  switch (trench::app::classify_import(path)) {
    case trench::app::ImportKind::kSound:
      loadReference(path);
      return;
    case trench::app::ImportKind::kPoleMaterial: {
      const auto rows = trench::app::read_pole_material(path);
      if (!rows) {
        status_label_->setText(QStringLiteral("POLES REJECTED · %1").arg(name));
        return;
      }
      state_.loadPoles(*rows);
      status_label_->setText(
          QStringLiteral("POLES · %1 SECTIONS")
              .arg(std::min(rows->size(), trench::core::native::kSections)));
      return;
    }
    case trench::app::ImportKind::kFormantTrack: {
      const auto track = trench::app::read_formant_track(path);
      if (!track || track->median.empty()) {
        status_label_->setText(
            QStringLiteral("FORMANT TABLE REJECTED · %1").arg(name));
        return;
      }
      state_.loadPoles(track->median);
      status_label_->setText(
          QStringLiteral("PRAAT · %1 FORMANTS · MEDIAN OF %2 FRAMES")
              .arg(std::min(track->median.size(), trench::core::native::kSections))
              .arg(track->frames));
      return;
    }
    case trench::app::ImportKind::kResponseTable: {
      if (const auto peq = trench::app::read_peq_list(path)) {
        trench::app::applyPeqList(state_, *peq);
        status_label_->setText(
            QStringLiteral("PEQ · %1 POLES · %2 ZEROS · %3 SKIPPED")
                .arg(std::min(peq->poles.size(), trench::core::native::kSections))
                .arg(peq->zeros.size())
                .arg(peq->skipped));
        return;
      }
      const auto curve = trench::app::read_response_curve(path);
      if (!curve) {
        status_label_->setText(
            QStringLiteral("RESPONSE REJECTED · %1").arg(name));
        return;
      }
      Reference reference;
      reference.name = QString::fromStdWString(path.filename().wstring());
      reference.frequency_hz = curve->frequency_hz;
      reference.magnitude_db = curve->magnitude_db;
      setReference(std::move(reference));
      status_label_->setText(QStringLiteral("RESPONSE · %1 POINTS")
                                 .arg(curve->frequency_hz.size()));
      return;
    }
    case trench::app::ImportKind::kDocument: {
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
      return;
    }
    case trench::app::ImportKind::kPackedBody:
      loadReference(path);
      return;
    case trench::app::ImportKind::kUnknown:
      status_label_->setText(
          QStringLiteral("UNSUPPORTED · %1")
              .arg(QString::fromStdString(trench::app::lower_extension(path))
                       .toUpper()));
      return;
  }
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

void MainWindow::applyZeroHabits() {
  const auto habits = state_.applyZeroHabits();
  status_label_->setText(habits.skirts + habits.trims == 0
                             ? QStringLiteral("ZEROS · NOTHING TO ADD")
                             : QStringLiteral("ZEROS · %1 SKIRT · %2 TRIMS")
                                   .arg(habits.skirts)
                                   .arg(habits.trims));
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

void MainWindow::loadUserShelf() {
  const QDir folder(templatesFolder());
  if (!folder.exists()) return;
  for (const QFileInfo& info : folder.entryInfoList(
           {QStringLiteral("*.fbw")}, QDir::Files, QDir::Name)) {
    const std::filesystem::path path(info.absoluteFilePath().toStdWString());
    auto material = trench::app::read_template_material(path);
    if (!material) continue;
    user_shelf_.push_back({templateName(path), std::move(material->poles),
                           std::move(material->zeros)});
  }
}

void MainWindow::keepTemplate() {
  bool accepted = false;
  const QString name =
      QInputDialog::getText(this, QStringLiteral("Keep this posture"),
                            QStringLiteral("NAME"), QLineEdit::Normal,
                            QString(), &accepted)
          .trimmed();
  if (!accepted || name.isEmpty()) return;

  std::vector<std::pair<double, double>> poles;
  std::vector<std::optional<std::pair<double, double>>> zeros;
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    if (!state_.sectionEnabled(index)) continue;
    if (!state_.rootPresent(index, EditorState::Lane::kPole)) continue;
    const auto& pole = resonant(state_.section(index).pole);
    poles.emplace_back(pole.hz, pole.bw_hz);
    if (state_.rootPresent(index, EditorState::Lane::kZero)) {
      const auto& zero = resonant(state_.section(index).zero);
      zeros.emplace_back(std::make_pair(zero.hz, zero.bw_hz));
    } else {
      zeros.emplace_back(std::nullopt);
    }
  }
  if (poles.empty()) {
    status_label_->setText(QStringLiteral("NOTHING TO KEEP"));
    return;
  }

  const QString folder = templatesFolder();
  QDir().mkpath(folder);
  const QString file =
      folder + QStringLiteral("/") + templateStem(name) + QStringLiteral(".fbw");
  std::ofstream stream(std::filesystem::path(file.toStdWString()));
  if (!stream) {
    status_label_->setText(QStringLiteral("KEEP FAILED · %1").arg(name.toUpper()));
    return;
  }
  stream << "# " << name.toStdString() << '\n';
  for (std::size_t index = 0; index < poles.size(); ++index) {
    stream << QString::number(poles[index].first, 'f', 4).toStdString() << ' '
           << QString::number(poles[index].second, 'f', 4).toStdString();
    if (zeros[index].has_value()) {
      stream << ' ' << QString::number(zeros[index]->first, 'f', 4).toStdString() << ' '
             << QString::number(zeros[index]->second, 'f', 4).toStdString();
    }
    stream << '\n';
  }
  stream.close();

  const auto kept = std::find_if(
      user_shelf_.begin(), user_shelf_.end(),
      [&name](const auto& entry) { return entry.name == name; });
  if (kept != user_shelf_.end()) {
    kept->poles = std::move(poles);
    kept->zeros = std::move(zeros);
  } else {
    user_shelf_.push_back({name, std::move(poles), std::move(zeros)});
    const std::size_t slot = shelf_.size() + user_shelf_.size() - 1;
    overlay_mine_->addAction(name, this, [this, slot] { chooseOverlay(slot); });
  }
  status_label_->setText(QStringLiteral("KEPT · %1").arg(name.toUpper()));
}

void MainWindow::analyzeReference() {
  if (!proposal_poles_.empty()) {
    adoptProposal();
    return;
  }
  if (!reference_ || !reference_->clip || reference_->clip->samples.empty()) {
    status_label_->setText(QStringLiteral("ANALYZE NEEDS A SOUND"));
    return;
  }
  const auto proposal = trench::app::analyzeSound(
      std::span<const float>(reference_->clip->samples),
      reference_->clip->sample_rate_hz);
  status_label_->setText(QStringLiteral("PROPOSED · %1 POLES %2 ZEROS")
                             .arg(proposal.poles.size())
                             .arg(proposal.zeros.size()));
  if (proposal.poles.empty()) return;
  proposal_poles_ = proposal.poles;
  proposal_zeros_ = proposal.zeros;
  std::vector<double> marks;
  marks.reserve(proposal_poles_.size());
  for (const auto& pole : proposal_poles_) marks.push_back(pole.first);
  cascade_plot_->setFormantMarks(std::move(marks));
  armadillo_editor_->setGhost(proposal_poles_);
  analyze_button_->setText(QStringLiteral("ADOPT"));
}

void MainWindow::adoptProposal() {
  state_.loadPoles(proposal_poles_);
  std::vector<bool> taken(proposal_poles_.size(), false);
  std::size_t dropped = 0;
  for (const auto& zero : proposal_zeros_) {
    std::size_t nearest = proposal_poles_.size();
    double best = std::numeric_limits<double>::max();
    for (std::size_t index = 0; index < proposal_poles_.size(); ++index) {
      if (taken[index]) continue;
      const double distance =
          std::abs(std::log2(zero.first / proposal_poles_[index].first));
      if (distance < best) {
        best = distance;
        nearest = index;
      }
    }
    if (nearest == proposal_poles_.size()) {
      ++dropped;
      continue;
    }
    taken[nearest] = true;
    state_.selectRoot(nearest, EditorState::Lane::kPole);
    state_.addZeroAt(zero.first, zero.second);
  }
  state_.selectRoot(0, EditorState::Lane::kPole);
  const std::size_t corner = state_.editingCorner();
  clearProposal();
  status_label_->setText(
      dropped == 0
          ? QStringLiteral("ADOPTED · CORNER %1").arg(corner + 1)
          : QStringLiteral("ADOPTED · CORNER %1 · %2 ZEROS UNPLACED")
                .arg(corner + 1)
                .arg(dropped));
}

void MainWindow::chooseOverlay(std::optional<std::size_t> slot) {
  clearProposal();
  if (!slot) {
    cascade_plot_->clearFormantMarks();
    armadillo_editor_->clearGhost();
    return;
  }
  std::vector<std::pair<double, double>> ghost;
  if (*slot < shelf_.size()) {
    for (const auto& pole : shelf_[*slot].poles) {
      if (pole.present) ghost.emplace_back(pole.hz, pole.bw_hz);
    }
  } else {
    if (*slot - shelf_.size() >= user_shelf_.size()) return;
    ghost = user_shelf_[*slot - shelf_.size()].poles;
  }
  std::vector<double> marks;
  marks.reserve(ghost.size());
  for (const auto& pole : ghost) marks.push_back(pole.first);
  cascade_plot_->setFormantMarks(std::move(marks));
  armadillo_editor_->setGhost(std::move(ghost));
}

void MainWindow::clearProposal() {
  if (proposal_poles_.empty()) return;
  proposal_poles_.clear();
  proposal_zeros_.clear();
  cascade_plot_->clearFormantMarks();
  armadillo_editor_->clearGhost();
  analyze_button_->setText(QStringLiteral("ANALYZE"));
}

bool MainWindow::loadReference(const std::filesystem::path& path) {
  try {
    Reference reference;
    reference.name = QString::fromStdWString(path.filename().wstring());
    const trench::app::ImportKind kind = trench::app::classify_import(path);
    if (kind == trench::app::ImportKind::kResponseTable) {
      const auto curve = trench::app::read_response_curve(path);
      if (!curve) throw std::runtime_error("response file is not readable");
      reference.frequency_hz = curve->frequency_hz;
      reference.magnitude_db = curve->magnitude_db;
    } else if (kind == trench::app::ImportKind::kSound) {
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
    } else if (kind == trench::app::ImportKind::kPackedBody) {
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
  clip_button_->setEnabled(reference_->clip.has_value());
  if (!reference_->clip) clip_button_->setChecked(false);
  if (audition_->running()) applyAuditionSource();
}

void MainWindow::applyAuditionSource() {
  if (clip_button_->isChecked() && reference_ && reference_->clip) {
    audition_->setClip(clipForDevice(*reference_->clip));
  } else {
    audition_->setSaw(73.42, 0.18F);
  }
}

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
}

void MainWindow::refresh() {
  std::array<trench::core::Biquad, trench::core::native::kSections> sections{};
  std::array<bool, trench::core::native::kSections> enabled{};
  std::vector<double> seed_hz;
  const auto seed_root = [&seed_hz](const Resonant& root) {
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
    seed_root(resonant(state_.section(index).pole));
    if (state_.rootPresent(index, EditorState::Lane::kZero)) {
      seed_root(resonant(state_.section(index).zero));
    }
  }
  const std::size_t selected = state_.selectedSection();
  const auto& selected_section = state_.section(selected);
  const bool selected_zero =
      state_.selectedLane() == EditorState::Lane::kZero &&
      state_.rootPresent(selected, EditorState::Lane::kZero);
  const double selected_frequency =
      resonant(selected_zero ? selected_section.zero : selected_section.pole).hz;
  cascade_plot_->setCascade(state_.cascade(), sections, enabled, seed_hz,
                            selected, selected_frequency,
                            EditorState::kDatumHz);
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

void MainWindow::toggleSectionDesk() {
  if (!section_desk_) {
    section_desk_ = new SectionDesk(&state_, this);
    section_desk_->move(frameGeometry().topRight() + QPoint(12, 0));
  }
  if (section_desk_->isVisible()) {
    section_desk_->hide();
  } else {
    section_desk_->show();
  }
}

void MainWindow::updateAuditionView() {
  if (!audition_) return;
  audition_->setView(state_.view());
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

void MainWindow::syncAuditionButton(bool checked) {
  const QSignalBlocker blocker(audition_button_);
  audition_button_->setChecked(checked);
}

void MainWindow::setAudition(bool enabled) {
  if (!enabled) {
    audition_->setGate(false);
    audition_->stop();
    syncAuditionButton(false);
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
