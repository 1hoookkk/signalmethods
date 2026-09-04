#include "harness.hpp"

#include "cascade_plot.hpp"
#include "editor_state.hpp"
#include "keyframe_grid.hpp"
#include "ladder.hpp"
#include "main_window.hpp"
#include "number_box.hpp"
#include "pole_templates.hpp"
#include "rows_table.hpp"
#include "vowel_space.hpp"

#include "trench/audio/audition.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QApplication>
#include <QComboBox>
#include <QPoint>
#include <QPushButton>
#include <QEvent>
#include <QPointF>
#include <QTest>
#include <QToolButton>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace {

using Resonant = trench::core::native::Resonant;
using RealRoots = trench::core::native::RealRoots;

constexpr std::size_t kCeiling = trench::core::native::kSections - 1;

const Resonant& pole(const EditorState& state, std::size_t corner, std::size_t section) {
  return std::get<Resonant>(state.sectionAt(corner, section).pole);
}

const Resonant& zero(const EditorState& state, std::size_t corner, std::size_t section) {
  return std::get<Resonant>(state.sectionAt(corner, section).zero);
}

double radius(double bw_hz) {
  return std::exp(-std::numbers::pi * bw_hz / EditorState::kDatumHz);
}

double peakDb(const EditorState& state, std::size_t corner, std::size_t section) {
  const auto& words = state.packed().words[corner][section];
  return trench::core::section_response_db(trench::core::section_words_to_biquad(words),
                                           pole(state, corner, section).hz,
                                           EditorState::kDatumHz);
}

void dragOnPlot(CascadePlot* plot, const QPoint& from, const QPoint& by) {
  QTest::mousePress(plot, Qt::LeftButton, Qt::NoModifier, from);
  QTest::mouseMove(plot, from + by);
  QTest::mouseRelease(plot, Qt::LeftButton, Qt::NoModifier, from + by);
}

void seatEqRow(MainWindow& window, std::size_t index, const QString& hz) {
  auto* type = window.findChild<QComboBox*>(QStringLiteral("type%1").arg(index));
  auto* note = window.findChild<NumberBox*>(QStringLiteral("noteLo%1").arg(index));
  type->setCurrentIndex(type->findText(QStringLiteral("EQ")));
  note->type(hz);
}

double poleHzOf(const trench::core::Biquad& section) {
  const double radius_squared = section[4];
  if (!(radius_squared > 0.0)) return 0.0;
  const double radius = std::sqrt(radius_squared);
  const double cosine = -section[3] / (2.0 * radius);
  if (!(std::abs(cosine) < 1.0)) return 0.0;
  return std::acos(cosine) * EditorState::kDatumHz / (2.0 * std::numbers::pi);
}

void seatPole(EditorState& state, std::size_t corner, std::size_t section, double hz,
              double bw_hz) {
  if (!state.sectionEnabledAt(corner, section)) state.toggleSectionAt(corner, section);
  state.setRootAt(corner, section, EditorState::Lane::kPole, hz, bw_hz);
}

double plotPeakHz(const CascadePlot* plot, double from_hz, double to_hz) {
  double best_hz = from_hz;
  double best_db = -1e9;
  for (double hz = from_hz; hz <= to_hz; hz *= 1.0005) {
    const double db = plot->curveDbAt(hz);
    if (db > best_db) {
      best_db = db;
      best_hz = hz;
    }
  }
  return best_hz;
}

double cornerDbAt(const EditorState& state, double morph, double q, double hz) {
  const trench::core::Cascade cascade = trench::audio::design_audition(
      {state.packed(), static_cast<float>(morph), static_cast<float>(q), 0.0, 0.0},
      EditorState::kDatumHz);
  const std::span<const trench::core::Biquad> six{cascade.data(),
                                                 trench::core::native::kSections};
  return trench::core::cascade_response_db(six, hz, EditorState::kDatumHz);
}

void throwDial(QWidget* dial, int pixels) {
  const QPoint start(dial->width() / 2, dial->height() - 16);
  QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(dial, start + QPoint(pixels, 0));
  QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier, start + QPoint(pixels, 0));
}

}

