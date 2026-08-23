#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"

namespace fs = std::filesystem;
namespace p2k = trench::core::p2k;

namespace {

constexpr std::size_t kColumns = 14;
constexpr std::array<const char*, kColumns> kHeadings{
    "max_step", "mean_step", "worst_m",  "worst_q",  "refused",  "exc_up",   "exc_dn",
    "bilin_max", "bilin_p95", "detour",  "loud_sw",  "loud_beyond", "prefix_hi", "prefix_lo"};

std::vector<std::pair<std::string, std::vector<std::uint8_t>>> bank(const fs::path& dir) {
  std::vector<fs::path> paths;
  for (const auto& entry : fs::directory_iterator(dir)) {
    if (entry.path().extension() == ".bin") {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> out;
  for (const auto& path : paths) {
    std::ifstream in(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    out.emplace_back(path.stem().string(), std::move(bytes));
  }
  return out;
}

std::array<double, kColumns> row_of(const p2k::InteriorAudit& a) {
  return {a.max_step_db,
          a.mean_step_db,
          static_cast<double>(a.worst_morph),
          static_cast<double>(a.worst_q),
          static_cast<double>(a.refused),
          a.excursion_up_db,
          a.excursion_down_db,
          a.bilinear_dev_max_db,
          a.bilinear_dev_p95_db,
          a.detour_max,
          a.loudness_swing_db,
          a.loudness_beyond_corners_db,
          a.prefix_headroom_db,
          a.prefix_floor_db};
}

double median_of(std::vector<double> values) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const std::size_t n = values.size();
  return n % 2 == 1 ? values[n / 2] : 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

constexpr int kNameWidth = 26;
constexpr int kCellWidth = 12;

void print_row(const std::string& name, const std::array<double, kColumns>& cells) {
  std::cout << std::left << std::setw(kNameWidth) << name << std::right << std::fixed
            << std::setprecision(2);
  for (const double value : cells) {
    std::cout << std::setw(kCellWidth) << value;
  }
  std::cout << '\n';
}

}  // namespace

int main() {
  const fs::path root{TRENCH_SOURCE_ROOT};
  const auto bodies = bank(root / "ref" / "presets");
  if (bodies.empty()) {
    std::cerr << "no bodies under " << (root / "ref" / "presets").string() << '\n';
    return 1;
  }

  std::cout << std::left << std::setw(kNameWidth) << "body" << std::right;
  for (const char* heading : kHeadings) {
    std::cout << std::setw(kCellWidth) << heading;
  }
  std::cout << '\n';

  std::vector<std::array<double, kColumns>> rows;
  rows.reserve(bodies.size());
  for (const auto& [name, body] : bodies) {
    const auto cells = row_of(p2k::interior_audit(body, p2k::grid(), 33, 33));
    print_row(name, cells);
    rows.push_back(cells);
  }

  std::array<double, kColumns> medians{};
  std::array<double, kColumns> maxima{};
  for (std::size_t ci = 0; ci < kColumns; ++ci) {
    std::vector<double> column;
    column.reserve(rows.size());
    for (const auto& row : rows) {
      column.push_back(row[ci]);
    }
    medians[ci] = median_of(column);
    maxima[ci] = *std::max_element(column.begin(), column.end());
  }
  print_row("median", medians);
  print_row("max", maxima);
  return 0;
}
