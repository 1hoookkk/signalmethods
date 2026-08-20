#include "trench/core/packed_body.hpp"

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(trench_native_research, module) {
  module.doc() = "Narrow research-only access to the native packed-word law";
  module.def("decode_word", &trench::core::decode_word);
  module.def("encode_word", &trench::core::encode_word);
}
