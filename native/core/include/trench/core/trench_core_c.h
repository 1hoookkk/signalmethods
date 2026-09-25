#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(TRENCH_CORE_C_BUILD)
#define TRENCH_CORE_C_API __declspec(dllexport)
#else
#define TRENCH_CORE_C_API __declspec(dllimport)
#endif
#else
#define TRENCH_CORE_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct TrenchSectionGeometry {
  int pole_type;
  double pole_a;
  double pole_b;
  int zero_type;
  double zero_a;
  double zero_b;
  double scale;
};

struct TrenchResonance {
  double hz;
  double bw_hz;
  double gain_db;
};

TRENCH_CORE_C_API double trench_decode_word(uint16_t word);
TRENCH_CORE_C_API double trench_decode_fractional(double word);
TRENCH_CORE_C_API uint16_t trench_encode_word(double value);
TRENCH_CORE_C_API uint16_t trench_interpolate_word(uint16_t a, uint16_t b, float fraction);

TRENCH_CORE_C_API void* trench_body_create_legacy(const uint8_t* bytes, size_t n);
TRENCH_CORE_C_API void* trench_body_create_native(const uint8_t* bytes, size_t n);
TRENCH_CORE_C_API void* trench_body_create_from_bytes(const uint8_t* bytes, size_t n);
TRENCH_CORE_C_API void trench_body_destroy(void* handle);

TRENCH_CORE_C_API int trench_body_get_words(const void* handle, size_t corner, size_t section, uint16_t out_words[5]);
TRENCH_CORE_C_API int trench_body_set_words(void* handle, size_t corner, size_t section, const uint16_t in_words[5]);
TRENCH_CORE_C_API int trench_body_get_legacy_bytes(const void* handle, uint8_t out_bytes[240]);
TRENCH_CORE_C_API void trench_body_get_native_bytes(const void* handle, uint8_t out_bytes[560]);
TRENCH_CORE_C_API int trench_body_is_legacy_representable(const void* handle);

TRENCH_CORE_C_API void trench_body_interpolate_words(const void* handle, float morph, float q, float z, uint16_t out_words[7][5]);
TRENCH_CORE_C_API void trench_body_interpolate_biquads(const void* handle, float morph, float q, float z, double out_biquads[7][5]);
TRENCH_CORE_C_API void trench_body_interpolate_biquads_float(const void* handle, float morph, float q, float z, double out_biquads[7][5]);
TRENCH_CORE_C_API void trench_body_cascade(const void* handle, float morph, float q, float z, double datum_hz, double host_hz, double out_biquads[7][5]);

TRENCH_CORE_C_API void trench_section_geometry_get(const uint16_t words[5], double datum_hz, struct TrenchSectionGeometry* out_geom);
TRENCH_CORE_C_API void trench_section_geometry_set(const struct TrenchSectionGeometry* in_geom, double datum_hz, uint16_t out_words[5]);
TRENCH_CORE_C_API void trench_section_design(const uint16_t words[5], double host_hz, double out_biquad[5]);

TRENCH_CORE_C_API void trench_cascade_response_db(const double biquads[7][5], size_t section_count, const double* freqs_hz, size_t point_count, double sample_rate_hz, double* out_db);
TRENCH_CORE_C_API double trench_section_response_db(const double biquad[5], double freq_hz, double sample_rate_hz);

TRENCH_CORE_C_API void* trench_runner_create(double sample_rate_hz);
TRENCH_CORE_C_API void trench_runner_destroy(void* runner);
TRENCH_CORE_C_API void trench_runner_set_target(void* runner, const double biquads[7][5], size_t section_count);
TRENCH_CORE_C_API void trench_runner_set_immediate(void* runner, const double biquads[7][5], size_t section_count);
TRENCH_CORE_C_API void trench_runner_set_glide(void* runner, const double biquads[7][5], size_t section_count, size_t samples);
TRENCH_CORE_C_API void trench_runner_set_bite(void* runner, double bite);
TRENCH_CORE_C_API void trench_runner_set_radius_distortion(void* runner, double threshold);
TRENCH_CORE_C_API void trench_runner_set_feedback_ceiling(void* runner, double linear);
TRENCH_CORE_C_API void trench_runner_set_sample_rate(void* runner, double sample_rate_hz);
TRENCH_CORE_C_API void trench_runner_set_ring_leveller(void* runner, int enabled);
TRENCH_CORE_C_API void trench_runner_reset(void* runner);
TRENCH_CORE_C_API void trench_runner_process(void* runner, float* block, size_t count);

TRENCH_CORE_C_API void trench_transpose_cascade(const double in_biquads[7][5], size_t section_count, double ratio, double host_hz, double out_biquads[7][5]);

TRENCH_CORE_C_API size_t trench_audio_speech_poles(const float* mono_samples, size_t sample_count, double sample_rate_hz, size_t max_count, struct TrenchResonance* out_resonances);
TRENCH_CORE_C_API size_t trench_audio_resonances(const float* mono_samples, size_t sample_count, double sample_rate_hz, size_t max_count, struct TrenchResonance* out_resonances);

TRENCH_CORE_C_API uint16_t trench_p2k_mag_word_for(double hz, uint16_t rsq_word);
TRENCH_CORE_C_API uint16_t trench_p2k_dial_word(size_t byte_val);
TRENCH_CORE_C_API size_t trench_p2k_dial_of_word(uint16_t word);

#ifdef __cplusplus
}
#endif