TRENCH_TEST(posture_throw_writes_the_morph_partner_transposed) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  voiceLadder(state, 0);
  CHECK(!state.sectionEnabledAt(1, 1));
  const auto before = state.document();
  auto* posture = window.findChild<QWidget*>(QStringLiteral("posture"));
  CHECK(posture != nullptr);
  throwDial(posture, 100);
  CHECK(state.editingCorner() == 0);
  CHECK(state.document().corners[0] == before.corners[0]);
  CHECK(state.sectionEnabledAt(1, 1));
  const double shift = std::exp2(3.0 / 12.0);
  for (std::size_t section = 0; section < 6; ++section) {
    CHECK_NEAR(pole(state, 1, section).hz, pole(state, 0, section).hz * shift, 1e-6);
    CHECK_NEAR(pole(state, 1, section).bw_hz, pole(state, 0, section).bw_hz, 1e-9);
  }
  CHECK(state.zeroPresentAt(1, 2));
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(sharpen_throw_writes_the_q_partner_with_raised_radii) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  voiceLadder(state, 0);
  CHECK(!state.sectionEnabledAt(2, 1));
  const auto before = state.document();
  auto* sharpen = window.findChild<QWidget*>(QStringLiteral("sharpen"));
  CHECK(sharpen != nullptr);
  throwDial(sharpen, 50);
  CHECK(state.editingCorner() == 0);
  CHECK(state.document().corners[0] == before.corners[0]);
  for (std::size_t section = 0; section < 6; ++section) {
    CHECK_NEAR(pole(state, 2, section).hz, pole(state, 0, section).hz, 1e-9);
    const double ceiling = radius(EditorState::kMinBandwidthHz);
    CHECK_NEAR(radius(pole(state, 2, section).bw_hz),
               std::min(radius(pole(state, 0, section).bw_hz) + 0.01, ceiling), 1e-9);
  }
  CHECK(state.sectionAt(2, 2).zero == state.sectionAt(0, 2).zero);
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(template_pick_seeds_the_editing_corner_poles) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  window.openFrames();
  grid->setSearch(QString());
  grid->setGroup(QStringLiteral("TYPES"));
  CHECK(grid->tileCount() > 1);
  const auto before = state.document();
  int vow = -1;
  for (int tile = 0; tile < grid->tileCount(); ++tile) {
    if (grid->tileName(tile).contains(QStringLiteral("VOW"))) vow = tile;
    CHECK(!grid->tileName(tile).contains(QStringLiteral("FLG")));
  }
  CHECK(vow >= 0);
  grid->pickTile(vow);
  CHECK(state.document() != before);
  CHECK(state.editingCorner() == 0);
  double last = 0.0;
  int enabled = 0;
  for (std::size_t section = 0; section < 6; ++section) {
    if (!state.sectionEnabledAt(0, section)) continue;
    ++enabled;
    const auto& p = pole(state, 0, section);
    CHECK(p.hz > last);
    CHECK(p.hz >= 20.0 && p.hz <= 22'050.0);
    last = p.hz;
  }
  CHECK(enabled >= 4);
  CHECK(!grid->isVisible());
  const double want = std::pow(10.0, trench::app::kTypeRowSeedGainDb / 20.0);
  for (std::size_t section = 0; section < kCeiling; ++section) {
    if (!state.sectionEnabledAt(0, section)) continue;
    CHECK(state.zeroPresentAt(0, section));
    const auto& p = pole(state, 0, section);
    const auto& z = zero(state, 0, section);
    CHECK_NEAR(z.hz, p.hz, p.hz * 0.02);
    CHECK_NEAR(z.bw_hz / p.bw_hz, want, want * 0.15);
  }
  CHECK(state.sectionEnabledAt(0, kCeiling));
  CHECK(state.packed().words[0][kCeiling][1] == trench::core::p2k::kS6ZeroRsqWord);
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(frames_grid_lists_every_entry) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  window.openFrames();
  CHECK(grid->isVisible());
  grid->setSearch(QString());
  grid->setGroup(QStringLiteral("ALL"));
  const int roster = static_cast<int>(trench::app::loadPoleTemplates().size());
  const int keys = static_cast<int>(trench::app::loadKeyframes().size());
  const int want = 132 + roster + keys;
  std::printf("grid holds %d tiles, 132 bank + %d types + %d keyframes = %d\n", grid->tileCount(),
              roster, keys, want);
  CHECK(keys == 158);
  CHECK(grid->tileCount() == want);
  grid->setGroup(QStringLiteral("VOWEL H95"));
  std::printf("VOWEL H95 holds %d tiles\n", grid->tileCount());
  CHECK(grid->tileCount() == 48);
  grid->setGroup(QStringLiteral("BANK"));
  CHECK(grid->tileCount() == 132);
  grid->setGroup(QStringLiteral("TYPES"));
  CHECK(grid->tileCount() == roster);
  grid->setGroup(QStringLiteral("HEAD"));
  std::printf("HEAD holds %d tiles\n", grid->tileCount());
  CHECK(grid->tileCount() == 35);
  grid->setGroup(QStringLiteral("ALL"));
  grid->setSearch(QStringLiteral("ooh"));
  std::printf("search ooh holds %d tiles\n", grid->tileCount());
  CHECK(grid->tileCount() == 4);
  for (int tile = 0; tile < grid->tileCount(); ++tile) {
    CHECK(grid->tileGroup(tile) == QStringLiteral("BANK"));
    CHECK(grid->tileName(tile).startsWith(QStringLiteral("Ooh To Eee")));
  }
  grid->hide();
  window.close();
}

