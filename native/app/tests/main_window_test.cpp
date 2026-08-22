#include "chassis_bar.hpp"
#include "fit_room.hpp"
#include "main_window.hpp"
#include "morph_strip.hpp"
#include "response_plot.hpp"
#include "section_strip.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/section_param.hpp"

#include <QTest>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QPointer>
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

std::filesystem::path fixture_path() {
  return std::filesystem::path(TRENCH_SOURCE_ROOT) / "ref/presets/P2k_013_talking_hedz.bin";
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
    if (param.type == trench::core::p2k::SectionType::kEq) return static_cast<int>(section);
  }
  return -1;
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

trench::core::p2k::PackedCorner flatten_first_corner(const trench::core::PackedBody& body) {
  trench::core::p2k::PackedCorner out{};
  for (std::size_t section = 0; section < trench::core::p2k::kStageCount; ++section) {
    for (std::size_t word = 0; word < trench::core::p2k::kWordCount; ++word) {
      out[section * trench::core::p2k::kWordCount + word] = body.words[0][section][word];
    }
  }
  return out;
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

}  // namespace

class MainWindowTest final : public QObject {
  Q_OBJECT

 private slots:
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
    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    QVERIFY(token->live);

    drag(plot, token->position, token->position + QPointF{-70.0, 22.0}, 5);

