#include "trench/core/body_from_audio.hpp"

#include <cmath>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what, double a = 0.0, double b = 0.0) {
  std::printf("%s  %s  (%.4f / %.4f)\n", ok ? "PASS" : "FAIL", what, a, b);
  if (!ok) ++failures;
}

constexpr double kFs = 44'100.0;
constexpr std::array<double, 3> kHz{250.0, 1'200.0, 2'800.0};
constexpr std::array<double, 3> kBw{40.0, 90.0, 180.0};

std::vector<float> damped_sum(std::size_t length) {
  std::vector<float> out(length, 0.0F);
  for (std::size_t n = 0; n < length; ++n) {
    double acc = 0.0;
    for (std::size_t k = 0; k < kHz.size(); ++k) {
      const double decay =
          std::exp(-std::numbers::pi * kBw[k] * static_cast<double>(n) / kFs);
      acc += decay * std::cos(2.0 * std::numbers::pi * kHz[k] *
                              static_cast<double>(n) / kFs);
    }
    out[n] = static_cast<float>(acc);
  }
  return out;
}

std::vector<float> resonated_noise(std::size_t length) {
  std::vector<float> out(length, 0.0F);
  std::uint32_t seed = 12345u;
  for (std::size_t n = 0; n < length; ++n) {
    seed = seed * 1664525u + 1013904223u;
    out[n] = static_cast<float>(
        2.0 * (static_cast<double>(seed) / 4294967296.0) - 1.0);
  }
  for (std::size_t k = 0; k < kHz.size(); ++k) {
    const double r = std::exp(-std::numbers::pi * kBw[k] / kFs);
    const double theta = 2.0 * std::numbers::pi * kHz[k] / kFs;
    const double a1 = -2.0 * r * std::cos(theta);
    const double a2 = r * r;
    const double b0 = 1.0 - r;
    double z1 = 0.0;
    double z2 = 0.0;
    for (std::size_t n = 0; n < length; ++n) {
      const double y = b0 * static_cast<double>(out[n]) - a1 * z1 - a2 * z2;
      z2 = z1;
      z1 = y;
      out[n] = static_cast<float>(y);
    }
  }
  return out;
}

std::vector<double> white_noise(std::size_t length, std::uint32_t seed) {
  std::vector<double> out(length, 0.0);
  for (std::size_t n = 0; n < length; ++n) {
    seed = seed * 1664525u + 1013904223u;
    out[n] = 2.0 * (static_cast<double>(seed) / 4294967296.0) - 1.0;
  }
  return out;
}

void add_resonance(std::vector<double>& out, const std::vector<double>& drive, double hz,
                   double bw_hz, double gain_db, double fs, double sign = 1.0) {
  const double r = std::exp(-std::numbers::pi * bw_hz / fs);
  const double w0 = 2.0 * std::numbers::pi * hz / fs;
  const double a1 = -2.0 * r * std::cos(w0);
  const double a2 = r * r;
  const double b0 = sign * std::pow(10.0, gain_db / 20.0) * 2.0 * (1.0 - r) * std::sin(w0);
  double z1 = 0.0;
  double z2 = 0.0;
  for (std::size_t n = 0; n < out.size() && n < drive.size(); ++n) {
    const double y = b0 * drive[n] - a1 * z1 - a2 * z2;
    z2 = z1;
    z1 = y;
    out[n] += y;
  }
}

std::vector<float> to_unit(const std::vector<double>& in, std::size_t length) {
  double peak = 0.0;
  for (std::size_t n = 0; n < length && n < in.size(); ++n) {
    peak = std::max(peak, std::abs(in[n]));
  }
  std::vector<float> out(length, 0.0F);
  for (std::size_t n = 0; n < length && n < in.size(); ++n) {
    out[n] = static_cast<float>(peak > 0.0 ? in[n] / peak : 0.0);
  }
  return out;
}

