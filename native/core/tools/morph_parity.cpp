#include "trench/core/native_body.hpp"
#include "trench/core/packed_body.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <span>
#include <string>
#include <vector>

namespace {

using trench::core::Cascade;
using trench::core::PackedBody;
using trench::core::interpolate_word;

constexpr int kSteps = 256;
constexpr int kGridMorph = 65;
constexpr int kWalk = 16;
constexpr double kDatumHz = 44'100.0;
constexpr double kRadiusLimit = 0.9995;

struct Body {
  std::string name;
  PackedBody packed;
};

std::vector<Body> load_bodies(const std::filesystem::path& dir) {
  std::vector<Body> out;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".body240") continue;
    std::ifstream in(entry.path(), std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() != trench::core::kLegacyBodyBytes) continue;
    out.push_back({entry.path().stem().string(), PackedBody::from_legacy_bytes(bytes)});
  }
  std::sort(out.begin(), out.end(), [](const Body& a, const Body& b) { return a.name < b.name; });
  return out;
}

int floor_step(int a, int b, float t) {
  return a + static_cast<int>(std::floor(static_cast<double>(b - a) * static_cast<double>(t)));
}

bool descending_fractional(int a, int b, float t) {
  const double product = static_cast<double>(b - a) * static_cast<double>(t);
  return b < a && product != std::floor(product);
}

struct WordReport {
  long total = 0;
  long mismatched = 0;
  int max_abs = 0;
  long unexplained = 0;
  long descending_states = 0;
};

WordReport word_parity(const PackedBody& body) {
  WordReport r;
  for (int mi = 0; mi <= kSteps; ++mi) {
    const float m = static_cast<float>(mi) / static_cast<float>(kSteps);
    for (int qi = 0; qi <= kSteps; ++qi) {
      const float q = static_cast<float>(qi) / static_cast<float>(kSteps);
      const auto got = body.interpolate_words(m, q, 0.0f);
      for (std::size_t s = 0; s < trench::core::kLegacySectionCount; ++s) {
        for (std::size_t w = 0; w < trench::core::kCoefficientCount; ++w) {
          const int a = body.words[0][s][w], b = body.words[1][s][w];
          const int c = body.words[2][s][w], d = body.words[3][s][w];
          const int e0 = floor_step(a, b, m);
          const int e1 = floor_step(c, d, m);
          const int want = floor_step(e0, e1, q);
          ++r.total;
          if (got[s][w] == want) continue;
          ++r.mismatched;
          r.max_abs = std::max(r.max_abs, std::abs(static_cast<int>(got[s][w]) - want));
          const int t0 = interpolate_word(static_cast<std::uint16_t>(a), static_cast<std::uint16_t>(b), m);
          const int t1 = interpolate_word(static_cast<std::uint16_t>(c), static_cast<std::uint16_t>(d), m);
          const bool descending = descending_fractional(a, b, m) || descending_fractional(c, d, m) ||
                                  descending_fractional(t0, t1, q);
          if (descending) ++r.descending_states; else ++r.unexplained;
        }
      }
    }
  }
  return r;
}

double pole_radius(const trench::core::Biquad& c) {
  const double a1 = c[3], a2 = c[4];
  const double disc = a1 * a1 - 4.0 * a2;
  if (disc < 0.0) return std::sqrt(std::max(a2, 0.0));
  const double root = std::sqrt(disc);
  return std::max(std::abs((-a1 + root) * 0.5), std::abs((-a1 - root) * 0.5));
}

struct GridReport {
  double node_max = 0.0;
  double interp_max = 0.0;
  int worst_mi = 0, worst_qi = 0, worst_section = 0;
  double worst_mf = 0.0, worst_qf = 0.0;
  long cells_over = 0;
  long states = 0;
};

std::vector<Cascade> build_grid(const PackedBody& body, double rate, int gridQ) {
  std::vector<Cascade> grid(static_cast<std::size_t>(kGridMorph) * static_cast<std::size_t>(gridQ));
  for (int qi = 0; qi < gridQ; ++qi) {
    for (int mi = 0; mi < kGridMorph; ++mi) {
      const auto words = body.interpolate_words(static_cast<float>(mi) / static_cast<float>(kGridMorph - 1),
                                                static_cast<float>(qi) / static_cast<float>(gridQ - 1), 0.0f);
      grid[static_cast<std::size_t>(qi * kGridMorph + mi)] = trench::core::native::rewarp_cascade(words, kDatumHz, rate);
    }
  }
  return grid;
}

