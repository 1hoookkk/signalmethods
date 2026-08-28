#include "main_window.hpp"

#include "armadillo_view.hpp"
#include "body_document.hpp"
#include "chassis_bar.hpp"
#include "fit_controller.hpp"
#include "lane_strip.hpp"
#include "morph_strip.hpp"
#include "posture_list.hpp"
#include "response_plot.hpp"
#include "section_readout.hpp"
#include "trench/audio/audio_boundary.hpp"
#include "trench/core/fit_target.hpp"
#include "trench/core/formants.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QSaveFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr std::array<const char*, 11> kFilterTypes{"LPF", "HPF", "BPF", "EQ+", "EQ-", "PHA",
                                                   "FLG", "REZ", "WAH", "DST", "SFX"};
constexpr int kTrenchBodyWriteVersion = 2;

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

bool is_audio(const std::filesystem::path& path) {
  auto ext = path.extension().string();
  for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac";
}

bool is_target_file(const std::filesystem::path& path) {
  if (is_audio(path)) return true;
  auto ext = path.extension().string();
  for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".csv" || ext == ".txt" || ext == ".body240" || ext == ".bin" ||
         ext == ".trenchbody";
}

std::optional<trench::core::native::Body> read_trenchbody(
    const std::filesystem::path& path) {
  namespace native = trench::core::native;
  QFile file(QString::fromStdWString(path.wstring()));
  if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
  const auto document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) return std::nullopt;
  const auto root = document.object();
  if (root.value(QStringLiteral("format")).toString() !=
      QStringLiteral("trench.native-body")) {
    return std::nullopt;
  }
  const auto version_value = root.value(QStringLiteral("version"));
  const int version = version_value.isUndefined() ? 1 : version_value.toInt(-1);
  if (version < 1 || version > kTrenchBodyWriteVersion) return std::nullopt;
  const double file_rate =
      root.value(QStringLiteral("sample_rate_hz")).toDouble(44100.0);
  if (!(file_rate > 0.0) || !std::isfinite(file_rate)) return std::nullopt;
  const native::RealRoots parked{std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity()};
  const auto parse_decay = [](const QJsonValue& value) -> std::optional<double> {
    if (value.isDouble()) {
      const auto decay = value.toDouble();
      return std::isfinite(decay) ? std::optional<double>{decay} : std::nullopt;
    }
    if (value.isString()) {
      const auto token = value.toString();
      if (token == QStringLiteral("infinity")) {
        return std::numeric_limits<double>::infinity();
      }
      if (token == QStringLiteral("-infinity")) {
        return -std::numeric_limits<double>::infinity();
      }
    }
    return std::nullopt;
  };
  const auto parse_roots =
      [&](const QJsonObject& value) -> std::optional<native::Roots> {
    const auto type = value.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("resonant")) {
      const double hz = value.value(QStringLiteral("frequency_hz")).toDouble();
      const double bw = value.value(QStringLiteral("bandwidth_hz")).toDouble();
      if (!(hz > 0.0) || !(bw > 0.0)) return std::nullopt;
      return native::Roots{native::Resonant{hz, bw}};
    }
    if (type == QStringLiteral("real_pair")) {
      if (version >= 2 && value.contains(QStringLiteral("decay_a_hz")) &&
          value.contains(QStringLiteral("decay_b_hz"))) {
        const auto a = parse_decay(value.value(QStringLiteral("decay_a_hz")));
        const auto b = parse_decay(value.value(QStringLiteral("decay_b_hz")));
        if (!a || !b) return std::nullopt;
        return native::Roots{native::RealRoots{*a, *b}};
      }
      const double a = value.value(QStringLiteral("root_a")).toDouble();
      const double b = value.value(QStringLiteral("root_b")).toDouble();
      return native::roots_from_coefficients(-(a + b), a * b, file_rate);
    }
    if (type == QStringLiteral("parked") || type.isEmpty()) {
      return native::Roots{parked};
    }
    return std::nullopt;
  };
  native::Body body;
  for (auto& corner : body.corners) {
    for (auto& section : corner.sections) {
      section.pole = parked;
      section.zero = parked;
      section.dc_stabilised = true;
    }
  }
  for (const auto corner_value : root.value(QStringLiteral("corners")).toArray()) {
    const auto corner_object = corner_value.toObject();
    const auto ci = corner_object.value(QStringLiteral("corner_index")).toInt(-1);
    if (ci < 0 || ci >= static_cast<int>(native::kCorners)) return std::nullopt;
    auto& corner = body.corners[static_cast<std::size_t>(ci)];
    corner.gain_db = corner_object.value(QStringLiteral("gain_db")).toDouble();
    for (const auto section_value :
         corner_object.value(QStringLiteral("sections")).toArray()) {
      const auto section_object = section_value.toObject();
      const auto si = section_object.value(QStringLiteral("section")).toInt(-1);
      if (si < 0 || si >= static_cast<int>(native::kSections)) return std::nullopt;
      auto& section = corner.sections[static_cast<std::size_t>(si)];
      const auto pole =
          parse_roots(section_object.value(QStringLiteral("pole")).toObject());
      const auto zero =
          parse_roots(section_object.value(QStringLiteral("zero")).toObject());
      if (!pole || !zero) return std::nullopt;
      section.pole = *pole;
      section.zero = *zero;
      section.dc_stabilised =
          section_object.value(QStringLiteral("dc_stabilised")).toBool(true);
    }
  }
  return body;
}

std::optional<QJsonValue> encode_decay(double value) {
  if (std::isfinite(value)) return QJsonValue(value);
  if (std::isinf(value)) {
    return QJsonValue(std::signbit(value) ? QStringLiteral("-infinity")
                                         : QStringLiteral("infinity"));
  }
  return std::nullopt;
}