std::vector<float> struck_bells_and_a_tilt(std::size_t length) {
  std::vector<double> sum(length, 0.0);
  for (std::size_t n = 0; n < length; ++n) {
    const double t = static_cast<double>(n) / kFs;
    double acc = 0.0;
    for (std::size_t k = 0; k < kHz.size(); ++k) {
      acc += std::exp(-std::numbers::pi * kBw[k] * t) *
             std::cos(2.0 * std::numbers::pi * kHz[k] * t);
    }
    acc += std::exp(-std::numbers::pi * 1'200.0 * t) *
           std::cos(2.0 * std::numbers::pi * 600.0 * t);
    sum[n] = acc;
  }
  return to_unit(sum, length);
}

constexpr std::array<double, 3> kFormantHz{500.0, 1'500.0, 2'500.0};
constexpr std::array<double, 3> kFormantBw{60.0, 90.0, 120.0};

std::vector<float> synthetic_vowel(std::size_t length) {
  std::vector<double> drive = white_noise(length, 987654321u);
  for (double& value : drive) value *= 0.05;
  const auto period = static_cast<std::size_t>(kFs / 100.0);
  std::uint32_t seed = 24680u;
  for (std::size_t at = 0; at < length;) {
    drive[at] += 1.0;
    seed = seed * 1664525u + 1013904223u;
    const double jitter = static_cast<double>(seed) / 4294967296.0 - 0.5;
    at += static_cast<std::size_t>(
        std::lround(static_cast<double>(period) * (1.0 + 0.08 * jitter)));
  }
  std::vector<double> sum(length, 0.0);
  for (std::size_t k = 0; k < kFormantHz.size(); ++k) {
    add_resonance(sum, drive, kFormantHz[k], kFormantBw[k], 0.0, kFs,
                  k % 2 == 0 ? 1.0 : -1.0);
  }
  return to_unit(sum, length);
}

void write_bytes(std::vector<std::uint8_t>& out, const void* data, std::size_t bytes) {
  const auto* source = static_cast<const std::uint8_t*>(data);
  out.insert(out.end(), source, source + bytes);
}

void write_tag(std::vector<std::uint8_t>& out, const char* four) {
  write_bytes(out, four, 4);
}

void write_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  write_bytes(out, &value, 4);
}

void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  write_bytes(out, &value, 2);
}

std::vector<std::uint8_t> wav_of(std::uint16_t format, std::uint16_t channels,
                                 std::uint32_t rate, std::uint16_t bits,
                                 const std::vector<std::uint8_t>& payload) {
  std::vector<std::uint8_t> out;
  const std::uint16_t block = static_cast<std::uint16_t>(channels * (bits / 8u));
  write_tag(out, "RIFF");
  write_u32(out, static_cast<std::uint32_t>(36 + payload.size()));
  write_tag(out, "WAVE");
  write_tag(out, "fmt ");
  write_u32(out, 16);
  write_u16(out, format);
  write_u16(out, channels);
  write_u32(out, rate);
  write_u32(out, rate * block);
  write_u16(out, block);
  write_u16(out, bits);
  write_tag(out, "data");
  write_u32(out, static_cast<std::uint32_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

bool put_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream stream(path, std::ios::binary);
  if (!stream) return false;
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return stream.good();
}

}

