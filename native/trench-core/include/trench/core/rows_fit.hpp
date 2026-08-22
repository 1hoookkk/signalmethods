#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>

#include "trench/core/p2k.hpp"
#include "trench/core/section_param.hpp"

namespace trench::core::p2k {

using Rows = std::array<SectionParam, 6>;

struct RowsStep {
  std::size_t section{};
  Rows rows{};
  CornerWords words{};
  double rms_db{};
};

struct RowsFit {
  Rows rows{};
  CornerWords words{};
  double rms_db{};
  bool stopped{};
};

struct RowsFitOptions {
  std::size_t max_passes = 8;
  double fc_span_oct = 1.0;
  double bw_lo_oct = 0.02;
  double bw_hi_oct = 4.0;
  double gain_lo_db = -48.0;
  double gain_hi_db = 48.0;
  std::size_t steps = 24;
};

Rows seed_rows_from_target(std::span<const double> target, const Grid& g = grid());

Rows rows_of_corner(const CornerWords& words, double sample_rate_hz = kP2kDatumHz);

bool row_held(std::size_t section, std::uint32_t mask);

CornerWords words_from_rows(const Rows& rows, const CornerWords& held, std::uint32_t mask,
                            double sample_rate_hz = kP2kDatumHz);

CornerWords words_from_rows(const Rows& rows, double sample_rate_hz = kP2kDatumHz);

double rows_rms_db(const Rows& rows, std::span<const double> target, const Grid& g = grid());

std::optional<RowsFit> fit_rows_watched(std::span<const double> target, Rows seed,
                                        const CornerWords& held, std::uint32_t mask,
                                        const RowsFitOptions& opts, const Grid& g,
                                        const std::function<bool()>& stop_requested,
                                        const std::function<void(const RowsStep&)>& on_step);

}  // namespace trench::core::p2k
