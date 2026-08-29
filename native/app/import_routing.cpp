#include "import_routing.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace trench::app {
namespace {

std::optional<TemplateMaterial> readRows(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return std::nullopt;
  TemplateMaterial material;
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
    material.poles.emplace_back(column_one, column_two);
    double column_three = 0.0;
    double column_four = 0.0;
    if (row >> column_three >> column_four) {
      if (!std::isfinite(column_three) || !std::isfinite(column_four)) {
        return std::nullopt;
      }
      material.zeros.emplace_back(std::make_pair(column_three, column_four));
    } else {
      material.zeros.emplace_back(std::nullopt);
    }
  }
  return material;
}

}

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
  if (extension == ".table") return ImportKind::kFormantTrack;
  if (extension == ".trenchbody") return ImportKind::kDocument;
  if (extension == ".body240" || extension == ".bin") {
    return ImportKind::kPackedBody;
  }
  return ImportKind::kUnknown;
}

std::optional<ResponseCurve> read_response_curve(
    const std::filesystem::path& path) {
  const auto material = readRows(path);
  if (!material || material->poles.size() < 2) return std::nullopt;
  const PoleRows& rows = material->poles;
  ResponseCurve curve;
  curve.frequency_hz.reserve(rows.size());
  curve.magnitude_db.reserve(rows.size());
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const double frequency = rows[index].first;
    if (!(frequency > 0.0)) return std::nullopt;
    if (index > 0 && !(frequency > rows[index - 1].first)) {
      return std::nullopt;
    }
    curve.frequency_hz.push_back(frequency);
    curve.magnitude_db.push_back(rows[index].second);
  }
  return curve;
}

std::optional<PoleRows> read_pole_material(const std::filesystem::path& path) {
  auto material = read_template_material(path);
  if (!material) return std::nullopt;
  return std::move(material->poles);
}

std::optional<TemplateMaterial> read_template_material(
    const std::filesystem::path& path) {
  auto material = readRows(path);
  if (!material || material->poles.empty()) return std::nullopt;
  for (const auto& row : material->poles) {
    if (!(row.first > 0.0) || !(row.second > 0.0)) return std::nullopt;
  }
  for (const auto& row : material->zeros) {
    if (!row.has_value()) continue;
    if (!(row->first > 0.0) || !(row->second > 0.0)) return std::nullopt;
  }
  return material;
}

namespace {

std::vector<std::string> splitCommas(std::string line) {
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
  std::vector<std::string> cells;
  std::string cell;
  for (const char letter : line) {
    if (letter == ',') {
      cells.push_back(cell);
      cell.clear();
    } else {
      cell.push_back(letter);
    }
  }
  cells.push_back(cell);
  return cells;
}

std::optional<double> numericCell(const std::vector<std::string>& cells, std::size_t index) {
  if (index >= cells.size()) return std::nullopt;
  const std::string& text = cells[index];
  if (text.empty() || text == "--undefined--") return std::nullopt;
  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || !std::isfinite(value)) return std::nullopt;
  return value;
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2;
  return values.size() % 2 == 1 ? values[middle]
                                : 0.5 * (values[middle - 1] + values[middle]);
}

}

