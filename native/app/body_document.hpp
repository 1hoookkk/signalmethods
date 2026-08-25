#pragma once

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
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
  using CornerSnapshot =
      std::array<trench::core::PackedSection, trench::core::kLegacySectionCount>;

  struct View {
    float morph{0};
    float q{0};
    double semitones{0.0};
  };

  BodyDocument(trench::core::PackedBody body, double sample_rate_hz,
               QObject* parent = nullptr);

  [[nodiscard]] const trench::core::PackedBody& body() const noexcept;
  [[nodiscard]] std::size_t corner() const noexcept;
  [[nodiscard]] double sampleRateHz() const noexcept;
  [[nodiscard]] QUndoStack* undoStack() noexcept;
  [[nodiscard]] std::uint32_t freedomMask() const noexcept;
  [[nodiscard]] const std::optional<std::vector<double>>& target() const noexcept;
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
  void applySection(std::size_t section, const trench::core::PackedSection& words);
  void applySection(std::size_t corner, std::size_t section,
                    const trench::core::PackedSection& words);
  void editSection(std::size_t section, const trench::core::PackedSection& words);
  void commitGesture(const CornerSnapshot& before);

  void applyCorner(const CornerSnapshot& words);
  void applyCorner(std::size_t corner, const CornerSnapshot& words);
  void applyFitStep(std::size_t corner, const trench::core::p2k::CornerWords& words);
  void commitFit(std::size_t corner, const CornerSnapshot& before);

  void applyCharacter(double amount);
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

  trench::core::PackedBody body_;
  std::size_t corner_{};
  std::array<int, trench::core::kLegacyCornerCount> corner_semitones_{};
  [[nodiscard]] double effectiveSemitones(float morph, float q) const;
  double sample_rate_hz_{};
  std::uint32_t freedom_mask_{};
  std::optional<std::vector<double>> target_;
  trench::core::p2k::PerceptualSpace space_{};
  trench::core::p2k::Grid grid_;
  trench::core::p2k::RoleIntent intent_{};
  View view_{};
  QUndoStack undo_stack_;
};