struct Cell {
  int mi{}, qi{};
  double mf{}, qf{};
};

Cascade grid_lerp(const std::vector<Cascade>& grid, int gridQ, float morph, float q, Cell& cell) {
  const float mPos = std::clamp(morph, 0.0f, 1.0f) * static_cast<float>(kGridMorph - 1);
  const float qPos = std::clamp(q, 0.0f, 1.0f) * static_cast<float>(gridQ - 1);
  cell.mi = std::min(static_cast<int>(mPos), kGridMorph - 2);
  cell.qi = std::min(static_cast<int>(qPos), gridQ - 2);
  cell.mf = static_cast<double>(mPos) - cell.mi;
  cell.qf = static_cast<double>(qPos) - cell.qi;
  const auto& c00 = grid[static_cast<std::size_t>(cell.qi * kGridMorph + cell.mi)];
  const auto& c10 = grid[static_cast<std::size_t>(cell.qi * kGridMorph + cell.mi + 1)];
  const auto& c01 = grid[static_cast<std::size_t>((cell.qi + 1) * kGridMorph + cell.mi)];
  const auto& c11 = grid[static_cast<std::size_t>((cell.qi + 1) * kGridMorph + cell.mi + 1)];
  Cascade out{};
  for (std::size_t s = 0; s < trench::core::kSectionCount; ++s) {
    for (std::size_t k = 0; k < trench::core::kCoefficientCount; ++k) {
      const double e0 = c00[s][k] + (c10[s][k] - c00[s][k]) * cell.mf;
      const double e1 = c01[s][k] + (c11[s][k] - c01[s][k]) * cell.mf;
      out[s][k] = e0 + (e1 - e0) * cell.qf;
    }
  }
  return out;
}

GridReport grid_stability(const PackedBody& body, double rate, int gridQ) {
  GridReport r;
  const auto grid = build_grid(body, rate, gridQ);
  for (const auto& node : grid)
    for (const auto& section : node) r.node_max = std::max(r.node_max, pole_radius(section));
  for (int qi = 0; qi + 1 < gridQ; ++qi) {
    for (int mi = 0; mi + 1 < kGridMorph; ++mi) {
      bool cell_over = false;
      for (int sq = 0; sq <= kWalk; ++sq) {
        const float q = static_cast<float>((qi + sq / static_cast<double>(kWalk)) / (gridQ - 1));
        for (int sm = 0; sm <= kWalk; ++sm) {
          const float morph = static_cast<float>((mi + sm / static_cast<double>(kWalk)) / (kGridMorph - 1));
          Cell cell;
          const auto lerped = grid_lerp(grid, gridQ, morph, q, cell);
          const int ci = cell.mi, cq = cell.qi;
          const double mf = cell.mf, qf = cell.qf;
          ++r.states;
          for (std::size_t s = 0; s < trench::core::kSectionCount; ++s) {
            const auto& c = lerped[s];
            const double radius = pole_radius(c);
            if (radius > kRadiusLimit) cell_over = true;
            if (radius > r.interp_max) {
              r.interp_max = radius;
              r.worst_mi = ci;
              r.worst_qi = cq;
              r.worst_mf = mf;
              r.worst_qf = qf;
              r.worst_section = static_cast<int>(s);
            }
          }
        }
      }
      if (cell_over) ++r.cells_over;
    }
  }
  return r;
}

double response_at(const Cascade& cascade, double hz, double rate) {
  return trench::core::cascade_response_db(std::span<const trench::core::Biquad>(cascade), hz, rate);
}

std::complex<double> complex_response(const Cascade& cascade, double hz, double rate) {
  const double w = 2.0 * std::numbers::pi * hz / rate;
  const std::complex<double> z1 = std::polar(1.0, -w);
  const std::complex<double> z2 = std::polar(1.0, -2.0 * w);
  std::complex<double> h{1.0, 0.0};
  for (const auto& c : cascade) h *= (c[0] + c[1] * z1 + c[2] * z2) / (1.0 + c[3] * z1 + c[4] * z2);
  return h;
}

struct SawPrediction {
  double residual_db{};
  double phase110_deg{}, phase220_deg{}, phase440_deg{};
};

