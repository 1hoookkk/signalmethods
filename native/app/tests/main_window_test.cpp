#include "main_window.hpp"
#include "response_plot.hpp"
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