std::optional<QJsonObject> encode_roots(const trench::core::native::Roots& roots) {
  namespace native = trench::core::native;
  QJsonObject object;
  if (const auto* resonant = std::get_if<native::Resonant>(&roots)) {
    if (!(resonant->hz > 0.0) || !(resonant->bw_hz > 0.0) ||
        !std::isfinite(resonant->hz) || !std::isfinite(resonant->bw_hz)) {
      return std::nullopt;
    }
    object.insert(QStringLiteral("type"), QStringLiteral("resonant"));
    object.insert(QStringLiteral("frequency_hz"), resonant->hz);
    object.insert(QStringLiteral("bandwidth_hz"), resonant->bw_hz);
    return object;
  }
  const auto& real = std::get<native::RealRoots>(roots);
  if (std::isinf(real.a_hz) && std::isinf(real.b_hz) && !std::signbit(real.a_hz) &&
      !std::signbit(real.b_hz)) {
    object.insert(QStringLiteral("type"), QStringLiteral("parked"));
    return object;
  }
  const auto a = encode_decay(real.a_hz);
  const auto b = encode_decay(real.b_hz);
  if (!a || !b) return std::nullopt;
  object.insert(QStringLiteral("type"), QStringLiteral("real_pair"));
  object.insert(QStringLiteral("decay_a_hz"), *a);
  object.insert(QStringLiteral("decay_b_hz"), *b);
  return object;
}

bool write_trenchbody(const std::filesystem::path& path,
                      const trench::core::native::Body& body,
                      double sample_rate_hz) {
  namespace native = trench::core::native;
  if (!(sample_rate_hz > 0.0) || !std::isfinite(sample_rate_hz)) return false;

  QJsonArray corners;
  for (std::size_t ci = 0; ci < native::kCorners; ++ci) {
    const auto& corner = body.corners[ci];
    if (!std::isfinite(corner.gain_db)) return false;
    QJsonArray sections;
    for (std::size_t si = 0; si < native::kSections; ++si) {
      const auto pole = encode_roots(corner.sections[si].pole);
      const auto zero = encode_roots(corner.sections[si].zero);
      if (!pole || !zero) return false;
      QJsonObject section;
      section.insert(QStringLiteral("section"), static_cast<int>(si));
      section.insert(QStringLiteral("pole"), *pole);
      section.insert(QStringLiteral("zero"), *zero);
      section.insert(QStringLiteral("dc_stabilised"), corner.sections[si].dc_stabilised);
      sections.append(section);
    }
    QJsonObject encoded_corner;
    encoded_corner.insert(QStringLiteral("corner_index"), static_cast<int>(ci));
    encoded_corner.insert(QStringLiteral("gain_db"), corner.gain_db);
    encoded_corner.insert(QStringLiteral("sections"), sections);
    corners.append(encoded_corner);
  }

  QJsonObject root;
  root.insert(QStringLiteral("format"), QStringLiteral("trench.native-body"));
  root.insert(QStringLiteral("version"), kTrenchBodyWriteVersion);
  root.insert(QStringLiteral("name"), QString::fromStdWString(path.stem().wstring()));
  root.insert(QStringLiteral("sample_rate_hz"), sample_rate_hz);
  root.insert(QStringLiteral("corners"), corners);

  QSaveFile file(QString::fromStdWString(path.wstring()));
  if (!file.open(QIODevice::WriteOnly)) return false;
  const auto json = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (file.write(json) != json.size()) return false;
  return file.commit();
}

std::optional<std::size_t> target_corner(const std::filesystem::path& path) {
  auto stem = path.stem().string();
  std::string key;
  for (const unsigned char c : stem) {
    if (std::isalnum(c)) key.push_back(static_cast<char>(std::tolower(c)));
  }
  if (key == "m0") return 0U;
  if (key == "m100") return 1U;
  if (key == "m0q100") return 2U;
  if (key == "m100q100") return 3U;
  return std::nullopt;
}

bool stereo_is_distinct(const trench::audio::AudioClip& audio) {
  if (audio.channels.size() != 2 ||
      audio.channels[0].size() != audio.channels[1].size()) {
    return false;
  }
  double signal = 0.0;
  double difference = 0.0;
  for (std::size_t i = 0; i < audio.channels[0].size(); ++i) {
    const double left = audio.channels[0][i];
    const double right = audio.channels[1][i];
    signal += left * left + right * right;
    const double delta = left - right;
    difference += delta * delta;
  }
  return signal > 0.0 && difference > signal * 1.0e-10;
}

double bandwidth_for_radius(double radius, double sample_rate_hz) {
  return -std::log(std::clamp(radius, 1.0e-12, 0.999999999)) * sample_rate_hz /
         std::numbers::pi;
}

bool roots_are_parked(const trench::core::native::Roots& roots) {
  const auto* real = std::get_if<trench::core::native::RealRoots>(&roots);
  return real != nullptr && !std::isfinite(real->a_hz) && !std::isfinite(real->b_hz);
}

trench::core::FitTarget grid_target(const trench::core::p2k::Grid& grid,
                                    std::vector<double> magnitude_db,
                                    trench::core::TargetKind kind,
                                    bool absolute_level) {
  trench::core::FitTarget target;
  target.frequency_hz = grid.hz;
  target.magnitude_db = std::move(magnitude_db);
  target.weight = grid.weight;
  target.kind = kind;
  target.absolute_level = absolute_level;
  return target;
}

trench::core::native::Corner vowel_corner(
    const trench::core::p2k::VowelFormants& vowel, double bandwidth_scale) {
  namespace native = trench::core::native;
  const native::RealRoots parked{std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity()};
  native::Corner corner;
  for (auto& section : corner.sections) {
    section.pole = parked;
    section.zero = parked;
    section.dc_stabilised = true;
  }
  for (std::size_t formant = 0; formant < vowel.f.size(); ++formant) {
    corner.sections[formant].pole = native::Resonant{
        vowel.f[formant].hz,
        std::max(1.0e-3, vowel.f[formant].bw_hz * bandwidth_scale)};
  }
  corner.gain_db = 0.0;
  return corner;
}

trench::core::native::Body empty_body() {
  trench::core::PackedBody body;
  for (auto& corner : body.words) corner.fill(trench::core::kIdentitySection);
  return trench::core::native::import_p2k(body.legacy_bytes());
}

struct BodyOverlaySource {
  QString name;
  trench::core::native::Body body;
};

const std::vector<BodyOverlaySource>& factory_body_overlays() {
  static const auto overlays = [] {
    std::vector<BodyOverlaySource> out;
    const auto directory = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                           "evidence/factory-data/p2k/bodies";
    std::error_code error;
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::directory_iterator it(directory, error), end;
         !error && it != end; it.increment(error)) {
      if (it->is_regular_file() && it->path().extension() == ".bin") {
        paths.push_back(it->path());
      }
    }
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
      try {
        auto name = QString::fromStdString(path.stem().string());
        name.replace(QLatin1Char('_'), QLatin1Char(' '));
        out.push_back({name, trench::core::native::import_p2k(read_bytes(path))});
      } catch (const std::exception&) {
        // Omit a broken reference body from this read-only workflow library.
      }
    }
    return out;
  }();
  return overlays;
}

}  // namespace

