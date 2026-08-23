#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "adversary_grade.hpp"

namespace fs = std::filesystem;
namespace p2k = trench::core::p2k;
namespace nb = trench::core::native;

namespace {

constexpr double kSr = p2k::kSr;
constexpr std::size_t kCaseCount = 7;
constexpr std::array<char, kCaseCount> kCaseLetters{'a', 'b', 'c', 'd', 'e', 'f', 'g'};

double bw_hz_of(double hz, double octaves) {
  return hz * (std::exp2(0.5 * octaves) - std::exp2(-0.5 * octaves));
}

double real_root(double decay_hz) {
  const double magnitude = std::exp(-2.0 * std::numbers::pi * std::abs(decay_hz) / kSr);
  return std::signbit(decay_hz) ? -magnitude : magnitude;
}

trench::core::RootPair pair_of(const nb::Roots& roots) {
  if (const auto* res = std::get_if<nb::Resonant>(&roots)) {
    return trench::core::ConjugatePair{res->hz, std::exp(-std::numbers::pi * res->bw_hz / kSr)};
  }
  const auto& real = std::get<nb::RealRoots>(roots);
  return trench::core::RealPair{real_root(real.a_hz), real_root(real.b_hz)};
}

nb::Roots bell_pole(p2k::Rng& rng) {
  const double hz = rng.uniform(100.0, 8000.0);
  return nb::Resonant{hz, bw_hz_of(hz, rng.uniform(0.1, 2.0))};
}

nb::Roots bell_zero(p2k::Rng& rng, const nb::Roots& pole) {
  double centre = 1000.0;
  if (const auto* res = std::get_if<nb::Resonant>(&pole)) centre = res->hz;
  const double hz = std::clamp(centre * std::exp2(rng.uniform(-1.0, 2.0)), 5.0, 0.49 * kSr);
  return nb::Resonant{hz, bw_hz_of(hz, rng.uniform(0.1, 2.0))};
}

nb::Section draw_section(p2k::Rng& rng, std::size_t which) {
  nb::Section out{};
  switch (which) {
    case 0: {
      const double r = 1.0 - rng.uniform(1.0e-6, 1.0e-4);
      out.pole = nb::Resonant{rng.uniform(20.0, 18'000.0), -std::log(r) * kSr / std::numbers::pi};
      out.zero = bell_zero(rng, out.pole);
      break;
    }
    case 1: {
      out.pole = bell_pole(rng);
      out.zero = rng.below(2) == 0 ? nb::RealRoots{0.0, 0.0} : nb::RealRoots{-0.0, -0.0};
      break;
    }
    case 2: {
      const double hz = rng.uniform(100.0, 8000.0);
      const double bw = bw_hz_of(hz, rng.uniform(0.1, 2.0));
      const double cents = rng.uniform(5.0, 50.0) * (rng.below(2) == 0 ? 1.0 : -1.0);
      out.pole = nb::Resonant{hz, bw};
      out.zero = nb::Resonant{hz * std::exp2(cents / 1200.0), bw};
      break;
    }
    case 3: {
      const double hz = rng.uniform(100.0, 8000.0);
      out.pole = nb::Resonant{hz, bw_hz_of(hz, rng.uniform(1.0e-4, 0.01))};
      out.zero = bell_zero(rng, out.pole);
      break;
    }
    case 4: {
      const double base = rng.below(2) == 0 ? 20.0 : 0.45 * kSr;
      const double hz = base * rng.uniform(0.95, 1.05);
      out.pole = nb::Resonant{hz, bw_hz_of(hz, rng.uniform(0.1, 2.0))};
      out.zero = bell_zero(rng, out.pole);
      break;
    }
    case 5: {
      out.pole = bell_pole(rng);
      out.zero = bell_zero(rng, out.pole);
      break;
    }
    default: {
      const double a = rng.uniform(1.0, 2000.0) * (rng.below(2) == 0 ? 1.0 : -1.0);
      const double b = rng.uniform(1.0, 2000.0) * (rng.below(2) == 0 ? 1.0 : -1.0);
      out.pole = nb::RealRoots{a, b};
      out.zero = bell_zero(rng, nb::Resonant{rng.uniform(100.0, 8000.0), 500.0});
      break;
    }
  }
  return out;
}

std::vector<double> mean_removed(std::vector<double> values) {
  double acc = 0.0;
  for (const double v : values) acc += v;
  const double mean = acc / static_cast<double>(values.size());
  for (auto& v : values) v -= mean;
  return values;
}

struct Proposal {
  std::size_t index{};
  std::array<std::size_t, kCaseCount> cases{};
  double snap_rms_db{};
  double snap_max_db{};
  double step_gap_db{};
  trench::adversary::ByteGrade grade{};
  std::array<std::uint8_t, 240> bytes{};
};

std::string case_mix(const std::array<std::size_t, kCaseCount>& cases) {
  std::ostringstream out;
  for (std::size_t ci = 0; ci < kCaseCount; ++ci) {
    out << kCaseLetters[ci] << cases[ci];
  }
  return out.str();
}

Proposal propose(p2k::Rng& rng, std::size_t index) {
  Proposal out{};
  out.index = index;
  nb::Body body{};
  for (auto& corner : body.corners) {
    for (auto& section : corner.sections) {
      const std::size_t which = rng.below(kCaseCount);
      ++out.cases[which];
      section = draw_section(rng, which);
    }
    corner.gain_db = rng.uniform(-20.0, 20.0);
  }

  std::array<p2k::PackedCorner, 4> packed{};
  for (std::size_t ci = 0; ci < 4; ++ci) {
    const auto& corner = body.corners[ci];
    const auto plan = nb::design(corner, kSr);
    p2k::CornerWords words{};
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      const trench::core::SectionGeometry geometry{pair_of(corner.sections[si].pole),
                                                   pair_of(corner.sections[si].zero), 1.0};
      const auto section = trench::core::words_from_geometry(geometry, kSr);
      words[si] = {section[0], section[1], section[2], section[3]};
    }
    const auto cascade = nb::cascade(plan, corner.gain_db);
    p2k::SectionBiquads biquads{};
    std::copy_n(cascade.begin(), p2k::kStageCount, biquads.begin());
    const auto target = p2k::response_db(biquads);
    const auto landed = p2k::polish_from_words(p2k::enter(words), target, 4).first;
    packed[ci] = p2k::pack_corner(landed, p2k::stage_gain_pass(landed));

    const auto want = mean_removed(target);
    const auto got = mean_removed(p2k::corner_response_db(p2k::packed_as_words(packed[ci])));
    double sq = 0.0;
    double worst = 0.0;
    for (std::size_t i = 0; i < want.size(); ++i) {
      const double d = std::abs(want[i] - got[i]);
      sq += d * d;
      worst = std::max(worst, d);
    }
    const double corner_rms = std::sqrt(sq / static_cast<double>(want.size()));
    if (worst > out.snap_max_db) {
      out.snap_max_db = worst;
      out.snap_rms_db = corner_rms;
    }
  }

  out.bytes = p2k::pack_body(packed);
  out.grade = trench::adversary::grade_bytes(out.bytes);
  out.step_gap_db = out.grade.max_step_db - p2k::interior_audit(body).max_step_db;
  return out;
}

constexpr int kCell = 11;

void write_row(std::ostream& os, const Proposal& p, std::uint64_t seed) {
  os << std::right << std::setw(5) << p.index << std::setw(12) << seed << "  " << std::left
     << std::setw(16) << case_mix(p.cases) << std::right << std::fixed << std::setprecision(3);
  for (const double v : {p.snap_rms_db, p.snap_max_db, p.step_gap_db, p.grade.max_step_db,
                         p.grade.dc_drift_db, p.grade.motion_db,
                         20.0 * std::log10(std::max(p.grade.moving_peak, 1.0e-15)),
                         p.grade.excursion_up_db, p.grade.loudness_beyond_corners_db}) {
    os << std::setw(kCell) << v;
  }
  os << std::setw(7) << p.grade.refused << std::setw(7) << p.grade.non_finite << std::setw(7)
     << p.grade.pole_over_ceiling << std::setw(7) << p.grade.off_lattice << '\n';
}

void write_header(std::ostream& os) {
  os << std::right << std::setw(5) << "idx" << std::setw(12) << "seed" << "  " << std::left
     << std::setw(16) << "cases" << std::right;
  for (const char* h : {"snap_rms", "snap_max", "step_gap", "max_step", "dc_drift", "motion",
                        "peak_db", "exc_up", "loud_bey"}) {
    os << std::setw(kCell) << h;
  }
  os << std::setw(7) << "refuse" << std::setw(7) << "nonfin" << std::setw(7) << "ceil"
     << std::setw(7) << "offlat" << '\n';
}

void report(std::ostream& os, const std::vector<Proposal>& all, std::uint64_t seed,
            const char* label, double (*score)(const Proposal&), const fs::path& dir) {
  std::vector<const Proposal*> sorted;
  sorted.reserve(all.size());
  for (const auto& p : all) sorted.push_back(&p);
  std::sort(sorted.begin(), sorted.end(),
            [score](const Proposal* a, const Proposal* b) { return score(*a) > score(*b); });
  os << "\nworst 10 by " << label << '\n';
  write_header(os);
  const std::size_t n = std::min<std::size_t>(10, sorted.size());
  for (std::size_t rank = 0; rank < n; ++rank) {
    write_row(os, *sorted[rank], seed);
    std::ostringstream name;
    name << label << '_' << rank << '_' << seed << '_' << sorted[rank]->index << ".bin";
    std::ofstream out(dir / name.str(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(sorted[rank]->bytes.data()),
              static_cast<std::streamsize>(sorted[rank]->bytes.size()));
  }
}

}  // namespace

int main(int argc, char** argv) {
  const std::uint64_t seed = argc > 1 ? std::stoull(argv[1]) : 20'260'823ULL;
  const std::size_t count = argc > 2 ? static_cast<std::size_t>(std::stoul(argv[2])) : 200;
  const fs::path dir = fs::path{TRENCH_SOURCE_ROOT} / "dev" / "adversary";
  fs::create_directories(dir);

  p2k::Rng rng(seed);
  std::vector<Proposal> all;
  all.reserve(count);
  write_header(std::cout);
  for (std::size_t i = 0; i < count; ++i) {
    all.push_back(propose(rng, i));
    write_row(std::cout, all.back(), seed);
  }

  std::ostringstream table;
  write_header(table);
  for (const auto& p : all) write_row(table, p, seed);

  std::ostringstream summary;
  report(summary, all, seed, "snap", [](const Proposal& p) { return p.snap_max_db; }, dir);
  report(summary, all, seed, "step", [](const Proposal& p) { return p.step_gap_db; }, dir);
  report(summary, all, seed, "dc", [](const Proposal& p) { return p.grade.dc_drift_db; }, dir);
  report(summary, all, seed, "motion", [](const Proposal& p) { return p.grade.motion_db; }, dir);

  std::cout << summary.str();
  std::ofstream out(dir / "adversary.txt");
  out << table.str() << summary.str();
  return 0;
}
