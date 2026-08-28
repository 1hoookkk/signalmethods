#pragma once

#include "trench/core/native_body.hpp"

#include <QObject>

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
  [[nodiscard]] std::size_t selectedSection() const noexcept;
  [[nodiscard]] Lane selectedLane() const noexcept;

  void selectSection(std::size_t index);
  void selectRoot(std::size_t index, Lane lane);
  void setRoot(std::size_t section, Lane lane, double frequency_hz,
               double bandwidth_hz);

 signals:
  void changed();
  void selectionChanged(std::size_t section);

 private:
  trench::core::native::Corner corner_{};
  std::size_t selected_section_{};
  Lane selected_lane_{Lane::kPole};
};
