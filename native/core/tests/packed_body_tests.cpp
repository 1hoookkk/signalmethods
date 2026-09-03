#include "trench/core/packed_body.hpp"
#include "trench/core/native_body.hpp"

#include <cmath>

#include <array>
#include <cstdint>
#include <cstdio>
#include <random>

namespace {

int failures = 0;

void check(bool ok, const char* what, long a = 0, long b = 0) {
  std::printf("%s  %s  (%ld / %ld)\n", ok ? "PASS" : "FAIL", what, a, b);
  if (!ok) ++failures;
}

std::uint16_t reference(std::uint16_t a, std::uint16_t b, float fraction) {
  const float difference = static_cast<float>(static_cast<std::int32_t>(b) - static_cast<std::int32_t>(a));
  return static_cast<std::uint16_t>(static_cast<std::int32_t>(a) + static_cast<std::int32_t>(difference * fraction));
}

}  // namespace

int main() {
  using trench::core::interpolate_word;
  using trench::core::PackedBody;

  check(interpolate_word(0x1234, 0xABCD, 0.0f) == 0x1234, "fraction 0 returns a");
  check(interpolate_word(0x1234, 0xABCD, 1.0f) == 0xABCD, "fraction 1 returns b");
  check(interpolate_word(0x0000, 0xFFFF, 0.5f) == 32767, "truncates toward zero on the way up", interpolate_word(0x0000, 0xFFFF, 0.5f), 32767);
  check(interpolate_word(0xFFFF, 0x0000, 0.5f) == 32768, "truncates toward zero on the way down", interpolate_word(0xFFFF, 0x0000, 0.5f), 32768);
  check(interpolate_word(0x01F0, 0xF000, 0.9f) == reference(0x01F0, 0xF000, 0.9f), "large positive difference matches int32 arithmetic", interpolate_word(0x01F0, 0xF000, 0.9f), reference(0x01F0, 0xF000, 0.9f));
  check(interpolate_word(0xF000, 0x01F0, 0.9f) == reference(0xF000, 0x01F0, 0.9f), "large negative difference matches int32 arithmetic", interpolate_word(0xF000, 0x01F0, 0.9f), reference(0xF000, 0x01F0, 0.9f));

  std::mt19937 rng(7);
  std::uniform_int_distribution<int> word(0, 0xFFFF);
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  int mismatches = 0;
  int non_monotonic = 0;
  for (int i = 0; i < 200000; ++i) {
    const auto a = static_cast<std::uint16_t>(word(rng));
    const auto b = static_cast<std::uint16_t>(word(rng));
    const float f = unit(rng);
    const auto got = interpolate_word(a, b, f);
    if (got != reference(a, b, f)) ++mismatches;
    const auto lo = a < b ? a : b;
    const auto hi = a < b ? b : a;
    if (got < lo || got > hi) ++non_monotonic;
  }
  check(mismatches == 0, "200000 random triples match the int32 reference", mismatches, 0);
  check(non_monotonic == 0, "result always lies between a and b", non_monotonic, 0);

  std::array<std::uint8_t, trench::core::kLegacyBodyBytes> bytes{};
  for (std::size_t corner = 0; corner < 4; ++corner) {
    for (std::size_t i = 0; i < 30; ++i) {
      const auto value = static_cast<std::uint16_t>(0x1000 * (corner + 1) + i);
      bytes[(corner * 30 + i) * 2] = static_cast<std::uint8_t>(value & 0xFF);
      bytes[(corner * 30 + i) * 2 + 1] = static_cast<std::uint8_t>(value >> 8);
    }
  }
  const auto body = PackedBody::from_legacy_bytes(bytes);
  check(body.interpolate_words(0.0f, 0.0f, 0.0f)[0][0] == 0x1000, "morph 0 q 0 is corner 0", body.interpolate_words(0.0f, 0.0f, 0.0f)[0][0], 0x1000);
  check(body.interpolate_words(1.0f, 0.0f, 0.0f)[0][0] == 0x2000, "morph 1 q 0 is corner 1", body.interpolate_words(1.0f, 0.0f, 0.0f)[0][0], 0x2000);
  check(body.interpolate_words(0.0f, 1.0f, 0.0f)[0][0] == 0x3000, "morph 0 q 1 is corner 2", body.interpolate_words(0.0f, 1.0f, 0.0f)[0][0], 0x3000);
  check(body.interpolate_words(1.0f, 1.0f, 0.0f)[0][0] == 0x4000, "morph 1 q 1 is corner 3", body.interpolate_words(1.0f, 1.0f, 0.0f)[0][0], 0x4000);
  check(body.interpolate_words(0.5f, 0.5f, 0.0f)[0][0] == body.interpolate_words(0.5f, 0.5f, 1.0f)[0][0], "z is inert for a legacy body");
  check(body.interpolate_words(0.5f, 0.5f, 0.0f)[6][0] == trench::core::kIdentitySection[0], "legacy seventh section stays identity");


  {
    using trench::core::PackedSection;
    using trench::core::encode_word;
    using trench::core::kIdentitySection;
    using trench::core::cascade_response_db;
    namespace nb = trench::core::native;
    const auto section_words = [](const nb::Section& sec, double rate, double scale) {
      const auto c = nb::design(sec, rate);
      const double d1 = 1.0 - c.b2;
      const double d0 = (c.b1 + 2.0 - d1) / 4.0;
      const double d3 = 1.0 - c.a2;
      const double d2 = (c.a1 + 2.0 - d3) / 4.0;
      return PackedSection{encode_word(d0), encode_word(d1), encode_word(d2),
                           encode_word(d3), encode_word(scale / 4.0)};
    };
    const double datum = 44100.0;
    const std::array<std::array<nb::Section, 3>, 4> corners = {{
      {{{nb::Resonant{120.0, 25.0}, nb::Resonant{60.0, 400.0}, true},
        {nb::Resonant{800.0, 60.0}, nb::Resonant{400.0, 800.0}, true},
        {nb::Resonant{2500.0, 150.0}, nb::Resonant{5000.0, 0.0}, true}}},
      {{{nb::Resonant{240.0, 40.0}, nb::Resonant{100.0, 500.0}, true},
        {nb::Resonant{1600.0, 90.0}, nb::Resonant{700.0, 900.0}, true},
        {nb::Resonant{3600.0, 200.0}, nb::Resonant{7000.0, 0.0}, true}}},
      {{{nb::Resonant{180.0, 18.0}, nb::Resonant{90.0, 300.0}, true},
        {nb::Resonant{1100.0, 70.0}, nb::Resonant{500.0, 850.0}, true},
        {nb::Resonant{3000.0, 170.0}, nb::Resonant{6000.0, 0.0}, true}}},
      {{{nb::Resonant{300.0, 45.0}, nb::Resonant{150.0, 550.0}, true},
        {nb::Resonant{2000.0, 110.0}, nb::Resonant{900.0, 950.0}, true},
        {nb::Resonant{4200.0, 220.0}, nb::Resonant{8000.0, 0.0}, true}}},
    }};
    PackedBody authored;
    for (auto& corner : authored.words) corner.fill(kIdentitySection);
    for (std::size_t ci = 0; ci < 4; ++ci) {
      for (std::size_t si = 0; si < 3; ++si) {
        authored.words[ci][si] = section_words(corners[ci][si], datum, 1.1);
        authored.words[ci + 4][si] = authored.words[ci][si];
      }
    }
    const auto legacy = authored.legacy_bytes();
    const auto physical = nb::import_p2k(legacy, datum);
    const std::array<std::pair<float, float>, 4> at = {{
      {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}}};
    const std::array<double, 6> freqs = {60.0, 120.0, 250.0, 1000.0, 2500.0, 9000.0};
    for (const double target : {44100.0, 48000.0, 96000.0}) {
      const auto bank = nb::rewarp_p2k_body(authored, datum, target);
      double worst = 0.0;
      double worst120 = 0.0;
      for (std::size_t ci = 0; ci < 4; ++ci) {
        const auto reference = authored.interpolate_biquads(at[ci].first, at[ci].second, 0.0f);
        const auto measured = bank.interpolate_biquads(at[ci].first, at[ci].second, 0.0f);
        for (const double f : freqs) {
          const double e = cascade_response_db(reference, f, datum);
          const double m = cascade_response_db(measured, f, target);
          const double d = std::abs(m - e);
          worst = std::max(worst, d);
          if (f == 120.0) worst120 = std::max(worst120, d);
        }
      }
      std::printf("rewarp %.0f Hz worst %.4f dB, at 120 Hz %.4f dB%s", target, worst, worst120, "\n");
      check(worst <= 0.2, "rewarped bank matches the designed corner response", (long) (worst * 10000.0), (long) target);
      check(worst120 <= 0.15, "low octave survives the rewarp (the 48k regression)", (long) (worst120 * 10000.0), (long) target);
      const auto c0 = bank.interpolate_biquads(0.0f, 0.0f, 0.0f);
      const double notch = cascade_response_db(c0, 5000.0, target);
      const double body_level = cascade_response_db(c0, 1000.0, target);
      check(notch < body_level - 30.0, "unit-circle zero stays a notch at its authored frequency", (long) notch, (long) body_level);
    }
  }

  std::printf("%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