MainWindow::~MainWindow() {
  disconnect(document_->undoStack(), nullptr, this, nullptr);
}

MainWindow::MainWindow(const std::filesystem::path& body_path,
                       double sample_rate_hz,
                       QWidget* parent)
    : QMainWindow(parent), body_path_(body_path) {
  setAcceptDrops(true);
  auto opened_body = [&] {
    if (body_path.empty()) return empty_body();
    auto extension = body_path.extension().string();
    for (auto& c : extension) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (extension == ".trenchbody") {
      auto body = read_trenchbody(body_path);
      if (!body) throw std::runtime_error("cannot read body: " + body_path.string());
      return *body;
    }
    return trench::core::native::import_p2k(read_bytes(body_path));
  };
  document_ = new BodyDocument(opened_body(), sample_rate_hz, this);
  initial_body_ = document_->body();
  fit_controller_ = new FitController(this);

  auto* central = new QWidget(this);
  auto* column = new QVBoxLayout(central);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);

  response_plot_ = new ResponsePlotWidget;
  response_plot_->setObjectName(QStringLiteral("responsePlot"));
  response_plot_->setBody(&document_->body(), document_->sampleRateHz(),
                          body_path.filename().string());
  response_plot_->setFreedomMask(document_->freedomMask());
  response_plot_->setSpace(document_->space());
  response_plot_->setToolTip(
      QStringLiteral("response, target, residual, sections · Space: audition"));

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
      QStringLiteral("drag: frequency / BW · Shift: BW only · double-click: pole"));
  armadillo_->setBody(&document_->body(), document_->sampleRateHz());

  for (const auto& overlay : factory_body_overlays()) {
    armadillo_->addBodyOverlay(overlay.name, overlay.body);
  }
  surface_layout->addWidget(armadillo_, 3);
  surface_layout->addWidget(response_plot_, 2);
  plot_layout->addWidget(surface_column, 1);
  auto* posture_column = new QWidget(plot_row);
  posture_column->setFixedWidth(176);
  auto* posture_layout = new QVBoxLayout(posture_column);
  posture_layout->setContentsMargins(0, 0, 0, 0);
  posture_layout->setSpacing(0);
  auto* library = new QComboBox(posture_column);
  library->setObjectName(QStringLiteral("bodyLibrary"));
  library->setAccessibleName(QStringLiteral("Body library"));
  library->setStyleSheet(QStringLiteral(
      "QComboBox { background: #16191c; color: #77848a; border: 1px solid #373f43; "
      "padding: 1px 6px; font: 600 8px 'Segoe UI'; }"
      "QComboBox QAbstractItemView { background: #1a1f23; color: #aebabe; "
      "selection-background-color: #263238; selection-color: #57decd; }"));
  library->addItem(QStringLiteral("LIBRARY"));
  {
    const auto root = std::filesystem::path(TRENCH_SOURCE_ROOT) / "recipes/cubes";
    std::error_code error;
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::recursive_directory_iterator it(root, error), end;
         !error && it != end; it.increment(error)) {
      if (it->is_regular_file() && it->path().extension() == ".trenchbody") {
        paths.push_back(it->path());
      }
    }
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
      library->addItem(QString::fromStdString(path.stem().string()),
                       QString::fromStdWString(path.wstring()));
    }
  }
  library->setFixedHeight(20);
  connect(library, &QComboBox::activated, this, [this, library](int index) {
    const auto data = library->itemData(index).toString();
    if (data.isEmpty()) {
      armadillo_->setOverlay(QString());
      return;
    }
    const auto path = std::filesystem::path(data.toStdWString());
    const auto body = read_trenchbody(path);
    if (!body) return;
    const auto name = QString::fromStdString(path.stem().string());
    armadillo_->addBodyOverlay(name, *body);
    armadillo_->setOverlay(
        QStringLiteral("%1 · C%2").arg(name).arg(document_->corner() + 1));
  });
  posture_layout->addWidget(library, 0);
  auto* fit_actions = new QWidget(posture_column);
  fit_actions->setFixedHeight(68);
  fit_actions->setStyleSheet(QStringLiteral(
      "QWidget { background: #111416; border-left: 1px solid #373f43; "
      "border-bottom: 1px solid #373f43; }"
      "QPushButton { background: #1a1f23; color: #aebabe; border: 1px solid #373f43; "
      "padding: 3px; font: 600 8px 'Segoe UI'; }"
      "QPushButton:hover { color: #57decd; border-color: #59656a; }"
      "QPushButton:disabled { color: #4a5257; }"));
  auto* fit_actions_layout = new QGridLayout(fit_actions);
  fit_actions_layout->setContentsMargins(5, 4, 5, 4);
  fit_actions_layout->setHorizontalSpacing(3);
  fit_actions_layout->setVerticalSpacing(3);
  fit_method_ = new QLabel(QStringLiteral("FIT · LOAD TARGET"), fit_actions);
  fit_method_->setObjectName(QStringLiteral("fitMethod"));
  fit_method_->setAccessibleName(QStringLiteral("Selected fitting target type"));
  fit_method_->setStyleSheet(QStringLiteral(
      "QLabel { color: #57decd; border: none; font: 600 8px 'Segoe UI'; }"));
  lpc_poles_ = new QPushButton(QStringLiteral("1  PEAKS → POLES"), fit_actions);
  lpc_poles_->setObjectName(QStringLiteral("lpcPoles"));
  lpc_poles_->setAccessibleName(QStringLiteral("Commit measured pole suggestions"));
  lpc_poles_->setToolTip(QStringLiteral("commit the selected target's measured poles"));
  zero_fit_ = new QPushButton(QStringLiteral("2  FIT ZEROS"), fit_actions);
  zero_fit_->setObjectName(QStringLiteral("fitZeros"));
  zero_fit_->setAccessibleName(QStringLiteral("Fit zeros only"));
  zero_fit_->setToolTip(QStringLiteral("hold poles; fit zeros; adjust level if measured"));
  free_fit_ = new QPushButton(QStringLiteral("3  REFINE FREE"), fit_actions);
  free_fit_->setObjectName(QStringLiteral("fitFree"));
  free_fit_->setAccessibleName(QStringLiteral("Fit released roots"));
  free_fit_->setToolTip(QStringLiteral("refine only roots released on the response"));
  fit_actions_layout->addWidget(fit_method_, 0, 0, 1, 2);
  fit_actions_layout->addWidget(lpc_poles_, 1, 0, 1, 2);
  fit_actions_layout->addWidget(zero_fit_, 2, 0);
  fit_actions_layout->addWidget(free_fit_, 2, 1);
  posture_layout->addWidget(fit_actions, 0);
  keep_posture_ = new QPushButton(QStringLiteral("SAVE POLES"), posture_column);
  keep_posture_->setObjectName(QStringLiteral("keepPosture"));
  keep_posture_->setFocusPolicy(Qt::NoFocus);
  keep_posture_->setCursor(Qt::PointingHandCursor);
  keep_posture_->setToolTip(QStringLiteral("save the current pole roots under USER"));
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
  connect(lpc_poles_, &QAbstractButton::clicked, this, &MainWindow::applyLpcPoles);
  connect(zero_fit_, &QAbstractButton::clicked, this, &MainWindow::startZeroFit);
  connect(free_fit_, &QAbstractButton::clicked, this, &MainWindow::startFit);
  column->addWidget(plot_row, 1);

  lane_strip_ = new LaneStrip(central);
  lane_strip_->setBody(&document_->body(), document_->sampleRateHz());
  response_plot_->setCascade(document_->viewCascade());
  column->addWidget(lane_strip_, 0);

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
  resize(1280, 720);

  connect(section_readout_, &SectionReadout::poleEdited, this, &MainWindow::applyPole);
  connect(section_readout_, &SectionReadout::zeroEdited, this, &MainWindow::applyZero);
  connect(lane_strip_, &LaneStrip::laneSelected, this,
          [this](std::size_t section, std::size_t corner) {
            document_->setCorner(corner);
            selectSection(section);
          });
  connect(lane_strip_, &LaneStrip::stageToggled, this, &MainWindow::setStageEnabled);
  connect(lane_strip_, &LaneStrip::lockToggled, this,
          [this](std::size_t section) { document_->toggleLane(section, true); });
  connect(lane_strip_, &LaneStrip::endpointPicked, this,
          [this](std::size_t corner) { document_->setCorner(corner); });
  connect(response_plot_, &ResponsePlotWidget::tokenSelected, this,
          [this](std::size_t section, ResponsePlotWidget::Lane lane) {
            selectSection(section, lane);
          });
  connect(response_plot_, &ResponsePlotWidget::pinToggled, this,
          [this](std::size_t section, ResponsePlotWidget::Lane lane) {
            document_->toggleLane(section, lane == ResponsePlotWidget::Lane::kPole);
          });
  connect(armadillo_, &ArmadilloView::rootPressed, this,
          [this](std::size_t section, bool zero) {
            document_->beginRootGesture();
            selectSection(section, zero ? ResponsePlotWidget::Lane::kZero
                                        : ResponsePlotWidget::Lane::kPole);
          });
  connect(armadillo_, &ArmadilloView::rootDragged, this,
          [this](std::size_t section, bool zero, double hz, double radius) {
            if (fit_active_) return;
            document_->editRoot(
                {section,
                 zero ? BodyDocument::RootLane::kZero : BodyDocument::RootLane::kPole, hz,
                 bandwidth_for_radius(radius, document_->sampleRateHz()), false});
          });
  connect(armadillo_, &ArmadilloView::rootReleased, this, [this] {
    document_->endRootGesture();
    updateInterior();
    updatePostureMatch();
  });
  connect(armadillo_, &ArmadilloView::zeroParked, this, [this](std::size_t section) {
    if (fit_active_) return;
    document_->editRoot({section, BodyDocument::RootLane::kZero, 0.0, 0.0, true});
  });
  connect(armadillo_, &ArmadilloView::poleParked, this, [this](std::size_t section) {
    if (fit_active_) return;
    document_->editRoot({section, BodyDocument::RootLane::kPole, 0.0, 0.0, true});
  });
  connect(armadillo_, &ArmadilloView::placeRequested, this,
          &MainWindow::placeResonanceAt);
  connect(document_, &BodyDocument::bodyChanged, this, [this] {
    response_plot_->setCascade(document_->viewCascade());
    armadillo_->refresh();
    lane_strip_->refresh();
    updateReadout();
    updateProbes();
    updateVerbs();
    if (!document_->rootGestureActive()) {
      updateInterior();
      updatePostureMatch();
    }
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
    response_plot_->setCascade(document_->viewCascade());
    lane_strip_->setCorner(corner);
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q, view.semitones);
    armadillo_->setTranspose(view.semitones);
    morph_strip_->setTranspose(static_cast<int>(std::lround(view.semitones)));
    updateProbes();
    updatePostureMatch();
    updateReadout();
  });
  connect(document_, &BodyDocument::viewChanged, this, [this] {
    const auto view = document_->view();
    morph_strip_->setView(view.morph, view.q);
    response_plot_->setView(view.morph, view.q, view.semitones);
    response_plot_->setCascade(document_->viewCascade());
    armadillo_->setTranspose(view.semitones);
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
    lane_strip_->setFreedomMask(mask);
            fit_controller_->setMask(mask);
            updateReadout();
          });
  connect(document_, &BodyDocument::targetChanged, this, [this] {
    response_plot_->setTarget(document_->target() ? &document_->targetGridDb() : nullptr);
    updateVerbs();
    updateProbes();
  });

  connect(fit_controller_, &FitController::previewReady, this,
          [this](quint64 generation, const trench::core::native::Corner& corner,
                 qsizetype section) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            document_->applyFitStep(fit_corner_, corner);
            if (section >= 0 &&
                section < static_cast<qsizetype>(trench::core::native::kSections)) {
              response_plot_->setHighlightedSection(static_cast<std::size_t>(section));
              response_plot_->flashLane(static_cast<std::size_t>(section));
            }
          });
  connect(fit_controller_, &FitController::finished, this,
          [this](quint64 generation, bool ok,
                 const trench::core::native::Corner& corner) {
            if (!fit_active_ || generation != fit_controller_->generation()) return;
            if (ok) {
              document_->applyFitStep(fit_corner_, corner);
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
      case ChassisBar::Verb::kTarget:
        chooseTarget();
        break;
      case ChassisBar::Verb::kUndo:
        document_->undoStack()->undo();
        break;
      case ChassisBar::Verb::kRedo:
        document_->undoStack()->redo();
        break;
      case ChassisBar::Verb::kReset:
        resetBody();
        break;
      case ChassisBar::Verb::kFit:
        startZeroFit();
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
  connect(save_action, &QAction::triggered, this, [this] {
    if (body_path_.empty()) {
      saveBodyAs();
    } else {
      saveBody(body_path_);
    }
  });
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
  connect(document_->undoStack(), &QUndoStack::canUndoChanged, this,
          [this] { updateVerbs(); });
  connect(document_->undoStack(), &QUndoStack::canRedoChanged, this,
          [this] { updateVerbs(); });

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

LaneStrip* MainWindow::laneStrip() const noexcept { return lane_strip_; }

SectionReadout* MainWindow::sectionReadout() const noexcept {
  return section_readout_;
}

const std::filesystem::path& MainWindow::bodyPath() const noexcept { return body_path_; }

void MainWindow::setCorner(std::size_t corner) { document_->setCorner(corner); }

bool MainWindow::saveBody(const std::filesystem::path& path) {
  if (fit_active_) return false;
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (extension == ".trenchbody") {
    if (!write_trenchbody(path, document_->body(), document_->sampleRateHz())) return false;
  } else if (extension == ".body240") {
    std::array<std::uint8_t, trench::core::kLegacyBodyBytes> bytes{};
    try {
      bytes = document_->exportP2k();
    } catch (const std::exception&) {
      return false;
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) return false;
  } else {
    return false;
  }
  body_path_ = path;
  chassis_bar_->setBodyName(QString::fromStdString(path.filename().string()));
  setWindowTitle(QStringLiteral("TRENCH — %1").arg(QString::fromStdString(path.stem().string())));
  return true;
}

bool MainWindow::saveCorner(const std::filesystem::path& path) {
  if (fit_active_) return false;
  const auto snapshot = document_->p2kCornerSnapshot();
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
  const auto current = document_->p2kCornerSnapshot();
  auto after = current;
  std::size_t at = 0;
  for (auto& section : after) {
    for (auto& word : section) {
      word = static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1] << 8U));
      at += 2;
    }
  }
  if (after == current) return true;
  document_->applyP2kCorner(after);
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
  if (fit_active_) return;
  double bandwidth_hz = bandwidth_for_radius(radius, document_->sampleRateHz());
  if (selected_overlay_ >= 0 &&
      selected_overlay_ < static_cast<int>(overlays_.size())) {
    for (const auto& peak : overlays_[selected_overlay_].suggested_poles) {
      if (std::abs(std::log2(frequency_hz / peak.hz)) < 0.08) {
        frequency_hz = peak.hz;
        bandwidth_hz = peak.bw_hz;
        break;
      }
    }
  }
  const auto corner = document_->cornerSnapshot();
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!roots_are_parked(corner.sections[section].pole)) continue;
    if (document_->editRoot(
            {section, BodyDocument::RootLane::kPole, frequency_hz,
             bandwidth_hz, false})) {
      selectSection(section);
    }
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
  auto suggestion = body_path_;
  if (suggestion.empty()) {
    suggestion = std::filesystem::path("Untitled.trenchbody");
  } else {
    suggestion.replace_extension(".trenchbody");
  }
  QString selected_filter = QStringLiteral("TRENCH Body (*.trenchbody)");
  const auto chosen = QFileDialog::getSaveFileName(
      this, QStringLiteral("SAVE"), QString::fromStdWString(suggestion.wstring()),
      QStringLiteral("TRENCH Body (*.trenchbody);;Packed P2K Body (*.body240)"),
      &selected_filter);
  if (chosen.isEmpty()) return;
  auto path = std::filesystem::path(chosen.toStdWString());
  if (!path.has_extension()) {
    path.replace_extension(selected_filter.startsWith(QStringLiteral("Packed"))
                               ? ".body240"
                               : ".trenchbody");
  }
  saveBody(path);
}

