#pragma once

#include "trench/core/native_body.hpp"

#include <QObject>

#include <array>
#include <cstddef>

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
  [[nodiscard]] trench::core::Cascade cascade(double sample_rate_hz = kDatumHz) const;
  [[nodiscard]] trench::core::Biquad sectionBiquad(
      std::size_t index, double sample_rate_hz = kDatumHz) const;
  [[nodiscard]] bool sectionEnabled(std::size_t index) const;
  [[nodiscard]] bool rootPresent(std::size_t index, Lane lane) const;
  [[nodiscard]] std::size_t selectedSection() const noexcept;
  [[nodiscard]] Lane selectedLane() const noexcept;

  void selectSection(std::size_t index);
  void selectRoot(std::size_t index, Lane lane);
  void toggleSection(std::size_t index);
  void addZeroAtNyquist();
  void setRoot(std::size_t section, Lane lane, double frequency_hz,
               double bandwidth_hz);

 signals:
  void changed();
  void selectionChanged(std::size_t section);

 private:
  trench::core::native::Corner corner_{};
  std::array<bool, trench::core::native::kSections> enabled_{};
  std::array<bool, trench::core::native::kSections> zero_present_{};
  std::size_t selected_section_{};
  Lane selected_lane_{Lane::kPole};
};