SawPrediction predict_saw_residual(const Cascade& datum, const Cascade& other, double other_rate) {
  double num = 0.0, den = 0.0;
  SawPrediction p;
  for (int k = 1; k * 110.0 < 20'000.0; ++k) {
    const double hz = 110.0 * k;
    const auto hd = complex_response(datum, hz, kDatumHz);
    const auto ho = complex_response(other, hz, other_rate);
    const double weight = 1.0 / (static_cast<double>(k) * k);
    num += std::norm(hd - ho) * weight;
    den += std::norm(hd) * weight;
    const double phase = std::arg(ho / hd) * 180.0 / std::numbers::pi;
    if (k == 1) p.phase110_deg = phase;
    if (k == 2) p.phase220_deg = phase;
    if (k == 4) p.phase440_deg = phase;
  }
  p.residual_db = 10.0 * std::log10(num / den);
  return p;
}

void response_report(const Body& body, double rate, int gridQ, std::ofstream& csv) {
  const auto freqs = trench::core::logarithmic_frequency_grid(20.0, 20'000.0, 400);
  const auto grid = build_grid(body.packed, rate, gridQ);
  const float morph = 0.5f;
  for (const float q : {0.0f, 0.03125f, 0.125f, 0.5f, 0.78125f, 1.0f}) {
    const auto words = body.packed.interpolate_words(morph, q, 0.0f);
    const auto datum = body.packed.interpolate_biquads(morph, q, 0.0f);
    const auto exact = trench::core::native::rewarp_cascade(words, kDatumHz, rate);
    Cell cell;
    const auto lerped = grid_lerp(grid, gridQ, morph, q, cell);
    double design_max = 0.0, design_sum = 0.0, grid_max = 0.0, grid_sum = 0.0;
    for (const double hz : freqs) {
      const double d = response_at(exact, hz, rate) - response_at(datum, hz, kDatumHz);
      const double g = response_at(lerped, hz, rate) - response_at(exact, hz, rate);
      design_max = std::max(design_max, std::abs(d));
      grid_max = std::max(grid_max, std::abs(g));
      design_sum += d * d;
      grid_sum += g * g;
    }
    const double d110 = response_at(exact, 110.0, rate) - response_at(datum, 110.0, kDatumHz);
    const double d220 = response_at(exact, 220.0, rate) - response_at(datum, 220.0, kDatumHz);
    const double d440 = response_at(exact, 440.0, rate) - response_at(datum, 440.0, kDatumHz);
    const double design_rms = std::sqrt(design_sum / static_cast<double>(freqs.size()));
    const double grid_rms = std::sqrt(grid_sum / static_cast<double>(freqs.size()));
    const auto exact_saw = predict_saw_residual(datum, exact, rate);
    const auto lerped_saw = predict_saw_residual(datum, lerped, rate);
    csv << body.name << ',' << rate << ',' << gridQ << ',' << morph << ',' << q << ',' << design_max << ','
        << design_rms << ',' << d110 << ',' << d220 << ',' << d440 << ',' << grid_max << ',' << grid_rms << ','
        << cell.qi << ',' << cell.qf << ',' << exact_saw.residual_db << ',' << lerped_saw.residual_db << ','
        << exact_saw.phase110_deg << ',' << exact_saw.phase220_deg << ',' << exact_saw.phase440_deg << '\n';
    std::printf("response %-14s %6.0f q%-2d m %.2f q %.5f design max %.3f rms %.3f dB (110 %+.3f 220 %+.3f 440 %+.3f) "
                "grid-lerp max %.4f rms %.4f dB (cell qi %d qf %.3f) saw-null predicted exact %.1f lerped %.1f dB "
                "phase 110 %+.2f 220 %+.2f 440 %+.2f deg\n",
                body.name.c_str(), rate, gridQ, morph, q, design_max, design_rms, d110, d220, d440, grid_max, grid_rms,
                cell.qi, cell.qf, exact_saw.residual_db, lerped_saw.residual_db, exact_saw.phase110_deg,
                exact_saw.phase220_deg, exact_saw.phase440_deg);
  }
}

std::vector<int> parse_ints(const std::string& text) {
  std::vector<int> out;
  std::size_t start = 0;
  while (start < text.size()) {
    const auto comma = text.find(',', start);
    out.push_back(std::stoi(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start)));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return out;
}

}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: trench_morph_parity <presets_dir> <out_dir> [--grid-q 5,17,33] [--rates 48000,96000] "
                 "[--skip-words] [--skip-grid]\n");
    return 2;
  }
  const std::filesystem::path presets = argv[1];
  const std::filesystem::path out = argv[2];
  std::vector<int> gridQs{5, 17, 33};
  std::vector<int> rates{48'000, 96'000};
  bool do_words = true, do_grid = true;
  std::string response_body;
  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--grid-q" && i + 1 < argc) gridQs = parse_ints(argv[++i]);
    else if (arg == "--rates" && i + 1 < argc) rates = parse_ints(argv[++i]);
    else if (arg == "--skip-words") do_words = false;
    else if (arg == "--skip-grid") do_grid = false;
    else if (arg == "--response" && i + 1 < argc) response_body = argv[++i];
  }
  std::filesystem::create_directories(out);
  const auto bodies = load_bodies(presets);
  std::printf("bodies %zu\n", bodies.size());

  if (!response_body.empty()) {
    std::ofstream csv(out / "response.csv");
    csv << "body,rate,grid_q,morph,q,design_max_db,design_rms_db,d110_db,d220_db,d440_db,grid_lerp_max_db,"
           "grid_lerp_rms_db,cell_qi,cell_qf,saw_null_exact_db,saw_null_lerped_db,phase110_deg,phase220_deg,"
           "phase440_deg\n";
    for (const auto& body : bodies) {
      if (body.name != response_body) continue;
      for (const int rate : rates)
        for (const int gridQ : gridQs) response_report(body, rate, gridQ, csv);
    }
  }

  if (do_words) {
    std::ofstream csv(out / "words.csv");
    csv << "body,total_words,mismatched,max_abs_diff,descending_states,unexplained\n";
    long sum_total = 0, sum_mismatch = 0, sum_unexplained = 0;
    int max_abs = 0;
    for (const auto& body : bodies) {
      const auto r = word_parity(body.packed);
      csv << body.name << ',' << r.total << ',' << r.mismatched << ',' << r.max_abs << ',' << r.descending_states
          << ',' << r.unexplained << '\n';
      std::printf("words %-14s total %ld mismatched %ld max_abs %d descending_only %s\n", body.name.c_str(), r.total,
                  r.mismatched, r.max_abs, r.unexplained == 0 ? "yes" : "no");
      sum_total += r.total;
      sum_mismatch += r.mismatched;
      sum_unexplained += r.unexplained;
      max_abs = std::max(max_abs, r.max_abs);
    }
    std::printf("words ALL total %ld mismatched %ld max_abs %d unexplained %ld\n", sum_total, sum_mismatch, max_abs,
                sum_unexplained);
  }

  if (do_grid) {
    std::ofstream csv(out / "grid.csv");
    csv << "body,rate,grid_q,node_max_radius,interp_max_radius,worst_mi,worst_qi,worst_mf,worst_qf,worst_section,"
           "cells_over_0.9995,states\n";
    for (const int gridQ : gridQs) {
      for (const int rate : rates) {
        int bodies_over = 0, bodies_node_over = 0;
        double worst = 0.0;
        std::string worst_body;
        for (const auto& body : bodies) {
          const auto r = grid_stability(body.packed, rate, gridQ);
          csv << body.name << ',' << rate << ',' << gridQ << ',' << std::to_string(r.node_max) << ','
              << std::to_string(r.interp_max) << ',' << r.worst_mi << ',' << r.worst_qi << ',' << r.worst_mf << ','
              << r.worst_qf << ',' << r.worst_section << ',' << r.cells_over << ',' << r.states << '\n';
          std::printf(
              "grid q%-2d %6d %-14s node_max %.6f interp_max %.6f worst mi %2d qi %2d mf %.4f qf %.4f s%d cells_over %ld\n",
              gridQ, rate, body.name.c_str(), r.node_max, r.interp_max, r.worst_mi, r.worst_qi, r.worst_mf, r.worst_qf,
              r.worst_section, r.cells_over);
          if (r.interp_max > kRadiusLimit) ++bodies_over;
          if (r.node_max > kRadiusLimit) ++bodies_node_over;
          if (r.interp_max > worst) {
            worst = r.interp_max;
            worst_body = body.name;
          }
        }
        std::printf("grid q%-2d %6d SUMMARY bodies_over_%.4f %d node_over %d worst %.6f (%s)\n", gridQ, rate,
                    kRadiusLimit, bodies_over, bodies_node_over, worst, worst_body.c_str());
      }
    }
  }
  return 0;
}
