#pragma once

#include <memory>
#include <string>

#include "trench/audio/audio_boundary.hpp"
#include "trench/core/packed_body.hpp"

namespace trench::audio {

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

  void setCascade(const trench::core::Cascade& cascade);
  void setSaw(double hz, float level);
  void setClip(MonoClip clip);
  void setGate(bool open);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace trench::audio
