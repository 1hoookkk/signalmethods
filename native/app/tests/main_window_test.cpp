#include "armadillo_view.hpp"
#include "chassis_bar.hpp"
#include "fit_room.hpp"
#include "main_window.hpp"
#include "morph_strip.hpp"
#include "posture_list.hpp"
#include "response_plot.hpp"
#include "section_readout.hpp"
#include "user_postures.hpp"
#include "trench/core/formants.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/section_param.hpp"

#include <QTest>
#include <QApplication>
#include <QPushButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QToolButton>
#include <QMouseEvent>
#include <QAction>
#include <QSignalSpy>
#include <QPointer>
#include <QAbstractButton>
#include <QDir>
#include <QSlider>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <variant>
#include <optional>
#include <vector>

namespace {

std::vector<std::uint8_t> read_fixture(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  const auto size = static_cast<std::size_t>(stream.tellg());
  std::vector<std::uint8_t> bytes(size);
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  return bytes;
}

std::filesystem::path tb303_path() {
  return std::filesystem::path(TRENCH_SOURCE_ROOT) / "dev/e2e/tb303.body240";
}

std::filesystem::path fixture_path() {
  return std::filesystem::path(TRENCH_SOURCE_ROOT) / "ref/presets/P2k_013_talking_hedz.bin";
}

QAction* overlay_action(QMenu* menu, const QString& name) {
  for (auto* action : menu->actions()) {
    if (action->menu() == nullptr) {
      if (action->text() == name) return action;
      continue;
    }
    for (auto* leaf : action->menu()->actions()) {
      if (leaf->text() == name) return leaf;
    }
  }
  return nullptr;
}

class PaintCounter final : public QObject {
 public:
  int count{};

 protected:
  bool eventFilter(QObject*, QEvent* event) override {
    if (event->type() == QEvent::Paint) ++count;
    return false;
  }
};

double peak_hz_below(const std::vector<double>& response_db,
                     const std::vector<double>& hz, double limit_hz) {
  double best_hz = 0.0;
  double best_db = -1.0e9;
  for (std::size_t index = 0; index < response_db.size() && hz[index] <= limit_hz; ++index) {
    if (response_db[index] > best_db) {
      best_db = response_db[index];
      best_hz = hz[index];
    }
  }
  return best_hz;
}

void send_mouse(QWidget* widget, QEvent::Type type, const QPointF& position,
                Qt::MouseButton button, Qt::MouseButtons buttons) {
  QMouseEvent event(type, position, widget->mapToGlobal(position), button, buttons,
                    Qt::NoModifier);
  QCoreApplication::sendEvent(widget, &event);
}

void press(QWidget* widget, const QPointF& position) {
  send_mouse(widget, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
}

void move(QWidget* widget, const QPointF& position) {
  send_mouse(widget, QEvent::MouseMove, position, Qt::NoButton, Qt::LeftButton);
}

void release(QWidget* widget, const QPointF& position) {
  send_mouse(widget, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
}

template <typename Rows>
double corner_dc_db(const Rows& rows) {
  trench::core::p2k::PackedCorner packed{};
  for (std::size_t si = 0; si < trench::core::kLegacySectionCount; ++si) {
    for (std::size_t wi = 0; wi < trench::core::p2k::kWordCount; ++wi) {
      packed[si * trench::core::p2k::kWordCount + wi] = rows[si][wi];
    }
  }
  return trench::core::p2k::dc_gain_db(packed);
}

template <typename Rows>
trench::core::p2k::CornerWords root_words(const Rows& rows) {
  trench::core::p2k::CornerWords out{};
  for (std::size_t section = 0; section < out.size(); ++section) {
    for (std::size_t word = 0; word < out[section].size(); ++word) {
      out[section][word] = rows[section][word];
    }
  }
  return out;
}

template <typename Rows>
bool corner_scales_are_one_gain(const Rows& rows) {
  for (std::size_t section = 1; section < trench::core::kLegacySectionCount; ++section) {
    if (rows[section][4] != rows[0][4]) return false;
  }
  return true;
}

double grid_power_db(const BodyDocument& document) {
  const auto response = document.viewResponseDb();
  const auto& grid = document.grid();
  double power = 0.0;
  for (std::size_t index = 0; index < response.size(); ++index) {
    power += grid.weight[index] * std::pow(10.0, response[index] / 10.0);
  }
  return 10.0 * std::log10(power / grid.weight_sum);
}

double worst_over(const trench::core::p2k::CornerWords& roots,
                  const std::vector<double>& ceiling,
                  const trench::core::p2k::Grid& grid) {
  namespace p2k = trench::core::p2k;
  auto corner = p2k::Corner::from_words(p2k::enter(roots), grid);
  auto model = corner.total();
  double dc = 0.0;
  for (const auto& row : roots) {
    const auto [numerator, denominator] = p2k::dc_terms(row);
    dc += 20.0 * std::log10(std::max(std::abs(numerator), 1.0e-15) /
                              std::max(std::abs(denominator), 1.0e-15));
  }
  double worst = -1.0e9;
  for (std::size_t index = 0; index < model.size(); ++index) {
    worst = std::max(worst, model[index] - dc - ceiling[index]);
  }
  return worst;
}

void double_click(QWidget* widget, const QPointF& position) {
  press(widget, position);
  release(widget, position);
  send_mouse(widget, QEvent::MouseButtonDblClick, position, Qt::LeftButton, Qt::LeftButton);
  release(widget, position);
}

void drag(QWidget* widget, const QPointF& from, const QPointF& to, int steps) {
  press(widget, from);
  for (int step = 1; step <= steps; ++step) {
    const auto fraction = static_cast<double>(step) / static_cast<double>(steps);
    move(widget, from + (to - from) * fraction);
  }
  release(widget, to);
}

int first_eq_row(const MainWindow& window) {
  for (std::size_t section = 0; section + 1 < trench::core::kLegacySectionCount; ++section) {
    const auto param = trench::core::p2k::param_of(window.body().words[0][section],
                                                   trench::core::kP2kDatumHz);
    if (param.type == trench::core::p2k::SectionType::kEq && param.fc_hz <= 4000.0) {
      return static_cast<int>(section);
    }
  }
  return -1;
}

std::optional<ArmadilloView::Marker> armadillo_marker(const ArmadilloView* view,
                                                      std::size_t section, bool zero) {
  for (const auto& marker : view->markers()) {
    if (marker.section == section && marker.zero == zero && !marker.ghost) return marker;
  }
  return std::nullopt;
}

std::optional<ResponsePlotWidget::TokenInfo> find_token(const ResponsePlotWidget* plot,
                                                        std::size_t section,
                                                        ResponsePlotWidget::Lane lane) {
  for (const auto& token : plot->tokens()) {
    if (token.section == section && token.lane == lane) return token;
  }
  return std::nullopt;
}

bool is_lattice_word(std::uint16_t word) {
  const auto& words = trench::core::p2k::lattice_words();
  return std::binary_search(words.begin(), words.end(), word);
}

double pair_radius_of(std::uint16_t word_mag, std::uint16_t word_rsq) {
  const auto [p, q] = trench::core::p2k::pq(word_mag, word_rsq);
  return trench::core::p2k::pair_radius(p, q);
}

void put32(std::ofstream& out, std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); }
void put16(std::ofstream& out, std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); }

std::filesystem::path write_filtered_sawtooth_wav(
    double f0, double seconds, const std::vector<trench::core::PackedSection>& sections) {
  const double rate = trench::core::kP2kDatumHz;
  const auto count = static_cast<std::size_t>(seconds * rate);
  std::vector<double> x(count, 0.0);
  for (std::size_t k = 1; static_cast<double>(k) * f0 < 0.5 * rate; ++k) {
    for (std::size_t i = 0; i < count; ++i) {
      x[i] += std::sin(2.0 * std::numbers::pi * static_cast<double>(k) * f0 * static_cast<double>(i) / rate) /
              static_cast<double>(k);
    }
  }
  for (const auto& words : sections) {
    const auto b = trench::core::section_words_to_biquad(words);
    double w1 = 0.0;
    double w2 = 0.0;
    for (auto& sample : x) {
      const double w0 = sample - b[3] * w1 - b[4] * w2;
      sample = b[0] * w0 + b[1] * w1 + b[2] * w2;
      w2 = w1;
      w1 = w0;
    }
  }
  double peak = 1e-9;
  for (const auto v : x) peak = std::max(peak, std::abs(v));
  const auto path = std::filesystem::temp_directory_path() / "trench_main_window_303.wav";
  std::ofstream out(path, std::ios::binary);
  const auto data_bytes = static_cast<std::uint32_t>(count * 2);
  out.write("RIFF", 4);
  put32(out, 36 + data_bytes);
  out.write("WAVEfmt ", 8);
  put32(out, 16);
  put16(out, 1);
  put16(out, 1);
  put32(out, static_cast<std::uint32_t>(rate));
  put32(out, static_cast<std::uint32_t>(rate) * 2);
  put16(out, 2);
  put16(out, 16);
  out.write("data", 4);
  put32(out, data_bytes);
  for (const auto v : x) {
    put16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(30000.0 * v / peak))));
  }
  return path;
}

std::optional<trench::core::ConjugatePair> conjugate_of(
    const trench::core::PackedBody& body, std::size_t section,
    ResponsePlotWidget::Lane lane) {
  const auto geometry = trench::core::geometry_from_words(body.words[0][section],
                                                          trench::core::kP2kDatumHz);
  const auto& pair = lane == ResponsePlotWidget::Lane::kPole ? geometry.pole : geometry.zero;
  if (const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&pair)) {
    return *conjugate;
  }
  return std::nullopt;
}

double extreme_hz(const ResponsePlotWidget* plot, const std::vector<double>& db,
                  bool maximum) {
  double best_hz = 0.0;
  double best_db = maximum ? -1.0e9 : 1.0e9;
  for (std::size_t index = 0; index < db.size(); ++index) {
    if (maximum ? db[index] > best_db : db[index] < best_db) {
      best_db = db[index];
      best_hz = plot->frequencyAt(index);
    }
  }
  return best_hz;
}

}  // namespace

