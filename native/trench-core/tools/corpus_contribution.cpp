#include "trench/core/packed_body.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace trench::core;

namespace {

struct CorpusBody {
  fs::path path;
  PackedBody body;
  double datum_hz{};
};

struct Counts {
  std::size_t cells{};
  std::size_t finite_cells{};
  std::size_t nonfinite_cells{};
  std::size_t at_most_04{};
  std::size_t at_most_1{};
  std::size_t at_most_3{};
  std::size_t at_most_6{};
  std::size_t above_6{};
};

std::vector<std::uint8_t> read_file(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) throw std::runtime_error("cannot read " + path.string());
  const auto size = static_cast<std::size_t>(stream.tellg());
  std::vector<std::uint8_t> bytes(size);
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  return bytes;
}

void write_u16(std::uint16_t value, std::span<std::uint8_t> bytes, std::size_t offset) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

PackedBody normalize_x3_runtime_block(std::span<const std::uint8_t> source) {
  constexpr std::size_t bytes_per_section_across_corners = kLegacyCornerCount * 10;
  if (source.empty() || source.size() % bytes_per_section_across_corners != 0) {
    throw std::invalid_argument("X3 block does not contain four equal corners");
  }
  const auto source_sections = source.size() / bytes_per_section_across_corners;
  if (source_sections > kLegacySectionCount) {
    throw std::invalid_argument("X3 block has more than six sections");
  }
  std::array<std::uint8_t, kLegacyBodyBytes> legacy{};
  for (std::size_t corner = 0; corner < kLegacyCornerCount; ++corner) {
    for (std::size_t section = 0; section < kLegacySectionCount; ++section) {
      const auto destination = (corner * kLegacySectionCount + section) * 10;
      if (section < source_sections) {
        const auto input = (corner * source_sections + section) * 10;
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(input), 10,
                    legacy.begin() + static_cast<std::ptrdiff_t>(destination));
      } else {
        for (std::size_t word = 0; word < kCoefficientCount; ++word) {
          write_u16(kIdentitySection[word], legacy, destination + word * 2);
        }
      }
    }
  }
  return PackedBody::from_legacy_bytes(legacy);
}

