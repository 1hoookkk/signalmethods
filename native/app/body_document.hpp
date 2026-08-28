#pragma once

#include "trench/core/native_body.hpp"
#include "trench/core/fit_target.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/role.hpp"

#include <QObject>
#include <QUndoStack>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class BodyDocument final : public QObject {
 Q_OBJECT

 public:
  using CornerSnapshot = trench::core::native::Corner;
  using P2kCorner = trench::core::native::P2kCorner;

  struct View {
    float morph{0};
    float q{0};
    double semitones{0.0};
  };

  enum class RootLane { kPole, kZero };

  struct RootEdit {
    std::size_t section{};
    RootLane lane{RootLane::kPole};
    double hz{};
    double bandwidth_hz{};
    bool parked{};
    std::optional<std::size_t> corner{};
  };

  BodyDocument(trench::core::native::Body body, double sample_rate_hz,
               QObject* parent = nullptr);

  [[nodiscard]] const trench::core::native::Body& body() const noexcept;
  [[nodiscard]] trench::core::PackedBody exportP2kBody() const;
  [[nodiscard]] std::array<std::uint8_t, trench::core::kLegacyBodyBytes> exportP2k() const;
  [[nodiscard]] P2kCorner p2kCornerSnapshot() const;
  [[nodiscard]] P2kCorner p2kCornerSnapshot(std::size_t corner) const;
  [[nodiscard]] std::size_t corner() const noexcept;
  [[nodiscard]] double sampleRateHz() const noexcept;
  [[nodiscard]] QUndoStack* undoStack() noexcept;
  [[nodiscard]] bool rootGestureActive() const noexcept;
  [[nodiscard]] std::uint32_t freedomMask() const noexcept;
  [[nodiscard]] const std::optional<trench::core::FitTarget>& target() const noexcept;
  [[nodiscard]] const std::vector<double>& targetGridDb() const noexcept;
  [[nodiscard]] CornerSnapshot cornerSnapshot() const;
  [[nodiscard]] CornerSnapshot cornerSnapshot(std::size_t corner) const;
  [[nodiscard]] trench::core::p2k::CornerWords seedWords() const;
  [[nodiscard]] bool seedIsInherited() const;

  [[nodiscard]] const trench::core::p2k::PerceptualSpace& space() const noexcept;
  [[nodiscard]] const trench::core::p2k::Grid& grid() const noexcept;
  [[nodiscard]] const trench::core::p2k::RoleIntent& intent() const noexcept;
  [[nodiscard]] View view() const noexcept;
  [[nodiscard]] bool atCorner() const noexcept;
  [[nodiscard]] std::vector<double> viewResponseDb() const;
  [[nodiscard]] double viewPowerDb() const;
  [[nodiscard]] double targetScoreDb() const;

  void setCorner(std::size_t corner);
  void setTarget(trench::core::FitTarget target);
  void setTarget(std::vector<double> target);
  void clearTarget();
  void setSpace(const trench::core::p2k::PerceptualSpace& space);
  void setIntent(std::size_t section, std::optional<trench::core::p2k::Role> role);
  void setView(float morph, float q);
  void setTranspose(int semitones);
  [[nodiscard]] int cornerTranspose(std::size_t corner) const noexcept;
  [[nodiscard]] trench::core::Cascade viewCascade() const;

  void applySpace(const trench::core::p2k::PerceptualSpace& space);
  void applyIntent(std::size_t section, std::optional<trench::core::p2k::Role> role);

  void toggleLane(std::size_t section, bool pole);
  void setFreedomMask(std::uint32_t mask);
  void beginRootGesture();
  bool editRoot(const RootEdit& edit);
  void endRootGesture();
  void applyP2kSection(std::size_t section, const trench::core::PackedSection& words);

  void applyCorner(const CornerSnapshot& words);
  void applyCorner(std::size_t corner, const CornerSnapshot& words);
  void applyBody(const trench::core::native::Body& body);
  void resetBody(const trench::core::native::Body& body);
  void applyP2kCorner(const P2kCorner& words);
  void applyP2kCorner(std::size_t corner, const P2kCorner& words);
  void applyFitStep(std::size_t corner, const CornerSnapshot& snapshot);
  void commitFit(std::size_t corner, const CornerSnapshot& before);

  void applyCharacter(double amount);
  [[nodiscard]] double meanLevelDb(const P2kCorner& rows) const;
  void commitCharacter(const CornerSnapshot& before_low, const CornerSnapshot& before_high);

 signals:
  void bodyChanged();
  void cornerChanged(std::size_t corner);
  void freedomMaskChanged(std::uint32_t mask);
  void targetChanged();
  void spaceChanged();
  void viewChanged();

 private:
  [[nodiscard]] std::size_t seedSource() const;

  trench::core::native::Body body_;
  std::size_t corner_{};
  std::array<int, trench::core::kLegacyCornerCount> corner_semitones_{};
  [[nodiscard]] double effectiveSemitones(float morph, float q) const;
  double sample_rate_hz_{};
  std::uint32_t freedom_mask_{};
  std::optional<trench::core::FitTarget> target_;
  std::vector<double> target_grid_db_;
  trench::core::p2k::PerceptualSpace space_{};
  trench::core::p2k::Grid grid_;
  trench::core::p2k::RoleIntent intent_{};
  View view_{};
  QUndoStack undo_stack_;
  std::uint64_t next_root_gesture_{};
  std::uint64_t active_root_gesture_{};
};
