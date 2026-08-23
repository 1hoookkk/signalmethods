#include "trench/core/packed_body.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace trench::core;

namespace {

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

PackedBody normalize_x3(std::span<const std::uint8_t> source) {
  if (source.empty() || source.size() % 40 != 0) {
    throw std::invalid_argument("invalid X3 runtime block size");
  }
  const auto source_sections = source.size() / 40;
  if (source_sections > kLegacySectionCount) {
    throw std::invalid_argument("X3 runtime block has too many sections");
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

struct CorpusItem {
  fs::path path;
  PackedBody body;
  double datum{};
  std::size_t original_size{};
  bool x3{};
};

void add_directory(std::vector<CorpusItem>& result, const fs::path& root,
                   const fs::path& relative, const std::string& extension,
                   double datum, bool x3 = false) {
  std::vector<fs::path> paths;
  for (const auto& entry : fs::directory_iterator(root / relative)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  for (const auto& path : paths) {
    const auto bytes = read_file(path);
    result.push_back({path, x3 ? normalize_x3(bytes) : PackedBody::from_body_bytes(bytes),
                      datum, bytes.size(), x3});
  }
}

std::vector<CorpusItem> corpus() {
  const fs::path root = TRENCH_SOURCE_ROOT;
  std::vector<CorpusItem> result;
  add_directory(result, root, "ref/morpheus/bodies", ".body", kMorpheusDatumHz);
  add_directory(result, root, "recipes/hero", ".body", kP2kDatumHz);
  add_directory(result, root, "recipes/extrusions", ".body", kP2kDatumHz);
  add_directory(result, root, "ref/presets", ".bin", kP2kDatumHz);
  add_directory(result, root, "ref/x3", ".bin", kP2kDatumHz, true);
  add_directory(result, root, "ref/md_templates", ".bin", kP2kDatumHz);
  return result;
}

}  // namespace

TEST(Minifloat, EveryWordIsAnEncodeDecodeFixedPoint) {
  for (std::uint32_t word = 0; word <= 0xFFFFU; ++word) {
    EXPECT_EQ(encode_word(decode_word(static_cast<std::uint16_t>(word))), word)
        << "word 0x" << std::hex << word;
  }
}

TEST(PackedBody, NativeAndLegacyContainersRoundTripExactly) {
  const auto items = corpus();
  for (const auto& item : items) {
    if (item.x3) continue;
    const auto original = read_file(item.path);
    if (item.original_size == kNativeBodyBytes) {
      const auto output = item.body.native_bytes();
      EXPECT_TRUE(std::equal(output.begin(), output.end(), original.begin())) << item.path;
    } else {
      ASSERT_EQ(item.original_size, kLegacyBodyBytes) << item.path;
      const auto output = item.body.legacy_bytes();
      EXPECT_TRUE(std::equal(output.begin(), output.end(), original.begin())) << item.path;
    }
  }
}

TEST(Geometry, AllCorpusCellsRoundTripWithoutLosingRealAxisPairs) {
  const auto items = corpus();
  std::size_t mismatches = 0;
  for (const auto& item : items) {
    for (const auto& corner : item.body.words) {
      for (const auto& packed : corner) {
        const auto geometry = geometry_from_words(packed, item.datum);
        const auto rebuilt = words_from_geometry(geometry, item.datum);
        if (rebuilt != packed) ++mismatches;
      }
    }
  }
  EXPECT_EQ(mismatches, 0U);
}

TEST(Interpolation, CornerAxesPreserveSectionIndices) {
  PackedBody body;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    for (std::size_t section = 0; section < kSectionCount; ++section) {
      body.words[corner][section].fill(
          static_cast<std::uint16_t>(1000 * corner + 10 * section));
    }
  }
  EXPECT_EQ(body.interpolate_words(0.0F, 0.0F, 0.0F), body.words[0]);
  EXPECT_EQ(body.interpolate_words(1.0F, 0.0F, 0.0F), body.words[1]);
  EXPECT_EQ(body.interpolate_words(0.0F, 1.0F, 0.0F), body.words[2]);
  EXPECT_EQ(body.interpolate_words(0.0F, 0.0F, 1.0F), body.words[4]);
}

TEST(Response, MarginalContributionIncludesMultiplicativeSectionGain) {
  const std::array<Biquad, 2> cascade{{
      {2.0, 0.0, 0.0, 0.0, 0.0},
      {0.5, 0.0, 0.0, 0.0, 0.0},
  }};
  const std::array<double, 2> frequencies{100.0, 10'000.0};
  const auto first = marginal_contribution_db(cascade, 0, frequencies, kP2kDatumHz);
  const auto second = marginal_contribution_db(cascade, 1, frequencies, kP2kDatumHz);
  const auto batch = marginal_contributions_db(cascade, frequencies, kP2kDatumHz);
  for (const auto value : first) EXPECT_NEAR(value, 20.0 * std::log10(2.0), 1e-12);
  for (const auto value : second) EXPECT_NEAR(value, 20.0 * std::log10(0.5), 1e-12);
  EXPECT_EQ(batch.size(), 2U);
  for (std::size_t i = 0; i < frequencies.size(); ++i) {
    EXPECT_NEAR(batch[0][i], first[i], 1e-12);
    EXPECT_NEAR(batch[1][i], second[i], 1e-12);
  }
  EXPECT_NEAR(cascade_response_db(cascade, 1000.0, kP2kDatumHz), 0.0, 1e-12);
}

TEST(Response, IdentitySectionHasZeroMarginalContribution) {
  const std::array<Biquad, 1> cascade{{section_words_to_biquad(kIdentitySection)}};
  const auto frequencies = logarithmic_frequency_grid(20.0, 20'000.0, 32);
  const auto contribution = marginal_contribution_db(cascade, 0, frequencies, kP2kDatumHz);
  for (const auto value : contribution) EXPECT_DOUBLE_EQ(value, 0.0);
}

TEST(Response, TalkingHedzMatchesTheRustCompleteCascadeOracle) {
  const auto bytes = read_file(fs::path(TRENCH_SOURCE_ROOT) /
                               "ref/presets/P2k_013_talking_hedz.bin");
  const auto body = PackedBody::from_legacy_bytes(bytes);
  const std::array<float, 3> morphs{0.0F, 0.5F, 1.0F};
  const std::array<std::array<long, 14>, 3> expected{{
      {4, 6, -1, -7, -9, -6, -14, -19, -27, -30, -35, -40, -30, -32},
      {1, 2, 6, 12, 13, -18, -25, -15, -12, -25, -24, -34, -41, -38},
      {4, 6, -3, -12, -22, -28, -25, 1, -3, -18, -15, -16, -35, -60},
  }};
  for (std::size_t pose = 0; pose < morphs.size(); ++pose) {
    const auto cascade = body.interpolate_biquads(morphs[pose], 0.0F, 0.0F);
    for (std::size_t point = 0; point < expected[pose].size(); ++point) {
      const auto frequency = 100.0 * std::pow(160.0, static_cast<double>(point + 1) / 14.0);
      const auto actual = cascade_response_db(cascade, frequency, kMorpheusDatumHz);
      EXPECT_EQ(std::lround(actual), expected[pose][point])
          << "pose " << pose << " point " << point << " at " << frequency << " Hz";
    }
  }
}
