#include "trench/core/section_param.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <variant>

#include "trench/core/p2k.hpp"

namespace trench::core::p2k {

namespace {

constexpr double kParkedHz = 15'000.0;
constexpr double kTypeOffsetOct = 2.0;
constexpr double kFarRatio = 8.0;
constexpr double kParkedRootHz = 18'000.0;
constexpr double kParkedRootR = 0.8;
constexpr double kPoleRMin = 0.3;
constexpr std::uint16_t kProbeScaleWord = 0xDFFF;

double root_hz(const RootPair& pair, double fallback_hz, double high_hz) {
  if (const auto* conjugate = std::get_if<ConjugatePair>(&pair)) {
    return conjugate->hz;
  }
  if (const auto* real = std::get_if<RealPair>(&pair)) {
    return real->root_a + real->root_b >= 0.0 ? 20.0 : high_hz;
  }
  return fallback_hz;
}

double far_hz(SectionType, double fc_hz, double sample_rate_hz) {
  return std::min(fc_hz * kFarRatio, 0.45 * sample_rate_hz);
}

double gain_of(const PackedSection& words, SectionType type, double fc_hz,
               double sample_rate_hz) {
  const auto biquad = section_words_to_biquad(words);
  return stage_db(biquad, fc_hz) - stage_db(biquad, far_hz(type, fc_hz, sample_rate_hz));
}

double bandwidth_hz(double fc_hz, double bw_oct) {
  return fc_hz * (std::pow(2.0, bw_oct * 0.5) - std::pow(2.0, -bw_oct * 0.5));
}

bool root_admissible(std::uint16_t mag, std::uint16_t rsq, bool is_pole) {
  const auto [p, q] = pq(mag, rsq);
  return is_legal(p, q, is_pole) && magnitude_admissible(nearest_lattice_word(mag), is_pole);
}

double pole_radius_of(double fc_hz, double bw_oct, double sample_rate_hz) {
  return std::clamp(std::exp(-std::numbers::pi *
                             bandwidth_hz(fc_hz, std::max(bw_oct, 0.0)) / sample_rate_hz),
                    kPoleRMin, kPoleRMax);
}

double realized_hz(double hz, double radius, double sample_rate_hz) {
  const auto [mag, rsq] = words_from_root(hz, radius);
  const auto pair = geometry_from_words({0, 0, mag, rsq, 0}, sample_rate_hz).pole;
  const auto* conjugate = std::get_if<ConjugatePair>(&pair);
  return conjugate != nullptr ? conjugate->hz : hz;
}

std::array<std::uint16_t, 4> roots_from(double pole_hz, double pole_r, double zero_hz,
                                        double zero_r, std::size_t section,
                                        const std::array<std::uint16_t, 4>& current) {
  const auto [pole_mag, pole_rsq] = words_from_root(pole_hz, pole_r);
  if (!root_admissible(pole_mag, pole_rsq, true)) {
    return current;
  }
  const auto [zero_mag, zero_rsq] = words_from_root(zero_hz, zero_r);
  const std::uint16_t rsq = section == 5 ? kS6ZeroRsqWord : zero_rsq;
  if (!root_admissible(zero_mag, rsq, false)) {
    return current;
  }
  return {zero_mag, rsq, pole_mag, pole_rsq};
}

}  // namespace

double trench_floor_radius() { return s6_zero_radius(); }

SectionParam param_of(const PackedSection& words, double sample_rate_hz) {
  const auto geometry = geometry_from_words(words, sample_rate_hz);
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  if (pole == nullptr) {
    return {};
  }
  const double zero_hz = root_hz(geometry.zero, pole->hz, 0.4665 * sample_rate_hz);
  if (pole->hz > kParkedHz && zero_hz > kParkedHz) {
    return {SectionType::kOff, pole->hz, 0.0, 0.0};
  }
  SectionParam out;
  out.fc_hz = pole->hz;
  const double bw_hz =
      -std::log(std::max(pole->radius, 1e-9)) * sample_rate_hz / std::numbers::pi;
  out.bw_oct = 2.0 * std::asinh(bw_hz / (2.0 * out.fc_hz)) / std::numbers::ln2;
  const double octaves = std::log2(std::max(zero_hz, 1.0) / std::max(pole->hz, 1.0));
  out.type = octaves >= kTypeOffsetOct ? SectionType::kLowPass : SectionType::kEq;
  if (out.type == SectionType::kLowPass) {
    out.trench_hz = zero_hz;
    return out;
  }
  out.gain_db = gain_of(words, out.type, out.fc_hz, sample_rate_hz);
  return out;
}

namespace {

double trench_of(const SectionParam& param, double pole_hz) {
  const double lo = pole_hz * std::pow(2.0, kTrenchMinOct);
  const double hi = std::min(pole_hz * std::pow(2.0, kTrenchMaxOct), kRootHiHz);
  const double wanted = param.trench_hz > 0.0 ? param.trench_hz : pole_hz * 32.0;
  return std::clamp(wanted, std::min(lo, hi), hi);
}

}  // namespace

std::array<std::uint16_t, 4> words_from_param(const SectionParam& param,
                                              const std::array<std::uint16_t, 4>& current,
                                              std::size_t section, double sample_rate_hz) {
  double pole_hz = kParkedRootHz;
  double pole_r = kParkedRootR;
  double zero_hz = kParkedRootHz;
  if (param.type != SectionType::kOff) {
    pole_hz = std::clamp(param.fc_hz, 20.0, kRootHiHz);
    pole_r = pole_radius_of(pole_hz, param.bw_oct, sample_rate_hz);
    zero_hz = param.type == SectionType::kLowPass ? trench_of(param, pole_hz) : pole_hz;
  }
  const auto [pole_mag, pole_rsq] = words_from_root(pole_hz, pole_r);
  if (!root_admissible(pole_mag, pole_rsq, true)) {
    return current;
  }

  const bool forced = section == 5;
  const bool trench = param.type == SectionType::kLowPass;
  if (forced || trench || param.type == SectionType::kOff) {
    const double radius = forced || trench ? s6_zero_radius() : kParkedRootR;
    const auto [zero_mag, zero_rsq] = words_from_root(zero_hz, radius);
    const std::uint16_t rsq = forced ? kS6ZeroRsqWord : zero_rsq;
    if (!root_admissible(zero_mag, rsq, false)) {
      return current;
    }
    return {zero_mag, rsq, pole_mag, pole_rsq};
  }

  std::array<std::uint16_t, 4> best = current;
  double best_error = std::numeric_limits<double>::infinity();
  for (const double decoded : lattice_decoded()) {
    const double radius = std::sqrt(std::max(1.0 - decoded, 0.0));
    const auto [zero_mag, zero_rsq] = words_from_root(zero_hz, radius);
    if (!root_admissible(zero_mag, zero_rsq, false)) {
      continue;
    }
    const PackedSection probe{zero_mag, zero_rsq, pole_mag, pole_rsq, kProbeScaleWord};
    const double error =
        std::abs(gain_of(probe, param.type, pole_hz, sample_rate_hz) - param.gain_db);
    if (error < best_error) {
      best_error = error;
      best = {zero_mag, zero_rsq, pole_mag, pole_rsq};
    }
  }
  return best;
}

std::array<std::uint16_t, 4> words_from_param_keeping_offset(
    const SectionParam& param, SectionEdit edit,
    const std::array<std::uint16_t, 4>& current, std::size_t section,
    double sample_rate_hz) {
  if (edit == SectionEdit::kType || param.type == SectionType::kOff) {
    return words_from_param(param, current, section, sample_rate_hz);
  }
  const PackedSection words{current[0], current[1], current[2], current[3],
                            kProbeScaleWord};
  const auto geometry = geometry_from_words(words, sample_rate_hz);
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  const auto* zero = std::get_if<ConjugatePair>(&geometry.zero);
  if (pole == nullptr) {
    return words_from_param(param, current, section, sample_rate_hz);
  }
  if (zero == nullptr) {
    if (edit == SectionEdit::kFc) {
      const double target = std::clamp(param.fc_hz, 20.0, kRootHiHz);
      const std::uint16_t pole_mag = mag_word_for(target, current[3]);
      if (!root_admissible(pole_mag, current[3], true)) return current;
      return {current[0], current[1], pole_mag, current[3]};
    }
    if (edit == SectionEdit::kBw) {
      const auto [pole_mag, pole_rsq] =
          words_from_root(pole->hz, pole_radius_of(pole->hz, param.bw_oct, sample_rate_hz));
      if (!root_admissible(pole_mag, pole_rsq, true)) return current;
      return {current[0], current[1], pole_mag, pole_rsq};
    }
    return words_from_param(param, current, section, sample_rate_hz);
  }

  double pole_hz = pole->hz;
  double pole_r = pole->radius;
  double zero_hz = zero->hz;
  const double zero_r = section == 5 ? s6_zero_radius() : zero->radius;

  if (param.type == SectionType::kLowPass && edit == SectionEdit::kGain) {
    return roots_from(pole_hz, pole_r, trench_of(param, pole_hz), s6_zero_radius(), section,
                      current);
  }
  if (edit == SectionEdit::kFc) {
    const double target = std::clamp(param.fc_hz, 20.0, kRootHiHz);
    const auto realized = realized_hz(target, pole_r, sample_rate_hz);
    zero_hz = std::clamp(zero_hz * realized / std::max(pole_hz, 1.0), 20.0, kRootHiHz);
    const std::uint16_t pole_mag = mag_word_for(target, current[3]);
    const std::uint16_t zero_mag = mag_word_for(zero_hz, current[1]);
    if (!root_admissible(pole_mag, current[3], true) ||
        !root_admissible(zero_mag, current[1], false)) {
      return current;
    }
    return {zero_mag, current[1], pole_mag, current[3]};
  } else if (edit == SectionEdit::kBw) {
    pole_r = pole_radius_of(pole_hz, param.bw_oct, sample_rate_hz);
  }
  if (edit != SectionEdit::kGain || section == 5) {
    return roots_from(pole_hz, pole_r, zero_hz, zero_r, section, current);
  }

  const auto [pole_mag, pole_rsq] = words_from_root(pole_hz, pole_r);
  if (!root_admissible(pole_mag, pole_rsq, true)) {
    return current;
  }
  std::array<std::uint16_t, 4> best = current;
  double best_error = std::numeric_limits<double>::infinity();
  for (const double decoded : lattice_decoded()) {
    const double radius = std::sqrt(std::max(1.0 - decoded, 0.0));
    const auto [zero_mag, zero_rsq] = words_from_root(zero_hz, radius);
    if (!root_admissible(zero_mag, zero_rsq, false)) {
      continue;
    }
    const PackedSection probe{zero_mag, zero_rsq, pole_mag, pole_rsq, kProbeScaleWord};
    const auto pair = geometry_from_words(probe, sample_rate_hz).zero;
    if (!std::holds_alternative<ConjugatePair>(pair)) {
      continue;
    }
    const double error =
        std::abs(gain_of(probe, param.type, pole_hz, sample_rate_hz) - param.gain_db);
    if (error < best_error) {
      best_error = error;
      best = {zero_mag, zero_rsq, pole_mag, pole_rsq};
    }
  }
  return best;
}

double mask_width_floor_hz(double sample_rate_hz) {
  return -std::log(s6_zero_radius()) * sample_rate_hz / std::numbers::pi;
}

std::optional<PoleReading> pole_of(const PackedSection& words, double sample_rate_hz) {
  const auto geometry = geometry_from_words(words, sample_rate_hz);
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  if (pole == nullptr) return std::nullopt;
  if (pole->hz > kParkedHz) return std::nullopt;
  return PoleReading{pole->hz,
                     -std::log(std::max(pole->radius, 1e-9)) * sample_rate_hz /
                         std::numbers::pi};
}

MaskParam parked_mask(double pole_hz, std::size_t section) {
  const double offset = section == 5 ? mask_offset_max_oct(section)
                                     : std::log2(kParkedZeroHz / std::max(pole_hz, 1.0));
  return {std::clamp(offset, mask_offset_min_oct(section), mask_offset_max_oct(section)),
          kParkedWidthHz};
}

MaskParam mask_of(const PackedSection& words, std::size_t section,
                  double sample_rate_hz) {
  const auto geometry = geometry_from_words(words, sample_rate_hz);
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  const auto* zero = std::get_if<ConjugatePair>(&geometry.zero);
  if (pole == nullptr) return {mask_offset_max_oct(section), kParkedWidthHz};
  if (zero == nullptr) return parked_mask(pole->hz, section);
  MaskParam out;
  out.offset_oct = std::clamp(std::log2(std::max(zero->hz, 1.0) / std::max(pole->hz, 1.0)),
                              mask_offset_min_oct(section), mask_offset_max_oct(section));
  out.zero_bw_hz =
      std::clamp(-std::log(std::max(zero->radius, 1e-9)) * sample_rate_hz / std::numbers::pi,
                 mask_width_floor_hz(sample_rate_hz), kMaskWidthMaxHz);
  return out;
}

std::array<std::uint16_t, 4> words_from_pole(double hz, double bw_hz,
                                             const std::array<std::uint16_t, 4>& current,
                                             std::size_t section, double sample_rate_hz) {
  const double pole_hz = std::clamp(hz, 20.0, kRootHiHz);
  const double radius = std::clamp(
      std::exp(-std::numbers::pi * std::max(bw_hz, 0.0) / sample_rate_hz), kPoleRMin,
      kPoleRMax);
  const auto [pole_mag, pole_rsq] = words_from_root(pole_hz, radius);
  if (!root_admissible(pole_mag, pole_rsq, true)) return current;
  const std::array<std::uint16_t, 4> out{current[0], current[1], pole_mag, pole_rsq};
  return words_with_parked_zero(out, section, sample_rate_hz);
}

PackedSection section_narrowed_toward_ceiling(const PackedSection& words, double amount,
                                              double sample_rate_hz) {
  if (words == kIdentitySection) return words;
  const auto geometry = geometry_from_words(words, sample_rate_hz);
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  if (pole == nullptr) return words;
  const double bw_hz =
      -std::log(std::max(pole->radius, 1e-9)) * sample_rate_hz / std::numbers::pi;
  if (bw_hz <= 0.0) return words;
  const double blend = std::clamp(amount, 0.0, 1.0);
  const double narrowed = std::exp((1.0 - blend) * std::log(bw_hz) +
                                   blend * std::log(kNarrowestPoleBwHz));
  const double radius = std::clamp(
      std::exp(-std::numbers::pi * narrowed / sample_rate_hz), kPoleRMin, kPoleRMax);
  const auto [mag, rsq] = words_from_root(pole->hz, radius);
  if (!root_admissible(mag, rsq, true)) return words;
  return {words[0], words[1], mag, rsq, words[4]};
}

std::array<std::uint16_t, 4> words_with_parked_zero(
    const std::array<std::uint16_t, 4>& current, std::size_t section,
    double sample_rate_hz) {
  const auto geometry = geometry_from_words(
      {current[0], current[1], current[2], current[3], 0}, sample_rate_hz);
  if (std::holds_alternative<ConjugatePair>(geometry.zero)) return current;
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  if (pole == nullptr) return current;
  return words_from_mask(parked_mask(pole->hz, section), current, section, sample_rate_hz);
}

std::array<std::uint16_t, 4> words_from_mask(const MaskParam& mask,
                                             const std::array<std::uint16_t, 4>& current,
                                             std::size_t section, double sample_rate_hz) {
  const PackedSection words{current[0], current[1], current[2], current[3],
                            kProbeScaleWord};
  const auto geometry = geometry_from_words(words, sample_rate_hz);
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  if (pole == nullptr) return current;
  const double offset = std::clamp(mask.offset_oct, mask_offset_min_oct(section),
                                   mask_offset_max_oct(section));
  const double zero_hz = std::clamp(pole->hz * std::pow(2.0, offset), 20.0, kRootHiHz);
  const double radius =
      section == 5 ? s6_zero_radius()
                   : std::clamp(std::exp(-std::numbers::pi *
                                         std::clamp(mask.zero_bw_hz, 0.0, kMaskWidthMaxHz) /
                                         sample_rate_hz),
                                0.0, s6_zero_radius());
  const auto [zero_mag, zero_rsq] = words_from_root(zero_hz, radius);
  const std::uint16_t rsq = section == 5 ? kS6ZeroRsqWord : zero_rsq;
  if (!root_admissible(zero_mag, rsq, false)) return current;
  return {zero_mag, rsq, current[2], current[3]};
}

}  // namespace trench::core::p2k