TRENCH_TEST(frames_grid_click_lands_on_the_editing_corner) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  auto* anchor = window.findChild<QToolButton*>(QStringLiteral("anchor"));
  CHECK(anchor != nullptr);
  CHECK(anchor->isChecked());
  window.openFrames();
  grid->setGroup(QStringLiteral("ALL"));
  grid->setSearch(QStringLiteral("az 0 el 0"));
  const int here = grid->tileNamed(QStringLiteral("left ear az 0 el 0"));
  CHECK(here >= 0);
  grid->pickTile(here);
  CHECK(!grid->isVisible());
  const std::array<double, 3> want{3'230.0, 4'378.0, 10'404.0};
  for (std::size_t section = 0; section < want.size(); ++section) {
    CHECK(state.sectionEnabledAt(0, section));
    const auto& p = pole(state, 0, section);
    std::printf("grid pick row %zu pole %.1f Hz wants %.1f Hz\n", section, p.hz, want[section]);
    CHECK_NEAR(p.hz, want[section], want[section] * 0.01);
  }
  window.openFrames();
  grid->setSearch(QStringLiteral("az 90 el 0"));
  const int there = grid->tileNamed(QStringLiteral("left ear az 90 el 0"));
  CHECK(there >= 0);
  grid->pickTileOnPartner(there);
  CHECK(grid->isVisible());
  CHECK(state.editingCorner() == 0);
  const std::array<double, 3> partner{4'607.0, 5'627.0, 6'840.0};
  for (const double hz : partner) {
    bool seated = false;
    for (std::size_t section = 0; section < EditorState::kSections; ++section) {
      if (!state.sectionEnabledAt(1, section)) continue;
      if (std::abs(pole(state, 1, section).hz - hz) <= hz * 0.01) seated = true;
    }
    std::printf("partner corner holds %.1f Hz %d\n", hz, static_cast<int>(seated));
    CHECK(seated);
  }
  CHECK(state.sectionEnabledAt(1, 0));
  std::printf("partner slot 0 %.1f Hz paired with %.1f Hz\n", pole(state, 1, 0).hz,
              pole(state, 0, 0).hz);
  CHECK_NEAR(pole(state, 1, 0).hz, 4'607.0, 46.07);
  grid->hide();
  window.close();
}

TRENCH_TEST(frames_grid_sparkline_is_the_response) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  window.openFrames();
  grid->setGroup(QStringLiteral("BANK"));
  grid->setSearch(QStringLiteral("ooh"));
  const int tile = grid->tileNamed(QStringLiteral("Ooh To Eee \u00b7 M0 Q0"));
  CHECK(tile >= 0);
  const auto& hz = grid->sparklineHz();
  CHECK(hz.size() == 96);
  CHECK_NEAR(hz.front(), 40.0, 1e-6);
  CHECK_NEAR(hz.back(), 16'000.0, 1e-3);
  const auto& drawn = grid->tileResponseDb(tile);
  CHECK(drawn.size() == hz.size());
  const auto frames = trench::app::loadFrames();
  const trench::app::PoleTemplate* chosen = nullptr;
  for (const auto& frame : frames) {
    if (frame.type == QStringLiteral("ooh_to_eee M0 Q0")) chosen = &frame;
  }
  CHECK(chosen != nullptr);
  EditorState scratch;
  scratch.setDocument(EditorState::blank());
  trench::app::applyFrame(scratch, *chosen, 0, false);
  const auto cascade = scratch.cascade(EditorState::kDatumHz);
  for (const double target : {611.0, 958.0, 4'936.0}) {
    std::size_t at = 0;
    for (std::size_t i = 1; i < hz.size(); ++i) {
      if (std::abs(std::log2(hz[i] / target)) < std::abs(std::log2(hz[at] / target))) at = i;
    }
    const double truth = trench::core::cascade_response_db(
        std::span<const trench::core::Biquad>(cascade.data(), cascade.size()), hz[at],
        EditorState::kDatumHz);
    std::printf("sparkline near %.0f Hz at %.1f Hz drawn %.2f dB truth %.2f dB\n", target, hz[at],
                drawn[at], truth);
    CHECK_NEAR(drawn[at], truth, 1.0);
  }
  grid->hide();
  window.close();
}

TRENCH_TEST(type_rows_compile_eq_lp_hp) {
  EditorState state;
  state.setDocument(EditorState::blank());
  const auto before = state.document();
  const std::vector<trench::app::TypeRow> rows{
      {trench::app::RowType::kHighPass, 100.0, 30.0, 0.0},
      {trench::app::RowType::kEq, 500.0, 50.0, 12.0},
      {trench::app::RowType::kEq, 2'000.0, 100.0, -6.0},
      {trench::app::RowType::kLowPass, 5'000.0, 500.0, 0.0}};
  trench::app::applyTypeRows(state, rows, 0);
  CHECK(state.sectionEnabledAt(0, 0));
  CHECK(state.sectionEnabledAt(0, 1));
  CHECK(state.sectionEnabledAt(0, 2));
  CHECK(!state.sectionEnabledAt(0, 3));
  CHECK(!state.sectionEnabledAt(0, 4));
  CHECK(state.sectionEnabledAt(0, kCeiling));
  CHECK(state.zeroPresentAt(0, 0));
  const auto* edge = std::get_if<RealRoots>(&state.sectionAt(0, 0).zero);
  CHECK(edge != nullptr);
  CHECK(edge->a_hz > 0.0);
  CHECK_NEAR(zero(state, 0, 1).hz, 500.0, 10.0);
  CHECK_NEAR(zero(state, 0, 1).bw_hz / pole(state, 0, 1).bw_hz, 3.981, 3.981 * 0.15);
  CHECK_NEAR(zero(state, 0, 2).hz, 2'000.0, 40.0);
  CHECK_NEAR(zero(state, 0, 2).bw_hz / pole(state, 0, 2).bw_hz, 0.501, 0.501 * 0.15);
  CHECK(state.packed().words[0][kCeiling][1] == trench::core::p2k::kS6ZeroRsqWord);
  CHECK_NEAR(pole(state, 0, kCeiling).hz, 5'000.0, 100.0);
  const auto cascade = state.cascade(EditorState::kDatumHz);
  const auto db = [&](double hz) {
    return trench::core::cascade_response_db(cascade, hz, EditorState::kDatumHz);
  };
  CHECK(db(500.0) - db(250.0) >= 6.0);
  state.undo();
  CHECK(state.document() == before);
}

TRENCH_TEST(row_six_eq_from_audio_lands_six_bells) {
  EditorState state;
  state.setDocument(EditorState::blank());
  RowsTable table(&state);
  std::vector<trench::app::TypeRow> rows;
  for (const double hz : {200.0, 400.0, 800.0, 1'600.0, 3'200.0, 6'400.0}) {
    rows.push_back({trench::app::RowType::kEq, hz, hz * 0.05, 12.0});
  }
  trench::app::applyTypeRows(state, rows, 0);
  table.refresh();
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    CHECK(state.sectionEnabledAt(0, section));
    CHECK(state.zeroPresentAt(0, section));
    const auto& p = pole(state, 0, section);
    const auto& z = zero(state, 0, section);
    CHECK_NEAR(z.hz, p.hz, p.hz * 0.02);
  }
  auto* type5 = table.findChild<QComboBox*>(QStringLiteral("type5"));
  auto* height5 = table.findChild<NumberBox*>(QStringLiteral("heightLo5"));
  CHECK(type5 != nullptr && height5 != nullptr);
  CHECK(type5->currentText() == QStringLiteral("EQ"));
  bool parsed = false;
  const double shown =
      height5->text().split(QLatin1Char(' ')).first().toDouble(&parsed);
  CHECK(parsed);
  CHECK_NEAR(shown, 12.0, 1.5);
  CHECK(state.packed().words[0][kCeiling][1] != trench::core::p2k::kS6ZeroRsqWord);
  std::printf("row six bell %s at %.0f Hz\n",
              height5->text().toUtf8().constData(), pole(state, 0, kCeiling).hz);
  table.close();
}

TRENCH_TEST(from_audio_seeds_the_editing_corner) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  const std::size_t corner = state.editingCorner();
  constexpr double fs = EditorState::kDatumHz;
  const std::array<double, 3> want_hz{250.0, 1'200.0, 2'800.0};
  const std::array<double, 3> want_bw{40.0, 90.0, 180.0};
  std::vector<float> buffer(static_cast<std::size_t>(fs), 0.0F);
  for (std::size_t n = 0; n < buffer.size(); ++n) {
    double acc = 0.0;
    for (std::size_t k = 0; k < want_hz.size(); ++k) {
      const double t = static_cast<double>(n) / fs;
      acc += std::exp(-std::numbers::pi * want_bw[k] * t) *
             std::cos(2.0 * std::numbers::pi * want_hz[k] * t);
    }
    buffer[n] = static_cast<float>(acc);
  }
  const auto before = state.document();
  window.seedFromAudio(buffer, fs);
  CHECK(state.document() != before);

  std::vector<std::pair<double, double>> eq_rows;
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(corner, section)) continue;
    CHECK(state.zeroPresentAt(corner, section));
    const auto& p = pole(state, corner, section);
    const auto& z = zero(state, corner, section);
    CHECK_NEAR(z.hz, p.hz, p.hz * 0.02);
    eq_rows.emplace_back(20.0 * std::log10(z.bw_hz / p.bw_hz), p.hz);
  }
  CHECK(eq_rows.size() == 3);
  std::sort(eq_rows.begin(), eq_rows.end(),
            [](const auto& left, const auto& right) { return left.first > right.first; });
  std::array<double, 3> loudest{eq_rows[0].second, eq_rows[1].second, eq_rows[2].second};
  std::sort(loudest.begin(), loudest.end());
  for (std::size_t k = 0; k < 3; ++k) {
    CHECK_NEAR(loudest[k], want_hz[k], want_hz[k] * 0.03);
  }

  CHECK(window.plot() != nullptr);
  CHECK(window.plot()->hasReference());
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(from_audio_speech_lands_bare_poles) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  window.setFromAudioMode(1);
  auto& state = window.state();
  const std::size_t corner = state.editingCorner();
  constexpr double fs = EditorState::kDatumHz;
  const std::array<double, 3> want_hz{500.0, 1'500.0, 2'500.0};
  const std::array<double, 3> want_bw{60.0, 90.0, 120.0};
  const auto length = static_cast<std::size_t>(fs);
  std::vector<double> drive(length, 0.0);
  std::uint32_t seed = 987654321u;
  for (std::size_t n = 0; n < length; ++n) {
    seed = seed * 1664525u + 1013904223u;
    drive[n] = 0.05 * (2.0 * (static_cast<double>(seed) / 4294967296.0) - 1.0);
  }
  const auto period = static_cast<std::size_t>(fs / 100.0);
  seed = 24680u;
  for (std::size_t at = 0; at < length;) {
    drive[at] += 1.0;
    seed = seed * 1664525u + 1013904223u;
    at += static_cast<std::size_t>(std::lround(
        static_cast<double>(period) *
        (1.0 + 0.08 * (static_cast<double>(seed) / 4294967296.0 - 0.5))));
  }
  std::vector<double> sum(length, 0.0);
  for (std::size_t k = 0; k < want_hz.size(); ++k) {
    const double r = radius(want_bw[k]);
    const double w0 = 2.0 * std::numbers::pi * want_hz[k] / fs;
    const double a1 = -2.0 * r * std::cos(w0);
    const double a2 = r * r;
    const double b0 = (k % 2 == 0 ? 1.0 : -1.0) * 2.0 * (1.0 - r) * std::sin(w0);
    double z1 = 0.0;
    double z2 = 0.0;
    for (std::size_t n = 0; n < length; ++n) {
      const double y = b0 * drive[n] - a1 * z1 - a2 * z2;
      z2 = z1;
      z1 = y;
      sum[n] += y;
    }
  }
  double peak = 0.0;
  for (const double value : sum) peak = std::max(peak, std::abs(value));
  std::vector<float> buffer(length, 0.0F);
  for (std::size_t n = 0; n < length; ++n) {
    buffer[n] = static_cast<float>(peak > 0.0 ? sum[n] / peak : 0.0);
  }

  const auto before = state.document();
  window.seedFromAudio(buffer, fs);
  CHECK(state.document() != before);

  std::vector<double> seated;
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(corner, section)) continue;
    CHECK(!state.zeroPresentAt(corner, section));
    seated.push_back(pole(state, corner, section).hz);
  }
  CHECK(seated.size() >= 3);
  std::sort(seated.begin(), seated.end());
  for (std::size_t k = 0; k < want_hz.size(); ++k) {
    double nearest = seated.empty() ? 0.0 : seated.front();
    for (const double hz : seated) {
      if (std::abs(hz - want_hz[k]) < std::abs(nearest - want_hz[k])) nearest = hz;
    }
    std::printf("speech pole %zu -> %.2f Hz\n", k, nearest);
    CHECK_NEAR(nearest, want_hz[k], want_hz[k] * 0.04);
  }

  CHECK(window.plot() != nullptr);
  CHECK(window.plot()->hasReference());
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(plot_handle_drags_the_note) {
  MainWindow window;
  window.resize(1600, 700);
  window.show();
  QTest::qWait(60);
  auto& state = window.state();
  seatEqRow(window, 0, QStringLiteral("500"));
  QTest::qWait(20);
  CascadePlot* plot = window.plot();
  CHECK(plot != nullptr);
  const auto centre = plot->handleCentre(0);
  CHECK(centre.has_value());
  const double before = pole(state, 0, 0).hz;
  dragOnPlot(plot, centre->toPoint(), QPoint(80, 0));
  const double after = pole(state, 0, 0).hz;
  std::printf("plot note drag %.1f Hz -> %.1f Hz (%.3f octaves)\n", before, after,
              std::log2(after / before));
  CHECK(std::log2(after / before) >= 0.25);
  CHECK(std::abs(std::log2(zero(state, 0, 0).hz / after)) < 0.05);
  state.undo();
  CHECK_NEAR(pole(state, 0, 0).hz, before, 1e-6);
  window.close();
}

TRENCH_TEST(plot_handle_drags_the_height) {
  MainWindow window;
  window.resize(1600, 700);
  window.show();
  QTest::qWait(60);
  auto& state = window.state();
  seatEqRow(window, 0, QStringLiteral("500"));
  QTest::qWait(20);
  CascadePlot* plot = window.plot();
  const auto centre = plot->handleCentre(0);
  CHECK(centre.has_value());
  const double before = peakDb(state, 0, 0);
  dragOnPlot(plot, centre->toPoint(), QPoint(0, -40));
  const double after = peakDb(state, 0, 0);
  std::printf("plot height drag %+.2f dB -> %+.2f dB\n", before, after);
  CHECK(after - before >= 3.0);
  window.close();

  MainWindow edge;
  edge.resize(1600, 700);
  edge.show();
  QTest::qWait(60);
  auto* type1 = edge.findChild<QComboBox*>(QStringLiteral("type1"));
  auto* note1 = edge.findChild<NumberBox*>(QStringLiteral("noteLo1"));
  auto* ring1 = edge.findChild<NumberBox*>(QStringLiteral("ringLo1"));
  CHECK(type1 != nullptr && note1 != nullptr && ring1 != nullptr);
  type1->setCurrentIndex(type1->findText(QStringLiteral("LOWPASS")));
  note1->type(QStringLiteral("2000"));
  ring1->setValue(90);
  QTest::qWait(20);
  const int ring_before = ring1->value();
  const auto edge_centre = edge.plot()->handleCentre(1);
  CHECK(edge_centre.has_value());
  dragOnPlot(edge.plot(), edge_centre->toPoint(), QPoint(0, -40));
  std::printf("plot ring drag %d -> %d\n", ring_before, ring1->value());
  CHECK(ring1->value() > ring_before);
  edge.close();
}

TRENCH_TEST(plot_wheel_rings) {
  MainWindow window;
  window.resize(1600, 700);
  window.show();
  QTest::qWait(60);
  auto& state = window.state();
  auto* table = window.findChild<RowsTable*>(QStringLiteral("rowsTable"));
  auto* ring = window.findChild<NumberBox*>(QStringLiteral("ringLo0"));
  CHECK(table != nullptr && ring != nullptr);
  seatEqRow(window, 0, QStringLiteral("500"));
  table->followNote(RowsTable::Side::kLo, 0);
  QTest::qWait(20);
  CHECK(ring->following());
  CascadePlot* plot = window.plot();
  const auto centre = plot->handleCentre(0);
  CHECK(centre.has_value());
  const std::uint16_t before = state.packed().words[0][0][3];
  QWheelEvent wheel(*centre, plot->mapToGlobal(*centre), QPoint(0, 0), QPoint(0, 120),
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QApplication::sendEvent(plot, &wheel);
  std::printf("wheel over the handle: rsq %04X -> %04X\n", before,
              state.packed().words[0][0][3]);
  CHECK(state.packed().words[0][0][3] != before);
  CHECK(!ring->following());
  window.close();
}

TRENCH_TEST(plot_follows_the_wheel) {
  MainWindow window;
  window.resize(1600, 700);
  window.show();
  QTest::qWait(60);
  auto& state = window.state();
  seatPole(state, 0, 0, 1'000.0, 90.0);
  seatPole(state, 1, 0, 2'000.0, 180.0);
  state.setEditingCorner(0);
  CascadePlot* plot = window.plot();
  CHECK(plot != nullptr);
  const double lo_hz = pole(state, 0, 0).hz;
  const double hi_hz = pole(state, 1, 0).hz;
  std::printf("wheel corners: LO %.1f Hz, HI %.1f Hz\n", lo_hz, hi_hz);

  state.setPadPosition(0.0, 0.0);
  QTest::qWait(20);
  const double at_lo = plot->curveDbAt(1'000.0);
  const double own_lo = cornerDbAt(state, 0.0, 0.0, 1'000.0);
  const double peak_lo = plotPeakHz(plot, 200.0, 8'000.0);
  std::printf("pad 0: curve %+.2f dB at 1 kHz, corner %+.2f dB, peak %.1f Hz\n", at_lo,
              own_lo, peak_lo);
  CHECK(std::abs(at_lo - own_lo) <= 1.0);
  CHECK_NEAR(peak_lo, 1'000.0, 20.0);

  state.setPadPosition(0.5, 0.0);
  QTest::qWait(20);
  const double peak_mid = plotPeakHz(plot, 200.0, 8'000.0);
  const double at_mid = plot->curveDbAt(1'000.0);
  std::printf("pad 0.5: peak %.1f Hz, curve %+.2f dB at 1 kHz (%+.2f dB)\n", peak_mid,
              at_mid, at_mid - at_lo);
  CHECK(peak_mid > 1'300.0 && peak_mid < 1'550.0);
  CHECK(at_lo - at_mid >= 3.0);

  state.setPadPosition(1.0, 0.0);
  QTest::qWait(20);
  const double peak_hi = plotPeakHz(plot, 200.0, 8'000.0);
  std::printf("pad 1: peak %.1f Hz\n", peak_hi);
  CHECK_NEAR(peak_hi, 2'000.0, 40.0);
  window.close();
}

TRENCH_TEST(plot_handle_rides_the_interpolated_pole) {
  MainWindow window;
  window.resize(1600, 700);
  window.show();
  QTest::qWait(60);
  auto& state = window.state();
  seatPole(state, 0, 0, 1'000.0, 90.0);
  seatPole(state, 1, 0, 2'000.0, 180.0);
  state.setEditingCorner(0);
  state.setPadPosition(0.5, 0.0);
  QTest::qWait(20);
  CascadePlot* plot = window.plot();
  CHECK(plot != nullptr);
  const double lo_before = pole(state, 0, 0).hz;
  const double hi_before = pole(state, 1, 0).hz;
  const double blended = std::sqrt(lo_before * hi_before);
  const auto centre = plot->handleCentre(0);
  CHECK(centre.has_value());
  const double want_x = plot->xForFrequency(blended);
  std::printf("handle x %.1f px, interpolated pole %.1f Hz at x %.1f px\n", centre->x(),
              blended, want_x);
  CHECK_NEAR(centre->x(), want_x, std::abs(want_x) * 0.02);

  const double span = plot->xForFrequency(20'000.0) - plot->xForFrequency(20.0);
  CHECK(span > 0.0);
  const double ratio = std::pow(1'000.0, 60.0 / span);
  dragOnPlot(plot, centre->toPoint(), QPoint(60, 0));
  const double lo_after = pole(state, 0, 0).hz;
  std::printf("drag ratio %.4f: LO %.1f Hz -> %.1f Hz (want %.1f Hz), HI %.1f Hz\n", ratio,
              lo_before, lo_after, lo_before * ratio, pole(state, 1, 0).hz);
  CHECK_NEAR(lo_after, lo_before * ratio, lo_before * ratio * 0.03);
  CHECK_NEAR(pole(state, 1, 0).hz, hi_before, 1e-9);
  window.close();
}

TRENCH_TEST(anchor_pairs_slots_by_nearest_log_frequency) {
  EditorState state;
  state.setDocument(EditorState::blank());
  const std::array<double, 3> held{3'230.0, 4'378.0, 10'404.0};
  for (std::size_t section = 0; section < held.size(); ++section) {
    seatPole(state, 0, section, held[section], held[section] / 30.0);
    state.setZeroAt(0, section, held[section], held[section] / 8.0);
  }
  const std::array<double, 4> incoming{11'417.0, 6'840.0, 5'627.0, 4'607.0};
  std::vector<trench::app::TypeRow> rows;
  for (const double hz : incoming) {
    rows.push_back({trench::app::RowType::kEq, hz, hz / 30.0, 12.0});
  }
  trench::app::applyTypeRows(state, rows, 1);
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(1, section)) continue;
    std::printf("anchor slot %zu -> %.0f Hz\n", section, pole(state, 1, section).hz);
  }
  CHECK_NEAR(pole(state, 1, 0).hz, 4'607.0, 4'607.0 * 0.01);
  CHECK_NEAR(pole(state, 1, 1).hz, 5'627.0, 5'627.0 * 0.01);
  CHECK_NEAR(pole(state, 1, 2).hz, 11'417.0, 11'417.0 * 0.01);
  bool loose = false;
  for (std::size_t section = 3; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(1, section)) continue;
    if (std::abs(pole(state, 1, section).hz - 6'840.0) <= 68.4) loose = true;
  }
  CHECK(loose);
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(1, section)) continue;
    CHECK(state.zeroPresentAt(1, section));
    CHECK_NEAR(zero(state, 1, section).hz, pole(state, 1, section).hz,
               pole(state, 1, section).hz * 0.02);
  }
}

TRENCH_TEST(square_anchor_pairs_against_both_partners) {
  EditorState state;
  state.setDocument(EditorState::blank());
  const std::array<double, 3> morph_held{3'230.0, 4'378.0, 10'404.0};
  const std::array<double, 3> q_held{3'300.0, 4'500.0, 10'000.0};
  for (std::size_t section = 0; section < morph_held.size(); ++section) {
    seatPole(state, 0, section, morph_held[section], morph_held[section] / 30.0);
    state.setZeroAt(0, section, morph_held[section], morph_held[section] / 8.0);
    seatPole(state, 2, section, q_held[section], q_held[section] / 30.0);
    state.setZeroAt(2, section, q_held[section], q_held[section] / 8.0);
  }
  const auto land = [&state](std::size_t corner, const std::vector<double>& incoming) {
    std::vector<trench::app::TypeRow> rows;
    for (const double hz : incoming) {
      rows.push_back({trench::app::RowType::kEq, hz, hz / 30.0, 12.0});
    }
    state.beginUndoGroup();
    trench::app::applyTypeRows(state, rows, corner, false);
    state.anchorCornerToSquare(corner);
    state.endUndoGroup();
  };

  land(1, {11'417.0, 6'840.0, 5'627.0, 4'607.0});
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(1, section)) continue;
    std::printf("square corner 2 slot %zu -> %.0f Hz\n", section, pole(state, 1, section).hz);
  }
  CHECK_NEAR(pole(state, 1, 0).hz, 4'607.0, 4'607.0 * 0.01);
  CHECK_NEAR(pole(state, 1, 1).hz, 5'627.0, 5'627.0 * 0.01);
  CHECK_NEAR(pole(state, 1, 2).hz, 11'417.0, 11'417.0 * 0.01);
  bool loose = false;
  for (std::size_t section = 3; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(1, section)) continue;
    if (std::abs(pole(state, 1, section).hz - 6'840.0) <= 68.4) loose = true;
  }
  CHECK(loose);

  land(3, {12'000.0, 4'700.0, 5'800.0});
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(3, section)) continue;
    std::printf("square corner 4 slot %zu -> %.0f Hz\n", section, pole(state, 3, section).hz);
  }
  CHECK_NEAR(pole(state, 3, 0).hz, 4'700.0, 4'700.0 * 0.01);
  CHECK_NEAR(pole(state, 3, 1).hz, 5'800.0, 5'800.0 * 0.01);
  CHECK_NEAR(pole(state, 3, 2).hz, 12'000.0, 12'000.0 * 0.01);
}

TRENCH_TEST(anchor_square_reorders_the_three_partners) {
  EditorState state;
  state.setDocument(EditorState::blank());
  const std::array<double, 3> reference{3'230.0, 4'378.0, 10'404.0};
  const std::array<double, 3> across{3'300.0, 4'500.0, 10'000.0};
  for (std::size_t section = 0; section < reference.size(); ++section) {
    seatPole(state, 0, section, reference[section], reference[section] / 30.0);
    seatPole(state, 2, section, across[section], across[section] / 30.0);
  }
  const auto low = state.packed().words[2][0];
  const auto high = state.packed().words[2][2];
  state.setWordsAt(2, 0, EditorState::Lane::kPole, high[2], high[3]);
  state.setWordsAt(2, 2, EditorState::Lane::kPole, low[2], low[3]);
  CHECK_NEAR(pole(state, 2, 0).hz, 10'000.0, 100.0);
  CHECK_NEAR(pole(state, 2, 2).hz, 3'300.0, 33.0);

  std::array<double, 3> held{};
  for (std::size_t section = 0; section < held.size(); ++section) {
    held[section] = pole(state, 0, section).hz;
  }
  state.anchorSquare();
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(2, section)) continue;
    std::printf("anchor square corner 3 slot %zu -> %.0f Hz\n", section,
                pole(state, 2, section).hz);
  }
  CHECK_NEAR(pole(state, 2, 0).hz, 3'300.0, 33.0);
  CHECK_NEAR(pole(state, 2, 2).hz, 10'000.0, 100.0);
  for (std::size_t section = 0; section < held.size(); ++section) {
    CHECK_NEAR(pole(state, 0, section).hz, held[section], 1.0e-9);
  }
  state.undo();
  CHECK_NEAR(pole(state, 2, 0).hz, 10'000.0, 100.0);
  CHECK_NEAR(pole(state, 2, 2).hz, 3'300.0, 33.0);
}

TRENCH_TEST(word_lerp_is_a_log_lerp) {
  EditorState state;
  state.setDocument(EditorState::blank());
  constexpr double kBwHz = 120.0;
  constexpr double kLoPoleHz = 3'230.0;
  constexpr double kHiPoleHz = 4'607.0;
  seatPole(state, 0, 0, kLoPoleHz, kBwHz);
  seatPole(state, 1, 0, kHiPoleHz, kBwHz);
  state.setPadPosition(0.5, 0.0);
  const double landed = poleHzOf(state.cascade(EditorState::kDatumHz)[0]);
  const double want = std::sqrt(kLoPoleHz * kHiPoleHz);
  std::printf("word lerp midpoint %.1f Hz vs log midpoint %.1f Hz (%+.2f%%)\n", landed,
              want, 100.0 * (landed / want - 1.0));
  CHECK_NEAR(landed, want, want * 0.03);
}

TRENCH_TEST(plot_shows_one_glide_per_paired_slot) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  const std::array<double, 2> lo{3'230.0, 4'378.0};
  const std::array<double, 2> hi{4'607.0, 5'627.0};
  for (std::size_t section = 0; section < lo.size(); ++section) {
    seatPole(state, 0, section, lo[section], 120.0);
    seatPole(state, 1, section, hi[section], 120.0);
  }
  seatPole(state, 0, 3, 900.0, 60.0);
  state.setPadPosition(0.5, 0.0);
  QTest::qWait(20);
  CascadePlot* plot = window.plot();
  CHECK(plot != nullptr);
  CHECK(plot->glideCount() == 2);
  const auto now = plot->glideNowHz(0);
  CHECK(now.has_value());
  const double want = std::sqrt(lo[0] * hi[0]);
  std::printf("glide now %.1f Hz vs log midpoint %.1f Hz\n", *now, want);
  CHECK_NEAR(*now, want, want * 0.03);
  CHECK(!plot->glideNowHz(3).has_value());
  window.close();
}

TRENCH_TEST(keyframe_pick_lands_rows) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  const auto keyframes = trench::app::loadKeyframes();
  CHECK(keyframes.size() > 100);
  window.openFrames();
  grid->setSearch(QString());
  grid->setGroup(QStringLiteral("HEAD"));
  const int head = grid->tileCount();
  const int pick = grid->tileNamed(QStringLiteral("left ear az 0 el 0"));
  std::printf("grid has %d entries, HEAD holds %d tiles, pick at %d\n", grid->entryCount(), head,
              pick);
  CHECK(head == 35);
  CHECK(pick >= 0);
  const auto before = state.document();
  grid->pickTile(pick);
  CHECK(state.document() != before);
  CHECK(!grid->isVisible());
  const std::array<double, 3> want{3'230.0, 4'378.0, 10'404.0};
  for (std::size_t section = 0; section < want.size(); ++section) {
    CHECK(state.sectionEnabledAt(0, section));
    const auto& p = pole(state, 0, section);
    std::printf("keyframe row %zu pole %.1f Hz wants %.1f Hz\n", section, p.hz, want[section]);
    CHECK_NEAR(p.hz, want[section], want[section] * 0.01);
    CHECK(state.zeroPresentAt(0, section));
    const auto& z = zero(state, 0, section);
    CHECK_NEAR(z.hz, p.hz, p.hz * 0.01);
  }
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(keyframe_pick_speech_lands_poles) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  const auto keyframes = trench::app::loadKeyframes();
  QString wanted;
  for (const auto& key : keyframes) {
    if (key.group != QStringLiteral("VOWEL DVTD")) continue;
    wanted = key.name;
    break;
  }
  CHECK(!wanted.isEmpty());
  window.openFrames();
  grid->setSearch(QString());
  grid->setGroup(QStringLiteral("VOWEL DVTD"));
  const int pick = grid->tileNamed(wanted);
  CHECK(pick >= 0);
  const auto before = state.document();
  grid->pickTile(pick);
  CHECK(state.document() != before);
  int seated = 0;
  for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
    if (!state.sectionEnabledAt(0, section)) continue;
    ++seated;
    std::printf("speech keyframe row %zu pole %.1f Hz zero %d\n", section,
                pole(state, 0, section).hz,
                static_cast<int>(state.zeroPresentAt(0, section)));
    CHECK(!state.zeroPresentAt(0, section));
  }
  CHECK(seated >= 4);
  state.undo();
  CHECK(state.document() == before);
  window.close();
}

TRENCH_TEST(row_from_keyframe_writes_that_slot) {
  EditorState state;
  state.setDocument(EditorState::blank());
  RowsTable table(&state);
  const auto keyframes = trench::app::loadKeyframes();
  const trench::app::Keyframe* chosen = nullptr;
  for (const auto& key : keyframes) {
    if (key.name == QStringLiteral("left ear az 0 el 0")) chosen = &key;
  }
  CHECK(chosen != nullptr);
  const auto frame = trench::app::keyframeFrame(*chosen);
  const auto want = trench::app::keyframeSlotWords(*chosen, 1);
  const auto before = state.document();
  table.rowFromFrame(3, frame, 1);
  CHECK(state.sectionEnabledAt(0, 3));
  const auto& landed = state.packed().words[0][3];
  for (std::size_t index = 0; index < 4; ++index) {
    std::printf("row from keyframe word %zu %04X wants %04X\n", index, landed[index],
                want[index]);
    CHECK(landed[index] == want[index]);
  }
  CHECK(trench::app::keyframeSlotWords(*chosen, 6) == trench::core::kIdentitySection);
  state.undo();
  CHECK(state.document() == before);
  table.close();
}

TRENCH_TEST(frames_hover_view_is_the_frame_on_every_corner) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  window.openFrames();
  grid->setGroup(QStringLiteral("ALL"));
  grid->setSearch(QStringLiteral("az 0 el 0"));
  const int tile = grid->tileNamed(QStringLiteral("left ear az 0 el 0"));
  CHECK(tile >= 0);
  const KeyframeGrid::Entry* entry = grid->entryAt(tile);
  CHECK(entry != nullptr);
  const trench::audio::AuditionView view = window.frameView(*entry);
  for (std::size_t corner = 1; corner < trench::core::native::kCorners; ++corner) {
    CHECK(view.packed.words[corner] == view.packed.words[0]);
  }
  trench::audio::AuditionView plain = view;
  plain.trim_db = 0.0;
  CHECK_NEAR(view.trim_db, trench::audio::level_trim_db(plain, EditorState::kDatumHz), 1e-9);
  trench::audio::AuditionView middle = plain;
  middle.morph = 0.5F;
  middle.q = 0.5F;
  const std::vector<double> hz{3'230.0, 4'378.0, 10'404.0};
  const auto want = grid->responseOn(tile, hz);
  CHECK(want.size() == hz.size());
  const auto cascade = trench::audio::design_audition(middle, EditorState::kDatumHz);
  for (std::size_t index = 0; index < hz.size(); ++index) {
    const double got = trench::core::cascade_response_db(
        std::span<const trench::core::Biquad>(cascade.data(), cascade.size()), hz[index],
        EditorState::kDatumHz);
    std::printf("frame view at %.0f Hz %.2f dB wants %.2f dB, trim %.2f dB\n", hz[index], got,
                want[index], view.trim_db);
    CHECK_NEAR(got, want[index], 1.0);
  }
  grid->hide();
  window.close();
}

TRENCH_TEST(frames_hover_calls_the_audition) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  window.openFrames();
  grid->setGroup(QStringLiteral("ALL"));
  grid->setSearch(QStringLiteral("az 0 el 0"));
  const int tile = grid->tileNamed(QStringLiteral("left ear az 0 el 0"));
  CHECK(tile >= 0);
  const KeyframeGrid::Entry* entry = grid->entryAt(tile);
  CHECK(entry != nullptr);
  const trench::audio::AuditionView want = window.frameView(*entry);
  int heard = 0;
  trench::audio::AuditionView last{};
  window.auditionTap = [&heard, &last](const trench::audio::AuditionView& view) {
    ++heard;
    last = view;
  };
  grid->hoverTileForTest(tile);
  std::printf("hover handed the audition %d views\n", heard);
  CHECK(heard == 1);
  for (std::size_t corner = 0; corner < trench::core::kCornerCount; ++corner) {
    CHECK(last.packed.words[corner] == want.packed.words[corner]);
  }
  CHECK_NEAR(last.trim_db, want.trim_db, 1e-9);
  grid->hoverLeaveForTest();
  std::printf("hover leave handed the audition %d views\n", heard);
  CHECK(heard == 2);
  const auto held = window.state().view();
  for (std::size_t corner = 0; corner < trench::core::kCornerCount; ++corner) {
    CHECK(last.packed.words[corner] == held.packed.words[corner]);
  }
  window.auditionTap = nullptr;
  grid->hide();
  window.close();
}

TRENCH_TEST(frames_grid_opens_without_a_device) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  CHECK(!window.auditionRunning());
  window.openFrames();
  CHECK(grid->isVisible());
  std::printf("grid opened with no device, %d tiles, audition running %d\n", grid->tileCount(),
              static_cast<int>(window.auditionRunning()));
  CHECK(grid->tileCount() > 0);
  grid->hide();
  CHECK(!grid->isVisible());
  CHECK(!window.auditionRunning());
  window.close();
}

TRENCH_TEST(vowel_space_mapping_round_trips) {
  VowelSpace space;
  space.resize(640, 400);
  const std::array<std::pair<double, double>, 4> want{
      std::pair<double, double>{300.0, 2300.0}, std::pair<double, double>{500.0, 1500.0},
      std::pair<double, double>{700.0, 1100.0}, std::pair<double, double>{800.0, 1000.0}};
  for (const auto& one : want) {
    const auto back = space.formantsAt(space.pointFor(one.first, one.second));
    std::printf("round trip F1 %.0f F2 %.0f reads %.3f %.3f\n", one.first, one.second, back.first,
                back.second);
    CHECK_NEAR(back.first, one.first, 1.0);
    CHECK_NEAR(back.second, one.second, 1.0);
  }
  const QPointF left_edge = space.pointFor(500.0, 99'999.0);
  const QPointF right_edge = space.pointFor(500.0, 1.0);
  const auto widest = space.formantsAt(left_edge);
  const auto narrowest = space.formantsAt(right_edge);
  const auto clamped = space.formantsAt(QPointF(left_edge.x() - 50.0, left_edge.y()));
  std::printf("fifty px outside the left edge reads F1 %.1f F2 %.1f, row runs %.1f to %.1f\n",
              clamped.first, clamped.second, widest.second, narrowest.second);
  CHECK(clamped.first >= 200.0);
  CHECK(clamped.first <= 850.0);
  CHECK(clamped.second <= widest.second + 1.0);
  CHECK(clamped.second >= narrowest.second - 1.0);
  CHECK_NEAR(clamped.first, 500.0, 1.0);
}

TRENCH_TEST(vowel_space_marks_are_the_table) {
  VowelSpace space;
  space.resize(640, 400);
  std::printf("vowel space carries %d marks\n", space.markCount());
  CHECK(space.markCount() == 48);
  const VowelSpace::Mark* heed = nullptr;
  for (const auto& mark : space.marks()) {
    if (mark.name == QStringLiteral("ee heed man")) heed = &mark;
  }
  CHECK(heed != nullptr);
  std::printf("man ee sits at F1 %.1f F2 %.1f labelled %s\n", heed->f1, heed->f2,
              heed->label.toUtf8().constData());
  CHECK_NEAR(heed->f1, 338.0, 1.0);
  CHECK_NEAR(heed->f2, 2319.0, 1.0);
  CHECK(heed->man);
  CHECK(heed->label == QStringLiteral("ee"));
}

TRENCH_TEST(vowel_space_click_lands_four_poles) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto& state = window.state();
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  window.openFrames();
  grid->setGroup(QStringLiteral("VOWEL SPACE"));
  CHECK(grid->group() == QStringLiteral("VOWEL SPACE"));
  auto* space = grid->vowelSpace();
  CHECK(space != nullptr);
  CHECK(space->isVisible());
  space->setFormants(500.0, 1500.0);
  const trench::app::Keyframe key = space->frame();
  CHECK(key.group == QStringLiteral("VOWEL SPACE"));
  CHECK(key.mode == QStringLiteral("poles"));
  CHECK(key.rows.size() == 4);
  for (const auto& row : key.rows) CHECK(row.type == trench::app::RowType::kPole);
  std::printf("vowel frame poles %.1f %.1f %.1f %.1f\n", key.rows[0].hz, key.rows[1].hz,
              key.rows[2].hz, key.rows[3].hz);
  CHECK_NEAR(key.rows[0].hz, 500.0, 1.0);
  CHECK_NEAR(key.rows[1].hz, 1'500.0, 1.0);
  CHECK(key.rows[2].hz >= 2'200.0);
  CHECK(key.rows[2].hz <= 2'800.0);
  CHECK_NEAR(key.rows[3].hz, 1.4 * key.rows[2].hz, 1.0);
  space->pickForTest(false);
  CHECK(!grid->isVisible());
  for (std::size_t section = 0; section < key.rows.size(); ++section) {
    CHECK(state.sectionEnabledAt(0, section));
    const auto& landed = pole(state, 0, section);
    std::printf("corner 0 row %zu pole %.1f Hz wants %.1f Hz\n", section, landed.hz,
                key.rows[section].hz);
    CHECK_NEAR(landed.hz, key.rows[section].hz, key.rows[section].hz * 0.01);
  }
  window.openFrames();
  grid->setGroup(QStringLiteral("VOWEL SPACE"));
  space->setFormants(300.0, 2'300.0);
  const trench::app::Keyframe partner = space->frame();
  space->pickForTest(true);
  CHECK(grid->isVisible());
  for (const auto& row : partner.rows) {
    bool seated = false;
    for (std::size_t section = 0; section < trench::core::native::kSections; ++section) {
      if (!state.sectionEnabledAt(1, section)) continue;
      if (std::abs(pole(state, 1, section).hz - row.hz) <= row.hz * 0.01) seated = true;
    }
    std::printf("corner 1 holds %.1f Hz %d\n", row.hz, static_cast<int>(seated));
    CHECK(seated);
  }
  grid->hide();
  window.close();
}

TRENCH_TEST(vowel_space_hover_hears_the_frame) {
  MainWindow window;
  window.show();
  QTest::qWait(30);
  auto* grid = window.keyframeGrid();
  CHECK(grid != nullptr);
  window.openFrames();
  grid->setGroup(QStringLiteral("VOWEL SPACE"));
  auto* space = grid->vowelSpace();
  CHECK(space != nullptr);
  int heard = 0;
  trench::audio::AuditionView last{};
  window.auditionTap = [&heard, &last](const trench::audio::AuditionView& view) {
    ++heard;
    last = view;
  };
  space->hoverForTest(600.0, 1'200.0);
  std::printf("vowel hover handed the audition %d views\n", heard);
  CHECK(heard == 1);
  const trench::audio::AuditionView want = window.frameView(space->frame());
  for (std::size_t corner = 0; corner < trench::core::kCornerCount; ++corner) {
    CHECK(last.packed.words[corner] == want.packed.words[corner]);
  }
  CHECK_NEAR(last.trim_db, want.trim_db, 1e-9);
  QEvent leaving(QEvent::Leave);
  QApplication::sendEvent(space, &leaving);
  std::printf("leaving the chart handed the audition %d views\n", heard);
  CHECK(heard == 2);
  const auto held = window.state().view();
  for (std::size_t corner = 0; corner < trench::core::kCornerCount; ++corner) {
    CHECK(last.packed.words[corner] == held.packed.words[corner]);
  }
  window.auditionTap = nullptr;
  grid->hide();
  window.close();
}