std::optional<FormantTrack> read_formant_track(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return std::nullopt;
  std::string line;
  if (!std::getline(stream, line)) return std::nullopt;
  const auto header = splitCommas(line);
  std::vector<std::pair<std::size_t, std::size_t>> columns;
  for (std::size_t k = 1;; ++k) {
    const std::string f = "F" + std::to_string(k) + "(Hz)";
    const std::string b = "B" + std::to_string(k) + "(Hz)";
    const auto fi = std::find(header.begin(), header.end(), f);
    const auto bi = std::find(header.begin(), header.end(), b);
    if (fi == header.end() || bi == header.end()) break;
    columns.emplace_back(static_cast<std::size_t>(fi - header.begin()),
                         static_cast<std::size_t>(bi - header.begin()));
  }
  if (columns.empty()) return std::nullopt;
  std::vector<std::vector<std::pair<double, double>>> frames;
  while (std::getline(stream, line)) {
    const auto cells = splitCommas(line);
    std::vector<std::pair<double, double>> pairs;
    for (const auto& [fi, bi] : columns) {
      const auto f = numericCell(cells, fi);
      const auto b = numericCell(cells, bi);
      if (!f || !b || !(*f > 0.0) || !(*b > 0.0)) break;
      pairs.emplace_back(*f, *b);
    }
    if (!pairs.empty()) frames.push_back(std::move(pairs));
  }
  if (frames.empty()) return std::nullopt;
  std::size_t deepest = 0;
  for (const auto& frame : frames) deepest = std::max(deepest, frame.size());
  const std::size_t needed = std::min<std::size_t>(4, deepest);
  std::vector<const std::vector<std::pair<double, double>>*> voiced;
  for (const auto& frame : frames) {
    if (frame.size() >= needed) voiced.push_back(&frame);
  }
  FormantTrack track;
  track.frames = voiced.size();
  const std::size_t depth = std::min<std::size_t>(6, deepest);
  for (std::size_t k = 0; k < depth; ++k) {
    std::vector<double> hz;
    std::vector<double> bw;
    for (const auto* frame : voiced) {
      if (frame->size() > k) {
        hz.push_back((*frame)[k].first);
        bw.push_back((*frame)[k].second);
      }
    }
    if (hz.empty()) break;
    track.median.emplace_back(median(hz), median(bw));
  }
  return track;
}

namespace {

std::vector<std::string> splitSpaces(const std::string& line) {
  std::vector<std::string> cells;
  std::string cell;
  for (const char letter : line) {
    if (letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n') {
      if (!cell.empty()) cells.push_back(cell);
      cell.clear();
    } else {
      cell.push_back(letter);
    }
  }
  if (!cell.empty()) cells.push_back(cell);
  return cells;
}

std::optional<std::size_t> columnOf(const std::vector<std::string>& header,
                                    const std::string& name) {
  const auto found = std::find(header.begin(), header.end(), name);
  if (found == header.end()) return std::nullopt;
  return static_cast<std::size_t>(found - header.begin());
}

}

std::optional<PeqList> read_peq_list(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) return std::nullopt;
  std::string line;
  bool declared = false;
  std::vector<std::string> header;
  for (std::size_t seen = 0; seen < 8 && std::getline(stream, line); ++seen) {
    const auto cells = splitSpaces(line);
    if (cells.size() == 1 && cells.front() == "Configurable_PEQ") declared = true;
    if (declared && !cells.empty() && cells.front() == "Number") {
      header = cells;
      break;
    }
  }
  if (!declared || header.empty()) return std::nullopt;
  const auto enabled = columnOf(header, "Enabled");
  const auto type = columnOf(header, "Type");
  const auto frequency = columnOf(header, "Frequency(Hz)");
  const auto gain = columnOf(header, "Gain(dB)");
  const auto quality = columnOf(header, "Q");
  const auto bandwidth = columnOf(header, "Bandwidth(Hz)");
  if (!enabled || !type || !frequency || !gain || (!quality && !bandwidth)) return std::nullopt;

  PeqList list;
  while (std::getline(stream, line)) {
    const auto cells = splitSpaces(line);
    if (cells.empty()) continue;
    const std::size_t needed = std::max({*enabled, *type, *frequency, *gain,
                                         quality ? *quality : 0, bandwidth ? *bandwidth : 0});
    if (cells.size() <= needed) {
      ++list.skipped;
      continue;
    }
    if (cells[*enabled] != "True" || cells[*type] != "PK") {
      ++list.skipped;
      continue;
    }
    const auto number = [&](std::size_t index) -> std::optional<double> {
      char* end = nullptr;
      const double value = std::strtod(cells[index].c_str(), &end);
      if (end == cells[index].c_str() || !std::isfinite(value)) return std::nullopt;
      return value;
    };
    const auto hz = number(*frequency);
    const auto db = number(*gain);
    std::optional<double> bw = bandwidth ? number(*bandwidth) : std::nullopt;
    if (!bw && quality) {
      if (const auto q = number(*quality); q && *q > 0.0 && hz) bw = *hz / *q;
    }
    if (!hz || !db || !bw || !(*hz > 0.0) || !(*bw > 0.0) || *db == 0.0) {
      ++list.skipped;
      continue;
    }
    (*db < 0.0 ? list.poles : list.zeros).emplace_back(*hz, *bw);
  }
  if (list.poles.empty() && list.zeros.empty()) return std::nullopt;
  return list;
}

}
