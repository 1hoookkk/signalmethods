#pragma once

#include "trench/core/native_body.hpp"

#include <QObject>

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace trench::app {
struct TemplateEntry;
}

class EditorState final : public QObject {
  Q_OBJECT

 public:
  enum class Lane { kPole, kZero };

  static constexpr double kDatumHz = 44'100.0;
  static constexpr double kLowHz = 20.0;
  static constexpr double kNyquistHz = kDatumHz * 0.5;
  static constexpr double kHighHz = kNyquistHz;
  static constexpr double kMinBandwidthHz = 1.0;
  static constexpr double kMaxBandwidthHz = 20'000.0;

  explicit EditorState(QObject* parent = nullptr);

  [[nodiscard]] const trench::core::native::Corner& corner() const noexcept;
  [[nodiscard]] const trench::core::native::Section& section(std::size_t index) const;
  [[nodiscard]] const trench::core::native::Section& sectionAt(
      std::size_t corner, std::size_t index) const;
  [[nodiscard]] bool sectionEnabledAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] bool zeroPresentAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] trench::core::Cascade cascade(double sample_rate_hz = kDatumHz) const;
  [[nodiscard]] trench::core::Biquad sectionBiquad(
      std::size_t index, double sample_rate_hz = kDatumHz) const;
  [[nodiscard]] bool sectionEnabled(std::size_t index) const;
  [[nodiscard]] bool rootPresent(std::size_t index, Lane lane) const;
  [[nodiscard]] std::size_t selectedSection() const noexcept;
  [[nodiscard]] Lane selectedLane() const noexcept;
  [[nodiscard]] std::size_t editingCorner() const noexcept;
  [[nodiscard]] double morphPos() const noexcept;
  [[nodiscard]] double qPos() const noexcept;

  void selectSection(std::size_t index);
  void selectRoot(std::size_t index, Lane lane);
  void setEditingCorner(std::size_t index);
  void setPadPosition(double morph01, double q01);
  void toggleSection(std::size_t index);
  void addZeroAt(double hz, double bw_hz);
  void removeZero();
  void loadTemplate(const trench::app::TemplateEntry& entry);
  void loadPoles(const std::vector<std::pair<double, double>>& poles);
  void setRoot(std::size_t section, Lane lane, double frequency_hz,
               double bandwidth_hz);
  void applyAffine(double semitones, double tract, double character,
                   double exaggerate);

 signals:
  void changed();
  void selectionChanged(std::size_t section);

 private:
  struct CornerState {
    trench::core::native::Corner corner{};
    std::array<bool, trench::core::native::kSections> enabled{};
    std::array<bool, trench::core::native::kSections> zero_present{};
  };

  [[nodiscard]] CornerState& editing() noexcept;
  [[nodiscard]] const CornerState& editing() const noexcept;
  [[nodiscard]] trench::core::Biquad interiorSectionBiquad(
      std::size_t index, double sample_rate_hz) const;

  std::array<CornerState, trench::core::native::kCorners> corners_{};
  std::size_t editing_corner_{};
  double morph_pos_{};
  double q_pos_{};
  std::size_t selected_section_{};
  Lane selected_lane_{Lane::kPole};
};
