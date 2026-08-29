#include "harness.hpp"

#include "body_io.hpp"
#include "editor_state.hpp"
#include "trench/audio/audition.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace {

namespace native = trench::core::native;
namespace p2k = trench::core::p2k;
using trench::core::Biquad;
using trench::core::Cascade;
using trench::core::PackedBody;
using Lane = EditorState::Lane;

constexpr std::array<double, 3> kRates{44'100.0, 48'000.0, 96'000.0};
constexpr std::array<std::pair<double, double>, 8> kPositions{{
    {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0},
    {0.25, 0.5}, {0.5, 0.5}, {0.75, 0.25}, {0.33, 0.9}}};

std::vector<double> grid() {
  return trench::core::logarithmic_frequency_grid(20.0, 20'000.0, 240);
}

double responseDb(std::span<const Biquad> sections, double hz, double rate) {
  return trench::core::cascade_response_db(sections, hz, rate);
}

void flatEq(EditorState& state) {
  constexpr std::array<std::pair<double, double>, 6> bands{{
      {80.0, 114.0}, {250.0, 250.0}, {700.0, 700.0}, {2'000.0, 2'000.0}, {5'500.0, 5'500.0}, {12'000.0, 12'000.0}}};
  const std::size_t editing = state.editingCorner();
  for (std::size_t corner = 0; corner < native::kCorners; ++corner) {
    state.setEditingCorner(corner);
    for (std::size_t index = 0; index < native::kSections; ++index) {
      if (!state.sectionEnabled(index)) state.toggleSection(index);
      state.setRoot(index, Lane::kPole, bands[index].first, bands[index].second);
      state.selectSection(index);
      if (!state.rootPresent(index, Lane::kZero)) state.addZeroAt(bands[index].first, bands[index].second);
      state.setRoot(index, Lane::kZero, bands[index].first, bands[index].second);
    }
  }
  state.setEditingCorner(editing);
  state.selectSection(0);
}

void shape(EditorState& state) {
  flatEq(state);
  state.setEditingCorner(1);
  state.setRoot(0, Lane::kPole, 120.0, 90.0);
  state.setRoot(2, Lane::kPole, 900.0, 60.0);
  state.selectSection(1);
  state.removeZero();
  state.toggleSection(2);
  state.setEditingCorner(2);
  state.setRoot(3, Lane::kPole, 2'400.0, 140.0);
  state.toggleSection(4);
  state.setEditingCorner(3);
  state.selectSection(0);
  state.removeZero();
  state.selectSection(5);
  state.removeZero();
  state.setRoot(5, Lane::kPole, 9'000.0, 400.0);
  state.setEditingCorner(0);
}

bool identityRoot(const trench::core::PackedSection& words, std::size_t first) {
  return words[first] == trench::core::kIdentitySection[first] &&
         words[first + 1] == trench::core::kIdentitySection[first + 1];
}

std::vector<std::uint8_t> readAll(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  const QByteArray bytes = file.readAll();
  return {bytes.begin(), bytes.end()};
}

}  // namespace

TRENCH_TEST(cascade_parity_graph_ears_export) {
  EditorState state;
  shape(state);
  const auto hz = grid();
  double worst_db = 0.0;
  for (const double rate : kRates) {
    for (const auto& [morph, q] : kPositions) {
      state.setPadPosition(morph, q);
      const Cascade graph = state.cascade(rate);
      const Cascade ears = trench::audio::design_audition(state.view(), rate);
      const auto bytes = state.packed().legacy_bytes();
      const Cascade exported = trench::audio::design_audition(
          {PackedBody::from_legacy_bytes(bytes), static_cast<float>(morph),
           static_cast<float>(q), 0.0},
          rate);
      for (std::size_t si = 0; si < trench::core::kSectionCount; ++si) {
        for (std::size_t ci = 0; ci < trench::core::kCoefficientCount; ++ci) {
          CHECK(graph[si][ci] == ears[si][ci]);
          CHECK_NEAR(graph[si][ci], exported[si][ci], 1.0e-12);
        }
      }
      for (const double f : hz) {
        const double delta = std::abs(responseDb(graph, f, rate) - responseDb(exported, f, rate));
        worst_db = std::max(worst_db, delta);
        CHECK_NEAR(responseDb(graph, f, rate), responseDb(exported, f, rate), 1.0e-9);
      }
    }
  }
  std::printf("graph == ears == exported bytes, worst |delta| = %.3e dB over %zu rates x %zu positions x %zu points\n",
              worst_db, kRates.size(), kPositions.size(), hz.size());
}

TRENCH_TEST(armadillo_encoding_round_trips) {
  constexpr std::array<std::pair<double, double>, 8> cases{{
      {20.0, 1.0}, {20.0, 20'000.0}, {250.0, 3.0}, {1'000.0, 100.0},
      {8'000.0, 2'000.0}, {21'000.0, 5.0}, {22'049.0, 40.0}, {60.0, 19'000.0}}};
  for (const auto& [hz, bw] : cases) {
    const double r = std::exp(-std::numbers::pi * bw / 44'100.0);
    const double theta = 2.0 * std::numbers::pi * hz / 44'100.0;
    const double b1 = -2.0 * r * std::cos(theta);
    const double b2 = r * r;
    const double k2 = -std::log(1.0 - b2);
    const double k1 = -std::log((b1 + 1.0 + b2) / 4.0);
    const double b2_back = 1.0 - std::exp(-k2);
    const double b1_back = -2.0 + std::exp(-k2) + 4.0 * std::exp(-k1);
    CHECK_NEAR(b1_back, b1, 1.0e-12);
    CHECK_NEAR(b2_back, b2, 1.0e-12);
    const auto roots = native::roots_from_coefficients(b1_back, b2_back, 44'100.0);
    const auto* resonant = std::get_if<native::Resonant>(&roots);
    CHECK(resonant != nullptr);
    CHECK_NEAR(resonant->hz, hz, 1.0e-6 * hz);
    CHECK_NEAR(resonant->bw_hz, bw, 1.0e-6 * bw);
    std::printf("%8.1f Hz / %8.1f bw -> k1 %.4f k2 %.4f (%.1f dB resonance)\n", hz, bw, k1, k2, 8.68 * k2 + 6.02);
    CHECK(k1 >= 0.0 && k2 >= 0.0);
  }
}

TRENCH_TEST(interior_is_the_armadillo_word_lerp_within_the_papers_error) {
  EditorState state;
  state.setEditingCorner(0);
  state.loadPoles({{300.0, 40.0}, {1'200.0, 150.0}, {3'000.0, 300.0}});
  state.setEditingCorner(1);
  state.loadPoles({{600.0, 160.0}, {2'400.0, 75.0}, {2'000.0, 500.0}});
  const auto& packed = state.packed();
  double worst_octaves = 0.0;
  double worst_bw_ratio = 0.0;
  for (const double morph : {0.25, 0.5, 0.75}) {
    state.setPadPosition(morph, 0.0);
    const Cascade shown = state.cascade(44'100.0);
    for (std::size_t si = 0; si < 3; ++si) {
      const double k0m = -std::log(trench::core::decode_word(packed.words[0][si][2]));
      const double k1m = -std::log(trench::core::decode_word(packed.words[1][si][2]));
      const double k0r = -std::log(trench::core::decode_word(packed.words[0][si][3]));
      const double k1r = -std::log(trench::core::decode_word(packed.words[1][si][3]));
      const double d_mag = std::exp(-((1.0 - morph) * k0m + morph * k1m));
      const double d_rsq = std::exp(-((1.0 - morph) * k0r + morph * k1r));
      const auto exact = native::roots_from_coefficients(4.0 * d_mag + d_rsq - 2.0, 1.0 - d_rsq, 44'100.0);
      const auto word = native::roots_from_coefficients(shown[si][3], shown[si][4], 44'100.0);
      const auto* e = std::get_if<native::Resonant>(&exact);
      const auto* w = std::get_if<native::Resonant>(&word);
      CHECK(e != nullptr && w != nullptr);
      const double octaves = std::abs(std::log2(w->hz / e->hz));
      const double bw_ratio = std::abs(std::log(w->bw_hz / e->bw_hz));
      worst_octaves = std::max(worst_octaves, octaves);
      worst_bw_ratio = std::max(worst_bw_ratio, bw_ratio);
      std::printf("morph %.2f S%zu: word-lerp %.1f Hz / %.1f bw, exact k-lerp %.1f Hz / %.1f bw\n",
                  morph, si + 1, w->hz, w->bw_hz, e->hz, e->bw_hz);
    }
  }
  std::printf("word-lerp vs exact k-linear: worst %.4f octaves in frequency, %.1f%% in bandwidth\n",
              worst_octaves, 100.0 * (std::exp(worst_bw_ratio) - 1.0));
  CHECK(worst_octaves < 0.08);
  CHECK(worst_bw_ratio < 0.25);
}

TRENCH_TEST(off_section_exports_identity) {
  EditorState plain;
  EditorState moved;
  flatEq(plain);
  flatEq(moved);
  for (std::size_t corner = 0; corner < native::kCorners; ++corner) {
    plain.setEditingCorner(corner);
    plain.toggleSection(2);
    moved.setEditingCorner(corner);
    moved.setRoot(2, Lane::kPole, 3'000.0, 50.0);
    moved.setRoot(2, Lane::kZero, 400.0, 30.0);
    moved.toggleSection(2);
  }
  CHECK(plain.packed().legacy_bytes() == moved.packed().legacy_bytes());

  const auto hz = grid();
  for (std::size_t corner = 0; corner < native::kCorners; ++corner) {
    const auto& words = plain.packed().words[corner][2];
    CHECK(identityRoot(words, 0));
    CHECK(identityRoot(words, 2));
    const Biquad biquad = trench::core::section_words_to_biquad(words);
    const double reference = trench::core::section_response_db(biquad, hz.front(), 44'100.0);
    for (const double f : hz) {
      CHECK_NEAR(trench::core::section_response_db(biquad, f, 44'100.0), reference, 1.0e-9);
    }
  }

  for (const double rate : kRates) {
    for (const auto& [morph, q] : kPositions) {
      plain.setPadPosition(morph, q);
      const Cascade cascade = plain.cascade(rate);
      const Biquad identity{1.0, 0.0, 0.0, 0.0, 0.0};
      CHECK(cascade[2] == identity);
      Cascade without = cascade;
      without[2] = identity;
      for (const double f : hz) {
        CHECK_NEAR(responseDb(cascade, f, rate), responseDb(without, f, rate), 1.0e-9);
      }
    }
  }
}

TRENCH_TEST(absent_zero_exports_flat) {
  EditorState plain;
  EditorState moved;
  flatEq(plain);
  flatEq(moved);
  plain.setEditingCorner(1);
  plain.selectSection(1);
  plain.removeZero();
  moved.setEditingCorner(1);
  moved.selectSection(1);
  moved.setRoot(1, Lane::kZero, 333.0, 44.0);
  moved.removeZero();
  CHECK(plain.packed().legacy_bytes() == moved.packed().legacy_bytes());

  const auto& packed = plain.packed();
  CHECK(identityRoot(packed.words[1][1], 0));
  CHECK(!identityRoot(packed.words[1][1], 2));
  for (const std::size_t corner : {0u, 2u, 3u}) {
    CHECK(!identityRoot(packed.words[corner][1], 0));
  }

  const auto hz = grid();
  for (const double rate : kRates) {
    plain.setPadPosition(1.0, 0.0);
    const Cascade cascade = plain.cascade(rate);
    const Biquad& section = cascade[1];
    CHECK(section[1] == 0.0);
    CHECK(section[2] == 0.0);
    CHECK(section[3] != 0.0 || section[4] != 0.0);
    const Biquad numerator{section[0], section[1], section[2], 0.0, 0.0};
    const double reference = trench::core::section_response_db(numerator, hz.front(), rate);
    for (const double f : hz) {
      CHECK_NEAR(trench::core::section_response_db(numerator, f, rate), reference, 1.0e-9);
    }
    const auto pole = native::roots_from_coefficients(section[3], section[4], rate);
    const auto* resonant = std::get_if<native::Resonant>(&pole);
    CHECK(resonant != nullptr);
    std::printf("corner 1 S2 pole after export at %.0f Hz: %.2f Hz / %.2f Hz bw (authored 250 / 250)\n",
                rate, resonant->hz, resonant->bw_hz);
  }
}

TRENCH_TEST(legacy_export_size_and_legality) {
  EditorState state;
  shape(state);
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("shape.body240"));
  CHECK(trench::app::saveBody240(state, path).isEmpty());
  CHECK(QFileInfo(path).size() == static_cast<qint64>(trench::core::kLegacyBodyBytes));
  const auto bytes = readAll(path);
  CHECK(bytes.size() == trench::core::kLegacyBodyBytes);
  const auto expected = state.packed().legacy_bytes();
  CHECK(std::equal(bytes.begin(), bytes.end(), expected.begin()));

  const auto packed = PackedBody::from_legacy_bytes(bytes);
  CHECK(packed.is_legacy_representable());
  for (std::size_t corner = 0; corner < native::kCorners; ++corner) {
    for (std::size_t si = 0; si < native::kSections; ++si) {
      const auto& w = packed.words[corner][si];
      const auto [pole_p, pole_q] = p2k::pq(w[2], w[3]);
      CHECK(p2k::is_legal(pole_p, pole_q, true));
      const auto [zero_p, zero_q] = p2k::pq(w[0], w[1]);
      CHECK(p2k::is_legal(zero_p, zero_q, false));
      CHECK(p2k::pole_is_legal({w[0], w[1], w[2], w[3]}));
    }
    p2k::PackedCorner flat{};
    for (std::size_t si = 0; si < native::kSections; ++si) {
      for (std::size_t wi = 0; wi < trench::core::kCoefficientCount; ++wi) {
        flat[si * trench::core::kCoefficientCount + wi] = packed.words[corner][si][wi];
      }
    }
    const double dc_db = p2k::dc_gain_db(flat);
    std::printf("corner %zu exported DC gain %.4f dB (authored 0 dB)\n", corner, dc_db);
    CHECK_NEAR(dc_db, 0.0, 0.5);
  }
  for (const double rate : kRates) {
    for (const auto& [morph, q] : kPositions) {
      const auto design = native::design(native::packed_interior_corner(packed, morph, q), rate);
      for (const auto& section : design) CHECK(native::is_stable(section));
    }
  }
}

TRENCH_TEST(s6_zero_export_law_is_the_existing_one) {
  EditorState state;
  flatEq(state);
  const auto& packed = state.packed();
  for (std::size_t corner = 0; corner < native::kCorners; ++corner) {
    CHECK(packed.words[corner][5][1] == p2k::kS6ZeroRsqWord);
  }
  const auto geometry = trench::core::geometry_from_words(packed.words[0][5], 44'100.0);
  const auto* zero = std::get_if<trench::core::ConjugatePair>(&geometry.zero);
  CHECK(zero != nullptr);
  CHECK_NEAR(zero->radius, p2k::s6_zero_radius(), 1.0e-12);
  CHECK_NEAR(zero->hz, 12'000.0, 0.03 * 12'000.0);
  const double forced_bw_hz = -std::log(zero->radius) * 44'100.0 / std::numbers::pi;
  std::printf("S6 zero authored 12000 Hz / 12000 Hz bw exports as %.2f Hz / %.4f Hz bw\n",
              zero->hz, forced_bw_hz);

  const auto hz = grid();
  const auto authored = native::design(state.body().corners[0], 44'100.0);
  const auto exported = native::design(native::packed_interior_corner(packed, 0.0, 0.0), 44'100.0);
  for (std::size_t si = 0; si < native::kSections; ++si) {
    const Biquad a = native::biquad(authored[si]);
    const Biquad e = native::biquad(exported[si]);
    double worst = 0.0;
    for (const double f : hz) {
      worst = std::max(worst, std::abs(trench::core::section_response_db(a, f, 44'100.0) -
                                       trench::core::section_response_db(e, f, 44'100.0)));
    }
    std::printf("corner 0 S%zu authored vs packed worst |delta| = %.4f dB\n", si + 1, worst);
  }

  EditorState absent;
  flatEq(absent);
  absent.selectSection(5);
  absent.removeZero();
  CHECK(identityRoot(absent.packed().words[0][5], 0));
}
