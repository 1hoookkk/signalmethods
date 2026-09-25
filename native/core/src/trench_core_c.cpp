#include "trench/core/trench_core_c.h"

#include "trench/core/audition.hpp"
#include "trench/core/body_from_audio.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/transpose.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <variant>

extern "C" {

double trench_decode_word(uint16_t word) {
  return trench::core::decode_word(word);
}

double trench_decode_fractional(double word) {
  return trench::core::decode_fractional(word);
}

uint16_t trench_encode_word(double value) {
  return trench::core::encode_word(value);
}

uint16_t trench_interpolate_word(uint16_t a, uint16_t b, float fraction) {
  return trench::core::interpolate_word(a, b, fraction);
}

void* trench_body_create_legacy(const uint8_t* bytes, size_t n) {
  if (!bytes || n < trench::core::kLegacyBodyBytes) return nullptr;
  auto* body = new trench::core::PackedBody();
  *body = trench::core::PackedBody::from_legacy_bytes(std::span<const uint8_t>(bytes, trench::core::kLegacyBodyBytes));
  return body;
}

void* trench_body_create_native(const uint8_t* bytes, size_t n) {
  if (!bytes || n < trench::core::kNativeBodyBytes) return nullptr;
  auto* body = new trench::core::PackedBody();
  *body = trench::core::PackedBody::from_native_bytes(std::span<const uint8_t>(bytes, trench::core::kNativeBodyBytes));
  return body;
}

void* trench_body_create_from_bytes(const uint8_t* bytes, size_t n) {
  if (!bytes) return nullptr;
  if (n == trench::core::kLegacyBodyBytes) return trench_body_create_legacy(bytes, n);
  if (n == trench::core::kNativeBodyBytes) return trench_body_create_native(bytes, n);
  return nullptr;
}

void trench_body_destroy(void* handle) {
  delete static_cast<trench::core::PackedBody*>(handle);
}

int trench_body_get_words(const void* handle, size_t corner, size_t section, uint16_t out_words[5]) {
  if (!handle || !out_words) return 0;
  if (corner >= trench::core::kCornerCount || section >= trench::core::kSectionCount) return 0;
  const auto* body = static_cast<const trench::core::PackedBody*>(handle);
  const auto& s = body->words[corner][section];
  for (size_t i = 0; i < 5; ++i) out_words[i] = s[i];
  return 1;
}

int trench_body_set_words(void* handle, size_t corner, size_t section, const uint16_t in_words[5]) {
  if (!handle || !in_words) return 0;
  if (corner >= trench::core::kCornerCount || section >= trench::core::kSectionCount) return 0;
  auto* body = static_cast<trench::core::PackedBody*>(handle);
  for (size_t i = 0; i < 5; ++i) body->words[corner][section][i] = in_words[i];
  return 1;
}

int trench_body_get_legacy_bytes(const void* handle, uint8_t out_bytes[240]) {
  if (!handle || !out_bytes) return 0;
  const auto* body = static_cast<const trench::core::PackedBody*>(handle);
  if (!body->is_legacy_representable()) return 0;
  const auto bytes = body->legacy_bytes();
  std::memcpy(out_bytes, bytes.data(), trench::core::kLegacyBodyBytes);
  return 1;
}

void trench_body_get_native_bytes(const void* handle, uint8_t out_bytes[560]) {
  if (!handle || !out_bytes) return;
  const auto* body = static_cast<const trench::core::PackedBody*>(handle);
  const auto bytes = body->native_bytes();
  std::memcpy(out_bytes, bytes.data(), trench::core::kNativeBodyBytes);
}

int trench_body_is_legacy_representable(const void* handle) {
  if (!handle) return 0;
  return static_cast<const trench::core::PackedBody*>(handle)->is_legacy_representable() ? 1 : 0;
}

void trench_body_interpolate_words(const void* handle, float morph, float q, float z, uint16_t out_words[7][5]) {
  if (!handle || !out_words) return;
  const auto* body = static_cast<const trench::core::PackedBody*>(handle);
  const auto cw = body->interpolate_words(morph, q, z);
  for (size_t s = 0; s < trench::core::kSectionCount; ++s) {
    for (size_t i = 0; i < 5; ++i) {
      out_words[s][i] = cw[s][i];
    }
  }
}

void trench_body_interpolate_biquads(const void* handle, float morph, float q, float z, double out_biquads[7][5]) {
  if (!handle || !out_biquads) return;
  const auto* body = static_cast<const trench::core::PackedBody*>(handle);
  const auto cascade = body->interpolate_biquads(morph, q, z);
  for (size_t s = 0; s < trench::core::kSectionCount; ++s) {
    for (size_t i = 0; i < 5; ++i) {
      out_biquads[s][i] = cascade[s][i];
    }
  }
}

void trench_body_interpolate_biquads_float(const void* handle, float morph, float q, float z, double out_biquads[7][5]) {
  if (!handle || !out_biquads) return;
  const auto* body = static_cast<const trench::core::PackedBody*>(handle);
  const auto cascade = body->interpolate_biquads_float(morph, q, z);
  for (size_t s = 0; s < trench::core::kSectionCount; ++s) {
    for (size_t i = 0; i < 5; ++i) {
      out_biquads[s][i] = cascade[s][i];
    }
  }
}

void trench_body_cascade(const void* handle, float morph, float q, float z, double datum_hz, double host_hz, double out_biquads[7][5]) {
  if (!handle || !out_biquads) return;
  const auto* body = static_cast<const trench::core::PackedBody*>(handle);
  trench::core::Cascade cascade{};
  if (std::abs(datum_hz - host_hz) < 1.0) {
    const auto corner = trench::core::native::packed_interior_corner(*body, morph, q, datum_hz);
    cascade = trench::core::native::cascade(trench::core::native::design(corner, host_hz), corner.gain_db);
  } else {
    const auto words = body->interpolate_words(morph, q, z);
    cascade = trench::core::native::rewarp_cascade(words, datum_hz, host_hz);
  }
  for (size_t s = 0; s < trench::core::kSectionCount; ++s) {
    for (size_t i = 0; i < 5; ++i) {
      out_biquads[s][i] = cascade[s][i];
    }
  }
}

void trench_section_geometry_get(const uint16_t words[5], double datum_hz, struct TrenchSectionGeometry* out_geom) {
  if (!words || !out_geom) return;
  trench::core::PackedSection sec{words[0], words[1], words[2], words[3], words[4]};
  const auto geom = trench::core::geometry_from_words(sec, datum_hz);
  if (const auto* conj = std::get_if<trench::core::ConjugatePair>(&geom.pole)) {
    out_geom->pole_type = 1;
    out_geom->pole_a = conj->hz;
    out_geom->pole_b = conj->radius;
  } else if (const auto* real = std::get_if<trench::core::RealPair>(&geom.pole)) {
    out_geom->pole_type = 2;
    out_geom->pole_a = real->root_a;
    out_geom->pole_b = real->root_b;
  } else {
    out_geom->pole_type = 0;
    out_geom->pole_a = 0.0;
    out_geom->pole_b = 0.0;
  }
  if (const auto* conj = std::get_if<trench::core::ConjugatePair>(&geom.zero)) {
    out_geom->zero_type = 1;
    out_geom->zero_a = conj->hz;
    out_geom->zero_b = conj->radius;
  } else if (const auto* real = std::get_if<trench::core::RealPair>(&geom.zero)) {
    out_geom->zero_type = 2;
    out_geom->zero_a = real->root_a;
    out_geom->zero_b = real->root_b;
  } else {
    out_geom->zero_type = 0;
    out_geom->zero_a = 0.0;
    out_geom->zero_b = 0.0;
  }
  out_geom->scale = geom.scale;
}

void trench_section_geometry_set(const struct TrenchSectionGeometry* in_geom, double datum_hz, uint16_t out_words[5]) {
  if (!in_geom || !out_words) return;
  trench::core::SectionGeometry geom{};
  if (in_geom->pole_type == 1) {
    geom.pole = trench::core::ConjugatePair{in_geom->pole_a, in_geom->pole_b};
  } else if (in_geom->pole_type == 2) {
    geom.pole = trench::core::RealPair{in_geom->pole_a, in_geom->pole_b};
  } else {
    geom.pole = trench::core::DegeneratePair{};
  }
  if (in_geom->zero_type == 1) {
    geom.zero = trench::core::ConjugatePair{in_geom->zero_a, in_geom->zero_b};
  } else if (in_geom->zero_type == 2) {
    geom.zero = trench::core::RealPair{in_geom->zero_a, in_geom->zero_b};
  } else {
    geom.zero = trench::core::DegeneratePair{};
  }
  geom.scale = in_geom->scale;
  const auto words = trench::core::words_from_geometry(geom, datum_hz);
  for (size_t i = 0; i < 5; ++i) out_words[i] = words[i];
}

void trench_section_design(const uint16_t words[5], double host_hz, double out_biquad[5]) {
  if (!words || !out_biquad) return;
  trench::core::PackedSection sec{words[0], words[1], words[2], words[3], words[4]};
  const auto bq = trench::core::section_words_to_biquad(sec);
  (void)host_hz;
  for (size_t i = 0; i < 5; ++i) out_biquad[i] = bq[i];
}

void trench_cascade_response_db(const double biquads[7][5], size_t section_count, const double* freqs_hz, size_t point_count, double sample_rate_hz, double* out_db) {
  if (!biquads || !freqs_hz || !out_db) return;
  const size_t count = std::min(section_count, trench::core::kSectionCount);
  std::vector<trench::core::Biquad> sections(count);
  for (size_t s = 0; s < count; ++s) {
    for (size_t i = 0; i < 5; ++i) {
      sections[s][i] = biquads[s][i];
    }
  }
  for (size_t p = 0; p < point_count; ++p) {
    out_db[p] = trench::core::cascade_response_db(sections, freqs_hz[p], sample_rate_hz);
  }
}

double trench_section_response_db(const double biquad[5], double freq_hz, double sample_rate_hz) {
  if (!biquad) return -120.0;
  trench::core::Biquad bq{biquad[0], biquad[1], biquad[2], biquad[3], biquad[4]};
  return trench::core::section_response_db(bq, freq_hz, sample_rate_hz);
}

void* trench_runner_create(double sample_rate_hz) {
  auto* runner = new trench::core::CascadeRunner();
  runner->set_sample_rate(sample_rate_hz);
  return runner;
}

void trench_runner_destroy(void* runner) {
  delete static_cast<trench::core::CascadeRunner*>(runner);
}

void trench_runner_set_target(void* runner, const double biquads[7][5], size_t section_count) {
  if (!runner || !biquads) return;
  auto* r = static_cast<trench::core::CascadeRunner*>(runner);
  trench::core::Cascade c{};
  const size_t count = std::min(section_count, trench::core::kSectionCount);
  for (size_t s = 0; s < count; ++s) {
    for (size_t i = 0; i < 5; ++i) c[s][i] = biquads[s][i];
  }
  for (size_t s = count; s < trench::core::kSectionCount; ++s) {
    c[s] = trench::core::section_words_to_biquad(trench::core::kIdentitySection);
  }
  r->set_target(trench::core::encode_cascade(c));
}

void trench_runner_set_immediate(void* runner, const double biquads[7][5], size_t section_count) {
  if (!runner || !biquads) return;
  auto* r = static_cast<trench::core::CascadeRunner*>(runner);
  trench::core::Cascade c{};
  const size_t count = std::min(section_count, trench::core::kSectionCount);
  for (size_t s = 0; s < count; ++s) {
    for (size_t i = 0; i < 5; ++i) c[s][i] = biquads[s][i];
  }
  for (size_t s = count; s < trench::core::kSectionCount; ++s) {
    c[s] = trench::core::section_words_to_biquad(trench::core::kIdentitySection);
  }
  r->set_immediate(c);
}

void trench_runner_set_glide(void* runner, const double biquads[7][5], size_t section_count, size_t samples) {
  if (!runner || !biquads) return;
  auto* r = static_cast<trench::core::CascadeRunner*>(runner);
  trench::core::Cascade c{};
  const size_t count = std::min(section_count, trench::core::kSectionCount);
  for (size_t s = 0; s < count; ++s) {
    for (size_t i = 0; i < 5; ++i) c[s][i] = biquads[s][i];
  }
  for (size_t s = count; s < trench::core::kSectionCount; ++s) {
    c[s] = trench::core::section_words_to_biquad(trench::core::kIdentitySection);
  }
  r->set_glide(c, samples);
}

void trench_runner_set_bite(void* runner, double bite) {
  if (!runner) return;
  static_cast<trench::core::CascadeRunner*>(runner)->set_pole_distortion(bite);
}

void trench_runner_set_radius_distortion(void* runner, double threshold) {
  if (!runner) return;
  static_cast<trench::core::CascadeRunner*>(runner)->set_radius_distortion(threshold);
}

void trench_runner_set_feedback_ceiling(void* runner, double linear) {
  if (!runner) return;
  static_cast<trench::core::CascadeRunner*>(runner)->set_feedback_ceiling(linear);
}

void trench_runner_set_sample_rate(void* runner, double sample_rate_hz) {
  if (!runner) return;
  static_cast<trench::core::CascadeRunner*>(runner)->set_sample_rate(sample_rate_hz);
}

void trench_runner_set_ring_leveller(void* runner, int enabled) {
  if (!runner) return;
  static_cast<trench::core::CascadeRunner*>(runner)->set_ring_leveller(enabled != 0);
}

void trench_runner_reset(void* runner) {
  if (!runner) return;
  static_cast<trench::core::CascadeRunner*>(runner)->reset();
}

void trench_runner_process(void* runner, float* block, size_t count) {
  if (!runner || !block || count == 0) return;
  static_cast<trench::core::CascadeRunner*>(runner)->process(std::span<float>(block, count));
}

void trench_transpose_cascade(const double in_biquads[7][5], size_t section_count, double ratio, double host_hz, double out_biquads[7][5]) {
  if (!in_biquads || !out_biquads) return;
  trench::core::Cascade c{};
  const size_t count = std::min(section_count, trench::core::kSectionCount);
  for (size_t s = 0; s < count; ++s) {
    for (size_t i = 0; i < 5; ++i) c[s][i] = in_biquads[s][i];
  }
  for (size_t s = count; s < trench::core::kSectionCount; ++s) {
    c[s] = trench::core::section_words_to_biquad(trench::core::kIdentitySection);
  }
  const auto transposed = trench::core::transpose_cascade(c, ratio, host_hz);
  for (size_t s = 0; s < trench::core::kSectionCount; ++s) {
    for (size_t i = 0; i < 5; ++i) {
      out_biquads[s][i] = transposed[s][i];
    }
  }
}

size_t trench_audio_speech_poles(const float* mono_samples, size_t sample_count, double sample_rate_hz, size_t max_count, struct TrenchResonance* out_resonances) {
  if (!mono_samples || sample_count == 0 || !out_resonances || max_count == 0) return 0;
  const auto res = trench::core::audio::speech_poles(std::span<const float>(mono_samples, sample_count), sample_rate_hz, max_count);
  const size_t written = std::min(res.size(), max_count);
  for (size_t i = 0; i < written; ++i) {
    out_resonances[i].hz = res[i].hz;
    out_resonances[i].bw_hz = res[i].bw_hz;
    out_resonances[i].gain_db = res[i].gain_db;
  }
  return written;
}

size_t trench_audio_resonances(const float* mono_samples, size_t sample_count, double sample_rate_hz, size_t max_count, struct TrenchResonance* out_resonances) {
  if (!mono_samples || sample_count == 0 || !out_resonances || max_count == 0) return 0;
  const auto res = trench::core::audio::resonances_from_audio(std::span<const float>(mono_samples, sample_count), sample_rate_hz, max_count);
  const size_t written = std::min(res.size(), max_count);
  for (size_t i = 0; i < written; ++i) {
    out_resonances[i].hz = res[i].hz;
    out_resonances[i].bw_hz = res[i].bw_hz;
    out_resonances[i].gain_db = res[i].gain_db;
  }
  return written;
}

uint16_t trench_p2k_mag_word_for(double hz, uint16_t rsq_word) {
  return trench::core::p2k::mag_word_for(hz, rsq_word);
}

uint16_t trench_p2k_dial_word(size_t byte_val) {
  return trench::core::p2k::dial_word(byte_val);
}

size_t trench_p2k_dial_of_word(uint16_t word) {
  return trench::core::p2k::dial_of_word(word);
}

}
