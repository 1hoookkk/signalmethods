#pragma once

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

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

  BodyDocument(trench::core::PackedBody body, double sample_rate_hz,
               QObject* parent = nullptr);

  [[nodiscard]] const trench::core::PackedBody& body() const noexcept;
  [[nodiscard]] std::size_t corner() const noexcept;
  [[nodiscard]] double sampleRateHz() const noexcept;
  [[nodiscard]] QUndoStack* undoStack() noexcept;
  [[nodiscard]] std::uint32_t freedomMask() const noexcept;
  [[nodiscard]] const std::optional<std::vector<double>>& target() const noexcept;
  [[nodiscard]] CornerSnapshot cornerSnapshot() const;
  [[nodiscard]] trench::core::p2k::CornerWords seedWords() const;

  void setCorner(std::size_t corner);
  void setTarget(std::vector<double> target);
  void clearTarget();

  void toggleLane(std::size_t section, bool pole);
  void applySection(std::size_t section, const trench::core::PackedSection& words);
  void applySection(std::size_t corner, std::size_t section,
                    const trench::core::PackedSection& words);
  void commitGesture(std::size_t section, const trench::core::PackedSection& before);

  void applyCorner(const CornerSnapshot& words);
  void applyCorner(std::size_t corner, const CornerSnapshot& words);
  void applyFitStep(std::size_t corner, const trench::core::p2k::CornerWords& words);
  void applyFitResult(std::size_t corner, const trench::core::p2k::StoredCorner& words);
  void commitFit(std::size_t corner, const CornerSnapshot& before);

 signals:
  void bodyChanged();
  void cornerChanged(std::size_t corner);
  void freedomMaskChanged(std::uint32_t mask);
  void targetChanged();

 private:
  trench::core::PackedBody body_;
  std::size_t corner_{};
  double sample_rate_hz_{};
  std::uint32_t freedom_mask_{};
  std::optional<std::vector<double>> target_;
  QUndoStack undo_stack_;
};