void append_directory(std::vector<CorpusBody>& result, const fs::path& root,
                      const fs::path& relative, const std::string& extension,
                      double datum_hz, bool x3 = false) {
  const auto directory = root / relative;
  if (!fs::exists(directory)) return;
  std::vector<fs::path> paths;
  for (const auto& entry : fs::directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  for (const auto& path : paths) {
    const auto bytes = read_file(path);
    result.push_back({path,
                      x3 ? normalize_x3_runtime_block(bytes)
                         : PackedBody::from_body_bytes(bytes),
                      datum_hz});
  }
}

std::vector<CorpusBody> load_corpus(const fs::path& root) {
  std::vector<CorpusBody> result;
  append_directory(result, root, "ref/morpheus/bodies", ".body", kMorpheusDatumHz);
  append_directory(result, root, "recipes/hero", ".body", kP2kDatumHz);
  append_directory(result, root, "recipes/extrusions", ".body", kP2kDatumHz);
  append_directory(result, root, "ref/presets", ".bin", kP2kDatumHz);
  append_directory(result, root, "ref/x3", ".bin", kP2kDatumHz, true);
  append_directory(result, root, "ref/md_templates", ".bin", kP2kDatumHz);
  return result;
}

double maximum_absolute_finite(std::span<const double> values, bool& finite) {
  double result = 0.0;
  finite = true;
  for (const auto value : values) {
    if (!std::isfinite(value)) {
      finite = false;
      continue;
    }
    result = std::max(result, std::abs(value));
  }
  return result;
}

void add_effect(Counts& counts, double effect_db, bool finite) {
  ++counts.cells;
  if (!finite) {
    ++counts.nonfinite_cells;
    return;
  }
  ++counts.finite_cells;
  if (effect_db <= 0.4) ++counts.at_most_04;
  else if (effect_db <= 1.0) ++counts.at_most_1;
  else if (effect_db <= 3.0) ++counts.at_most_3;
  else if (effect_db <= 6.0) ++counts.at_most_6;
  else ++counts.above_6;
}

void print_counts(const std::string& label, const Counts& counts) {
  const auto pct = [&counts](std::size_t value) {
    return counts.finite_cells == 0 ? 0.0
                                    : 100.0 * static_cast<double>(value) /
                                          static_cast<double>(counts.finite_cells);
  };
  std::cout << label << " (finite n=" << counts.finite_cells << ")\n"
            << "  <=0.4 dB  " << std::setw(6) << counts.at_most_04 << "  "
            << std::fixed << std::setprecision(1) << pct(counts.at_most_04) << "%\n"
            << "  0.4-1 dB  " << std::setw(6) << counts.at_most_1 << "  "
            << pct(counts.at_most_1) << "%\n"
            << "  1-3 dB    " << std::setw(6) << counts.at_most_3 << "  "
            << pct(counts.at_most_3) << "%\n"
            << "  3-6 dB    " << std::setw(6) << counts.at_most_6 << "  "
            << pct(counts.at_most_6) << "%\n"
            << "  >6 dB     " << std::setw(6) << counts.above_6 << "  "
            << pct(counts.above_6) << "%\n";
  if (counts.nonfinite_cells != 0) {
    std::cout << "  nonfinite " << std::setw(6) << counts.nonfinite_cells << "\n";
  }
}

enum class IntervalClass { none, local, cross };

IntervalClass interval_class(const SectionGeometry& geometry) {
  const auto* pole = std::get_if<ConjugatePair>(&geometry.pole);
  const auto* zero = std::get_if<ConjugatePair>(&geometry.zero);
  if (pole == nullptr || zero == nullptr || pole->hz <= 0.0 || zero->hz <= 0.0) {
    return IntervalClass::none;
  }
  const double semitones = 12.0 * std::log2(zero->hz / pole->hz);
  return std::abs(semitones) <= 24.0 ? IntervalClass::local : IntervalClass::cross;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::current_path();
    const auto corpus = load_corpus(root);
    Counts all;
    Counts local;
    Counts cross;
    std::size_t interval_none = 0;
    for (const auto& item : corpus) {
      const auto grid = logarithmic_frequency_grid(20.0, item.datum_hz * 0.49, 768);
      for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
        Cascade cascade{};
        for (std::size_t section = 0; section < kSectionCount; ++section) {
          cascade[section] = section_words_to_biquad(item.body.words[corner][section]);
        }
        const auto curves = marginal_contributions_db(cascade, grid, item.datum_hz);
        for (std::size_t section = 0; section < kSectionCount; ++section) {
          bool finite = false;
          const auto effect = maximum_absolute_finite(curves[section], finite);
          add_effect(all, effect, finite);
          const auto geometry = geometry_from_words(item.body.words[corner][section],
                                                    item.datum_hz);
          switch (interval_class(geometry)) {
            case IntervalClass::local:
              add_effect(local, effect, finite);
              break;
            case IntervalClass::cross:
              add_effect(cross, effect, finite);
              break;
            case IntervalClass::none:
              ++interval_none;
              break;
          }
        }
      }
    }
    std::cout << "Trench marginal cascade contribution census\n"
              << "  bodies " << corpus.size() << "\n"
              << "  cells  " << all.cells << "\n"
              << "  grid   768 logarithmic points, 20 Hz .. 0.49 * datum\n"
              << "  law    cascade dB(all) - cascade dB(all minus section), packed scale included\n\n";
    print_counts("all cells", all);
    std::cout << "\nold interval labels (descriptive only)\n"
              << "  local (|zero-pole| <=24 st) " << local.cells << "\n"
              << "  cross (|zero-pole| >24 st)  " << cross.cells << "\n"
              << "  unlabelled geometry          " << interval_none << "\n\n";
    print_counts("local-labelled cells", local);
    std::cout << '\n';
    print_counts("cross-labelled cells", cross);
    return corpus.size() == 424 && all.cells == 23'744 ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