BodyDocument* MainWindow::document() const noexcept { return document_; }

FitController* MainWindow::fitController() const noexcept { return fit_controller_; }

trench::core::PackedBody MainWindow::body() const {
  return document_->exportP2kBody();
}

QUndoStack* MainWindow::undoStack() noexcept { return document_->undoStack(); }

std::uint32_t MainWindow::freedomMask() const noexcept {
  return document_->freedomMask();
}

bool MainWindow::fitRunning() const noexcept { return fit_active_; }

void MainWindow::applySection(std::size_t section,
                              const trench::core::PackedSection& words) {
  document_->applyP2kSection(section, words);
}

void MainWindow::applyPole(std::size_t section, double frequency_hz, double bw_hz) {
  if (section >= trench::core::native::kSections || fit_active_) return;
  document_->editRoot(
      {section, BodyDocument::RootLane::kPole, frequency_hz, bw_hz, false});
}

void MainWindow::setStageEnabled(std::size_t section, bool on) {
  if (section >= trench::core::native::kSections || fit_active_) return;
  std::optional<trench::core::native::Resonant> seed;
  if (on) {
    for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
      const auto& roots = document_->cornerSnapshot(corner).sections[section].pole;
      if (const auto* resonant =
              std::get_if<trench::core::native::Resonant>(&roots)) {
        seed = *resonant;
        break;
      }
    }
    if (!seed) {
      const auto radius = 0.96;
      seed = trench::core::native::Resonant{
          1000.0, bandwidth_for_radius(radius, document_->sampleRateHz())};
    }
  }
  document_->undoStack()->beginMacro(on ? QStringLiteral("stage on")
                                        : QStringLiteral("stage off"));
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    if (on) {
      document_->editRoot({section, BodyDocument::RootLane::kPole, seed->hz, seed->bw_hz,
                           false, corner});
    } else {
      document_->editRoot({section, BodyDocument::RootLane::kPole, 0.0, 0.0, true, corner});
    }
  }
  document_->undoStack()->endMacro();
}

