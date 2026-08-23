#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "trench/core/packed_body.hpp"
#include "trench/core/section_param.hpp"

namespace trench::core::p2k {

struct Formant {
  double hz{};
  double bw_hz{};
};

struct VowelFormants {
  std::string_view symbol;
  std::array<Formant, 3> f;
};

std::span<const VowelFormants> klatt_vowels();
const VowelFormants* klatt_vowel(std::string_view symbol);

struct FormantRecipe {
  std::array<SectionParam, 6> rows;
};

FormantRecipe rows_from_formants(std::span<const Formant> formants, double tilt_lo_hz = 225.0);

std::array<std::array<std::uint16_t, 4>, 6> words_from_recipe(const FormantRecipe& recipe,
                                                             double sample_rate_hz = kP2kDatumHz);

struct NamedRecipe {
  std::string_view name;
  FormantRecipe recipe;
};

std::span<const NamedRecipe> manual_recipes();
const NamedRecipe* manual_recipe(std::string_view name);

struct SpectralPeak {
  double hz{};
  double db{};
  double bw_hz{};
};

std::vector<SpectralPeak> peaks_of_envelope(std::span<const double> hz, std::span<const double> db,
                                            std::size_t max_peaks = 6);

}  // namespace trench::core::p2k
