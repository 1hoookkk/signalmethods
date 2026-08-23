#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "trench/core/audition.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/packed_body.hpp"

namespace nb = trench::core::native;
using trench::core::Cascade;

namespace {

constexpr double kSr = 44'100.0;
constexpr double kSeconds = 3.0;
constexpr double kInf = std::numeric_limits<double>::infinity();

void write_wav(const std::filesystem::path& path, const std::vector<float>& samples) {
  std::ofstream out(path, std::ios::binary);
  const auto put32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  const auto put16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
  const std::uint32_t bytes = static_cast<std::uint32_t>(samples.size() * 2);
  out.write("RIFF", 4);
  put32(36 + bytes);
  out.write("WAVEfmt ", 8);
  put32(16);
  put16(1);
  put16(1);
  put32(static_cast<std::uint32_t>(kSr));
  put32(static_cast<std::uint32_t>(kSr) * 2);
  put16(2);
  put16(16);
  out.write("data", 4);
  put32(bytes);
  for (const float s : samples) {
    const double c = std::clamp(static_cast<double>(s), -1.0, 1.0);
    put16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lrint(c * 32767.0))));
  }
}

nb::Resonant pole_of(const nb::Section& s) { return std::get<nb::Resonant>(s.pole); }

struct Variant {
  std::string name;
  nb::Corner corner;
};

void render(const Variant& v, const std::filesystem::path& dir, std::ofstream& csv) {
  const Cascade c = nb::cascade(nb::design(v.corner, kSr), 0.0);
  for (int i = 0; i < 400; ++i) {
    const double hz = 20.0 * std::pow(1000.0, i / 399.0);
    csv << v.name << ',' << hz << ',' << trench::core::cascade_response_db(c, hz, kSr) << '\n';
  }
  trench::core::SawSource saw(49.0, kSr, 0.25F);
  trench::core::CascadeRunner runner;
  runner.set_target(c);
  const std::size_t n = static_cast<std::size_t>(kSr * kSeconds);
  std::vector<float> out(n);
  for (auto& s : out) s = saw.next();
  runner.process(out);
  float peak = 0.0F;
  for (const float s : out) peak = std::max(peak, std::abs(s));
  for (auto& s : out) s = s / peak * 0.5F;
  write_wav(dir / (v.name + ".wav"), out);
  std::cout << v.name << ": peak " << 20.0 * std::log10(peak) << " dBFS";
  for (const auto& s : v.corner.sections) {
    const auto p = pole_of(s);
    std::cout << "  " << std::lrint(p.hz) << "Hz=" << trench::core::cascade_response_db(c, p.hz, kSr);
  }
  std::cout << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: mask_proof <body.bin> <out_dir>\n";
    return 1;
  }
  std::ifstream in(argv[1], std::ios::binary);
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
  const std::filesystem::path dir(argv[2]);
  std::filesystem::create_directories(dir);
  std::ofstream csv(dir / "response.csv");
  csv << "variant,hz,db\n";

  const nb::Corner factory = nb::import_p2k(bytes).corners[0];
  for (const auto& s : factory.sections) {
    if (!std::holds_alternative<nb::Resonant>(s.pole)) {
      std::cerr << "corner 0 has a real pole pair; pick another body\n";
      return 1;
    }
  }
  nb::Corner skeleton = factory;
  skeleton.gain_db = 0.0;
  for (auto& s : skeleton.sections) s.zero = nb::RealRoots{kInf, kInf};

  std::vector<std::size_t> order(nb::kSections);
  for (std::size_t i = 0; i < nb::kSections; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](auto a, auto b) {
    return pole_of(skeleton.sections[a]).hz < pole_of(skeleton.sections[b]).hz;
  });

  std::vector<Variant> variants;
  variants.push_back({"0_factory", factory});
  variants.push_back({"1_skeleton", skeleton});

  nb::Corner parked = skeleton;
  for (auto& s : parked.sections) s.zero = nb::Resonant{16'000.0, 9'000.0};
  variants.push_back({"2_parked", parked});

  nb::Corner bells = skeleton;
  for (auto& s : bells.sections) {
    const auto p = pole_of(s);
    s.zero = nb::Resonant{p.hz * std::exp2(0.15), p.bw_hz * 4.0};
  }
  variants.push_back({"3_bells", bells});

  nb::Corner notches = parked;
  for (std::size_t k = 0; k + 1 < nb::kSections; ++k) {
    const auto lo = pole_of(skeleton.sections[order[k]]);
    const auto hi = pole_of(skeleton.sections[order[k + 1]]);
    notches.sections[order[k]].zero = nb::Resonant{std::sqrt(lo.hz * hi.hz), 40.0};
  }
  variants.push_back({"4_notches", notches});

  nb::Corner cancel = parked;
  const auto top = order.back();
  cancel.sections[top].zero = pole_of(skeleton.sections[top]);
  variants.push_back({"5_cancel_top", cancel});

  nb::Corner vowel = parked;
  for (std::size_t k = 0; k < 3; ++k) {
    const auto p = pole_of(skeleton.sections[order[k]]);
    vowel.sections[order[k]].zero = nb::Resonant{p.hz * std::exp2(-0.3), p.bw_hz * 2.5};
  }
  variants.push_back({"6_low_bells", vowel});

  for (const auto& v : variants) render(v, dir, csv);
  return 0;
}
