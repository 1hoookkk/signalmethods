#include "trench/core/fit_target.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>

namespace trench::core {
namespace {

std::string trim(std::string value) {
  const auto whitespace = [](unsigned char c) { return std::isspace(c) != 0; };
  value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), whitespace));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(),
              value.end());
  return value;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::vector<std::string> fields(const std::string& line, const bool comma_separated) {
  std::vector<std::string> out;
  if (comma_separated) {
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) out.push_back(trim(std::move(field)));
    if (!line.empty() && line.back() == ',') out.emplace_back();
    return out;
  }
  std::stringstream stream(line);
  std::string field;
  while (stream >> field) out.push_back(std::move(field));
  return out;
}

std::optional<double> number(const std::string& text) {
  if (text.empty()) return std::nullopt;
  try {
    std::size_t used = 0;
    const double value = std::stod(text, &used);
    if (used != text.size() || !std::isfinite(value)) return std::nullopt;
    return value;
  } catch (...) {
    return std::nullopt;
  }
}

struct Row {
  double hz{};
  double db{};
  double phase{std::numeric_limits<double>::quiet_NaN()};
  double weight{1.0};
};

}

bool FitTarget::valid() const noexcept {
  const auto size = frequency_hz.size();
  if (size < 2 || magnitude_db.size() != size) return false;
  if (!phase_rad.empty() && phase_rad.size() != size) return false;
  if (!weight.empty() && weight.size() != size) return false;
  bool positive_weight = weight.empty();
  double previous = 0.0;
  for (std::size_t index = 0; index < size; ++index) {
    if (!std::isfinite(frequency_hz[index]) || frequency_hz[index] <= previous ||
        !std::isfinite(magnitude_db[index])) {
      return false;
    }
    previous = frequency_hz[index];
    if (!weight.empty()) {
      if (!std::isfinite(weight[index]) || weight[index] < 0.0) return false;
      positive_weight = positive_weight || weight[index] > 0.0;
    }
  }
  return positive_weight;
}

std::optional<FitTarget> read_fit_target(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return std::nullopt;

  std::string header;
  while (std::getline(stream, header)) {
    header = trim(std::move(header));
    if (!header.empty() && header.front() != '#') break;
  }
  if (header.empty()) return std::nullopt;
  const bool comma_separated = header.find(',') != std::string::npos;
  auto names = fields(header, comma_separated);
  for (auto& name : names) name = lower(trim(std::move(name)));
  const auto column = [&names](std::string_view name) -> std::optional<std::size_t> {
    const auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(names.begin(), found));
  };
  const auto hz_column = column("frequency_hz");
  const auto db_column = column("magnitude_db");
  const auto phase_column = column("phase_deg");
  const auto weight_column = column("weight");
  if (!hz_column || !db_column) return std::nullopt;

  std::vector<Row> rows;
  bool any_phase = false;
  std::string line;
  while (std::getline(stream, line)) {
    line = trim(std::move(line));
    if (line.empty() || line.front() == '#') continue;
    const auto values = fields(line, comma_separated);
    const auto cell = [&values](std::size_t index) -> std::string {
      return index < values.size() ? values[index] : std::string{};
    };
    const auto hz = number(cell(*hz_column));
    const auto db = number(cell(*db_column));
    if (!hz || !db || *hz <= 0.0) return std::nullopt;
    Row row{*hz, *db};
    if (phase_column) {
      const auto phase = number(cell(*phase_column));
      if (phase) {
        row.phase = *phase * std::numbers::pi / 180.0;
        any_phase = true;
      }
    }
    if (weight_column) {
      const auto parsed_weight = number(cell(*weight_column));
      if (!parsed_weight || *parsed_weight < 0.0) return std::nullopt;
      row.weight = *parsed_weight;
    }
    rows.push_back(row);
  }
  if (rows.size() < 2) return std::nullopt;
  std::stable_sort(rows.begin(), rows.end(),
                   [](const Row& lhs, const Row& rhs) { return lhs.hz < rhs.hz; });
  if (std::adjacent_find(rows.begin(), rows.end(), [](const Row& lhs, const Row& rhs) {
        return lhs.hz == rhs.hz;
      }) != rows.end()) {
    return std::nullopt;
  }

  FitTarget target;
  target.frequency_hz.reserve(rows.size());
  target.magnitude_db.reserve(rows.size());
  target.weight.reserve(rows.size());
  if (any_phase) target.phase_rad.reserve(rows.size());
  for (const auto& row : rows) {
    target.frequency_hz.push_back(row.hz);
    target.magnitude_db.push_back(row.db);
    target.weight.push_back(row.weight);
    if (any_phase) target.phase_rad.push_back(row.phase);
  }
  target.kind = any_phase ? TargetKind::kTransferFunction : TargetKind::kEnvelope;
  target.absolute_level = any_phase;
  if (!target.valid()) return std::nullopt;
  return target;
}

std::vector<double> magnitude_on_grid(const FitTarget& target,
                                      std::span<const double> frequency_hz) {
  std::vector<double> out;
  if (!target.valid()) return out;
  out.reserve(frequency_hz.size());
  for (const double hz : frequency_hz) {
    if (hz <= target.frequency_hz.front()) {
      out.push_back(target.magnitude_db.front());
      continue;
    }
    if (hz >= target.frequency_hz.back()) {
      out.push_back(target.magnitude_db.back());
      continue;
    }
    const auto upper = std::upper_bound(target.frequency_hz.begin(),
                                        target.frequency_hz.end(), hz);
    const auto hi = static_cast<std::size_t>(
        std::distance(target.frequency_hz.begin(), upper));
    const auto lo = hi - 1;
    const double log_lo = std::log(target.frequency_hz[lo]);
    const double log_hi = std::log(target.frequency_hz[hi]);
    const double t = (std::log(hz) - log_lo) / (log_hi - log_lo);
    out.push_back(std::lerp(target.magnitude_db[lo], target.magnitude_db[hi], t));
  }
  return out;
}

}
