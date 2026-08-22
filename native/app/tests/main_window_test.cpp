#include "main_window.hpp"
#include "response_plot.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QTest>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
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
    QCOMPARE(tokens.size(), trench::core::kLegacySectionCount * 2);
    for (const auto& token : tokens) {
      QVERIFY(token.section < trench::core::kLegacySectionCount);
    }
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

    const auto pole = find_token(plot, 3, ResponsePlotWidget::Lane::kPole);
    const auto zero = find_token(plot, 3, ResponsePlotWidget::Lane::kZero);
    QVERIFY(pole.has_value());
    QVERIFY(zero.has_value());
    QVERIFY(!pole->live);
    QVERIFY(!zero->live);

    drag(plot, pole->position, pole->position + QPointF{90.0, -40.0}, 6);

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

  void pressPublishesSelectedTokenReadout() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();

    QVERIFY(window.readoutText().isEmpty());
    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    press(plot, token->position);
    release(plot, token->position);

    const auto mag = window.body().words[0][0][2];
    const auto rsq = window.body().words[0][0][3];
    const auto text = window.readoutText();
    QVERIFY(text.contains(QString::number(mag, 16).toUpper().rightJustified(4, '0')));
    QVERIFY(text.contains(QString::number(rsq, 16).toUpper().rightJustified(4, '0')));
    QVERIFY(text.contains(
        QStringLiteral("#%1").arg(trench::core::p2k::nearest_lattice_word(mag))));
    const auto geometry = trench::core::geometry_from_words(window.body().words[0][0],
                                                            trench::core::kP2kDatumHz);
    const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
    QVERIFY(conjugate != nullptr);
    QVERIFY(text.contains(QString::number(conjugate->radius, 'f', 5)));

    press(plot, token->position);
    release(plot, token->position);
    QCOMPARE(window.freedomMask() & trench::core::p2k::pole_bit(0),
             trench::core::p2k::pole_bit(0));

    drag(plot, token->position, token->position + QPointF{-70.0, 22.0}, 5);
    const auto moved_mag = window.body().words[0][0][2];
    QVERIFY(moved_mag != mag);
    QVERIFY(window.readoutText().contains(
        QString::number(moved_mag, 16).toUpper().rightJustified(4, '0')));
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
};

QTEST_MAIN(MainWindowTest)
#include "main_window_test.moc"
