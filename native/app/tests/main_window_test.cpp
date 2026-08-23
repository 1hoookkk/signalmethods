#include "chassis_bar.hpp"
#include "fit_room.hpp"
#include "main_window.hpp"
#include "morph_strip.hpp"
#include "posture_list.hpp"
#include "response_plot.hpp"
#include "section_strip.hpp"
#include "trench/core/formants.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/section_param.hpp"

#include <QTest>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QAction>
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

}  // namespace

class MainWindowTest final : public QObject {
  Q_OBJECT

 private slots:
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
    auto* plot = window.responsePlot();
    const auto before = window.body().words[0][0];

    double_click(plot, QPointF{plot->xForFrequency(1000.0), plot->height() * 0.5});
    QTest::qWait(20);

    const auto pole = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(pole.has_value());
    QVERIFY2(std::abs(pole->hz - 1000.0) <= 10.0, qPrintable(QString::number(pole->hz)));
    QVERIFY2(std::abs(pole->bw_hz - 120.0) <= 12.0, qPrintable(QString::number(pole->bw_hz)));
    QCOMPARE(window.body().words[0][0][4], before[4]);
    QCOMPARE(window.body().words[4][0], window.body().words[0][0]);
    QCOMPARE(window.undoStack()->count(), 1);
    for (std::size_t section = 1; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(window.body().words[0][section], trench::core::kIdentitySection);
    }
  }

  void draggingAResonanceUpNarrowsIt() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    double_click(plot, QPointF{plot->xForFrequency(1000.0), plot->height() * 0.5});
    QTest::qWait(20);

    const auto placed = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(placed.has_value());
    const auto token = find_token(plot, 0, ResponsePlotWidget::Lane::kPole);
    QVERIFY(token.has_value());
    drag(plot, token->position, token->position + QPointF{0.0, -60.0}, 6);

    const auto moved = p2k::pole_of(window.body().words[0][0], trench::core::kP2kDatumHz);
    QVERIFY(moved.has_value());
    QVERIFY2(moved->bw_hz < placed->bw_hz, qPrintable(QString::number(moved->bw_hz)));
    QVERIFY2(std::abs(std::log2(moved->hz / placed->hz)) < 0.014,
             qPrintable(QString::number(moved->hz)));
    QCOMPARE(window.undoStack()->count(), 2);
  }

  void theOffsetFaderMovesOnlyTheZeroWords() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    double_click(plot, QPointF{plot->xForFrequency(1000.0), plot->height() * 0.5});
    QTest::qWait(20);

    auto* strip = window.sectionStrip(0);
    auto* offset = strip->findChild<QSlider*>(QStringLiteral("offsetFader"));
    auto* readout = strip->findChild<QLabel*>(QStringLiteral("offsetValue"));
    QVERIFY(offset != nullptr && readout != nullptr);
    QCOMPARE(offset->minimum(), static_cast<int>(std::lround(p2k::kMaskOffsetMinOct * 100.0)));
    QCOMPARE(offset->maximum(), static_cast<int>(std::lround(p2k::kMaskOffsetMaxOct * 100.0)));

    const auto before = window.body().words[0][0];
    offset->setValue(100);

    const auto after = window.body().words[0][0];
    QCOMPARE(after[2], before[2]);
    QCOMPARE(after[3], before[3]);
    QCOMPARE(after[4], before[4]);
    QVERIFY(after[0] != before[0] || after[1] != before[1]);
    const auto geometry =
        trench::core::geometry_from_words(after, trench::core::kP2kDatumHz);
    const auto zero = std::get<trench::core::ConjugatePair>(geometry.zero);
    const auto pole = std::get<trench::core::ConjugatePair>(geometry.pole);
    QVERIFY(std::abs(std::log2(zero.hz / pole.hz) - 1.0) < 0.05);
    QVERIFY2(std::abs(readout->text().toDouble() - 1.0) < 0.05,
             qPrintable(readout->text()));
    QCOMPARE(window.undoStack()->count(), 2);
  }

  void aFreshSkeletonGetsAParkedMask() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    double_click(plot, QPointF{plot->xForFrequency(1000.0), plot->height() * 0.5});
    QTest::qWait(20);

    const auto words = window.body().words[0][0];
    QVERIFY(words[0] != trench::core::kIdentitySection[0] ||
            words[1] != trench::core::kIdentitySection[1]);
    const auto zero = std::get<trench::core::ConjugatePair>(
        trench::core::geometry_from_words(words, trench::core::kP2kDatumHz).zero);
    QVERIFY2(zero.hz >= 10'000.0, qPrintable(QString::number(zero.hz)));
    QCOMPARE(p2k::mask_of(words, 0, trench::core::kP2kDatumHz).zero_bw_hz,
             p2k::mask_width_floor_hz(trench::core::kP2kDatumHz));

    const auto masked = trench::core::section_words_to_biquad(words);
    auto bare = masked;
    bare[1] = 0.0;
    bare[2] = 0.0;
    const auto shape = [](const trench::core::Biquad& section, double hz) {
      return trench::core::section_response_db(section, hz, trench::core::kP2kDatumHz) -
             trench::core::section_response_db(section, 0.0, trench::core::kP2kDatumHz);
    };
    for (const auto& probe : {std::pair{1'000.0, 0.1}, std::pair{5'000.0, 1.2}}) {
      const auto delta = std::abs(shape(masked, probe.first) - shape(bare, probe.first));
      QVERIFY2(delta < probe.second,
               qPrintable(QStringLiteral("%1 Hz %2 dB").arg(probe.first).arg(delta)));
    }
    QCOMPARE(window.body().words[4][0], words);
    QCOMPARE(window.undoStack()->count(), 1);

    window.clearSection(0);
    QCOMPARE(window.body().words[0][0], trench::core::kIdentitySection);
    QCOMPARE(window.undoStack()->count(), 2);
  }

  void aRingDroppedOnAPolePairsWithIt() {
    namespace p2k = trench::core::p2k;
    MainWindow window(std::filesystem::path{}, trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    double_click(plot, QPointF{plot->xForFrequency(300.0), plot->height() * 0.5});
    QTest::qWait(20);
    double_click(plot, QPointF{plot->xForFrequency(3000.0), plot->height() * 0.5});
    QTest::qWait(20);
    window.selectSection(0);
    QTest::qWait(20);

    const auto target = p2k::pole_of(window.body().words[0][1], trench::core::kP2kDatumHz);
    QVERIFY(target.has_value());
    const auto before = window.body().words[0][0];
    const auto ring = find_token(plot, 0, ResponsePlotWidget::Lane::kZero);
    QVERIFY(ring.has_value());

    drag(plot, ring->position,
         QPointF{plot->xForFrequency(target->hz), ring->position.y()}, 8);
    QTest::qWait(20);

    const auto after = window.body().words[0][0];
    QCOMPARE(after[2], before[2]);
    QCOMPARE(after[3], before[3]);
    QCOMPARE(after[4], before[4]);
    const auto zero = std::get<trench::core::ConjugatePair>(
        trench::core::geometry_from_words(after, trench::core::kP2kDatumHz).zero);
    QVERIFY2(std::abs(zero.hz / target->hz - 1.0) <= 0.01,
             qPrintable(QStringLiteral("%1 %2").arg(zero.hz).arg(target->hz)));
  }

  void aVerticalRingDragNarrowsTheZeroAsOneUndo() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* plot = window.responsePlot();
    const auto floor_hz = p2k::mask_width_floor_hz(trench::core::kP2kDatumHz);
    std::size_t chosen = trench::core::kLegacySectionCount;
    for (std::size_t section = 0; section + 1 < trench::core::kLegacySectionCount; ++section) {
      const auto mask = p2k::mask_of(window.body().words[0][section], section,
                                     trench::core::kP2kDatumHz);
      if (mask.zero_bw_hz > floor_hz * 1.5) {
        chosen = section;
        break;
      }
    }
    QVERIFY(chosen < trench::core::kLegacySectionCount);
    window.selectSection(chosen);
    QTest::qWait(20);

    const auto before = p2k::mask_of(window.body().words[0][chosen], chosen,
                                     trench::core::kP2kDatumHz);
    const auto pole = window.body().words[0][chosen][2];
    const auto ring = find_token(plot, chosen, ResponsePlotWidget::Lane::kZero);
    QVERIFY(ring.has_value());
    drag(plot, ring->position, ring->position + QPointF{0.0, -50.0}, 6);
    QTest::qWait(20);

    const auto after = p2k::mask_of(window.body().words[0][chosen], chosen,
                                    trench::core::kP2kDatumHz);
    QVERIFY2(after.zero_bw_hz < before.zero_bw_hz,
             qPrintable(QStringLiteral("%1 %2").arg(before.zero_bw_hz).arg(after.zero_bw_hz)));
    QCOMPARE(window.body().words[0][chosen][2], pole);
    QCOMPARE(window.undoStack()->count(), 1);
  }

  void theCharacterDialNarrowsTheQCornersFromTheQ0Corners() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* dial = window.morphStrip()->findChild<QSlider*>(QStringLiteral("characterSlider"));
    QVERIFY(dial != nullptr);
    const auto source = window.body().words[0];
    const auto before_q = window.body().words[2];

    dial->setValue(dial->maximum() / 2);
    QTest::qWait(20);
    QCOMPARE(window.undoStack()->count(), 1);

    const auto narrowed = window.body().words[2];
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(narrowed[section][0], source[section][0]);
      QCOMPARE(narrowed[section][1], source[section][1]);
      QCOMPARE(narrowed[section][4], source[section][4]);
      const auto before =
          trench::core::geometry_from_words(source[section], trench::core::kP2kDatumHz);
      const auto* pole = std::get_if<trench::core::ConjugatePair>(&before.pole);
      if (pole == nullptr) {
        QCOMPARE(narrowed[section], source[section]);
        continue;
      }
      const auto bw_hz =
          -std::log(pole->radius) * trench::core::kP2kDatumHz / std::numbers::pi;
      const auto want = std::exp(0.5 * std::log(bw_hz) +
                                 0.5 * std::log(p2k::kNarrowestPoleBwHz));
      const auto [mag, rsq] = p2k::words_from_root(
          pole->hz, std::exp(-std::numbers::pi * want / trench::core::kP2kDatumHz));
      QCOMPARE(narrowed[section][2], mag);
      QCOMPARE(narrowed[section][3], rsq);
    }

    window.undoStack()->undo();
    QCOMPARE(window.body().words[2], before_q);
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

    const auto low = p2k::param_of(window.body().words[0][5], trench::core::kP2kDatumHz);
    QCOMPARE(low.type, p2k::SectionType::kLowPass);
    QVERIFY(std::log2(low.trench_hz / low.fc_hz) >= p2k::kTrenchMinOct - 1e-9);
  }

  void oneStripEditIsOneUndoEntryAndUndoRestoresTheBytes() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    const auto row = first_eq_row(window);
    QVERIFY(row >= 0);
    const auto section = static_cast<std::size_t>(row);
    auto* strip = window.sectionStrip(section);
    QVERIFY(strip != nullptr);
    auto* offset = strip->findChild<QSlider*>(QStringLiteral("offsetFader"));
    QVERIFY(offset != nullptr);

    const auto before_bytes = window.body().native_bytes();
    const auto before = window.body().words[0][section];
    offset->setValue(offset->value() + 100);

    const auto after = window.body().words[0][section];
    QVERIFY(after != before);
    QCOMPARE(window.body().words[4][section], after);
    QCOMPARE(window.undoStack()->count(), 1);

    window.undoStack()->undo();
    QVERIFY(window.body().native_bytes() == before_bytes);
  }

  void pressingAPlotMarkerSelectsThatStripsDepth() {
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

    for (std::size_t index = 0; index < trench::core::kLegacySectionCount; ++index) {
      auto* control = window.sectionStrip(index)->findChild<QDoubleSpinBox*>(
          QStringLiteral("widthControl"));
      QVERIFY(control != nullptr);
      QCOMPARE(control->isVisible(), index == section);
    }
  }

  void sectionSixKeepsItsTrenchControl() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* strip = window.sectionStrip(5);
    auto* offset = strip->findChild<QSlider*>(QStringLiteral("offsetFader"));
    QVERIFY(offset != nullptr);
    QCOMPARE(offset->minimum(), static_cast<int>(std::lround(p2k::kTrenchMinOct * 100.0)));
    QCOMPARE(offset->maximum(), static_cast<int>(std::lround(p2k::kTrenchMaxOct * 100.0)));

    const auto before = window.body().words[0][5];
    const auto chosen = p2k::param_of(before, trench::core::kP2kDatumHz);
    QCOMPARE(chosen.type, p2k::SectionType::kLowPass);

    offset->setValue(offset->value() - 100);

    const auto after = window.body().words[0][5];
    const auto moved = p2k::param_of(after, trench::core::kP2kDatumHz);
    QCOMPARE(moved.type, p2k::SectionType::kLowPass);
    QVERIFY(moved.trench_hz < chosen.trench_hz);
    QVERIFY(std::log2(moved.trench_hz / moved.fc_hz) >= p2k::kTrenchMinOct - 1.0e-9);
    QCOMPARE(after[1], p2k::kS6ZeroRsqWord);
    QCOMPARE(after[2], before[2]);
    QCOMPARE(after[3], before[3]);
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
    QCOMPARE(window.document()->view().semitones, 12);
    QVERIFY(plot->tokens().empty());
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

  void hoveringAStripHighlightsItsTokens() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* strip = window.sectionStrip(1);
    QVERIFY(strip != nullptr);
    const QPointF at{4.0, 4.0};
    QEnterEvent enter(at, at, strip->mapToGlobal(at));
    QCoreApplication::sendEvent(strip, &enter);
    QCOMPARE(window.responsePlot()->highlightedSection(), std::optional<std::size_t>{1});
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(strip, &leave);
    QCOMPARE(window.responsePlot()->highlightedSection(), std::optional<std::size_t>{});
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
    QCOMPARE(window.postureList()->rowOf(QStringLiteral("para A")) >= 0, true);
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
    const std::array<std::size_t, 4> rows{1, 2, 3, 4};
    for (std::size_t index = 0; index < skeleton->pole_count; ++index) {
      const auto geometry =
          trench::core::geometry_from_words(after[rows[index]], trench::core::kP2kDatumHz);
      const auto* pole = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
      QVERIFY(pole != nullptr);
      QVERIFY(std::abs(pole->hz - skeleton->poles[index].hz) <=
              0.01 * skeleton->poles[index].hz);
    }
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(after[section][0], before[section][0]);
      QCOMPARE(after[section][1], before[section][1]);
      QCOMPARE(after[section][4], before[section][4]);
    }
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
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
      QCOMPARE(after[section][0], before[section][0]);
      QCOMPARE(after[section][1], before[section][1]);
      QCOMPARE(after[section][4], before[section][4]);
    }
    QCOMPARE(list->matched(), name);
    QVERIFY(!list->hasFocus());
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
    QVERIFY(list->matched() != name);
  }

  void aMouthTemplateWritesSixAscendingPoleRowsWithParkedZeros() {
    namespace p2k = trench::core::p2k;
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* list = window.postureList();
    const auto name = QStringLiteral("s1 bahn a");
    const auto row = list->rowOf(name);
    QVERIFY(row >= 0);
    const auto* skeleton = p2k::posture("s1 bahn a");
    QVERIFY(skeleton != nullptr);
    QCOMPARE(skeleton->pole_count, std::size_t{6});
    QCOMPARE(skeleton->type, std::string_view{"MOUTHS S1"});
    list->scrollToItem(list->item(row));
    const auto before = window.body().words[0];
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(),
                      list->visualItemRect(list->item(row)).center());
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
      QVERIFY(std::holds_alternative<trench::core::ConjugatePair>(geometry.zero));
      const std::array<std::uint16_t, 4> roots{after[section][0], after[section][1],
                                               after[section][2], after[section][3]};
      const auto parked =
          p2k::words_with_parked_zero(roots, section, trench::core::kP2kDatumHz);
      QCOMPARE(parked, roots);
    }
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      QCOMPARE(after[section][4], before[section][4]);
    }
    QCOMPARE(list->matched(), name);
    window.undoStack()->undo();
    QCOMPARE(window.body().words[0], before);
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
    QCOMPARE(window.document()->view().semitones, 7);
    morph->setValue(500);
    QCOMPARE(transpose->value(), 7);
    QCOMPARE(window.document()->view().semitones, 7);
    q->setValue(500);
    QCOMPARE(transpose->value(), 7);
    QCOMPARE(window.document()->view().semitones, 7);
    window.setCorner(2);
    QCOMPARE(transpose->value(), 7);
    QCOMPARE(window.document()->view().semitones, 7);
    const auto shifted = peak_hz_below(window.document()->viewResponseDb(),
                                       window.document()->grid().hz, 6000.0);
    transpose->setValue(0);
    QCOMPARE(window.document()->view().semitones, 0);
    const auto plain = peak_hz_below(window.document()->viewResponseDb(),
                                     window.document()->grid().hz, 4000.0);
    QVERIFY(std::abs(std::log2(shifted / plain) - 7.0 / 12.0) < 0.08);
  }

  void undoRestoresAStripEdit() {
    MainWindow window(fixture_path(), trench::core::kP2kDatumHz);
    window.resize(960, 540);
    window.show();
    QTest::qWait(20);
    auto* strip = window.sectionStrip(1);
    QVERIFY(strip != nullptr);
    auto* offset = strip->findChild<QSlider*>(QStringLiteral("offsetFader"));
    QVERIFY(offset != nullptr);
    const auto before = window.body().native_bytes();
    const auto before_offset = offset->value();
    offset->setValue(before_offset - 120);
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
    QCOMPARE(offset->value(), before_offset);
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

};

QTEST_MAIN(MainWindowTest)
#include "main_window_test.moc"
