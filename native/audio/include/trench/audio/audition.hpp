#pragma once

#include <memory>
#include <string>

#include "trench/audio/audio_boundary.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/packed_body.hpp"

namespace trench::audio {

struct AuditionView {
  trench::core::PackedBody packed;
  float morph{};
  float q{};
  double semitones{};
};

[[nodiscard]] trench::core::Cascade design_audition(
    const AuditionView& view, double device_sample_rate_hz);

class Audition {
 public:
  Audition();
  ~Audition();
  Audition(const Audition&) = delete;
  Audition& operator=(const Audition&) = delete;

  std::string start();
  void stop();
  [[nodiscard]] bool running() const noexcept;
  [[nodiscard]] double sampleRateHz() const noexcept;
  [[nodiscard]] std::string deviceName() const;

  void setView(AuditionView view);
  void setSaw(double hz, float level);
  void setClip(MonoClip clip);
  void setGate(bool open);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace trench::audio