int main() {
  using trench::core::audio::read_wav_mono;
  using trench::core::audio::Resonance;
  using trench::core::audio::resonances_from_audio;

  {
    const auto clip = damped_sum(static_cast<std::size_t>(kFs));
    const std::vector<Resonance> found = resonances_from_audio(clip, kFs, 3);
    check(found.size() == 3, "three damped sinusoids give three resonances",
          static_cast<double>(found.size()), 3.0);
    if (found.size() == 3) {
      for (std::size_t k = 0; k < 3; ++k) {
        std::printf("     damped %zu -> %.2f Hz  bw %.2f Hz\n", k, found[k].hz,
                    found[k].bw_hz);
        check(std::abs(found[k].hz - kHz[k]) <= kHz[k] * 0.02,
              "damped sinusoid frequency recovered", found[k].hz, kHz[k]);
        check(found[k].bw_hz > 0.0 && found[k].bw_hz < kHz[0],
              "damped sinusoid keeps a narrow bandwidth", found[k].bw_hz, kHz[0]);
      }
      check(found[0].hz < found[1].hz && found[1].hz < found[2].hz,
            "damped resonances come back in ascending order", found[0].hz, found[2].hz);
    }
  }

  {
    const auto clip = resonated_noise(static_cast<std::size_t>(2.0 * kFs));
    const std::vector<Resonance> found = resonances_from_audio(clip, kFs, 3);
    check(found.size() == 3, "resonated noise gives three resonances",
          static_cast<double>(found.size()), 3.0);
    if (found.size() == 3) {
      for (std::size_t k = 0; k < 3; ++k) {
        std::printf("     noise %zu -> %.2f Hz  bw %.2f Hz\n", k, found[k].hz,
                    found[k].bw_hz);
        check(std::abs(found[k].hz - kHz[k]) <= kHz[k] * 0.03,
              "resonator frequency recovered from noise", found[k].hz, kHz[k]);
        check(std::abs(found[k].bw_hz - kBw[k]) <= kBw[k] * 0.40,
              "resonator bandwidth recovered from noise", found[k].bw_hz, kBw[k]);
      }
    }
  }

  {
    using trench::core::audio::all_pole_model;
    using trench::core::audio::model_order;
    using trench::core::audio::neutral_rows;

    const auto clip = struck_bells_and_a_tilt(static_cast<std::size_t>(kFs));

    const auto model = all_pole_model(clip, kFs, model_order(6));
    const std::vector<Resonance> rows = neutral_rows(model, 6);
    for (const auto& row : rows) {
      std::printf("     neutral row -> %.2f Hz  bw %.2f Hz  gain %.2f dB\n", row.hz,
                  row.bw_hz, row.gain_db);
    }
    check(rows.size() == 3, "three bells become three neutral rows",
          static_cast<double>(rows.size()), 3.0);
    if (rows.size() == 3) {
      for (std::size_t k = 0; k < 3; ++k) {
        check(std::abs(rows[k].hz - kHz[k]) <= kHz[k] * 0.03,
              "neutral row lands on the bell", rows[k].hz, kHz[k]);
        check(rows[k].bw_hz <= rows[k].hz / 10.0 + 1e-9,
              "neutral row seeds a Q of ten or better", rows[k].bw_hz,
              rows[k].hz / 10.0);
        check(rows[k].gain_db >= 3.0 && rows[k].gain_db <= 40.0,
              "neutral row gain stays inside three to forty dB", rows[k].gain_db, 40.0);
      }
    }
    const bool tilt_row = std::any_of(rows.begin(), rows.end(), [](const Resonance& row) {
      return std::abs(std::log2(row.hz / 600.0)) < 0.25;
    });
    check(!tilt_row, "a Q of one half is a tilt, not a row");

    const auto held = resonated_noise(static_cast<std::size_t>(2.0 * kFs));
    const auto wide = neutral_rows(all_pole_model(held, kFs, model_order(6)), 6);
    check(!wide.empty(), "the resonated clip still seeds rows",
          static_cast<double>(wide.size()), 1.0);
    if (!wide.empty()) {
      std::printf("     clamped row -> %.2f Hz  bw %.2f Hz\n", wide[0].hz, wide[0].bw_hz);
      check(std::abs(wide[0].bw_hz - wide[0].hz / 10.0) < 1e-9,
            "a wider measurement is clamped to a Q of ten", wide[0].bw_hz,
            wide[0].hz / 10.0);
    }
  }

  {
    using trench::core::audio::speech_poles;
    const auto vowel = synthetic_vowel(static_cast<std::size_t>(kFs));
    const std::vector<Resonance> poles = speech_poles(vowel, kFs, 3);
    check(poles.size() == 3, "the vowel gives three formant poles",
          static_cast<double>(poles.size()), 3.0);
    if (poles.size() == 3) {
      for (std::size_t k = 0; k < 3; ++k) {
        std::printf("     formant %zu -> %.2f Hz  bw %.2f Hz\n", k, poles[k].hz,
                    poles[k].bw_hz);
        check(std::abs(poles[k].hz - kFormantHz[k]) <= kFormantHz[k] * 0.04,
              "formant frequency recovered at the model rate", poles[k].hz,
              kFormantHz[k]);
        check(std::abs(poles[k].bw_hz - kFormantBw[k]) <= kFormantBw[k] * 0.60,
              "formant bandwidth recovered at the model rate", poles[k].bw_hz,
              kFormantBw[k]);
      }
    }
  }

  {
    using trench::core::audio::resample;
    std::vector<float> sine(static_cast<std::size_t>(kFs), 0.0F);
    for (std::size_t n = 0; n < sine.size(); ++n) {
      sine[n] = static_cast<float>(
          std::sin(2.0 * std::numbers::pi * 1'000.0 * static_cast<double>(n) / kFs));
    }
    const std::vector<float> down = resample(sine, kFs, 11'025.0);
    check(down.size() > 10'000, "the sine resamples to the model rate",
          static_cast<double>(down.size()), 11'025.0);
    if (down.size() > 1'000) {
      const std::size_t first = 300;
      const std::size_t last = down.size() - 300;
      std::size_t crossings = 0;
      for (std::size_t n = first + 1; n < last; ++n) {
        if ((down[n - 1] < 0.0F) != (down[n] < 0.0F)) ++crossings;
      }
      const double seconds = static_cast<double>(last - first - 1) / 11'025.0;
      const double measured = static_cast<double>(crossings) / (2.0 * seconds);
      std::printf("     resampled sine -> %.3f Hz\n", measured);
      check(std::abs(measured - 1'000.0) <= 5.0, "resampling keeps the pitch", measured,
            1'000.0);
    }
    const std::vector<float> same = resample(sine, kFs, kFs);
    check(same.size() == sine.size(), "an equal rate copies the clip",
          static_cast<double>(same.size()), static_cast<double>(sine.size()));
  }

  {
    const std::filesystem::path folder =
        std::filesystem::temp_directory_path() / "trench_from_audio";
    std::error_code code;
    std::filesystem::create_directories(folder, code);

    std::vector<std::uint8_t> stereo;
    for (std::size_t n = 0; n < 400; ++n) {
      const double value =
          0.5 * std::sin(2.0 * std::numbers::pi * 200.0 * static_cast<double>(n) / 8000.0);
      const auto left = static_cast<std::int16_t>(std::lround(value * 32767.0));
      const auto right = static_cast<std::int16_t>(-left);
      write_bytes(stereo, &left, 2);
      write_bytes(stereo, &right, 2);
    }
    const std::filesystem::path pcm = folder / "cancel.wav";
    check(put_file(pcm, wav_of(1, 2, 8000, 16, stereo)), "wrote a 16-bit stereo WAV");
    const auto read_pcm = read_wav_mono(pcm);
    check(read_pcm.has_value(), "16-bit stereo WAV reads");
    if (read_pcm) {
      check(read_pcm->sample_rate_hz == 8000.0, "stereo WAV reports 8000 Hz",
            read_pcm->sample_rate_hz, 8000.0);
      check(read_pcm->samples.size() == 400, "stereo WAV gives 400 mono samples",
            static_cast<double>(read_pcm->samples.size()), 400.0);
      double worst = 0.0;
      for (const float sample : read_pcm->samples) {
        worst = std::max(worst, std::abs(static_cast<double>(sample)));
      }
      check(worst < 1e-3, "opposed channels cancel to silence", worst, 1e-3);
    }

    std::vector<std::uint8_t> ramp;
    std::vector<float> wanted(256, 0.0F);
    for (std::size_t n = 0; n < wanted.size(); ++n) {
      wanted[n] = static_cast<float>(static_cast<double>(n) / 256.0 - 0.5);
      write_bytes(ramp, &wanted[n], 4);
    }
    const std::filesystem::path floats = folder / "ramp.wav";
    check(put_file(floats, wav_of(3, 1, 44100, 32, ramp)), "wrote a float32 mono WAV");
    const auto read_float = read_wav_mono(floats);
    check(read_float.has_value(), "float32 mono WAV reads");
    if (read_float) {
      check(read_float->samples.size() == wanted.size(), "float WAV keeps every sample",
            static_cast<double>(read_float->samples.size()),
            static_cast<double>(wanted.size()));
      double worst = 0.0;
      for (std::size_t n = 0; n < read_float->samples.size() && n < wanted.size(); ++n) {
        worst = std::max(worst, std::abs(static_cast<double>(read_float->samples[n]) -
                                         static_cast<double>(wanted[n])));
      }
      check(worst < 1e-6, "float ramp reads back sample for sample", worst, 1e-6);
    }

    std::vector<std::uint8_t> wide;
    write_tag(wide, "RIFF");
    write_u32(wide, static_cast<std::uint32_t>(60 + stereo.size()));
    write_tag(wide, "WAVE");
    write_tag(wide, "fmt ");
    write_u32(wide, 40);
    write_u16(wide, 0xFFFE);
    write_u16(wide, 2);
    write_u32(wide, 8000);
    write_u32(wide, 8000 * 4);
    write_u16(wide, 4);
    write_u16(wide, 16);
    write_u16(wide, 22);
    write_u16(wide, 16);
    write_u32(wide, 3);
    write_u16(wide, 1);
    write_u16(wide, 0);
    for (int pad = 0; pad < 12; ++pad) wide.push_back(0);
    write_tag(wide, "data");
    write_u32(wide, static_cast<std::uint32_t>(stereo.size()));
    wide.insert(wide.end(), stereo.begin(), stereo.end());
    const std::filesystem::path extensible = folder / "extensible.wav";
    check(put_file(extensible, wide), "wrote a WAVE_FORMAT_EXTENSIBLE file");
    const auto read_wide = read_wav_mono(extensible);
    check(read_wide.has_value(), "extensible WAV reads through its sub-format tag");
    if (read_wide) {
      check(read_wide->samples.size() == 400, "extensible WAV gives 400 mono samples",
            static_cast<double>(read_wide->samples.size()), 400.0);
    }

    check(!read_wav_mono(folder / "missing.wav").has_value(), "a missing file is refused");
  }

  std::printf("%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