class MainWindowTest final : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase() {
    const auto dir = QDir::tempPath() + QStringLiteral("/trench_qt_test_postures");
    QDir(dir).removeRecursively();
    UserPostures::setBaseDir(dir);
  }

  void theAppOpensEmpty() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    std::size_t off = 0;
    for (std::size_t corner = 0; corner < trench::core::kLegacyCornerCount; ++corner) {
      for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
        if (window.body().words[corner][section] == trench::core::kIdentitySection) ++off;
      }
    }
    QCOMPARE(off, std::size_t{24});
    QVERIFY(window.responsePlot()->tokens().empty());
    QCOMPARE(window.undoStack()->count(), 0);
  }

  void aDoubleClickPlacesAResonance() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    const auto before = window.body().words[0][0];

    double_click(armadillo, QPointF{armadillo->xForFrequency(1000.0),
                                    armadillo->yForRadius(0.96)});
    QTest::qWait(20);

    const auto geometry = trench::core::geometry_from_words(window.body().words[0][0],
                                                            trench::core::kP2kDatumHz);
    const auto* pole = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
    QVERIFY(pole != nullptr);
    QVERIFY2(std::abs(std::log2(pole->hz / 1000.0)) < 0.06,
             qPrintable(QString::number(pole->hz)));
    QVERIFY2(std::abs(pole->radius - 0.96) < 0.01,
             qPrintable(QString::number(pole->radius)));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(window.body().words[4][0], window.body().words[0][0]);
    QCOMPARE(window.undoStack()->count(), 1);
    for (std::size_t section = 1; section < trench::core::kLegacySectionCount; ++section) {
      for (std::size_t word = 0; word < 4; ++word) {
        QCOMPARE(window.body().words[0][section][word],
                 trench::core::kIdentitySection[word]);
      }
    }
  }

  void draggingAResonanceUpNarrowsIt() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    double_click(armadillo, QPointF{armadillo->xForFrequency(1000.0),
                                    armadillo->yForRadius(0.9)});
    QTest::qWait(20);

    const auto placed = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(placed.has_value());
    const auto token = armadillo_marker(armadillo, 0, false);
    QVERIFY(token.has_value());
    drag(armadillo, token->position, token->position + QPointF{0.0, -25.0}, 6);

    const auto moved = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(moved.has_value());
    QVERIFY2(moved->bw_hz < placed->bw_hz, qPrintable(QString::number(moved->bw_hz)));
    QVERIFY2(std::abs(std::log2(moved->hz / placed->hz)) < 0.03,
             qPrintable(QString::number(moved->hz)));
    QCOMPARE(window.undoStack()->count(), 2);
  }

  void aTypedHzMovesThePole() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);

    const auto before = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(before.has_value());
    const auto before_words = window.body().words[0][0];

    auto* field = window.sectionReadout()->findChild<QLineEdit*>(
        QStringLiteral("poleHzField"));
    QVERIFY(field != nullptr);
    QCOMPARE(field->text(), QString::number(before->hz, 'f', 0));

    field->setFocus(Qt::MouseFocusReason);
    QTest::qWait(20);
    field->setText(QStringLiteral("2000"));
    QTest::keyClick(field, Qt::Key_Return);
    QTest::qWait(20);

    const auto after = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(after.has_value());
    QVERIFY2(std::abs(std::log2(after->hz / 2000.0)) < 0.015,
             qPrintable(QString::number(after->hz)));
    QVERIFY2(std::abs(after->bw_hz / before->bw_hz - 1.0) < 0.05,
             qPrintable(QStringLiteral("%1 %2").arg(before->bw_hz).arg(after->bw_hz)));
    QCOMPARE(window.undoStack()->count(), 1);

    window.undoStack()->undo();
    QCOMPARE(window.body().words[0][0], before_words);
  }

  void aTypedWidthNarrowsThePole() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);

    const auto before = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(before.has_value());
    const auto before_words = window.body().words[0][0];

    auto* field = window.sectionReadout()->findChild<QLineEdit*>(
        QStringLiteral("poleWidthField"));
    QVERIFY(field != nullptr);
    QCOMPARE(field->text(), QString::number(before->bw_hz, 'f', 0));

    field->setFocus(Qt::MouseFocusReason);
    QTest::qWait(20);
    field->setText(QString::number(before->bw_hz * 0.5, 'f', 0));
    QTest::keyClick(field, Qt::Key_Return);
    QTest::qWait(20);

    const auto after = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(after.has_value());
    QVERIFY2(after->bw_hz < before->bw_hz * 0.75,
             qPrintable(QStringLiteral("%1 %2").arg(before->bw_hz).arg(after->bw_hz)));
    QVERIFY2(std::abs(std::log2(after->hz / before->hz)) < 0.02,
             qPrintable(QStringLiteral("%1 %2").arg(before->hz).arg(after->hz)));
    QCOMPARE(window.undoStack()->count(), 1);

    window.undoStack()->undo();
    QCOMPARE(window.body().words[0][0], before_words);
  }

  void aFreshSkeletonBindsItsZeroToThePole() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    double_click(window.armadilloView(),
                 QPointF{window.armadilloView()->xForFrequency(1000.0),
                         window.armadilloView()->yForRadius(0.96)});
    QTest::qWait(20);

    const auto words = window.body().words[0][0];
    QCOMPARE(words[0], trench::core::kIdentitySection[0]);
    QCOMPARE(words[1], trench::core::kIdentitySection[1]);
    const auto dormant = find_token(plot, 0, ResponsePlotWidget::Lane::kZero);
    QVERIFY(dormant.has_value());

    const auto masked = trench::core::section_words_to_biquad(words);
    auto bare = masked;
    bare[1] = 0.0;
    bare[2] = 0.0;
    const auto shape = [](const trench::core::Biquad& section, double hz) {
      return trench::core::section_response_db(section, hz, trench::core::kP2kDatumHz) -
             trench::core::section_response_db(section, 0.0, trench::core::kP2kDatumHz);
    };
    const auto rise = [&](double hz) { return shape(masked, hz) - shape(bare, hz); };
    QVERIFY2(std::abs(rise(300.0)) < 3.0, qPrintable(QString::number(rise(300.0))));
    QVERIFY2(std::abs(rise(16'000.0)) < 3.0, qPrintable(QString::number(rise(16'000.0))));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(window.body().words[4][0], words);
    QCOMPARE(window.undoStack()->count(), 1);

    window.clearSection(0);
    QCOMPARE(window.body().words[0][0], trench::core::kIdentitySection);
    QCOMPARE(window.undoStack()->count(), 2);
  }

  void theAxisHoldsTheAudibleBandWhenAPostureIsApplied() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("REZ dead_ringer c1"));
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    const auto [low_db, high_db] = plot->dbRange();
    double band_low = 1.0e9;
    double band_high = -1.0e9;
    for (std::size_t index = 0; index < plot->responsePointCount(); ++index) {
      const auto hz = plot->frequencyAt(index);
      if (hz < 100.0 || hz > 16'000.0) continue;
      band_low = std::min(band_low, plot->responseDbAt(index));
      band_high = std::max(band_high, plot->responseDbAt(index));
    }
    const auto shown = QStringLiteral("%1 .. %2 for %3 .. %4")
                           .arg(low_db)
                           .arg(high_db)
                           .arg(band_low)
                           .arg(band_high);
    QVERIFY2(high_db - low_db <= 144.0, qPrintable(shown));
    QVERIFY2(high_db >= band_high + 3.0 && high_db < band_high + 15.0, qPrintable(shown));
    QVERIFY2(low_db <= band_low || high_db - low_db == 144.0, qPrintable(shown));
  }

  void windowOwnsAndReleasesQtObjects() {
    const auto fixture = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                         "ref/presets/P2k_013_talking_hedz.bin";
    auto* window = new MainWindow(fixture, trench::core::kP2kDatumHz);
    QPointer<ResponsePlotWidget> plot = window->responsePlot();
    QPointer<QUndoStack> undo_stack = window->findChild<QUndoStack*>();
    QVERIFY(!plot.isNull());
    QVERIFY(!undo_stack.isNull());

    delete window;
    QVERIFY(plot.isNull());
    QVERIFY(undo_stack.isNull());
  }

  void coreResponseReachesThePainterSurface() {
    const auto fixture = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                         "ref/presets/P2k_013_talking_hedz.bin";
    MainWindow window(fixture, trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);

    auto* plot = window.responsePlot();
    QVERIFY(plot != nullptr);
    QCOMPARE(plot->responsePointCount(), 512U);

    const auto body = trench::core::PackedBody::from_legacy_bytes(read_fixture(fixture));
    const auto cascade = body.interpolate_biquads(0.0F, 0.0F, 0.0F);
    constexpr std::size_t sample = 256;
    const auto expected = trench::core::cascade_response_db(
        cascade, plot->frequencyAt(sample), trench::core::kP2kDatumHz);
    QVERIFY(std::isfinite(plot->responseDbAt(sample)));
    QVERIFY(std::abs(plot->responseDbAt(sample) - expected) < 1.0e-12);

    window.resize(1200, 700);
    QTest::qWait(20);
    QCOMPARE(window.centralWidget()->size(), window.contentsRect().size());
  }

  void dragWritesLatticeWords() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto before = window.body().words[0][0];
    const auto token = armadillo_marker(window.armadilloView(), 0, false);
    QVERIFY(token.has_value());

    drag(window.armadilloView(), token->position,
         token->position + QPointF{-70.0, 22.0}, 5);

    const auto& after = window.body().words[0][0];
    QVERIFY(after != before);
    QVERIFY(is_lattice_word(after[2]));
    QVERIFY(is_lattice_word(after[3]));
    const auto [p, q] = trench::core::p2k::pq(after[2], after[3]);
    QVERIFY(trench::core::p2k::is_legal(p, q, true));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(after[0], before[0]);
    QCOMPARE(after[1], before[1]);
    QCOMPARE(window.body().words[4][0], after);
  }

  void undoRestoresExactBytes() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto before_native = window.body().native_bytes();
    const auto before_legacy = window.body().legacy_bytes();

    const auto token = armadillo_marker(window.armadilloView(), 1, false);
    QVERIFY(token.has_value());
    drag(window.armadilloView(), token->position,
         token->position + QPointF{60.0, 25.0}, 5);
    QCOMPARE(window.undoStack()->count(), 1);
    QVERIFY(window.body().native_bytes() != before_native);

    window.undoStack()->undo();

    QVERIFY(window.body().native_bytes() == before_native);
    const auto after_legacy = window.body().legacy_bytes();
    QVERIFY(after_legacy == before_legacy);
    const auto round_trip = trench::core::PackedBody::from_legacy_bytes(after_legacy);
    QVERIFY(round_trip.legacy_bytes() == before_legacy);
  }

  void pinnedTokenIsImmobile() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto token = find_token(plot, 2, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    const auto bit = trench::core::p2k::pole_bit(2);
    QVERIFY((window.freedomMask() & bit) != 0U);

    press(plot, token->position);
    release(plot, token->position);
    QCOMPARE(window.freedomMask() & bit, 0U);

    const auto pinned = find_token(plot, 2, ResponsePlotWidget::Lane::kPole);
    QVERIFY(pinned.has_value());
    QVERIFY(pinned->pinned);

    const auto before = window.body().words[0][2];
    const auto marker = armadillo_marker(window.armadilloView(), 2, false);
    QVERIFY(marker.has_value());
    drag(window.armadilloView(), marker->position,
         marker->position + QPointF{-80.0, 26.0}, 5);
    QCOMPARE(window.body().words[0][2], before);
    QCOMPARE(window.undoStack()->count(), 0);

    press(plot, token->position);
    release(plot, token->position);
    QCOMPARE(window.freedomMask() & bit, bit);
  }

  void identitySectionHasNoToken() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);

    const auto tokens = window.responsePlot()->tokens();
    std::size_t poles = 0;
    std::size_t zeros = 0;
    for (const auto& token : tokens) {
      QVERIFY(token.section < trench::core::kLegacySectionCount);
      QVERIFY(window.body().words[0][token.section] != trench::core::kIdentitySection);
      if (token.lane == ResponsePlotWidget::Lane::kZero) {
        ++zeros;
      } else {
        ++poles;
      }
    }
    QVERIFY(poles <= trench::core::kLegacySectionCount);
    QVERIFY(zeros <= poles);
    QVERIFY2(zeros > 1, qPrintable(QString::number(zeros)));
  }

  void degeneratePairRendersInert() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    auto degenerate = window.body().words[0][3];
    degenerate[0] = trench::core::kIdentitySection[0];
    degenerate[1] = trench::core::kIdentitySection[1];
    degenerate[2] = trench::core::kIdentitySection[2];
    degenerate[3] = trench::core::kIdentitySection[3];
    window.applySection(3, degenerate);
    QTest::qWait(20);

    window.selectSection(3);
    QVERIFY(!find_token(plot, 3, ResponsePlotWidget::Lane::kPole).has_value());
    QVERIFY(!find_token(plot, 3, ResponsePlotWidget::Lane::kZero).has_value());

    QCOMPARE(window.body().words[0][3], degenerate);
    QCOMPARE(window.body().words[4][3], degenerate);
    QCOMPARE(window.undoStack()->count(), 0);
  }

  void illegalCandidateIsRefused() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    drag(plot, token->position, QPointF{token->position.x(), 1.0}, 10);

    const auto& after = window.body().words[0][0];
    QVERIFY(is_lattice_word(after[2]));
    QVERIFY(is_lattice_word(after[3]));
    const auto [p, q] = trench::core::p2k::pq(after[2], after[3]);
    QVERIFY(trench::core::p2k::is_legal(p, q, true));
    QVERIFY(pair_radius_of(after[2], after[3]) <= trench::core::p2k::pole_radius_ceiling());
    QCOMPARE(window.body().words[4][0], after);
  }

  void runningLevelLaneIsTheCascadePrefixPeak() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto body = trench::core::PackedBody::from_legacy_bytes(read_fixture(fixture_path()));
    const auto cascade = body.interpolate_biquads(0.0F, 0.0F, 0.0F);
    std::vector<double> cumulative(plot->responsePointCount(), 0.0);
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      double expected = -1.0e9;
      for (std::size_t index = 0; index < cumulative.size(); ++index) {
        cumulative[index] += trench::core::section_response_db(
            cascade[section], plot->frequencyAt(index), trench::core::kP2kDatumHz);
        expected = std::max(expected, cumulative[index]);
      }
      QVERIFY(std::abs(plot->runningPeakDb(section) - expected) < 1.0e-9);
    }

    double full_peak = -1.0e9;
    for (std::size_t index = 0; index < plot->responsePointCount(); ++index) {
      full_peak = std::max(full_peak, plot->responseDbAt(index));
    }
    QVERIFY(std::abs(plot->runningPeakDb(trench::core::kLegacySectionCount - 1) -
                     full_peak) < 1.0e-6);
  }

  void residualStripIsMeanRemovedTargetMinusCurrent() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    QCOMPARE(plot->residualPointCount(), 0U);

    const auto target_body = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                             "ref/presets/P2k_002_early_rizer.bin";
    QVERIFY(window.loadTarget(target_body));
    QCOMPARE(plot->residualPointCount(), trench::core::p2k::kNpts);

    const auto target = trench::core::p2k::corner_response_db(
        trench::core::p2k::rom_corner_words(read_fixture(target_body), 0));
    const auto& grid = trench::core::p2k::grid();
    double mean = 0.0;
    for (std::size_t index = 0; index < target.size(); ++index) {
      mean += grid.weight[index] * (target[index] - plot->responseDbAt(index));
    }
    mean /= grid.weight_sum;
    double weighted_sum = 0.0;
    for (std::size_t index = 0; index < target.size(); ++index) {
      const auto expected = target[index] - plot->responseDbAt(index) - mean;
      QVERIFY(std::abs(plot->alignedTargetDbAt(index) - (target[index] - mean)) < 1.0e-9);
      QVERIFY(std::abs(plot->residualDbAt(index) - expected) < 1.0e-9);
      weighted_sum += grid.weight[index] * plot->residualDbAt(index);
    }
    QVERIFY(std::abs(weighted_sum) < 1.0e-6);
  }

  void audioTargetMeasuresTheRecordingOntoTheFitGrid() {
    const auto sections = std::vector<trench::core::PackedSection>{
        trench::core::words_from_geometry(
            {trench::core::ConjugatePair{1200.0, 0.97}, trench::core::DegeneratePair{}, 0.25},
            trench::core::kP2kDatumHz),
        trench::core::words_from_geometry(
            {trench::core::ConjugatePair{400.0, 0.9}, trench::core::ConjugatePair{3000.0, 0.8}, 1.0},
            trench::core::kP2kDatumHz)};
    const auto wav = write_filtered_sawtooth_wav(49.14, 2.0, sections);

    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    QCOMPARE(window.sourceModel(), trench::core::measure::Source::kFlat);
    window.setSourceModel(trench::core::measure::Source::kSawtooth);
    QVERIFY(window.loadTarget(wav));
    QVERIFY(window.document()->target().has_value());
    QCOMPARE(window.document()->target()->size(), trench::core::p2k::kNpts);

    const auto& grid = trench::core::p2k::grid();
    std::vector<double> hz;
    std::vector<double> weight;
    std::vector<double> target;
    for (std::size_t i = 0; i < grid.hz.size(); ++i) {
      if (grid.hz[i] >= 49.14 && grid.hz[i] <= 12000.0) {
        hz.push_back(grid.hz[i]);
        weight.push_back(grid.weight[i]);
        target.push_back((*window.document()->target())[i]);
      }
    }
    std::vector<std::uint16_t> words;
    for (const auto& s : sections) words.insert(words.end(), s.begin(), s.end());
    const auto report = trench::core::measure::score_words(
        words, trench::core::kP2kDatumHz, hz, weight, target);
    QVERIFY2(report.rms_db < 0.5, qPrintable(QString::number(report.rms_db)));
    std::filesystem::remove(wav);
  }

  void fitRequiresATarget() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    window.startFit();
    QVERIFY(!window.fitRunning());
    QCOMPARE(window.undoStack()->count(), 0);
  }

  void stopAndKeepCommitsOneUndoEntry() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("REZ dead_ringer c1"));
    window.undoStack()->clear();
    window.document()->setTarget(std::vector<double>(trench::core::p2k::kNpts, 0.0));
    const auto before = window.body().native_bytes();
    const auto before_corner = window.body().words[0];

    window.startFit();
    QVERIFY(window.fitRunning());
    window.stopAndKeep();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);

    QCOMPARE(window.undoStack()->count(), 1);
    QVERIFY(window.body().is_legacy_representable());
    QVERIFY(window.body().native_bytes() != before);
    const auto after = window.body().words[0];
    bool zero_changed = false;
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(after[section][2], before_corner[section][2]);
      QCOMPARE(after[section][3], before_corner[section][3]);
      zero_changed |= after[section][0] != before_corner[section][0] ||
                      after[section][1] != before_corner[section][1];
    }
    QVERIFY(zero_changed);
    QVERIFY(corner_scales_are_one_gain(after));
    QVERIFY(std::abs(corner_dc_db(after)) < 0.5);
    QVERIFY(window.grab().save(QString(TRENCH_SOURCE_ROOT) +
                               "/dev/e2e/app_zero_fit_flat.png"));
    window.undoStack()->undo();
    QVERIFY(window.body().native_bytes() == before);
  }

  void discardRestoresPreFitBytesAndDropsStragglers() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("REZ dead_ringer c1"));
    window.undoStack()->clear();
    window.document()->setTarget(std::vector<double>(trench::core::p2k::kNpts, 0.0));
    const auto before = window.body().native_bytes();

    window.startFit();
    window.discardFit();
    QVERIFY(window.body().native_bytes() == before);
    QTest::qWait(300);
    QVERIFY(window.body().native_bytes() == before);
    QCOMPARE(window.undoStack()->count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);
  }

  void pinnedSectionsAreNeverTouchedByFit() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("REZ dead_ringer c1"));
    window.undoStack()->clear();
    window.document()->setTarget(std::vector<double>(trench::core::p2k::kNpts, 0.0));

    auto* document = window.document();
    for (std::size_t section = 0; section < 6; ++section) {
      if (section == 3) continue;
      document->toggleLane(section, false);
    }
    std::array<trench::core::PackedSection, 6> before{};
    for (std::size_t section = 0; section < 6; ++section) {
      before[section] = window.body().words[0][section];
    }

    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);
    QVERIFY(window.body().words[0][3][0] != before[3][0] ||
            window.body().words[0][3][1] != before[3][1]);
    for (std::size_t section = 0; section < 6; ++section) {
      if (section == 3) continue;
      for (std::size_t wi = 0; wi < 4; ++wi) {
        QCOMPARE(window.body().words[0][section][wi], before[section][wi]);
      }
    }
    QVERIFY(corner_scales_are_one_gain(window.body().words[0]));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
  }

  void fitUsesTheAlignedTargetAndLeavesComparisonVisible() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("REZ dead_ringer c1"));
    window.undoStack()->clear();

    const auto before = window.body().words[0];
    const auto model = window.document()->viewResponseDb();
    const auto& grid = window.document()->grid();
    std::vector<double> target(model.size(), 0.0);
    for (std::size_t index = 0; index < target.size(); ++index) {
      const auto phase = static_cast<double>(index) /
                         static_cast<double>(target.size() - 1);
      target[index] = 5.0 * std::sin(phase * 5.0 * std::numbers::pi);
    }
    window.document()->setTarget(target);

    double offset = 0.0;
    for (std::size_t index = 0; index < target.size(); ++index) {
      offset += grid.weight[index] * (target[index] - model[index]);
    }
    offset /= grid.weight_sum;
    std::vector<double> ceiling(target.size());
    for (std::size_t index = 0; index < target.size(); ++index) {
      ceiling[index] = target[index] - offset;
    }
    const auto before_over = worst_over(root_words(before), ceiling, grid);

    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);
    const auto after = window.body().words[0];
    const auto after_over = worst_over(root_words(after), ceiling, grid);
    QVERIFY2(after_over < before_over,
             qPrintable(QStringLiteral("%1 !< %2").arg(after_over).arg(before_over)));
    bool zero_changed = false;
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(after[section][2], before[section][2]);
      QCOMPARE(after[section][3], before[section][3]);
      zero_changed |= after[section][0] != before[section][0] ||
                      after[section][1] != before[section][1];
    }
    QVERIFY(zero_changed);
    QVERIFY(corner_scales_are_one_gain(after));
    QVERIFY(std::abs(corner_dc_db(after)) < 0.5);

    auto* plot = window.responsePlot();
    QCOMPARE(plot->residualPointCount(), p2k::kNpts);
    for (std::size_t index = 0; index < plot->responsePointCount(); ++index) {
      QVERIFY(std::abs(plot->alignedTargetDbAt(index) - plot->responseDbAt(index) -
                       plot->residualDbAt(index)) < 1.0e-9);
    }
    QVERIFY(window.grab().save(QString(TRENCH_SOURCE_ROOT) +
                               "/dev/e2e/app_target_compare.png"));
    QCOMPARE(window.undoStack()->count(), 1);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
  }

  void editAtCornerTwoWritesOnlyThatCorner() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto untouched = window.body();
    window.setCorner(2);
    QCOMPARE(window.document()->corner(), std::size_t{2});
    const auto token = armadillo_marker(window.armadilloView(), 0, false);
    QVERIFY(token.has_value());
    drag(window.armadilloView(), token->position,
         token->position + QPointF{-70.0, 22.0}, 5);
    const auto& after = window.body();
    QVERIFY(after.words[2][0] != untouched.words[2][0]);
    QCOMPARE(after.words[6][0], after.words[2][0]);
    for (const std::size_t corner : {0U, 1U, 3U, 4U, 5U, 7U}) {
      QCOMPARE(after.words[corner], untouched.words[corner]);
    }
  }

  void undoLandsOnTheCornerThatWasEdited() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto untouched = window.body();
    window.setCorner(2);
    window.selectSection(1);
    const auto token = armadillo_marker(window.armadilloView(), 1, true);
    QVERIFY(token.has_value());
    drag(window.armadilloView(), token->position,
         token->position + QPointF{60.0, -18.0}, 5);
    QVERIFY(window.body().words[2][1] != untouched.words[2][1]);
    window.setCorner(0);
    QCOMPARE(window.undoStack()->count(), 1);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[2][1], untouched.words[2][1]);
    QCOMPARE(window.body().words[6][1], untouched.words[2][1]);
    QCOMPARE(window.body().words[0], untouched.words[0]);
    QCOMPARE(window.document()->corner(), std::size_t{0});
  }

  void saveWritesTheLegacyBytesBackExactly() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto out = std::filesystem::temp_directory_path() / "trench_native_save_test.body240";
    QVERIFY(window.saveBody(out));
    QCOMPARE(window.bodyPath(), out);
    QVERIFY(read_fixture(out) == read_fixture(fixture_path()));
    window.setCorner(3);
    const auto token = armadillo_marker(window.armadilloView(), 0, false);
    QVERIFY(token.has_value());
    drag(window.armadilloView(), token->position,
         token->position + QPointF{-70.0, 22.0}, 5);
    QVERIFY(window.saveBody(out));
    const auto reloaded = trench::core::PackedBody::from_body_bytes(read_fixture(out));
    QVERIFY(reloaded.words == window.body().words);
    QVERIFY(reloaded.words[3][0] != trench::core::PackedBody::from_body_bytes(read_fixture(fixture_path())).words[3][0]);
    std::filesystem::remove(out);
  }

  void fitAtCornerThreeLeavesTheOtherCornersAlone() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    const auto target_body = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                             "ref/presets/P2k_002_early_rizer.bin";
    QVERIFY(window.loadTarget(target_body));
    window.setCorner(3);
    window.applyVowel(QStringLiteral("REZ dead_ringer c1"));
    window.undoStack()->clear();
    const auto before = window.body();
    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);
    QVERIFY(window.body().words[3] != before.words[3]);
    for (const std::size_t corner : {0U, 1U, 2U, 4U, 5U, 6U}) {
      QCOMPARE(window.body().words[corner], before.words[corner]);
    }
    QCOMPARE(window.undoStack()->count(), 1);
    window.undoStack()->undo();
    QVERIFY(window.body().words == before.words);
  }

  void anIdentityCornerSeedsFromItsFittedNeighbourFirst() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    BodyDocument::CornerSnapshot identity{};
    identity.fill(trench::core::kIdentitySection);
    window.document()->applyCorner(3, identity);
    window.setCorner(3);
    QVERIFY(window.document()->seedIsInherited());
    const auto seed = window.document()->seedWords();
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      for (std::size_t word = 0; word < seed[section].size(); ++word) {
        QCOMPARE(seed[section][word], window.body().words[2][section][word]);
      }
    }
    window.setCorner(0);
    QVERIFY(!window.document()->seedIsInherited());
  }

  void anIdentityCornerSeedsItsFitFromCornerZero() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    BodyDocument::CornerSnapshot identity{};
    identity.fill(trench::core::kIdentitySection);
    window.document()->applyCorner(1, identity);
    window.setCorner(1);
    const auto seed = window.document()->seedWords();
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      for (std::size_t word = 0; word < seed[section].size(); ++word) {
        QCOMPARE(seed[section][word], window.body().words[0][section][word]);
      }
    }
  }

  void theSpaceIsOneUndoEntryAndLeavesTheBodyAlone() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    auto* document = window.document();
    const auto before_bytes = window.body().native_bytes();
    const auto before_lo = document->grid().hz.front();
    QVERIFY(std::abs(before_lo - trench::core::p2k::kLoHz) < 1.0e-9);

    auto space = document->space();
    space.lo_hz = 200.0;
    document->setSpace(space);

    QCOMPARE(window.undoStack()->count(), 1);
    QVERIFY(std::abs(document->grid().hz.front() - 200.0) < 1.0e-9);
    QVERIFY(window.body().native_bytes() == before_bytes);

    window.undoStack()->undo();
    QVERIFY(std::abs(document->grid().hz.front() - before_lo) < 1.0e-12);
    QVERIFY(window.body().native_bytes() == before_bytes);
  }

  void roleIntentIsOneUndoEntryAndRoundTrips() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    auto* document = window.document();
    QVERIFY(!document->intent()[5].has_value());

    document->setIntent(5, trench::core::p2k::Role::kTilt);
    QCOMPARE(window.undoStack()->count(), 1);
    QVERIFY(document->intent()[5].has_value());
    QCOMPARE(*document->intent()[5], trench::core::p2k::Role::kTilt);

    window.undoStack()->undo();
    QVERIFY(!document->intent()[5].has_value());
    window.undoStack()->redo();
    QCOMPARE(*document->intent()[5], trench::core::p2k::Role::kTilt);
    QCOMPARE(window.undoStack()->count(), 1);
  }

  void theViewIsTheCornerSelectorAtItsCorners() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    auto* document = window.document();
    QVERIFY(document->atCorner());
    const auto corner_zero = document->viewResponseDb();
    QCOMPARE(corner_zero.size(), trench::core::p2k::kNpts);

    document->setView(1.0F, 0.0F);
    QCOMPARE(document->corner(), std::size_t{1});
    QVERIFY(document->atCorner());

    document->setView(0.5F, 0.5F);
    QCOMPARE(document->corner(), std::size_t{1});
    QVERIFY(!document->atCorner());
    const auto interior = document->viewResponseDb();
    QCOMPARE(interior.size(), corner_zero.size());
    bool differs = false;
    for (std::size_t index = 0; index < interior.size(); ++index) {
      differs = differs || std::abs(interior[index] - corner_zero[index]) > 1.0e-9;
    }
    QVERIFY(differs);

    document->setCorner(2);
    QCOMPARE(document->view().morph, 0.0F);
    QCOMPARE(document->view().q, 1.0F);
    QVERIFY(document->atCorner());
  }

  void theTargetScoreFollowsThePerceptualSpace() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    auto* document = window.document();
    QVERIFY(std::isnan(document->targetScoreDb()));

    QVERIFY(window.loadTarget(std::filesystem::path(TRENCH_SOURCE_ROOT) /
                              "ref/presets/P2k_002_early_rizer.bin"));
    const auto wide = document->targetScoreDb();
    QVERIFY(std::isfinite(wide));
    QVERIFY(wide > 0.0);

    auto space = document->space();
    space.lo_hz = 300.0;
    space.hi_hz = 4000.0;
    document->setSpace(space);
    const auto narrow = document->targetScoreDb();
    QVERIFY(std::isfinite(narrow));
    QVERIFY(std::abs(narrow - wide) > 1.0e-6);
  }

  void zeroFitKeepsSectionSixPoleExactAndDcUnity() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    QVERIFY(window.loadTarget(std::filesystem::path(TRENCH_SOURCE_ROOT) /
                              "ref/presets/P2k_002_early_rizer.bin"));

    auto* document = window.document();
    auto tilted = window.body().words[0][5];
    tilted[0] = p2k::words_from_root(4000.0, p2k::s6_zero_radius()).first;
    const auto pole = p2k::words_from_root(500.0, 0.76);
    tilted[2] = pole.first;
    tilted[3] = pole.second;
    document->applySection(5, tilted);
    QVERIFY(p2k::within_envelope(p2k::Role::kTilt, window.body().words[0][5],
                                 trench::core::kP2kDatumHz));
    document->setIntent(5, p2k::Role::kTilt);

    const auto before = window.body().native_bytes();
    const auto pole_before = std::array<std::uint16_t, 2>{
        window.body().words[0][5][2], window.body().words[0][5][3]};
    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 60000);

    QVERIFY(window.body().native_bytes() != before);
    QCOMPARE(window.body().words[0][5][2], pole_before[0]);
    QCOMPARE(window.body().words[0][5][3], pole_before[1]);
    QVERIFY(corner_scales_are_one_gain(window.body().words[0]));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
  }

  void pressingAPlotMarkerShowsThatSectionInTheReadout() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto row = first_eq_row(window);
    QVERIFY(row >= 0);
    const auto section = static_cast<std::size_t>(row);

    auto* plot = window.responsePlot();
    const auto token = find_token(plot, section, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    press(plot, token->position);
    release(plot, token->position);
    QTest::qWait(20);

    const auto pole = p2k::pole_of(window.body().words[0][section],
                                   trench::core::kP2kDatumHz);
    QVERIFY(pole.has_value());
    auto* hz = window.sectionReadout()->findChild<QLineEdit*>(
        QStringLiteral("poleHzField"));
    auto* bw = window.sectionReadout()->findChild<QLineEdit*>(
        QStringLiteral("poleWidthField"));
    QVERIFY(hz != nullptr && bw != nullptr);
    QCOMPARE(hz->text(), QString::number(pole->hz, 'f', 0));
    QCOMPARE(bw->text(), QString::number(pole->bw_hz, 'f', 0));
  }

  void thePlotBandFollowsTheSpace() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    auto space = window.document()->space();
    space.lo_hz = 100.0;
    space.hi_hz = 8000.0;
    window.document()->setSpace(space);

    QCOMPARE(plot->responsePointCount(), 512U);
    QVERIFY(std::abs(plot->frequencyAt(0) - 100.0) < 0.5);
    QVERIFY(std::abs(plot->frequencyAt(plot->responsePointCount() - 1) - 8000.0) < 4.0);
  }

  void morphStripShowsTheInteriorAndSelectsCorners() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    const auto corner_zero_db = plot->responseDbAt(256);
    QVERIFY(!plot->tokens().empty());

    auto* morph = window.morphStrip()->findChild<QSlider*>(QStringLiteral("morphSlider"));
    QVERIFY(morph != nullptr);
    morph->setValue(morph->maximum() / 2);
    QVERIFY(!window.document()->atCorner());
    QVERIFY(plot->tokens().empty());
    QVERIFY(std::abs(plot->responseDbAt(256) - corner_zero_db) > 1.0e-9);

    morph->setValue(morph->maximum());
    QCOMPARE(window.document()->corner(), std::size_t{1});
    QVERIFY(window.document()->atCorner());
    QVERIFY(!plot->tokens().empty());
  }

  void theStripReadsTheWorstInteriorStep() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto worst = window.morphStrip()->worstStepDb();
    QVERIFY(worst > 5.0);
    QVERIFY(worst < 6.5);
  }

  void spaceEditRescoresWithoutTouchingTheBody() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    QVERIFY(window.loadTarget(std::filesystem::path(TRENCH_SOURCE_ROOT) /
                              "ref/presets/P2k_002_early_rizer.bin"));
    const auto bytes = window.body().native_bytes();
    const auto before = window.chassisBar()->scoreDb();
    QVERIFY(std::isfinite(before));

    auto space = window.document()->space();
    space.lo_hz = 200.0;
    window.document()->setSpace(space);

    QCOMPARE(window.document()->space().lo_hz, 200.0);
    QVERIFY(std::abs(window.chassisBar()->scoreDb() - before) > 1.0e-9);
    QVERIFY(window.body().native_bytes() == bytes);
  }

  void fitRoomSelectsTheOverlayAsTheTarget() {
    FitRoom room;
    room.setGridHz({100.0, 1000.0, 10000.0});
    room.setResponse({0.0, 6.0, -3.0});
    room.setOverlays({{QStringLiteral("one"), {1.0, 2.0, 3.0}},
                      {QStringLiteral("two"), {4.0, 5.0, 6.0}}},
                     1);
    QCOMPARE(room.overlayCount(), 2);
    QCOMPARE(room.selectedOverlay(), 1);
    QCOMPARE(room.pointCount(), std::size_t{3});

    QSignalSpy spy(&room, &FitRoom::overlaySelected);
    room.overlayList()->setCurrentRow(0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 0);
    QCOMPARE(room.selectedOverlay(), 0);
  }

  void fitRoomDifferenceIsOverlayMinusResponse() {
    FitRoom room;
    room.setGridHz({100.0, 1000.0, 10000.0});
    room.setResponse({0.0, 6.0, -3.0});
    room.setOverlays({{QStringLiteral("one"), {1.0, 2.0, 3.0}}}, 0);
    QCOMPARE(room.differenceDbAt(0), 1.0);
    QCOMPARE(room.differenceDbAt(1), -4.0);
    QCOMPARE(room.differenceDbAt(2), 6.0);
  }

  void postureRowEmitsItsNameAndTheHeaderDoesNot() {
    PostureList list;
    list.setGroups({{QStringLiteral("VOW"), {QStringLiteral("aa"), QStringLiteral("iy")}}});
    QCOMPARE(list.count(), 3);
    QCOMPARE(list.item(0)->text(), QStringLiteral("VOW"));
    QCOMPARE(list.focusPolicy(), Qt::NoFocus);
    QSignalSpy spy(&list, &PostureList::postureChosen);
    emit list.itemClicked(list.item(0));
    QCOMPARE(spy.count(), 0);
    emit list.itemClicked(list.item(2));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("iy"));
  }

  void fitRoomScreenshot() {
    FitRoom room;
    std::vector<double> hz(256);
    std::vector<double> response(256);
    std::vector<double> overlay(256);
    for (std::size_t index = 0; index < hz.size(); ++index) {
      const auto fraction = static_cast<double>(index) / 255.0;
      hz[index] = 40.0 * std::pow(18000.0 / 40.0, fraction);
      const auto octaves = std::log2(hz[index] / 1000.0);
      response[index] = 12.0 * std::exp(-octaves * octaves * 1.5) - 6.0;
      overlay[index] = response[index] + 3.0 * std::sin(fraction * 18.0);
    }
    room.setGridHz(hz);
    room.setResponse(response);
    room.setOverlays({{QStringLiteral("vowel aa"), overlay}}, 0);
    room.setScoreDb(2.1);
    room.resize(640, 460);
    room.show();
    QTest::qWait(50);
    const auto path = QString(TRENCH_SOURCE_ROOT) + "/dev/app_slice_fitroom.png";
    QVERIFY(room.grab().save(path));
  }

  void overlaysAreTheTargetsAndTheSelectedOneIsTheTarget() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto root = std::filesystem::path(TRENCH_SOURCE_ROOT);
    QVERIFY(window.loadTarget(root / "ref/presets/P2k_002_early_rizer.bin"));
    QVERIFY(window.loadTarget(root / "ref/presets/P2k_013_talking_hedz.bin"));
    QCOMPARE(window.overlayCount(), 2);
    QCOMPARE(window.fitRoom()->selectedOverlay(), 1);
    window.openFitRoom();
    window.fitRoom()->resize(640, 460);
    QTest::qWait(50);
    QVERIFY(window.fitRoom()->grab().save(QString(TRENCH_SOURCE_ROOT) +
                                          "/dev/app_slice_fitroom_hedz.png"));
    const auto hedz = *window.document()->target();
    window.fitRoom()->overlayList()->setCurrentRow(0);
    QVERIFY(*window.document()->target() != hedz);
    window.removeOverlay(0);
    QCOMPARE(window.overlayCount(), 1);
    QCOMPARE(*window.document()->target(), hedz);
    window.removeOverlay(0);
    QVERIFY(!window.document()->target().has_value());
  }

  void aCornerFileRoundTripsIntoAnotherSlotAsOneUndo() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto path = std::filesystem::temp_directory_path() / "trench_test_corner.corner";
    window.setCorner(2);
    QVERIFY(window.saveCorner(path));
    QCOMPARE(std::filesystem::file_size(path), std::uintmax_t{60});
    const auto source = window.body().words[2];
    window.setCorner(0);
    const auto before = window.body().native_bytes();
    QVERIFY(window.loadCorner(path));
    QCOMPARE(window.undoStack()->count(), 1);
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(window.body().words[0][section], source[section]);
      QCOMPARE(window.body().words[4][section], source[section]);
    }
    window.undoStack()->undo();
    QCOMPARE(window.body().native_bytes(), before);
    std::filesystem::remove(path);
  }

  void tuneInMovesTheViewNotTheBody() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto bytes = window.body().native_bytes();
    auto* plot = window.responsePlot();
    double peak_hz = 0.0;
    double peak_db = -1e9;
    for (std::size_t i = 0; i < plot->responsePointCount(); ++i) {
      if (plot->frequencyAt(i) > 3000.0) break;
      if (plot->responseDbAt(i) > peak_db) {
        peak_db = plot->responseDbAt(i);
        peak_hz = plot->frequencyAt(i);
      }
    }
    auto* transpose = window.morphStrip()->findChild<QSlider*>(QStringLiteral("transposeSlider"));
    QVERIFY(transpose != nullptr);
    transpose->setValue(12);
    QCOMPARE(window.document()->view().semitones, 12.0);
    QVERIFY(!plot->tokens().empty());
    double up_hz = 0.0;
    double up_db = -1e9;
    for (std::size_t i = 0; i < plot->responsePointCount(); ++i) {
      if (plot->frequencyAt(i) > 6000.0) break;
      if (plot->responseDbAt(i) > up_db) {
        up_db = plot->responseDbAt(i);
        up_hz = plot->frequencyAt(i);
      }
    }
    QVERIFY(std::abs(std::log2(up_hz / peak_hz) - 1.0) < 0.08);
    QCOMPARE(window.body().native_bytes(), bytes);
    QCOMPARE(window.undoStack()->count(), 0);
    transpose->setValue(0);
    QVERIFY(!plot->tokens().empty());
  }

  void transposeKeepsTheViewedCascadeAtUnityDc() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.applyVowel(QStringLiteral("REZ dead_ringer c1"));
    const auto bytes = window.body().native_bytes();
    for (const int semitones : {-36, -12, 7, 24, 36}) {
      window.document()->setTranspose(semitones);
      const auto dc = trench::core::cascade_response_db(
          window.document()->viewCascade(), 0.0, trench::core::kP2kDatumHz);
      QVERIFY2(std::abs(dc) < 1.0e-9,
               qPrintable(QStringLiteral("%1 st: %2 dB").arg(semitones).arg(dc)));
      QCOMPARE(window.body().native_bytes(), bytes);
    }
  }

  void interiorViewKeepsTheViewedCascadeAtUnityDc() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto bytes = window.body().native_bytes();
    window.document()->setView(0.37F, 0.62F);
    const auto dc = trench::core::cascade_response_db(
        window.document()->viewCascade(), 0.0, trench::core::kP2kDatumHz);
    QVERIFY2(std::abs(dc) < 1.0e-9, qPrintable(QString::number(dc)));
    QCOMPARE(window.body().native_bytes(), bytes);
    QCOMPARE(window.undoStack()->count(), 0);
  }

  void plotAndDocumentShareTheTransposedCascade() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    window.document()->setTranspose(12);
    auto* plot = window.responsePlot();
    const auto cascade = window.document()->viewCascade();
    for (std::size_t index = 0; index < plot->responsePointCount(); ++index) {
      const auto expected = trench::core::cascade_response_db(
          cascade, plot->frequencyAt(index), trench::core::kP2kDatumHz);
      QVERIFY(std::abs(plot->responseDbAt(index) - expected) < 1.0e-9);
    }
  }

  void hoveringAPlotTokenHighlightsItsSection() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    const auto token = find_token(plot, 1, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    move(plot, token->position);
    QCOMPARE(plot->highlightedSection(), std::optional<std::size_t>{1});
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(plot, &leave);
    QCOMPARE(plot->highlightedSection(), std::optional<std::size_t>{});
  }

  void aVowelWritesTypedRowsAsOneUndoEntry() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto before = window.body().native_bytes();
    window.applyVowel(QStringLiteral("aa"));
    QVERIFY(window.body().native_bytes() != before);
    QCOMPARE(window.undoStack()->count(), 1);
    std::size_t eq = 0;
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      const auto param = p2k::param_of(window.body().words[0][section], trench::core::kP2kDatumHz);
      eq += param.type == p2k::SectionType::kEq ? 1U : 0U;
    }
    QCOMPARE(eq, std::size_t{3});
    window.undoStack()->undo();
    QCOMPARE(window.body().native_bytes(), before);
  }

  void aManualRecipeWritesItsRowsThroughTheSameChooser() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto before = window.body().native_bytes();
    window.applyVowel(QStringLiteral("comb 8ve"));
    QVERIFY(window.body().native_bytes() != before);
    QCOMPARE(window.undoStack()->count(), 1);
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      const auto param = p2k::param_of(window.body().words[0][section], trench::core::kP2kDatumHz);
      QCOMPARE(param.type, p2k::SectionType::kEq);
      QVERIFY(param.gain_db < -12.0);
    }
  }

  void aPostureWritesOnlyThePoleHalfOfItsRows() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto* skeleton = p2k::posture("VOW ooh_to_eee c1");
    QVERIFY(skeleton != nullptr);
    const auto before = window.body().words[0];
    window.applyVowel(QStringLiteral("VOW ooh_to_eee c1"));
    QCOMPARE(window.undoStack()->count(), 1);
    const auto after = window.body().words[0];
    for (std::size_t index = 0; index < skeleton->pole_count; ++index) {
      const auto geometry = trench::core::geometry_from_words(
          after[skeleton->rows[index]], trench::core::kP2kDatumHz);
      const auto* pole = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
      QVERIFY(pole != nullptr);
      QVERIFY(std::abs(pole->hz - skeleton->poles[index].hz) <=
              0.01 * skeleton->poles[index].hz);
    }
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(after[section][0], trench::core::kIdentitySection[0]);
      QCOMPARE(after[section][1], trench::core::kIdentitySection[1]);
    }
    QVERIFY(std::abs(corner_dc_db(after)) < 0.5);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
  }

  void aPosturePickDisplaysItsPoleCascade() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("VOW ooh_to_eee c1"));
    auto* plot = window.responsePlot();
    const auto cascade = window.document()->viewCascade();
    QCOMPARE(plot->responsePointCount(), window.document()->grid().hz.size());
    for (std::size_t index = 0; index < plot->responsePointCount(); ++index) {
      const auto expected = trench::core::cascade_response_db(
          cascade, plot->frequencyAt(index), trench::core::kP2kDatumHz);
      QVERIFY(std::abs(plot->responseDbAt(index) - expected) < 1.0e-9);
    }
    for (const auto& row : window.body().words[0]) {
      QCOMPARE(row[0], trench::core::kIdentitySection[0]);
      QCOMPARE(row[1], trench::core::kIdentitySection[1]);
    }
    QVERIFY(window.grab().save(QString(TRENCH_SOURCE_ROOT) +
                               "/dev/e2e/app_pole_cascade.png"));
  }

  void aRingDragAfterATemplateKeepsTheCornerAtUnity() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("VOW ooh_to_eee c1"));
    QTest::qWait(20);
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);

    window.selectSection(1);
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    std::optional<ArmadilloView::Marker> ring;
    for (const auto& marker : armadillo->markers()) {
      if (marker.zero && marker.section == 1) ring = marker;
    }
    QVERIFY(ring.has_value());
    drag(armadillo, ring->position,
         QPointF{armadillo->xForFrequency(12000.0), armadillo->yForRadius(0.9)}, 8);
    QTest::qWait(20);
    QVERIFY2(std::abs(corner_dc_db(window.body().words[0])) < 0.5,
             qPrintable(QString::number(corner_dc_db(window.body().words[0]))));

    const auto again = armadillo_marker(armadillo, 1, true);
    QVERIFY(again.has_value());
    drag(armadillo, again->position,
         QPointF{armadillo->xForFrequency(60.0), again->position.y()}, 8);
    QTest::qWait(20);
    QVERIFY2(std::abs(corner_dc_db(window.body().words[0])) < 0.5,
             qPrintable(QString::number(corner_dc_db(window.body().words[0]))));
  }

  void aZeroDragUpdatesLevelWithoutMeterWrites() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    window.applyVowel(QStringLiteral("VOW ooh_to_eee c1"));
    window.undoStack()->clear();
    window.selectSection(1, ResponsePlotWidget::Lane::kZero);
    QTest::qWait(20);

    auto* armadillo = window.armadilloView();
    std::optional<ArmadilloView::Marker> ring;
    for (const auto& marker : armadillo->markers()) {
      if (marker.zero && marker.section == 1) ring = marker;
    }
    QVERIFY(ring.has_value());
    const auto before_level = window.chassisBar()->powerDb();
    QVERIFY(std::abs(before_level - grid_power_db(*window.document())) < 1.0e-9);
    QSignalSpy writes(window.document(), &BodyDocument::bodyChanged);

    const auto start = ring->position;
    const auto end = QPointF{armadillo->xForFrequency(2500.0),
                             armadillo->yForRadius(0.92)};
    press(armadillo, start);
    move(armadillo, end);
    const auto writes_after_edit = writes.count();
    QVERIFY(writes_after_edit > 0);
    const auto during_level = window.chassisBar()->powerDb();
    QVERIFY(std::abs(during_level - before_level) > 1.0e-6);
    QVERIFY(std::abs(during_level - grid_power_db(*window.document())) < 1.0e-9);
    QCoreApplication::processEvents();
    QCOMPARE(writes.count(), writes_after_edit);
    release(armadillo, end);

    QCOMPARE(window.undoStack()->count(), 1);
    QVERIFY(corner_scales_are_one_gain(window.body().words[0]));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QVERIFY(window.grab().save(QString(TRENCH_SOURCE_ROOT) +
                               "/dev/e2e/app_zero_level.png"));
  }

  void clickingAPostureRowWritesPoleWordsAndMarksTheRow() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* list = window.postureList();
    QVERIFY(list != nullptr);
    const auto name = QStringLiteral("LPF klub_klassik c2");
    const auto row = list->rowOf(name);
    QVERIFY(row >= 0);
    list->scrollToItem(list->item(row));
    const auto before = window.body().words[0];
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
                      list->visualItemRect(list->item(row)).center());
    const auto after = window.body().words[0];
    QVERIFY(after != before);
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(after[section][0], trench::core::kIdentitySection[0]);
      QCOMPARE(after[section][1], trench::core::kIdentitySection[1]);
    }
    QVERIFY(std::abs(corner_dc_db(after)) < 0.5);
    QVERIFY(window.grab().save(QString(TRENCH_SOURCE_ROOT) +
                               "/dev/e2e/app_posture_dc.png"));
    QCOMPARE(list->matched(), name);
    QVERIFY(!list->hasFocus());
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
    QVERIFY(list->matched() != name);
  }

  void aMouthTemplateWritesSixAscendingPoleRowsWithParkedZeros() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto name = QStringLiteral("s1 bahn a");
    const auto* skeleton = p2k::posture("s1 bahn a");
    QVERIFY(skeleton != nullptr);
    QCOMPARE(skeleton->pole_count, std::size_t{6});
    QCOMPARE(skeleton->type, std::string_view{"MOUTHS S1"});
    const auto before = window.body().words[0];
    window.applyVowel(name);
    QCOMPARE(window.undoStack()->count(), 1);
    const auto after = window.body().words[0];
    const std::array<std::size_t, 6> rows{1, 2, 3, 4, 0, 5};
    double previous_hz = 0.0;
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto section = rows[index];
      const auto geometry =
          trench::core::geometry_from_words(after[section], trench::core::kP2kDatumHz);
      const auto* pole = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
      QVERIFY(pole != nullptr);
      QVERIFY(std::abs(pole->hz - skeleton->poles[index].hz) <= 0.02 * skeleton->poles[index].hz);
      QVERIFY(pole->hz > previous_hz);
      previous_hz = pole->hz;
      QCOMPARE(after[section][0], trench::core::kIdentitySection[0]);
      QCOMPARE(after[section][1], trench::core::kIdentitySection[1]);
    }
    QVERIFY(std::abs(corner_dc_db(after)) < 0.5);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
  }

  void aCubeTemplateWritesAscendingPoleRowsWithParkedZeros() {
    namespace p2k = trench::core::p2k;
    const p2k::Posture* skeleton = nullptr;
    for (const auto& candidate : p2k::templates()) {
      if (candidate.type == std::string_view{"CUBES"}) {
        skeleton = &candidate;
        break;
      }
    }
    QVERIFY(skeleton != nullptr);
    QVERIFY(skeleton->pole_count >= 2);
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    const auto name = QString::fromUtf8(skeleton->name.data(),
                                        static_cast<int>(skeleton->name.size()));
    const auto before = window.body().words[0];
    window.applyVowel(name);
    QCOMPARE(window.undoStack()->count(), 1);
    const auto after = window.body().words[0];
    const auto poles = p2k::pole_words_from_posture(*skeleton);
    QCOMPARE(poles.size(), skeleton->pole_count);
    double previous_hz = 0.0;
    for (std::size_t index = 0; index < poles.size(); ++index) {
      const auto section = poles[index].row;
      const auto geometry =
          trench::core::geometry_from_words(after[section], trench::core::kP2kDatumHz);
      const auto* pole = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
      QVERIFY(pole != nullptr);
      QVERIFY(std::abs(pole->hz - skeleton->poles[index].hz) <= 0.02 * skeleton->poles[index].hz);
      QVERIFY(pole->hz > previous_hz);
      previous_hz = pole->hz;
      QCOMPARE(after[section][0], trench::core::kIdentitySection[0]);
      QCOMPARE(after[section][1], trench::core::kIdentitySection[1]);
    }
    QVERIFY(std::abs(corner_dc_db(after)) < 0.5);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
  }

  void theListIsTheBankPosturesAndTheCompiledVowelCorners() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    auto* list = window.postureList();
    QVERIFY(list != nullptr);
    QStringList rows;
    for (int row = 0; row < list->count(); ++row) {
      const auto name = list->item(row)->data(Qt::UserRole).toString();
      rows.push_back(name.isEmpty() ? QStringLiteral("[") + list->item(row)->text() +
                                          QStringLiteral("]")
                                    : name);
    }
    const QStringList expected{QStringLiteral("[LPF]"),
                               QStringLiteral("LPF millennium c1"),
                               QStringLiteral("LPF klub_klassik c2"),
                               QStringLiteral("[EQ+]"),
                               QStringLiteral("EQ+ tb_or_not_tb c1"),
                               QStringLiteral("EQ+ dj_alkaline c0"),
                               QStringLiteral("[VOW]"),
                               QStringLiteral("VOW ooh_to_eee c1"),
                               QStringLiteral("VOW talking_hedz c0"),
                               QStringLiteral("VOW AahAyEeh M0Q0"),
                               QStringLiteral("VOW AahAyEeh M100Q0"),
                               QStringLiteral("VOW AahAyEeh M0Q100"),
                               QStringLiteral("VOW AahAyEeh M100Q100"),
                               QStringLiteral("VOW OohToAah M0Q0"),
                               QStringLiteral("VOW OohToAah M100Q0"),
                               QStringLiteral("VOW OohToAah M0Q100"),
                               QStringLiteral("VOW OohToAah M100Q100"),
                               QStringLiteral("[PHA]"),
                               QStringLiteral("PHA cruz_pusher c1"),
                               QStringLiteral("[REZ]"),
                               QStringLiteral("REZ dead_ringer c1"),
                               QStringLiteral("[DST]"),
                               QStringLiteral("DST fuzzi_face c0")};
    QCOMPARE(rows, expected);
  }

  void tuneInSurvivesMorphAndCornerMoves() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* strip = window.morphStrip();
    auto* transpose = strip->findChild<QSlider*>(QStringLiteral("transposeSlider"));
    auto* morph = strip->findChild<QSlider*>(QStringLiteral("morphSlider"));
    auto* q = strip->findChild<QSlider*>(QStringLiteral("qSlider"));
    QVERIFY(transpose != nullptr && morph != nullptr && q != nullptr);
    transpose->setValue(7);
    QCOMPARE(window.document()->view().semitones, 7.0);
    QCOMPARE(window.document()->cornerTranspose(0), 7);
    morph->setValue(500);
    QVERIFY(std::abs(window.document()->view().semitones - 3.5) < 1.0e-9);
    window.setCorner(1);
    QCOMPARE(window.document()->view().semitones, 0.0);
    transpose->setValue(-5);
    QCOMPARE(window.document()->view().semitones, -5.0);
    window.setCorner(0);
    QCOMPARE(window.document()->view().semitones, 7.0);
    QCOMPARE(transpose->value(), 7);
    morph->setValue(500);
    QVERIFY(std::abs(window.document()->view().semitones - 1.0) < 1.0e-9);
    QCOMPARE(window.document()->cornerTranspose(0), 7);
    QCOMPARE(window.document()->cornerTranspose(1), -5);
    q->setValue(0);
    window.setCorner(0);
    const auto shifted = peak_hz_below(window.document()->viewResponseDb(),
                                       window.document()->grid().hz, 6000.0);
    transpose->setValue(0);
    QCOMPARE(window.document()->view().semitones, 0.0);
    const auto plain = peak_hz_below(window.document()->viewResponseDb(),
                                     window.document()->grid().hz, 4000.0);
    QVERIFY(std::abs(std::log2(shifted / plain) - 7.0 / 12.0) < 0.08);
  }

  void undoRestoresAZeroEdit() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    window.selectSection(1);
    QTest::qWait(20);
    const auto ring = armadillo_marker(window.armadilloView(), 1, true);
    QVERIFY(ring.has_value());
    const auto before = window.body().native_bytes();
    drag(window.armadilloView(), ring->position,
         ring->position + QPointF{-90.0, 10.0}, 6);
    QTest::qWait(20);
    QVERIFY(window.body().native_bytes() != before);
    QCOMPARE(window.undoStack()->count(), 1);
    QAction* undo_action = nullptr;
    for (auto* action : window.findChildren<QAction*>()) {
      if (action->shortcut() == QKeySequence(QKeySequence::Undo)) undo_action = action;
    }
    QVERIFY(undo_action != nullptr);
    QVERIFY(undo_action->isEnabled());
    window.undoStack()->undo();
    QCOMPARE(window.body().native_bytes(), before);
  }

  void keepingACornerAddsALitMineRowThatPersists() {
    namespace p2k = trench::core::p2k;
    const auto previous = UserPostures::baseDir();
    const auto temp = QDir::tempPath() + QStringLiteral("/trench_user_postures_keep");
    QDir(temp).removeRecursively();
    UserPostures::setBaseDir(temp);

    std::vector<UserPostures::Pole> kept;
    {
      MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
      window.resize(960, 540);
      window.show();
      QTest::qWait(20);
      auto* keep = window.findChild<QAbstractButton*>(QStringLiteral("keepPosture"));
      QVERIFY(keep != nullptr);
      QVERIFY(keep->isEnabled());
      const auto undo_before = window.undoStack()->count();
      keep->click();
      QCOMPARE(window.undoStack()->count(), undo_before);

      auto* list = window.postureList();
      QCOMPARE(list->item(0)->text(), QStringLiteral("MINE"));
      QCOMPARE(list->item(1)->data(Qt::UserRole).toString(), QStringLiteral("mine 1"));
      QCOMPARE(list->matched(), QStringLiteral("mine 1"));

      const auto corner = window.body().words[0];
      for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
        if (!p2k::pole_of(corner[section], trench::core::kP2kDatumHz)) continue;
        kept.push_back({section, corner[section][2], corner[section][3]});
      }
      QVERIFY(!kept.empty());
    }

    UserPostures store;
    const auto* entry = store.find(QStringLiteral("mine 1"));
    QVERIFY(entry != nullptr);
    QCOMPARE(entry->poles.size(), kept.size());
    for (std::size_t index = 0; index < kept.size(); ++index) {
      QCOMPARE(entry->poles[index].row, kept[index].row);
      QCOMPARE(entry->poles[index].mag, kept[index].mag);
      QCOMPARE(entry->poles[index].rsq, kept[index].rsq);
    }

    MainWindow reopened(fixture_path(), trench::core::kP2kDatumHz);
    QCOMPARE(reopened.postureList()->item(1)->data(Qt::UserRole).toString(),
             QStringLiteral("mine 1"));

    QDir(temp).removeRecursively();
    UserPostures::setBaseDir(previous);
  }

  void aMineRowWritesItsPoleWordsWithParkedZeros() {
    namespace p2k = trench::core::p2k;
    const auto previous = UserPostures::baseDir();
    const auto temp = QDir::tempPath() + QStringLiteral("/trench_user_postures_apply");
    QDir(temp).removeRecursively();
    UserPostures::setBaseDir(temp);
    {
      MainWindow keeper(fixture_path(), trench::core::kP2kDatumHz);
      auto* keep = keeper.findChild<QAbstractButton*>(QStringLiteral("keepPosture"));
      QVERIFY(keep != nullptr);
      keep->click();
    }
    UserPostures store;
    const auto* entry = store.find(QStringLiteral("mine 1"));
    QVERIFY(entry != nullptr);

    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* list = window.postureList();
    const auto row = list->rowOf(QStringLiteral("mine 1"));
    QVERIFY(row >= 0);
    const auto before = window.body().words[0];
    list->scrollToItem(list->item(row));
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
                      list->visualItemRect(list->item(row)).center());
    const auto after = window.body().words[0];
    QCOMPARE(window.undoStack()->count(), 1);
    QCOMPARE(list->matched(), QStringLiteral("mine 1"));
    for (const auto& pole : entry->poles) {
      const auto expected = p2k::words_with_parked_zero(
          {before[pole.row][0], before[pole.row][1], pole.mag, pole.rsq}, pole.row,
          trench::core::kP2kDatumHz);
      for (std::size_t word = 0; word < expected.size(); ++word) {
        QCOMPARE(after[pole.row][word], expected[word]);
      }
    }
    QVERIFY(std::abs(corner_dc_db(after)) < 0.5);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);

    QDir(temp).removeRecursively();
    UserPostures::setBaseDir(previous);
  }

  void theResponsePlotIsStillAtRest() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(120);
    auto* plot = window.responsePlot();
    const auto first = plot->grab().toImage();
    PaintCounter counter;
    plot->installEventFilter(&counter);
    QTest::qWait(300);
    plot->removeEventFilter(&counter);
    QCOMPARE(counter.count, 0);
    const auto second = plot->grab().toImage();
    QVERIFY(first == second);
  }

  void anOverlayLeavesTheDocumentAlone() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    const auto before = window.body();
    const auto undo_before = window.undoStack()->count();
    const auto* skeleton = trench::core::p2k::posture("s1 bahn a");
    QVERIFY(skeleton != nullptr);

    auto* action = overlay_action(plot->overlayMenu(), QStringLiteral("s1 bahn a"));
    QVERIFY(action != nullptr);
    action->trigger();
    QTest::qWait(20);

    QCOMPARE(plot->overlay(), QStringLiteral("s1 bahn a"));
    QCOMPARE(plot->overlayGhostCount(), skeleton->pole_count);
    for (std::size_t corner = 0; corner < trench::core::kLegacyCornerCount; ++corner) {
      QCOMPARE(window.body().words[corner], before.words[corner]);
    }
    QCOMPARE(window.undoStack()->count(), undo_before);

    auto* none = overlay_action(plot->overlayMenu(), QStringLiteral("none"));
    QVERIFY(none != nullptr);
    none->trigger();
    QTest::qWait(20);
    QCOMPARE(plot->overlayGhostCount(), std::size_t{0});
    QVERIFY(plot->overlay().isEmpty());
    QCOMPARE(window.undoStack()->count(), undo_before);
  }

  void theAxisWindowIgnoresTheOverlay() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    const auto before = plot->dbRange();

    auto* action = overlay_action(plot->overlayMenu(), QStringLiteral("s2 bahn a"));
    QVERIFY(action != nullptr);
    action->trigger();
    QTest::qWait(20);

    QVERIFY(plot->overlayGhostCount() > 0);
    const auto after = plot->dbRange();
    QCOMPARE(after.first, before.first);
    QCOMPARE(after.second, before.second);
  }

  void theOverlayPickerListsNoneAndEveryTemplateGroup() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    QVERIFY(plot->overlayPicker() != nullptr);
    const auto actions = plot->overlayMenu()->actions();
    QVERIFY(!actions.isEmpty());
    QCOMPARE(actions.front()->text(), QStringLiteral("none"));
    QCOMPARE(actions.front()->menu(), nullptr);

    QStringList groups;
    for (const auto& skeleton : trench::core::p2k::templates()) {
      const auto type = QString::fromUtf8(skeleton.type.data(),
                                          static_cast<int>(skeleton.type.size()));
      if (groups.isEmpty() || groups.back() != type) groups.push_back(type);
    }
    QCOMPARE(actions.size(), groups.size() + 1);
    for (int index = 0; index < groups.size(); ++index) {
      auto* entry = actions.at(index + 1);
      QVERIFY(entry->menu() != nullptr);
      QCOMPARE(entry->text(), groups.at(index));
      QVERIFY(!entry->menu()->actions().isEmpty());
    }
  }

  void aSelectedPoleExposesItsBell() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto pole = conjugate_of(window.body(), 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(pole.has_value());
    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    press(plot, token->position);
    QTest::qWait(20);

    const auto exposed = plot->exposedDb();
    QCOMPARE(exposed.size(), plot->responsePointCount());
    const auto peak_hz = extreme_hz(plot, exposed, true);
    QVERIFY2(std::abs(peak_hz / pole->hz - 1.0) < 0.02,
             qPrintable(QStringLiteral("%1 %2").arg(peak_hz).arg(pole->hz)));
  }

  void aSelectedZeroExposesItsNotch() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    std::optional<trench::core::ConjugatePair> zero;
    std::size_t chosen = 0;
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      const auto candidate = conjugate_of(window.body(), section,
                                          ResponsePlotWidget::Lane::kZero);
      if (!candidate || candidate->radius < 0.97) continue;
      if (candidate->hz < 500.0 || candidate->hz > 8000.0) continue;
      zero = candidate;
      chosen = section;
      break;
    }
    QVERIFY(zero.has_value());
    window.selectSection(chosen, ResponsePlotWidget::Lane::kZero);
    QTest::qWait(20);

    const auto exposed = plot->exposedDb();
    QCOMPARE(exposed.size(), plot->responsePointCount());
    const auto notch_hz = extreme_hz(plot, exposed, false);
    QVERIFY2(std::abs(notch_hz / zero->hz - 1.0) < 0.02,
             qPrintable(QStringLiteral("%1 %2 %3").arg(chosen).arg(notch_hz).arg(zero->hz)));
  }

  void everyLiveSectionDrawsItsPrimitive() {
    MainWindow window(tb303_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    std::size_t live = 0;
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      if (conjugate_of(window.body(), section, ResponsePlotWidget::Lane::kPole)) ++live;
    }
    QVERIFY(live > 0);
    QCOMPARE(plot->primitiveCount(), live);
    QCOMPARE(plot->exposedDb().size(), plot->responsePointCount());
  }

  void aDeadSectionExposesNothing() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    QVERIFY(plot->tokens().empty());
    QVERIFY(plot->exposedDb().empty());
    QCOMPARE(plot->primitiveCount(), std::size_t{0});

    window.selectSection(3, ResponsePlotWidget::Lane::kZero);
    QTest::qWait(20);
    QVERIFY(plot->exposedDb().empty());
  }

  void aCornerPadSwitchesTheEditedCorner() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* first = window.morphStrip()->findChild<QPushButton*>(QStringLiteral("cornerPad1"));
    auto* third = window.morphStrip()->findChild<QPushButton*>(QStringLiteral("cornerPad3"));
    QVERIFY(first != nullptr);
    QVERIFY(third != nullptr);
    QVERIFY(first->isChecked());
    QTest::mouseClick(third, Qt::LeftButton);
    QCOMPARE(window.document()->corner(), std::size_t{2});
    QCOMPARE(window.document()->view().morph, 0.0F);
    QCOMPARE(window.document()->view().q, 1.0F);
    QVERIFY(third->isChecked());
    QVERIFY(!first->isChecked());
    QCOMPARE(window.undoStack()->count(), 0);
  }

  void theSaveVerbWritesTheBodyFile() {
    const auto path = std::filesystem::temp_directory_path() / "trench_save_verb.body240";
    std::filesystem::copy_file(fixture_path(), path,
                               std::filesystem::copy_options::overwrite_existing);
    MainWindow window(path, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    std::filesystem::remove(path);
    auto* chassis = window.chassisBar();
    std::optional<QPointF> save_center;
    for (const auto& pad : chassis->pads()) {
      if (pad.verb == ChassisBar::Verb::kSave) {
        QVERIFY(pad.available);
        save_center = pad.rect.center();
      }
    }
    QVERIFY(save_center.has_value());
    QTest::mouseClick(chassis, Qt::LeftButton, Qt::KeyboardModifiers(),
                      save_center->toPoint());
    QTest::qWait(20);
    QVERIFY(std::filesystem::exists(path));
    std::array<char, trench::core::kLegacyBodyBytes> bytes{};
    {
      std::ifstream stream(path, std::ios::binary);
      stream.read(bytes.data(), bytes.size());
    }
    const auto expected = window.body().legacy_bytes();
    QVERIFY(std::equal(expected.begin(), expected.end(), bytes.begin(),
                       [](std::uint8_t lhs, char rhs) {
                         return lhs == static_cast<std::uint8_t>(rhs);
                       }));
    std::filesystem::remove(path);
  }

  void aFilterIsCreatedFromTheEmptyAppByGesturesAlone() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    auto* armadillo = window.armadilloView();
    QVERIFY(plot->tokens().empty());

    double_click(armadillo, QPointF{armadillo->xForFrequency(800.0),
                                    armadillo->yForRadius(0.95)});
    QTest::qWait(20);
    QVERIFY(p2k::param_of(window.body().words[0][0], trench::core::kP2kDatumHz).type !=
            p2k::SectionType::kOff);
    QVERIFY(find_token(plot, 0, ResponsePlotWidget::Lane::kPole).has_value());
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(window.undoStack()->count(), 1);

    auto* list = window.postureList();
    const auto row = list->rowOf(QStringLiteral("VOW ooh_to_eee c1"));
    QVERIFY(row >= 0);
    list->scrollToItem(list->item(row));
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
                      list->visualItemRect(list->item(row)).center());
    QTest::qWait(20);
    QVERIFY(!plot->tokens().empty());
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);

    window.selectSection(1, ResponsePlotWidget::Lane::kZero);
    QTest::qWait(20);
    std::optional<ArmadilloView::Marker> ring;
    for (const auto& marker : armadillo->markers()) {
      if (marker.zero && marker.section == 1) ring = marker;
    }
    QVERIFY(ring.has_value());
    const auto zero_before = window.body().words[0][1];
    drag(armadillo, ring->position,
         QPointF{armadillo->xForFrequency(1200.0), armadillo->yForRadius(0.9)}, 8);
    QTest::qWait(20);
    QVERIFY(window.body().words[0][1] != zero_before);
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);

    auto* second = window.morphStrip()->findChild<QPushButton*>(QStringLiteral("cornerPad2"));
    QVERIFY(second != nullptr);
    QTest::mouseClick(second, Qt::LeftButton);
    QCOMPARE(window.document()->corner(), std::size_t{1});
    const auto corner0 = window.body().words[0];
    double_click(armadillo, QPointF{armadillo->xForFrequency(1500.0),
                                    armadillo->yForRadius(0.95)});
    QTest::qWait(20);
    QVERIFY(p2k::param_of(window.body().words[1][0], trench::core::kP2kDatumHz).type !=
            p2k::SectionType::kOff);
    QCOMPARE(window.body().words[0], corner0);

    QVERIFY(window.grab().save(QString(TRENCH_SOURCE_ROOT) +
                               "/dev/e2e/app_created_from_empty.png"));
  }

  void ctrlClickingAPadCopiesTheCurrentCornerThere() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto source = window.body().words[0];
    const auto before = window.body().words[1];
    QVERIFY(source != before);
    auto* second = window.morphStrip()->findChild<QPushButton*>(QStringLiteral("cornerPad2"));
    QVERIFY(second != nullptr);
    QTest::mouseClick(second, Qt::LeftButton, Qt::ControlModifier);
    QCOMPARE(window.document()->corner(), std::size_t{1});
    QCOMPARE(window.body().words[1], source);
    QCOMPARE(window.body().words[0], source);
    QCOMPARE(window.undoStack()->count(), 1);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[1], before);
  }

  void theChooserListsTheCompiledVowelCorners() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* list = window.postureList();
    const auto name = QStringLiteral("VOW OohToAah M0Q0");
    const auto row = list->rowOf(name);
    QVERIFY(row >= 0);
    list->scrollToItem(list->item(row));
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
                      list->visualItemRect(list->item(row)).center());
    QTest::qWait(20);
    QVERIFY(!window.responsePlot()->tokens().empty());
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(list->matched(), name);
  }

  void theArmadilloShowsEverySectionsRoots() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    QVERIFY(armadillo != nullptr);
    QVERIFY(armadillo->height() > window.responsePlot()->height());
    std::size_t poles = 0;
    std::size_t zeros = 0;
    for (const auto& marker : armadillo->markers()) {
      if (marker.zero) {
        ++zeros;
      } else {
        ++poles;
      }
    }
    QCOMPARE(poles, std::size_t{6});
    QCOMPARE(zeros, std::size_t{6});
    QVERIFY(window.grab().save(QString(TRENCH_SOURCE_ROOT) +
                               "/dev/e2e/app_armadillo_primary.png"));
  }

  void draggingAPoleOnTheArmadilloWritesItsWords() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    std::optional<ArmadilloView::Marker> pole;
    for (const auto& marker : armadillo->markers()) {
      if (!marker.zero && marker.section == 1 && !marker.real) pole = marker;
    }
    QVERIFY(pole.has_value());
    const auto target = QPointF{armadillo->xForFrequency(1000.0),
                                armadillo->yForRadius(0.95)};
    drag(armadillo, pole->position, target, 8);
    QTest::qWait(20);
    const auto geometry = trench::core::geometry_from_words(window.body().words[0][1],
                                                            trench::core::kP2kDatumHz);
    const auto* written = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
    QVERIFY(written != nullptr);
    QVERIFY2(std::abs(std::log2(written->hz / 1000.0)) < 0.06,
             qPrintable(QString::number(written->hz)));
    QVERIFY2(std::abs(written->radius - 0.95) < 0.01,
             qPrintable(QString::number(written->radius)));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(window.undoStack()->count(), 1);
  }

  void doubleClickOnTheArmadilloPlacesAPolePairAtDepth() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    QVERIFY(armadillo->markers().empty());
    double_click(armadillo, QPointF{armadillo->xForFrequency(2000.0),
                                    armadillo->yForRadius(0.97)});
    QTest::qWait(20);
    const auto geometry = trench::core::geometry_from_words(window.body().words[0][0],
                                                            trench::core::kP2kDatumHz);
    const auto* placed = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
    QVERIFY(placed != nullptr);
    QVERIFY2(std::abs(std::log2(placed->hz / 2000.0)) < 0.06,
             qPrintable(QString::number(placed->hz)));
    QVERIFY2(std::abs(placed->radius - 0.97) < 0.01,
             qPrintable(QString::number(placed->radius)));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(window.undoStack()->count(), 1);
  }

  void aGhostZeroAtTheEdgeDragsIntoALiveZero() {
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    double_click(armadillo, QPointF{armadillo->xForFrequency(800.0),
                                    armadillo->yForRadius(0.96)});
    QTest::qWait(20);
    std::optional<ArmadilloView::Marker> ghost;
    for (const auto& marker : armadillo->markers()) {
      if (marker.zero && marker.ghost) ghost = marker;
    }
    QVERIFY(ghost.has_value());
    QCOMPARE(ghost->section, std::size_t{0});
    drag(armadillo, ghost->position,
         QPointF{armadillo->xForFrequency(3000.0), armadillo->yForRadius(0.9)}, 8);
    QTest::qWait(20);
    const auto geometry = trench::core::geometry_from_words(window.body().words[0][0],
                                                            trench::core::kP2kDatumHz);
    const auto* zero = std::get_if<trench::core::ConjugatePair>(&geometry.zero);
    QVERIFY(zero != nullptr);
    QVERIFY2(std::abs(std::log2(zero->hz / 3000.0)) < 0.06,
             qPrintable(QString::number(zero->hz)));
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(window.undoStack()->count(), 2);
  }

  void draggingAZeroOffTheRightEdgeParksIt() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* armadillo = window.armadilloView();
    std::optional<ArmadilloView::Marker> zero;
    for (const auto& marker : armadillo->markers()) {
      if (marker.zero && marker.section == 1 && !marker.real && !marker.ghost) {
        zero = marker;
      }
    }
    QVERIFY(zero.has_value());
    const auto pole_words = std::array<std::uint16_t, 2>{
        window.body().words[0][1][2], window.body().words[0][1][3]};
    drag(armadillo, zero->position,
         QPointF{static_cast<double>(armadillo->width()) + 30.0, zero->position.y()}, 8);
    QTest::qWait(20);
    QCOMPARE(window.body().words[0][1][0], trench::core::kIdentitySection[0]);
    QCOMPARE(window.body().words[0][1][1], trench::core::kIdentitySection[1]);
    QCOMPARE(window.body().words[0][1][2], pole_words[0]);
    QCOMPARE(window.body().words[0][1][3], pole_words[1]);
    QVERIFY(std::abs(corner_dc_db(window.body().words[0])) < 0.5);
    QCOMPARE(window.undoStack()->count(), 1);
  }

};

int main(int argc, char* argv[]) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
  }
  QApplication application(argc, argv);
  MainWindowTest tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "main_window_test.moc"
