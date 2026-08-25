#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace p2k = trench::core::p2k;
using nlohmann::json;

namespace {

const json& fixture() {
  static const json f = [] {
    const std::string path =
        std::string(TRENCH_SOURCE_ROOT) + "/native/trench-core/tests/fixtures/p2k_parity.json";
    std::ifstream in(path);
    if (!in) {
      throw std::runtime_error("parity fixture missing: " + path);
    }
    return json::parse(in);
  }();
  return f;
}

std::vector<double> floats(const json& v) { return v.get<std::vector<double>>(); }

p2k::CornerWords words4(const json& v) {
  p2k::CornerWords out{};
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    for (std::size_t wi = 0; wi < 4; ++wi) {
      out[si][wi] = v[si][wi].get<std::uint16_t>();
    }
  }
  return out;
}

p2k::StoredCorner words5(const json& v) {
  p2k::StoredCorner out{};
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    for (std::size_t wi = 0; wi < 5; ++wi) {
      out[si][wi] = v[si][wi].get<std::uint16_t>();
    }
  }
  return out;
}

bool close(double a, double b, double tol) {
  return std::abs(a - b) <= tol * std::max({std::abs(a), std::abs(b), 1.0});
}

std::vector<std::uint8_t> hex_bytes(const std::string& s) {
  std::vector<std::uint8_t> out;
  out.reserve(s.size() / 2);
  for (std::size_t i = 0; i + 1 < s.size(); i += 2) {
    out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
  }
  return out;
}

}  // namespace

TEST(P2kParity, TheContainerConstantsAreTheMeasuredOnes) {
  const auto& f = fixture();
  EXPECT_EQ(f["sr"].get<double>(), p2k::kSr);
  EXPECT_EQ(f["npts"].get<std::size_t>(), p2k::kNpts);
  EXPECT_EQ(f["lo_hz"].get<double>(), p2k::kLoHz);
  EXPECT_EQ(f["hi_hz"].get<double>(), p2k::kHiHz);
  EXPECT_EQ(f["root_hi_hz"].get<double>(), p2k::kRootHiHz);
  EXPECT_EQ(f["max_mag_byte"].get<std::size_t>(), p2k::kMaxMagByte);
  EXPECT_EQ(f["pole_r_max"].get<double>(), p2k::kPoleRMax);
  EXPECT_EQ(f["s6_zero_rsq_word"].get<std::uint16_t>(), p2k::kS6ZeroRsqWord);
}

TEST(P2kParity, TheGridAndItsCriticalBandWeightsAreTheReferenceOnes) {
  const auto& f = fixture();
  const auto& g = p2k::grid();
  const auto hz = floats(f["hz"]);
  const auto weights = floats(f["weights"]);
  for (std::size_t i = 0; i < p2k::kNpts; ++i) {
    ASSERT_EQ(g.hz[i], hz[i]) << "grid point " << i;
    ASSERT_TRUE(close(g.weight[i], weights[i], 1e-15))
        << "weight " << i << ": " << g.weight[i] << " vs " << weights[i];
  }
}

TEST(P2kParity, TheLatticeIsTheSameSetOfWordsAndDecodesTheSame) {
  const auto& f = fixture();
  const auto want_w = f["lattice_words"].get<std::vector<std::uint16_t>>();
  const auto want_d = floats(f["lattice_decoded"]);
  const auto& got_w = p2k::lattice_words();
  ASSERT_EQ(got_w.size(), want_w.size());
  for (std::size_t i = 0; i < want_w.size(); ++i) {
    ASSERT_EQ(got_w[i], want_w[i]) << "lattice candidate " << i;
    ASSERT_EQ(trench::core::decode_word(got_w[i]), want_d[i]) << "lattice candidate " << i;
  }
}

TEST(P2kParity, AStoredWordPairDecodesToTheSamePAndQ) {
  for (const auto& c : fixture()["pq"]) {
    const auto wm = c["w_mag"].get<std::uint16_t>();
    const auto wr = c["w_rsq"].get<std::uint16_t>();
    const auto [p, q] = p2k::pq(wm, wr);
    ASSERT_EQ(p, c["p"].get<double>()) << "p for " << wm << "/" << wr;
    ASSERT_EQ(q, c["q"].get<double>()) << "q for " << wm << "/" << wr;
  }
}

