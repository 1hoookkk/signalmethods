#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "trench/core/p2k.hpp"

namespace p2k = trench::core::p2k;
using nlohmann::json;

namespace {

const json& fixture() {
  static const json f = [] {
    const std::string path =
        std::string(TRENCH_SOURCE_ROOT) + "/native/trench-core/tests/fixtures/p2k_parity.json";
    std::ifstream in(path);
    return json::parse(in);
  }();
  return f;
}

p2k::CornerWords words4(const json& v) {
  p2k::CornerWords out{};
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    for (std::size_t wi = 0; wi < 4; ++wi) {
      out[si][wi] = v[si][wi].get<std::uint16_t>();
    }
  }
  return out;
}

double pole_hz_of(const p2k::StageWords& w) {
  const auto [p, q] = p2k::pq(w[2], w[3]);
  const double denom = 2.0 * std::sqrt(std::max(q, 0.0));
  if (denom <= 1e-12) {
    return -1.0;
  }
  const double cos_w = -p / denom;
  if (!(cos_w >= -1.0 && cos_w <= 1.0)) {
    return -1.0;
  }
  return std::acos(cos_w) * p2k::kSr / (2.0 * std::numbers::pi);
}

double pole_radius_of(const p2k::StageWords& w) {
  const auto [p, q] = p2k::pq(w[2], w[3]);
  return p2k::pair_radius(p, q);
}

std::uint32_t conjugate_poles(const p2k::CornerWords& w) {
  std::uint32_t mask = 0;
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    if (pole_hz_of(w[si]) > 0.0) {
      mask |= p2k::pole_bit(si);
    }
  }
  return mask;
}

p2k::CornerWords detune_radii(const p2k::CornerWords& w, std::uint32_t live, int rungs) {
  p2k::CornerWords out = w;
  const auto& lat = p2k::lattice_decoded();
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    if (!p2k::pole_free(live, si)) {
      continue;
    }
    const auto here = static_cast<int>(p2k::nearest_lattice_word(w[si][3]));
    const int moved = std::clamp(here + rungs, 0, static_cast<int>(p2k::lattice_len()) - 1);
    const double r = std::sqrt(std::max(1.0 - lat[static_cast<std::size_t>(moved)], 0.0));
    const auto [mag, rsq] = p2k::words_from_root(pole_hz_of(w[si]), r);
    const auto [p, q] = p2k::pq(mag, rsq);
    if (!p2k::is_legal(p, q, true) ||
        !p2k::magnitude_admissible(p2k::nearest_lattice_word(mag), true)) {
      continue;
    }
    out[si][2] = mag;
    out[si][3] = rsq;
  }
  return out;
}

p2k::CornerWords shift_poles(const p2k::CornerWords& w, double factor) {
  p2k::CornerWords out = w;
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    const double hz = pole_hz_of(w[si]);
    if (hz <= 0.0) {
      continue;
    }
    const double want = std::clamp(hz * factor, 40.0, 0.9 * p2k::kRootHiHz);
    const auto [mag, rsq] = p2k::words_from_root(want, pole_radius_of(w[si]));
    const auto [p, q] = p2k::pq(mag, rsq);
    if (!p2k::is_legal(p, q, true) ||
        !p2k::magnitude_admissible(p2k::nearest_lattice_word(mag), true)) {
      continue;
    }
    out[si][2] = mag;
    out[si][3] = rsq;
  }
  return out;
}

}  // namespace

TEST(P2kWidths, PerturbedRadiiAreRecoveredAndEveryPoleFrequencyIsHeld) {
  double worst_rms = 0.0;
  double worst_drift = 0.0;
  for (const auto& c : fixture()["corners"]) {
    const auto truth = words4(c["polished_words"]);
    const auto target = p2k::Corner::from_words(truth).total();
    const std::uint32_t live = conjugate_poles(truth);
    ASSERT_NE(live, 0U) << "corner " << c["corner"] << " has no conjugate pole pair";

    const auto start = detune_radii(truth, live, 5);
    const auto fit = p2k::fit_pole_widths(target, start, live);
    ASSERT_TRUE(fit.has_value()) << "corner " << c["corner"];

    EXPECT_LT(fit->shape_rms_db, 0.5)
        << "corner " << c["corner"] << " recovered rms " << fit->shape_rms_db;
    EXPECT_LT(std::abs(p2k::dc_gain_db(fit->packed)), 0.1) << "corner " << c["corner"];

    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      EXPECT_EQ(fit->words[si][0], start[si][0]) << "corner " << c["corner"] << " zero mag " << si;
      EXPECT_EQ(fit->words[si][1], start[si][1]) << "corner " << c["corner"] << " zero rsq " << si;
      if (!p2k::pole_free(live, si)) {
        EXPECT_EQ(fit->words[si][2], start[si][2]) << "corner " << c["corner"] << " dead " << si;
        EXPECT_EQ(fit->words[si][3], start[si][3]) << "corner " << c["corner"] << " dead " << si;
        continue;
      }
      const double held = pole_hz_of(start[si]);
      const double drift = std::abs(fit->pole_hz[si] - held);
      EXPECT_EQ(fit->words[si][2], p2k::mag_word_for(held, fit->words[si][3]))
          << "corner " << c["corner"] << " section " << si << " left the held frequency";
      EXPECT_LE(drift, p2k::kHeldHzTolerance * held)
          << "corner " << c["corner"] << " section " << si << " drifted to " << fit->pole_hz[si]
          << " from " << held;
      EXPECT_GT(fit->pole_bw_hz[si], 0.0);
      worst_drift = std::max(worst_drift, drift / held);
    }
    worst_rms = std::max(worst_rms, fit->shape_rms_db);
  }
  RecordProperty("worst_recovered_rms_db", worst_rms);
  RecordProperty("worst_hz_drift_fraction", worst_drift);
  std::cout << "width refit: worst recovered rms " << worst_rms << " dB, worst pole drift "
            << 100.0 * worst_drift << " %" << std::endl;
}

TEST(P2kWidths, AMisplacedTargetConvergesWithoutMovingAnyPole) {
  const auto& c = fixture()["corners"][0];
  const auto truth = words4(c["polished_words"]);
  const std::uint32_t live = conjugate_poles(truth);
  const auto target = p2k::Corner::from_words(shift_poles(truth, 2.0)).total();

  const auto fit = p2k::fit_pole_widths(target, truth, live);
  ASSERT_TRUE(fit.has_value());
  EXPECT_GT(fit->shape_rms_db, 1.0) << "a misplaced target should not fit well: "
                                    << fit->shape_rms_db;

  double worst_drift = 0.0;
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    EXPECT_EQ(fit->words[si][0], truth[si][0]) << "zero mag " << si;
    EXPECT_EQ(fit->words[si][1], truth[si][1]) << "zero rsq " << si;
    if (!p2k::pole_free(live, si)) {
      continue;
    }
    const double held = pole_hz_of(truth[si]);
    const double drift = std::abs(fit->pole_hz[si] - held);
    EXPECT_EQ(fit->words[si][2], p2k::mag_word_for(held, fit->words[si][3])) << "section " << si;
    EXPECT_LE(drift, p2k::kHeldHzTolerance * held)
        << "section " << si << " drifted to " << fit->pole_hz[si] << " from " << held;
    worst_drift = std::max(worst_drift, drift / held);
  }
  RecordProperty("misplaced_rms_db", fit->shape_rms_db);
  std::cout << "misplaced target: rms " << fit->shape_rms_db << " dB, worst pole drift "
            << 100.0 * worst_drift << " %" << std::endl;
}
