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
      for (std::size_t si = 0; si < trench::core::kSectionCount; ++si) {
        for (std::size_t ci = 0; ci < trench::core::kCoefficientCount; ++ci) {
          CHECK(graph[si][ci] == ears[si][ci]);
        }
        CHECK(native::is_stable({graph[si][1] / graph[si][0], graph[si][2] / graph[si][0],
                                 graph[si][3], graph[si][4], true}));
      }
      const bool corner = (morph == 0.0 || morph == 1.0) && (q == 0.0 || q == 1.0);
      if (!corner) continue;
      const auto bytes = state.packed().legacy_bytes();
      const auto exported = native::design(
          native::packed_interior_corner(PackedBody::from_legacy_bytes(bytes), morph, q), rate);
      Cascade packed_cascade = native::cascade(exported, 0.0);
      for (const double f : hz) {
        worst_db = std::max(worst_db, std::abs(responseDb(graph, f, rate) - responseDb(packed_cascade, f, rate)));
      }
    }
  }
  std::printf("graph == ears exactly at %zu rates x %zu positions; authored corners vs packed export worst |delta| = %.2f dB (lattice + S6 law)\n",
              kRates.size(), kPositions.size(), worst_db);
}

TRENCH_TEST(interior_follows_the_patent_law) {
  EditorState state;
  state.setEditingCorner(0);
  state.loadPoles({{300.0, 40.0}, {1'200.0, 150.0}});
  state.setEditingCorner(1);
  state.loadPoles({{600.0, 160.0}, {2'400.0, 75.0}});
  state.setEditingCorner(2);
  state.loadPoles({{300.0, 40.0}, {1'200.0, 150.0}});
  state.setEditingCorner(3);
  state.loadPoles({{600.0, 160.0}, {2'400.0, 75.0}});
  for (const double rate : kRates) {
    state.setPadPosition(0.5, 0.0);
    const Cascade middle = state.cascade(rate);
    const std::array<std::pair<double, double>, 2> ends[2] = {
        {{{300.0, 40.0}, {600.0, 160.0}}}, {{{1'200.0, 150.0}, {2'400.0, 75.0}}}};
    for (std::size_t si = 0; si < 2; ++si) {
      const auto pole = native::roots_from_coefficients(middle[si][3], middle[si][4], rate);
      const auto* resonant = std::get_if<native::Resonant>(&pole);
      CHECK(resonant != nullptr);
      const double expected_hz = std::sqrt(ends[si][0].first * ends[si][1].first);
      const double r0 = std::exp(-std::numbers::pi * ends[si][0].second / rate);
      const double r1 = std::exp(-std::numbers::pi * ends[si][1].second / rate);
      const double expected_r = 1.0 - std::sqrt((1.0 - r0) * (1.0 - r1));
      const double expected_bw = -std::log(expected_r) * rate / std::numbers::pi;
      std::printf("S%zu at morph 0.5, %.0f Hz: %.2f Hz / %.2f Hz bw (law %.2f / %.2f)\n", si + 1, rate,
                  resonant->hz, resonant->bw_hz, expected_hz, expected_bw);
      CHECK_NEAR(resonant->hz, expected_hz, 1.0e-6 * expected_hz);
      CHECK_NEAR(resonant->bw_hz, expected_bw, 1.0e-6 * expected_bw);
    }
  }
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
