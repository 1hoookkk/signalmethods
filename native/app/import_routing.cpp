#include "import_routing.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace trench::app {
namespace {

std::optional<PoleRows> readRows(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return std::nullopt;
  PoleRows rows;
  std::string line;
  while (std::getline(stream, line)) {
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) continue;
    if (line[first] == '#' || line[first] == ';' || line[first] == '*') continue;
    std::replace(line.begin(), line.end(), ',', ' ');
    std::stringstream row(line);
    double column_one = 0.0;
    double column_two = 0.0;
    if (!(row >> column_one >> column_two)) return std::nullopt;
    if (!std::isfinite(column_one) || !std::isfinite(column_two)) {
      return std::nullopt;
    }
    rows.emplace_back(column_one, column_two);
  }
  return rows;
}

}  // namespace

std::string lower_extension(const std::filesystem::path& path) {
  std::string result = path.extension().string();
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

ImportKind classify_import(const std::filesystem::path& path) {
  const std::string extension = lower_extension(path);
  if (extension == ".wav" || extension == ".aif" || extension == ".aiff" ||
      extension == ".flac") {
    return ImportKind::kSound;
  }
  if (extension == ".fbw") return ImportKind::kPoleMaterial;
  if (extension == ".csv" || extension == ".txt") {
    return ImportKind::kResponseTable;
  }
  if (extension == ".trenchbody") return ImportKind::kDocument;
  if (extension == ".body240" || extension == ".bin") {
    return ImportKind::kPackedBody;
  }
  return ImportKind::kUnknown;
}

std::optional<ResponseCurve> read_response_curve(
    const std::filesystem::path& path) {
  const auto rows = readRows(path);
  if (!rows || rows->size() < 2) return std::nullopt;
  ResponseCurve curve;
  curve.frequency_hz.reserve(rows->size());
  curve.magnitude_db.reserve(rows->size());
  for (std::size_t index = 0; index < rows->size(); ++index) {
    const double frequency = (*rows)[index].first;
    if (!(frequency > 0.0)) return std::nullopt;
    if (index > 0 && !(frequency > (*rows)[index - 1].first)) {
      return std::nullopt;
    }
    curve.frequency_hz.push_back(frequency);
    curve.magnitude_db.push_back((*rows)[index].second);
  }
  return curve;
}

std::optional<PoleRows> read_pole_material(const std::filesystem::path& path) {
  auto rows = readRows(path);
  if (!rows || rows->empty()) return std::nullopt;
  for (const auto& row : *rows) {
    if (!(row.first > 0.0) || !(row.second > 0.0)) return std::nullopt;
  }
  return rows;
}

}  // namespace trench::app