void MainWindow::applyZero(std::size_t section, double frequency_hz, double bw_hz) {
  if (section >= trench::core::native::kSections || fit_active_) return;
  document_->editRoot(
      {section, BodyDocument::RootLane::kZero, frequency_hz, bw_hz, false});
}

void MainWindow::applyLanePole(std::size_t section, std::size_t corner,
                               double frequency_hz, double bw_hz) {
  if (section >= trench::core::native::kSections || fit_active_) return;
  if (corner >= trench::core::native::kCorners) return;
  document_->editRoot(
      {section, BodyDocument::RootLane::kPole, frequency_hz, bw_hz, false, corner});
}

void MainWindow::clearSection(std::size_t section) {
  if (section >= trench::core::native::kSections || fit_active_) return;
  document_->editRoot(
      {section, BodyDocument::RootLane::kPole, 0.0, 0.0, true});
}

void MainWindow::selectSection(std::size_t section, ResponsePlotWidget::Lane lane) {
  if (section >= trench::core::kLegacySectionCount) return;
  selected_section_ = section;
  selected_lane_zero_ = lane == ResponsePlotWidget::Lane::kZero;
  response_plot_->setSelectedSection(section, lane);
  armadillo_->setSelected(section, lane == ResponsePlotWidget::Lane::kZero);
  lane_strip_->setSelected(section);
  updateReadout();
}

