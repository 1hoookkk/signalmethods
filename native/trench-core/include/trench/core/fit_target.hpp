#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace trench::core {

enum class TargetKind { kEnvelope, kTransferFunction };

struct FitTarget {
  std::vector<double> frequency_hz;
  std::vector<double> magnitude_db;
  std::vector<double> phase_rad;
  std::vector<double> weight;
  TargetKind kind{TargetKind::kEnvelope};
  bool absolute_level{false};

  [[nodiscard]] bool valid() const noexcept;
  bool operator==(const FitTarget&) const = default;
};

std::optional<FitTarget> read_fit_target(const std::filesystem::path& path);
std::vector<double> magnitude_on_grid(const FitTarget& target,
                                      std::span<const double> frequency_hz);

}  // namespace trench::core