    const auto& after = window.body().words[0][0];
    QVERIFY(after != before);
    QVERIFY(is_lattice_word(after[2]));
    QVERIFY(is_lattice_word(after[3]));
    const auto [p, q] = trench::core::p2k::pq(after[2], after[3]);
    QVERIFY(trench::core::p2k::is_legal(p, q, true));
    QCOMPARE(after[4], before[4]);
    QCOMPARE(after[0], before[0]);
    QCOMPARE(after[1], before[1]);
    QCOMPARE(window.body().words[4][0], after);
  }

  void oneGestureIsOneUndoEntry() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    drag(plot, token->position, token->position + QPointF{-90.0, 30.0}, 9);

    QCOMPARE(window.undoStack()->count(), 1);
  }

  void undoRestoresExactBytes() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto before_native = window.body().native_bytes();
    const auto before_legacy = window.body().legacy_bytes();

    const auto token = find_token(plot, 1, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    drag(plot, token->position, token->position + QPointF{60.0, 25.0}, 5);
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
    drag(plot, token->position, token->position + QPointF{-80.0, 26.0}, 5);
    QCOMPARE(window.body().words[0][2], before);
    QCOMPARE(window.undoStack()->count(), 0);

    press(plot, token->position);
    release(plot, token->position);
    QCOMPARE(window.freedomMask() & bit, bit);
  }

  void s6ZeroRadiusWordIsUntouched() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    QCOMPARE(window.body().words[0][5][1], trench::core::p2k::kS6ZeroRsqWord);
    window.selectSection(5);
    const auto token = find_token(plot, 5, ResponsePlotWidget::Lane::kZero);
    QVERIFY(token.has_value());
    QVERIFY(token->live);

    const auto before = window.body().words[0][5][0];
    const auto before_mirror = window.body().words[4][5][0];
    drag(plot, token->position, token->position + QPointF{0.0, 60.0}, 6);
    QCOMPARE(window.body().words[0][5][0], before);
    QCOMPARE(window.body().words[4][5][0], before_mirror);
    QCOMPARE(window.body().words[0][5][1], trench::core::p2k::kS6ZeroRsqWord);
    QCOMPARE(window.body().words[4][5][1], trench::core::p2k::kS6ZeroRsqWord);

    drag(plot, token->position, token->position + QPointF{-120.0, 0.0}, 6);

    QVERIFY(window.body().words[0][5][0] != before);
    QCOMPARE(window.body().words[4][5][0], window.body().words[0][5][0]);
    QCOMPARE(window.body().words[0][5][1], trench::core::p2k::kS6ZeroRsqWord);
    QCOMPARE(window.body().words[4][5][1], trench::core::p2k::kS6ZeroRsqWord);
  }

  void identitySectionHasNoToken() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);

    const auto tokens = window.responsePlot()->tokens();
    std::size_t zeros = 0;
    for (const auto& token : tokens) {
      QVERIFY(token.section < trench::core::kLegacySectionCount);
      if (token.lane == ResponsePlotWidget::Lane::kZero) {
        QCOMPARE(token.section, std::size_t{0});
        ++zeros;
      }
    }
    QCOMPARE(zeros, std::size_t{1});
    QVERIFY(tokens.size() <= trench::core::kLegacySectionCount + 1);
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

  void unityVerbRenormalizesDcAsOneUndo() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    drag(plot, token->position, token->position + QPointF{-70.0, 22.0}, 5);
    QCOMPARE(window.undoStack()->count(), 1);

    const auto drifted = flatten_first_corner(window.body());
    QVERIFY(std::abs(window.dcDriftDb() - trench::core::p2k::dc_gain_db(drifted)) <
            1.0e-12);

    const auto before_bytes = window.body().native_bytes();
    const auto before_words = window.body().words[0];
    window.renormalizeDc();

    QCOMPARE(window.undoStack()->count(), 2);
    QVERIFY(std::abs(window.dcDriftDb()) < 0.05);
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      for (std::size_t word = 0; word < 4; ++word) {
        QCOMPARE(window.body().words[0][section][word], before_words[section][word]);
      }
      QCOMPARE(window.body().words[4][section], window.body().words[0][section]);
    }

    window.undoStack()->undo();
    QVERIFY(window.body().native_bytes() == before_bytes);
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
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    const auto target_body = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                             "ref/presets/P2k_002_early_rizer.bin";
    QVERIFY(window.loadTarget(target_body));
    const auto before = window.body().native_bytes();

    window.startFit();
    QVERIFY(window.fitRunning());
    QTRY_VERIFY_WITH_TIMEOUT(window.body().native_bytes() != before, 30000);
    window.stopAndKeep();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);

    QCOMPARE(window.undoStack()->count(), 1);
    QVERIFY(window.body().is_legacy_representable());
    QVERIFY(window.body().native_bytes() != before);
    window.undoStack()->undo();
    QVERIFY(window.body().native_bytes() == before);
  }

  void discardRestoresPreFitBytesAndDropsStragglers() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    const auto target_body = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                             "ref/presets/P2k_002_early_rizer.bin";
    QVERIFY(window.loadTarget(target_body));
    const auto before = window.body().native_bytes();

    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(window.body().native_bytes() != before, 30000);
    window.discardFit();
    QVERIFY(window.body().native_bytes() == before);
    QTest::qWait(300);
    QVERIFY(window.body().native_bytes() == before);
    QCOMPARE(window.undoStack()->count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);
  }

  void pinnedSectionsAreNeverTouchedByFit() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.show();
    QTest::qWait(20);
    const auto target_body = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                             "ref/presets/P2k_002_early_rizer.bin";
    QVERIFY(window.loadTarget(target_body));

    auto* document = window.document();
    for (std::size_t section = 0; section < 6; ++section) {
      if (section == 3) continue;
      document->toggleLane(section, true);
      document->toggleLane(section, false);
    }
    std::array<trench::core::PackedSection, 6> before{};
    for (std::size_t section = 0; section < 6; ++section) {
      before[section] = window.body().words[0][section];
    }

    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(window.body().words[0][3] != before[3], 30000);
    for (std::size_t section = 0; section < 6; ++section) {
      if (section == 3) continue;
      for (std::size_t wi = 0; wi < 4; ++wi) {
        QCOMPARE(window.body().words[0][section][wi], before[section][wi]);
      }
    }
    window.discardFit();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);

    for (std::size_t section = 0; section < 6; ++section) {
      QCOMPARE(window.body().words[0][section], before[section]);
    }
  }

  void editAtCornerTwoWritesOnlyThatCorner() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto untouched = window.body();
    window.setCorner(2);
    QCOMPARE(window.document()->corner(), std::size_t{2});
    auto* plot = window.responsePlot();
    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    drag(plot, token->position, token->position + QPointF{-70.0, 22.0}, 5);
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
    auto* plot = window.responsePlot();
    window.selectSection(1);
    const auto token = find_token(plot, 1, ResponsePlotWidget::Lane::kZero);
    QVERIFY(token.has_value());
    drag(plot, token->position, token->position + QPointF{60.0, -18.0}, 5);
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
    auto* plot = window.responsePlot();
    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    drag(plot, token->position, token->position + QPointF{-70.0, 22.0}, 5);
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
    const auto before = window.body();
    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(window.body().words[3] != before.words[3], 30000);
    for (const std::size_t corner : {0U, 1U, 2U, 4U, 5U, 6U}) {
      QCOMPARE(window.body().words[corner], before.words[corner]);
    }
    window.discardFit();
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 30000);
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

  void aTiltIntentKeepsSectionSixInsideItsEnvelope() {
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
    window.startFit();
    QTRY_VERIFY_WITH_TIMEOUT(window.body().native_bytes() != before, 60000);
    QTRY_VERIFY_WITH_TIMEOUT(!window.fitRunning(), 60000);

    QVERIFY(p2k::within_envelope(p2k::Role::kTilt, window.body().words[0][5],
                                 trench::core::kP2kDatumHz));
  }

  void theGainFaderWritesLatticeWordsAsOneUndoStep() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto row = first_eq_row(window);
    QVERIFY(row >= 0);
    auto* strip = window.sectionStrip(static_cast<std::size_t>(row));
    QVERIFY(strip != nullptr);
    auto* fader = strip->findChild<QSlider*>(QStringLiteral("gainFader"));
    QVERIFY(fader != nullptr);

    const auto before = window.body().words[0][static_cast<std::size_t>(row)];
    const auto start =
        trench::core::p2k::param_of(before, trench::core::kP2kDatumHz).gain_db;
    fader->setSliderDown(true);
    for (int step = 1; step <= 12; ++step) {
      fader->setValue(fader->value() + 5);
    }
    fader->setSliderDown(false);

    const auto after = window.body().words[0][static_cast<std::size_t>(row)];
    QVERIFY(after != before);
    QVERIFY(is_lattice_word(after[0]));
    QVERIFY(is_lattice_word(after[1]));
    QCOMPARE(after[2], before[2]);
    QCOMPARE(after[3], before[3]);
    QCOMPARE(after[4], before[4]);
    QVERIFY(trench::core::p2k::param_of(after, trench::core::kP2kDatumHz).gain_db > start);
    QCOMPARE(window.body().words[4][static_cast<std::size_t>(row)], after);
    QCOMPARE(window.undoStack()->count(), 1);
  }

  void pressingAPlotMarkerSelectsThatStripsBandwidth() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto row = first_eq_row(window);
    QVERIFY(row >= 1);
    const auto section = static_cast<std::size_t>(row);

    auto* plot = window.responsePlot();
    const auto token = find_token(plot, section, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    press(plot, token->position);
    release(plot, token->position);
    QTest::qWait(20);

    for (std::size_t index = 0; index < trench::core::kLegacySectionCount; ++index) {
      auto* control = window.sectionStrip(index)->findChild<QDoubleSpinBox*>(
          QStringLiteral("bwControl"));
      auto* type = window.sectionStrip(index)->findChild<QComboBox*>(
          QStringLiteral("typeCombo"));
      QVERIFY(control != nullptr);
      QVERIFY(type != nullptr);
      QCOMPARE(control->isVisible(), index == section);
      QCOMPARE(type->isVisible(), index == section);
    }
  }

  void anFcEditSlidesTheBandAndKeepsTheZeroOffset() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto row = first_eq_row(window);
    QVERIFY(row >= 0);
    const auto section = static_cast<std::size_t>(row);

    const auto offset_of = [&] {
      const auto geometry = trench::core::geometry_from_words(
          window.body().words[0][section], trench::core::kP2kDatumHz);
      const auto& pole = std::get<trench::core::ConjugatePair>(geometry.pole);
      const auto& zero = std::get<trench::core::ConjugatePair>(geometry.zero);
      return std::log2(zero.hz / pole.hz);
    };
    const auto before = offset_of();
    QVERIFY(std::abs(before) > 0.1);

    auto param = trench::core::p2k::param_of(window.body().words[0][section],
                                             trench::core::kP2kDatumHz);
    param.fc_hz *= 0.8;
    window.applyParam(section, trench::core::p2k::SectionEdit::kFc, param);

    const auto pole = std::get<trench::core::ConjugatePair>(
        trench::core::geometry_from_words(window.body().words[0][section],
                                          trench::core::kP2kDatumHz)
            .pole);
    QVERIFY(std::abs(pole.hz - param.fc_hz) <= param.fc_hz * 0.05);
    QVERIFY(std::abs(offset_of() - before) < 0.05);
    QCOMPARE(window.undoStack()->count(), 1);
  }

  void aGainEditMovesOnlyTheZeroRadius() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto row = first_eq_row(window);
    QVERIFY(row >= 0);
    const auto section = static_cast<std::size_t>(row);

    const auto before = window.body().words[0][section];
    auto param = trench::core::p2k::param_of(before, trench::core::kP2kDatumHz);
    param.gain_db += 12.0;
    window.applyParam(section, trench::core::p2k::SectionEdit::kGain, param);

    const auto after = window.body().words[0][section];
    QCOMPARE(after[2], before[2]);
    QCOMPARE(after[3], before[3]);
    QVERIFY(pair_radius_of(after[0], after[1]) != pair_radius_of(before[0], before[1]));
    QVERIFY(trench::core::p2k::param_of(after, trench::core::kP2kDatumHz).gain_db >
            trench::core::p2k::param_of(before, trench::core::kP2kDatumHz).gain_db);
    QCOMPARE(window.undoStack()->count(), 1);
  }

  void choosingLowPassPutsTheZeroTwoOctavesAboveFc() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto row = first_eq_row(window);
    QVERIFY(row >= 0);
    const auto section = static_cast<std::size_t>(row);
    auto* type = window.sectionStrip(section)->findChild<QComboBox*>(
        QStringLiteral("typeCombo"));
    QVERIFY(type != nullptr);

    const auto fc = trench::core::p2k::param_of(window.body().words[0][section],
                                                trench::core::kP2kDatumHz)
                        .fc_hz;
    type->setCurrentIndex(
        SectionStrip::typeIndex(trench::core::p2k::SectionType::kLowPass));

    const auto geometry = trench::core::geometry_from_words(
        window.body().words[0][section], trench::core::kP2kDatumHz);
    const auto& zero = std::get<trench::core::ConjugatePair>(geometry.zero);
    QVERIFY(std::log2(zero.hz / fc) >= 2.0);
    QCOMPARE(trench::core::p2k::param_of(window.body().words[0][section],
                                         trench::core::kP2kDatumHz)
                 .type,
             trench::core::p2k::SectionType::kLowPass);
    QCOMPARE(window.undoStack()->count(), 1);
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

  void fitRoomVowelChoiceEmitsTheSymbol() {
    FitRoom room;
    room.setVowels({QStringLiteral("aa"), QStringLiteral("iy")});
    QCOMPARE(room.vowelBox()->count(), 2);
    QSignalSpy spy(&room, &FitRoom::vowelRequested);
    room.vowelBox()->setCurrentIndex(1);
    emit room.vowelBox()->activated(1);
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
    room.setVowels({QStringLiteral("aa"), QStringLiteral("iy"), QStringLiteral("uw")});
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
};

QTEST_MAIN(MainWindowTest)
#include "main_window_test.moc"