bool MainWindow::loadTarget(const std::filesystem::path& path) {
  if (is_audio(path)) {
    const auto decoded = trench::audio::decode_audio(path);
    if (!decoded || decoded->channels.empty()) return false;
    trench::audio::MonoClip clip;
    clip.sample_rate_hz = decoded->sample_rate_hz;
    clip.samples.assign(decoded->channels.front().size(), 0.0F);
    const float channel_scale = 1.0F / static_cast<float>(decoded->channels.size());
    for (const auto& channel : decoded->channels) {
      for (std::size_t index = 0; index < channel.size(); ++index) {
        clip.samples[index] += channel[index] * channel_scale;
      }
    }
    std::vector<double> target;
    std::vector<double> marks;
    std::vector<trench::core::native::Resonant> suggested_poles;
    try {
      const auto envelope = trench::core::measure::spectral_envelope(
          clip.samples, clip.sample_rate_hz);
      target = trench::core::measure::target_on_grid(envelope, document_->grid().hz);
    } catch (const std::exception&) {
      return false;
    }
    for (const auto& peak :
         trench::core::measure::envelope_peaks(document_->grid().hz, target)) {
      marks.push_back(peak.hz);
      suggested_poles.push_back({peak.hz, peak.bw_hz});
    }
    const auto corner = target_corner(path);
    auto name = QString::fromStdString(path.filename().string());
    if (corner) name += QStringLiteral(" · C%1").arg(*corner + 1);
    if (stereo_is_distinct(*decoded)) {
      try {
        addOverlay(name + QStringLiteral(" transfer"),
                   trench::core::measure::transfer_function(
                       decoded->channels[0], decoded->channels[1],
                       decoded->sample_rate_hz),
                   {}, {}, FitRoom::SourceKind::kTransfer, corner);
      } catch (const std::exception&) {
        // The envelope targets remain usable when a stereo pair lacks coherent input.
      }
    }
    addOverlay(name,
               grid_target(document_->grid(), std::move(target),
                           trench::core::TargetKind::kEnvelope, false),
               std::move(marks), std::move(suggested_poles),
               FitRoom::SourceKind::kAudioEnvelope, corner);
    audition_clip_ = clip;
    if (audition_) audition_->setClip(clip);
    return true;
  }
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (extension == ".txt" || extension == ".csv") {
    auto target = trench::core::read_fit_target(path);
    if (!target) return false;
    const auto corner = target_corner(path);
    auto name = QString::fromStdString(path.filename().string());
    if (corner) name += QStringLiteral(" · C%1").arg(*corner + 1);
    addOverlay(name, std::move(*target), {}, {}, FitRoom::SourceKind::kCurve,
               corner);
    return true;
  }
  std::optional<trench::core::native::Body> imported_body;
  if (extension == ".trenchbody") {
    imported_body = read_trenchbody(path);
    if (!imported_body) return false;
  } else {
    std::vector<std::uint8_t> bytes;
    try {
      bytes = read_bytes(path);
    } catch (const std::exception&) {
      return false;
    }
    if (bytes.size() != trench::core::kLegacyBodyBytes) return false;
    imported_body = trench::core::native::import_p2k(bytes);
  }
  const auto& imported = *imported_body;
  const auto& corner = imported.corners[document_->corner()];
  const auto cascade = trench::core::native::cascade(
      trench::core::native::design(corner, document_->sampleRateHz()), corner.gain_db);
  std::vector<double> magnitude_db;
  magnitude_db.reserve(document_->grid().hz.size());
  for (const double hz : document_->grid().hz) {
    magnitude_db.push_back(trench::core::cascade_response_db(
        cascade, hz, document_->sampleRateHz()));
  }
  auto overlay_name = QString::fromStdString(path.stem().string());
  armadillo_->addBodyOverlay(overlay_name, imported);
  addOverlay(QStringLiteral("%1 · C%2")
                 .arg(QString::fromStdString(path.filename().string()))
                 .arg(document_->corner() + 1),
             grid_target(document_->grid(), std::move(magnitude_db),
                         trench::core::TargetKind::kTransferFunction, true),
             {}, {}, FitRoom::SourceKind::kBody, document_->corner());
  return true;
}

std::size_t MainWindow::loadTargets(std::vector<std::filesystem::path> paths) {
  std::stable_sort(paths.begin(), paths.end(), [](const auto& lhs, const auto& rhs) {
    const auto left = target_corner(lhs);
    const auto right = target_corner(rhs);
    if (left && right) return *left < *right;
    if (left) return true;
    if (right) return false;
    return lhs.filename().string() < rhs.filename().string();
  });
  std::vector<int> preferred;
  for (const auto& path : paths) {
    if (loadTarget(path)) preferred.push_back(selected_overlay_);
  }
  if (!preferred.empty()) selectOverlay(preferred.front());
  return preferred.size();
}

void MainWindow::addOverlay(
    const QString& name, trench::core::FitTarget target,
    std::vector<double> marks_hz,
    std::vector<trench::core::native::Resonant> suggested_poles,
    FitRoom::SourceKind source, std::optional<std::size_t> corner) {
  overlays_.push_back(FitRoom::Overlay{name, std::move(target), std::move(marks_hz),
                                       std::move(suggested_poles), source, corner});
  selectOverlay(static_cast<int>(overlays_.size()) - 1);
}

