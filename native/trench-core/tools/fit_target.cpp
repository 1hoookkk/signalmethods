#include "trench/core/p2k.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace p2k = trench::core::p2k;

namespace {

std::vector<double> read_target(const char* path) {
  std::ifstream stream(path);
  std::vector<double> values;
  double value = 0.0;
  while (stream >> value) values.push_back(value);
  return values;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::fprintf(stderr, "usage: fit_target <target_a.txt> <target_b.txt> <out.body240>\n");
    return 2;
  }
  std::array<p2k::PackedCorner, 4> corners{};
  for (int which = 0; which < 2; ++which) {
    const auto target = read_target(argv[1 + which]);
    if (target.size() != p2k::kNpts) {
      std::fprintf(stderr, "target %s has %zu points, need %zu\n", argv[1 + which],
                   target.size(), p2k::kNpts);
      return 2;
    }
    const std::array<p2k::Seed, 2> seeds{p2k::SeedPeel{}, p2k::SeedContinuous{}};
    p2k::FitOptions opts;
    opts.allow_continuous = true;
    const auto fit = p2k::fit_corner(target, seeds, opts);
    if (!fit) {
      std::fprintf(stderr, "fit failed for %s\n", argv[1 + which]);
      return 1;
    }
    std::printf("%s: shape_rms %.3f dB seed %.*s\n", argv[1 + which], fit->shape_rms_db,
                static_cast<int>(fit->seed_used.size()), fit->seed_used.data());
    corners[static_cast<std::size_t>(which)] = fit->packed;
    corners[static_cast<std::size_t>(which + 2)] = fit->packed;
  }
  const auto body = p2k::pack_body(corners);
  std::ofstream out(argv[3], std::ios::binary);
  out.write(reinterpret_cast<const char*>(body.data()),
            static_cast<std::streamsize>(body.size()));
  return out ? 0 : 1;
}
