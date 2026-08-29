#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace trench::app {

enum class ImportKind {
  kSound,
  kPoleMaterial,
  kResponseTable,
  kFormantTrack,
  kDocument,
  kPackedBody,
  kUnknown
};

struct ResponseCurve {
  std::vector<double> frequency_hz;
  std::vector<double> magnitude_db;
};

using PoleRows = std::vector<std::pair<double, double>>;

struct FormantTrack {
  PoleRows median;
  std::size_t frames{};
};

[[nodiscard]] std::string lower_extension(const std::filesystem::path& path);
[[nodiscard]] ImportKind classify_import(const std::filesystem::path& path);
[[nodiscard]] std::optional<ResponseCurve> read_response_curve(
    const std::filesystem::path& path);
[[nodiscard]] std::optional<PoleRows> read_pole_material(
    const std::filesystem::path& path);
[[nodiscard]] std::optional<FormantTrack> read_formant_track(
    const std::filesystem::path& path);

}  // namespace trench::app