void MainWindow::selectOverlay(int index) {
  if (index < 0 || index >= overlays_.size()) return;
  selected_overlay_ = index;
  if (overlays_[index].corner) document_->setCorner(*overlays_[index].corner);
  chassis_bar_->setTargetName(overlays_[index].name);
  auto source = overlays_[index].name;
  if (source.endsWith(QStringLiteral(" LPC"))) source.chop(4);
  armadillo_->setLpcFormants(source, overlays_[index].marks_hz);
  document_->setTarget(overlays_[index].target);
  updateVerbs();
}

void MainWindow::removeOverlay(int index) {
  if (index < 0 || index >= overlays_.size()) return;
  overlays_.removeAt(index);
  if (overlays_.isEmpty()) {
    selected_overlay_ = -1;
    chassis_bar_->setTargetName(QString());
    armadillo_->setLpcFormants(QString(), {});
    document_->clearTarget();
    updateVerbs();
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
  fit_room_->setOverlays(overlays_, selected_overlay_);
}

void MainWindow::rebuildPostureGroups() {
  QList<PostureList::Group> groups;
  PostureList::Group mine{QStringLiteral("USER"), {}};
  for (const auto& entry : user_postures_.entries()) mine.names.push_back(entry.name);
  groups.push_back(mine);
  for (const auto* type : kFilterTypes) {
    PostureList::Group group{QString::fromLatin1(type), {}};
    for (const auto& skeleton : trench::core::p2k::postures()) {
      if (skeleton.type != type) continue;
      group.names.push_back(
          QString::fromUtf8(skeleton.name.data(), static_cast<int>(skeleton.name.size())));
    }
    groups.push_back(group);
  }
  posture_list_->setGroups(groups);
}

std::vector<UserPostures::Pole> MainWindow::currentPolePosture() const {
  std::vector<UserPostures::Pole> poles;
  const auto current = document_->p2kCornerSnapshot();
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

std::optional<BodyDocument::P2kCorner> MainWindow::cornerWithPosture(
    const QString& symbol) const {
  namespace p2k = trench::core::p2k;
  if (const auto* mine = user_postures_.find(symbol)) {
    BodyDocument::P2kCorner after;
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
  BodyDocument::P2kCorner after;
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
  return posturePolesHeld(symbol, document_->corner());
}

QString MainWindow::matchedPosture(std::size_t corner) const {
  if (posture_list_ == nullptr) return {};
  for (int row = 0; row < posture_list_->count(); ++row) {
    const auto name = posture_list_->item(row)->data(Qt::UserRole).toString();
    if (name.isEmpty()) continue;
    if (posturePolesHeld(name, corner)) return name;
  }
  return {};
}

void MainWindow::updateEndpointNames() {
  if (lane_strip_ == nullptr) return;
  lane_strip_->setEndpointNames(matchedPosture(lane_strip_->fromCorner()),
                                matchedPosture(lane_strip_->toCorner()));
}

bool MainWindow::posturePolesHeld(const QString& symbol, std::size_t corner) const {
  namespace p2k = trench::core::p2k;
  if (corner >= trench::core::native::kCorners) return false;
  const auto current = document_->p2kCornerSnapshot(corner);
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
  updateEndpointNames();
}

void MainWindow::applyVowel(const QString& symbol) {
  if (fit_active_) return;
  const auto after = cornerWithPosture(symbol);
  if (!after) return;
  const auto before = document_->cornerSnapshot();
  if (*after == document_->p2kCornerSnapshot()) {
    updatePostureMatch();
    return;
  }
  document_->applyP2kCorner(*after);
  document_->commitFit(document_->corner(), before);
  updatePostureMatch();
}

void MainWindow::applyVowelJourney(const QString& from, const QString& to,
                                   double high_q_bandwidth_scale) {
  namespace native = trench::core::native;
  namespace p2k = trench::core::p2k;
  if (fit_active_) return;
  const auto* from_vowel = p2k::klatt_vowel(from.toStdString());
  const auto* to_vowel = p2k::klatt_vowel(to.toStdString());
  if (from_vowel == nullptr || to_vowel == nullptr) return;

  const auto high_q = std::clamp(high_q_bandwidth_scale, 0.15, 1.0);
  const std::array<native::Corner, native::kCorners> corners{
      vowel_corner(*from_vowel, 1.0), vowel_corner(*to_vowel, 1.0),
      vowel_corner(*from_vowel, high_q), vowel_corner(*to_vowel, high_q)};

  auto* stack = document_->undoStack();
  stack->beginMacro(QString());
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const auto before = document_->cornerSnapshot(corner);
    if (before == corners[corner]) continue;
    document_->applyCorner(corner, corners[corner]);
    document_->commitFit(corner, before);
  }
  stack->endMacro();

  std::uint32_t fit_mask = native::kGainBit;
  for (std::size_t section = 0; section < native::kSections; ++section) {
    fit_mask |= native::zero_bit(section);
  }
  document_->setFreedomMask(fit_mask);
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
  const auto chosen = QFileDialog::getOpenFileNames(
      this, QStringLiteral("TARGET"), QString(),
      QStringLiteral("Target (*.csv *.txt *.body240 *.bin *.wav *.aif *.aiff *.flac)"));
  std::vector<std::filesystem::path> paths;
  paths.reserve(static_cast<std::size_t>(chosen.size()));
  for (const auto& file : chosen) paths.emplace_back(file.toStdWString());
  loadTargets(std::move(paths));
}

void MainWindow::startFit() {
  if (fit_active_ || !document_->target()) return;
  pre_fit_ = document_->cornerSnapshot();
  fit_corner_ = document_->corner();
  fit_active_ = true;
  response_plot_->setFitRunning(true);
  updateVerbs();
  fit_controller_->start(*document_->target(), document_->cornerSnapshot(),
                         document_->freedomMask(), document_->sampleRateHz());
}

void MainWindow::startZeroFit() {
  if (fit_active_ || !document_->target()) return;
  std::uint32_t mask = trench::core::native::kGainBit;
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    const auto bit = trench::core::native::zero_bit(section);
    if ((document_->freedomMask() & bit) != 0U) mask |= bit;
  }
  document_->setFreedomMask(mask);
  startFit();
}

void MainWindow::applyLpcPoles() {
  namespace native = trench::core::native;
  if (fit_active_ || selected_overlay_ < 0 ||
      selected_overlay_ >= static_cast<int>(overlays_.size())) return;
  const auto& poles = overlays_[selected_overlay_].suggested_poles;
  if (poles.empty()) return;

  const native::RealRoots parked{std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity()};
  const auto before = document_->cornerSnapshot();
  auto after = before;
  for (auto& section : after.sections) {
    section.pole = parked;
    section.zero = parked;
    section.dc_stabilised = true;
  }
  for (std::size_t section = 0;
       section < std::min(poles.size(), after.sections.size()); ++section) {
    after.sections[section].pole = poles[section];
  }
  after.gain_db = 0.0;
  if (after != before) {
    document_->applyCorner(after);
    document_->commitFit(document_->corner(), before);
  }

  std::uint32_t mask = native::kGainBit;
  for (std::size_t section = 0; section < native::kSections; ++section) {
    mask |= native::zero_bit(section);
  }
  document_->setFreedomMask(mask);
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

void MainWindow::resetBody() {
  if (fit_active_) return;
  document_->resetBody(initial_body_);
  document_->setFreedomMask(trench::core::native::kAllFree);
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
  const auto view = document_->view();
  audition_->setView({document_->body(), document_->exportP2kBody(), view.morph,
                      view.q, view.semitones});
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Space && !key->isAutoRepeat()) {
      setAuditionGate(event->type() == QEvent::KeyPress);
      return true;
    }
    if ((key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) &&
        event->type() == QEvent::KeyPress && !fit_active_) {
      auto* focus = QApplication::focusWidget();
      const bool editing = qobject_cast<QLineEdit*>(focus) != nullptr ||
                           qobject_cast<QAbstractSpinBox*>(focus) != nullptr ||
                           qobject_cast<QComboBox*>(focus) != nullptr ||
                           qobject_cast<QListWidget*>(focus) != nullptr;
      if (!editing) {
        document_->editRoot({selected_section_,
                             selected_lane_zero_ ? BodyDocument::RootLane::kZero
                                                 : BodyDocument::RootLane::kPole,
                             0.0, 0.0, true});
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (!fit_active_ && event->mimeData()->hasUrls()) {
    const auto urls = event->mimeData()->urls();
    const bool all_targets = !urls.empty() && std::all_of(
        urls.begin(), urls.end(), [](const QUrl& url) {
          return url.isLocalFile() &&
                 is_target_file(std::filesystem::path(
                     url.toLocalFile().toStdWString()));
        });
    if (all_targets) {
      event->acceptProposedAction();
      return;
    }
  }
  QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent* event) {
  if (!fit_active_ && event->mimeData()->hasUrls()) {
    std::vector<std::filesystem::path> paths;
    for (const auto& url : event->mimeData()->urls()) {
      if (!url.isLocalFile()) {
        paths.clear();
        break;
      }
      const auto path = std::filesystem::path(url.toLocalFile().toStdWString());
      if (!is_target_file(path)) {
        paths.clear();
        break;
      }
      paths.push_back(path);
    }
    if (!paths.empty() && loadTargets(std::move(paths)) > 0) {
      event->acceptProposedAction();
      return;
    }
  }
  QMainWindow::dropEvent(event);
}

void MainWindow::updateProbes() {
  chassis_bar_->setPowerDb(document_->viewPowerDb());
  chassis_bar_->setScoreDb(document_->targetScoreDb());
  refreshFitRoom();
  updateAudition();
}

void MainWindow::updateReadout() {
  const auto& section =
      document_->body().corners[document_->corner()].sections[selected_section_];
  const auto* pole = std::get_if<trench::core::native::Resonant>(&section.pole);
  const auto* zero = std::get_if<trench::core::native::Resonant>(&section.zero);
  section_readout_->setReading(
      selected_section_, pole == nullptr ? std::nullopt
                                         : std::optional{*pole},
      zero == nullptr ? std::nullopt : std::optional{*zero}, selected_lane_zero_);
}

void MainWindow::updateInterior() {
  const auto audit = trench::core::p2k::interior_audit(document_->body(), document_->grid());
  morph_strip_->setWorstStepDb(audit.max_step_db);
}

void MainWindow::endRun() {
  fit_active_ = false;
  response_plot_->setFitRunning(false);
  response_plot_->setHighlightedSection(std::nullopt);
  updateVerbs();
}

void MainWindow::updateVerbs() {
  const auto has_target = document_->target().has_value();
  chassis_bar_->setState(has_target, fit_active_);
  chassis_bar_->setHistoryState(document_->undoStack()->canUndo(),
                                document_->undoStack()->canRedo(),
                                document_->body() != initial_body_);
  const bool has_lpc = selected_overlay_ >= 0 &&
                       selected_overlay_ < static_cast<int>(overlays_.size()) &&
                       !overlays_[selected_overlay_].suggested_poles.empty();
  if (fit_method_ != nullptr) {
    QString method = QStringLiteral("FIT · LOAD TARGET");
    if (fit_active_) {
      method = QStringLiteral("FIT · RUNNING");
    } else if (has_target && selected_overlay_ >= 0 &&
               selected_overlay_ < static_cast<int>(overlays_.size())) {
      switch (overlays_[selected_overlay_].source) {
        case FitRoom::SourceKind::kAudioEnvelope:
          method = QStringLiteral("FIT · AUDIO ENVELOPE");
          break;
        case FitRoom::SourceKind::kTransfer:
          method = QStringLiteral("FIT · TRANSFER");
          break;
        case FitRoom::SourceKind::kBody:
          method = QStringLiteral("FIT · BODY RESPONSE");
          break;
        case FitRoom::SourceKind::kCurve:
          method = QStringLiteral("FIT · RESPONSE CURVE");
          break;
      }
    }
    fit_method_->setText(method);
  }
  if (lpc_poles_ != nullptr) lpc_poles_->setEnabled(has_lpc && !fit_active_);
  if (zero_fit_ != nullptr) zero_fit_->setEnabled(has_target && !fit_active_);
  if (free_fit_ != nullptr) free_fit_->setEnabled(has_target && !fit_active_);
  refreshFitRoom();
  if (undo_action_ != nullptr) {
    undo_action_->setEnabled(!fit_active_ && document_->undoStack()->canUndo());
  }
  if (redo_action_ != nullptr) {
    redo_action_->setEnabled(!fit_active_ && document_->undoStack()->canRedo());
  }
}
