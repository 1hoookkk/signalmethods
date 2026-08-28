#include "editor_state.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace {

using Resonant = trench::core::native::Resonant;

const Resonant& resonant(const trench::core::native::Roots& roots) {
  const auto* value = std::get_if<Resonant>(&roots);
  if (value == nullptr) throw std::logic_error("editor state contains non-resonant roots");
  return *value;
}

}  // namespace

EditorState::EditorState(QObject* parent) : QObject(parent) {
  constexpr std::array<double, trench::core::native::kSections> bandwidths{
      55.0, 85.0, 150.0, 280.0, 620.0, 1'400.0};
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const Resonant parked{kNyquistHz, bandwidths[index]};
    const Resonant pole = index == 0 ? Resonant{70.0, bandwidths[index]}
                                     : parked;
    corner_.sections[index] = {pole, parked, true};
  }
}

const trench::core::native::Corner& EditorState::corner() const noexcept {
  return corner_;
}

const trench::core::native::Section& EditorState::section(std::size_t index) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return corner_.sections[index];
}

trench::core::Cascade EditorState::cascade(double sample_rate_hz) const {
  return trench::core::native::cascade(
      trench::core::native::design(corner_, sample_rate_hz), corner_.gain_db);
}

std::size_t EditorState::selectedSection() const noexcept {
  return selected_section_;
}

EditorState::Lane EditorState::selectedLane() const noexcept {
  return selected_lane_;
}

void EditorState::selectSection(std::size_t index) {
  if (index >= trench::core::native::kSections || index == selected_section_) return;
  selected_section_ = index;
  emit selectionChanged(selected_section_);
}

void EditorState::selectRoot(std::size_t index, Lane lane) {
  if (index >= trench::core::native::kSections) return;
  if (index == selected_section_ && lane == selected_lane_) return;
  selected_section_ = index;
  selected_lane_ = lane;
  emit selectionChanged(selected_section_);
}

void EditorState::setRoot(std::size_t section_index, Lane lane,
                          double frequency_hz, double bandwidth_hz) {
  if (section_index >= trench::core::native::kSections) return;
  const Resonant wanted{
      std::clamp(frequency_hz, kLowHz, kHighHz),
      std::clamp(bandwidth_hz, kMinBandwidthHz, kMaxBandwidthHz)};
  auto& roots = lane == Lane::kPole ? corner_.sections[section_index].pole
                                    : corner_.sections[section_index].zero;
  if (resonant(roots) == wanted) return;
  roots = wanted;
  emit changed();
}