TEST(P2kParity, OneSecondOrderFactorHasTheSameMagnitudeCurve) {
  const auto& f = fixture();
  const auto& g = p2k::grid();
  const auto probe = f["probe_idx"].get<std::vector<std::size_t>>();
  std::vector<double> out(p2k::kNpts, 0.0);
  for (std::size_t ci = 0; ci < f["pq"].size(); ++ci) {
    const auto& c = f["pq"][ci];
    g.factor_db(c["p"].get<double>(), c["q"].get<double>(), out);
    const auto want = floats(f["factor_db_at_probe"][ci]);
    for (std::size_t k = 0; k < probe.size(); ++k) {
      ASSERT_LE(std::abs(out[probe[k]] - want[k]), 1e-9)
          << "case " << ci << " point " << probe[k];
    }
  }
}

TEST(P2kParity, TheContainerAdmitsAndRejectsTheSameCandidates) {
  const auto& f = fixture();
  for (std::size_t ci = 0; ci < f["pq"].size(); ++ci) {
    const auto& c = f["pq"][ci];
    const double p = c["p"].get<double>();
    const double q = c["q"].get<double>();
    const auto& want = f["legal"][ci];
    ASSERT_EQ(p2k::is_legal(p, q, false), want["as_zero"].get<bool>()) << "zero " << ci;
    ASSERT_EQ(p2k::is_legal(p, q, true), want["as_pole"].get<bool>()) << "pole " << ci;
  }
}

TEST(P2kParity, AValueSnapsToTheSameLatticeByte) {
  for (const auto& c : fixture()["nearest_lattice"]) {
    const double v = c["value"].get<double>();
    ASSERT_EQ(p2k::nearest_lattice(v), c["byte"].get<std::size_t>()) << "value " << v;
  }
}

TEST(P2kParity, ARootSnapsToTheSameTwoWords) {
  for (const auto& c : fixture()["words_from_root"]) {
    const double hz = c["hz"].get<double>();
    const double r = c["r"].get<double>();
    const auto [wm, wr] = p2k::words_from_root(hz, r);
    ASSERT_EQ(wm, c["w_mag"].get<std::uint16_t>()) << "mag for " << hz << " Hz r=" << r;
    ASSERT_EQ(wr, c["w_rsq"].get<std::uint16_t>()) << "rsq for " << hz << " Hz r=" << r;
  }
}

TEST(P2kParity, AScaleSnapsToTheSameGainWord) {
  for (const auto& c : fixture()["nearest_gain_word"]) {
    const double s = c["scale"].get<double>();
    ASSERT_EQ(p2k::nearest_gain_word(s), c["word"].get<std::uint16_t>()) << "scale " << s;
  }
}

TEST(P2kParity, TheWeightedResidualVarianceAgrees) {
  const auto& f = fixture();
  const auto target = floats(f["residual_var"]["target"]);
  const auto model = floats(f["residual_var"]["model"]);
  p2k::Scratch s;
  const double got = p2k::residual_var(target, model, s);
  const double want = f["residual_var"]["value"].get<double>();
  ASSERT_TRUE(close(got, want, 1e-14)) << got << " vs " << want;
}

TEST(P2kParity, AFactoryCornerDecodesToTheReferenceResponse) {
  for (const auto& c : fixture()["corners"]) {
    const auto rom = words5(c["rom_words"]);
    const auto got = p2k::corner_response_db(rom);
    const auto want = floats(c["target"]);
    for (std::size_t i = 0; i < p2k::kNpts; ++i) {
      ASSERT_TRUE(close(got[i], want[i], 1e-12))
          << "corner " << c["corner"] << " point " << i << ": " << got[i] << " vs " << want[i];
    }
  }
}

TEST(P2kParity, AFactoryCornerSnapsOntoTheLatticeTheSameWay) {
  for (const auto& c : fixture()["corners"]) {
    const auto got = p2k::rom_seed(words5(c["rom_words"]));
    ASSERT_EQ(got, words4(c["seed_words"])) << "corner " << c["corner"];
  }
}

TEST(P2kParity, OneExhaustiveAxisSweepLandsOnTheSameWord) {
  for (const auto& c : fixture()["corners"]) {
    const auto target = floats(c["target"]);
    auto corner = p2k::Corner::from_words(words4(c["seed_words"]));
    p2k::Scratch s;
    const double seed_var = p2k::corner_var(corner, target, s);
    ASSERT_TRUE(close(seed_var, c["seed_var"].get<double>(), 1e-13))
        << "corner " << c["corner"] << " seed variance";

    const auto& kase = c["sweep_axis"];
    const auto si = kase["stage"].get<std::size_t>();
    const auto wi = kase["word"].get<std::size_t>();
    const auto [v, moved] = p2k::sweep_axis(corner, target, si, wi, seed_var, s);
    ASSERT_EQ(moved, kase["moved"].get<bool>()) << "corner " << c["corner"];
    ASSERT_TRUE(close(v, kase["var"].get<double>(), 1e-13));
    const auto want = kase["words"].get<std::vector<std::uint16_t>>();
    ASSERT_TRUE(std::equal(corner.w[si].begin(), corner.w[si].end(), want.begin(), want.end()))
        << "corner " << c["corner"];
  }
}

