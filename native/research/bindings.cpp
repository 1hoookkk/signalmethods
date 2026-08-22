#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace py = pybind11;

namespace {

std::vector<trench::core::Biquad> cascade_from_words(
    const std::vector<std::uint16_t>& words) {
  if (words.size() % trench::core::kCoefficientCount != 0) {
    throw std::invalid_argument("words must be a multiple of 5");
  }
  const auto sections = words.size() / trench::core::kCoefficientCount;
  std::vector<trench::core::Biquad> cascade;
  cascade.reserve(sections);
  for (std::size_t section = 0; section < sections; ++section) {
    trench::core::PackedSection packed{};
    for (std::size_t word = 0; word < trench::core::kCoefficientCount; ++word) {
      packed[word] = words[section * trench::core::kCoefficientCount + word];
    }
    cascade.push_back(trench::core::section_words_to_biquad(packed));
  }
  return cascade;
}

std::vector<double> cascade_db(const std::vector<std::uint16_t>& words,
                               const std::vector<double>& frequencies_hz,
                               double sample_rate_hz) {
  const auto cascade = cascade_from_words(words);
  std::vector<double> out;
  out.reserve(frequencies_hz.size());
  for (const auto frequency_hz : frequencies_hz) {
    out.push_back(
        trench::core::cascade_response_db(cascade, frequency_hz, sample_rate_hz));
  }
  return out;
}

std::vector<double> section_db(const std::vector<std::uint16_t>& words,
                               const std::vector<double>& frequencies_hz,
                               double sample_rate_hz) {
  const auto cascade = cascade_from_words(words);
  if (cascade.size() != 1) throw std::invalid_argument("expected one section");
  std::vector<double> out;
  out.reserve(frequencies_hz.size());
  for (const auto frequency_hz : frequencies_hz) {
    out.push_back(trench::core::section_response_db(cascade.front(), frequency_hz,
                                                    sample_rate_hz));
  }
  return out;
}

py::dict geometry(const std::vector<std::uint16_t>& words, double sample_rate_hz) {
  if (words.size() != trench::core::kCoefficientCount) {
    throw std::invalid_argument("expected five words");
  }
  trench::core::PackedSection packed{};
  for (std::size_t word = 0; word < trench::core::kCoefficientCount; ++word) {
    packed[word] = words[word];
  }
  const auto parsed = trench::core::geometry_from_words(packed, sample_rate_hz);
  py::dict out;
  out["scale"] = parsed.scale;
  for (const auto lane : {0, 1}) {
    const auto& pair = lane == 0 ? parsed.pole : parsed.zero;
    py::dict entry;
    if (const auto* conjugate = std::get_if<trench::core::ConjugatePair>(&pair)) {
      entry["kind"] = "conjugate";
      entry["hz"] = conjugate->hz;
      entry["radius"] = conjugate->radius;
    } else if (const auto* real = std::get_if<trench::core::RealPair>(&pair)) {
      entry["kind"] = "real";
      entry["root_a"] = real->root_a;
      entry["root_b"] = real->root_b;
    } else {
      entry["kind"] = "degenerate";
    }
    out[lane == 0 ? "pole" : "zero"] = entry;
  }
  return out;
}

std::pair<std::uint16_t, std::uint16_t> lattice_words_from_root(double hz, double radius) {
  return trench::core::p2k::words_from_root(hz, radius);
}

}  // namespace

PYBIND11_MODULE(trench_native_research, module) {
  module.doc() = "Narrow research-only access to the native packed-word law";
  module.def("decode_word", &trench::core::decode_word);
  module.def("encode_word", &trench::core::encode_word);
  module.def("cascade_db", &cascade_db, py::arg("words"), py::arg("frequencies_hz"),
             py::arg("sample_rate_hz"));
  module.def("section_db", &section_db, py::arg("words"), py::arg("frequencies_hz"),
             py::arg("sample_rate_hz"));
  module.def("geometry", &geometry, py::arg("words"), py::arg("sample_rate_hz"));
  module.def("lattice_words_from_root", &lattice_words_from_root, py::arg("hz"),
             py::arg("radius"));
  module.def("nearest_gain_word", &trench::core::p2k::nearest_gain_word);
  module.def("erb_grid_hz", [] { return trench::core::p2k::grid().hz; });
  module.def("erb_grid_weight", [] { return trench::core::p2k::grid().weight; });
  module.attr("kP2kDatumHz") = trench::core::kP2kDatumHz;
  module.attr("kMorpheusDatumHz") = trench::core::kMorpheusDatumHz;
  module.attr("kIdentitySection") =
      std::vector<std::uint16_t>(trench::core::kIdentitySection.begin(),
                                 trench::core::kIdentitySection.end());
}