TEST(P2kParity, OneConstantFrequencyRadiusMoveLandsOnTheSameWords) {
  for (const auto& c : fixture()["corners"]) {
    const auto target = floats(c["target"]);
    auto corner = p2k::Corner::from_words(words4(c["seed_words"]));
    p2k::Scratch s;
    const double seed_var = p2k::corner_var(corner, target, s);

    const auto& kase = c["sweep_radius"];
    const auto si = kase["stage"].get<std::size_t>();
    const auto root = kase["root"].get<std::size_t>();
    const auto [v, moved] = p2k::sweep_radius(corner, target, si, root, seed_var, s);
    ASSERT_EQ(moved, kase["moved"].get<bool>()) << "corner " << c["corner"];
    ASSERT_TRUE(close(v, kase["var"].get<double>(), 1e-13));
    const auto want = kase["words"].get<std::vector<std::uint16_t>>();
    ASSERT_TRUE(std::equal(corner.w[si].begin(), corner.w[si].end(), want.begin(), want.end()))
        << "corner " << c["corner"];
  }
}

TEST(P2kParity, TheWholeLatticePolishLandsOnTheSameWords) {
  for (const auto& c : fixture()["corners"]) {
    const auto target = floats(c["target"]);
    const auto [corner, rms] = p2k::polish_from_words(words4(c["seed_words"]), target, 24);
    ASSERT_EQ(corner.w, words4(c["polished_words"]))
        << "corner " << c["corner"] << " polished to different words";
    ASSERT_TRUE(close(rms, c["polished_rms"].get<double>(), 1e-9))
        << "corner " << c["corner"] << " rms " << rms << " vs " << c["polished_rms"];
  }
}

TEST(P2kParity, ACrossSeededPolishLandsOnTheSameWords) {
  for (const auto& c : fixture()["corners"]) {
    const auto target = floats(c["target"]);
    const auto [corner, rms] = p2k::polish_from_words(words4(c["cross_seed_words"]), target, 24);
    ASSERT_EQ(corner.w, words4(c["cross_polished_words"]))
        << "corner " << c["corner"] << " polished to different words from a foreign seed";
    ASSERT_TRUE(close(rms, c["cross_polished_rms"].get<double>(), 1e-9))
        << "corner " << c["corner"] << " cross rms " << rms << " vs " << c["cross_polished_rms"];
  }
}

TEST(P2kParity, TheGainPassProducesTheSameScales) {
  for (const auto& c : fixture()["corners"]) {
    const auto corner = p2k::Corner::from_words(words4(c["polished_words"]));
    const auto scales = p2k::stage_gain_pass(corner);
    const auto want = floats(c["scales"]);
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      ASSERT_TRUE(close(scales[si], want[si], 1e-13))
          << "corner " << c["corner"] << " stage " << si << " scale " << scales[si] << " vs "
          << want[si];
    }
  }
}

TEST(P2kParity, ThePackedCornerAndTheWrittenBodyAreByteIdentical) {
  const auto& f = fixture();
  std::array<p2k::PackedCorner, 4> bodies{};
  for (const auto& c : f["corners"]) {
    const auto ci = c["corner"].get<std::size_t>();
    const auto corner = p2k::Corner::from_words(words4(c["polished_words"]));
    const auto scales = p2k::stage_gain_pass(corner);
    const auto packed = p2k::pack_corner(corner, scales);
    const auto want = c["packed_words"].get<std::vector<std::uint16_t>>();
    ASSERT_TRUE(std::equal(packed.begin(), packed.end(), want.begin(), want.end()))
        << "corner " << ci << " packed words";
    bodies[ci] = packed;
  }
  const auto got = p2k::pack_body(bodies);
  const auto want = hex_bytes(f["body_hex"].get<std::string>());
  ASSERT_TRUE(std::equal(got.begin(), got.end(), want.begin(), want.end())) << "240-byte body";
}
