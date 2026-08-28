#include "editor_state.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

using Resonant = trench::core::native::Resonant;

constexpr double kAbsentRootBandwidthHz = 1.0e9;

const Resonant& resonant(const trench::core::native::Roots& roots) {
  const auto* value = std::get_if<Resonant>(&roots);
  if (value == nullptr) throw std::logic_error("editor state contains non-resonant roots");
  return *value;
}

}  // namespace

EditorState::EditorState(QObject* parent) : QObject(parent) {
  // Exact poles from the retained E-mu Ooh-To-Eee M0/Q0 corner. The source
  // zeros are deliberately stripped: this is a pole-template starting state,
  // not a reconstruction of the factory response.
  constexpr std::array<Resonant, trench::core::native::kSections> poles{{
      {848.96, 82.70}, {541.09, 279.50}, {2'154.04, 236.83},
      {2'841.05, 159.36}, {3'621.98, 86.16}, {4'371.87, 180.43},
  }};
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const Resonant absent_zero{kNyquistHz, kAbsentRootBandwidthHz};
    corner_.sections[index] = {poles[index], absent_zero, true};
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
  trench::core::Cascade out{};
  for (auto& biquad : out) biquad = {1.0, 0.0, 0.0, 0.0, 0.0};
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    if (enabled_[index]) out[index] = sectionBiquad(index, sample_rate_hz);
  }
  return out;
}

trench::core::Biquad EditorState::sectionBiquad(std::size_t index,
                                                double sample_rate_hz) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return trench::core::native::biquad(
      trench::core::native::design(corner_.sections[index], sample_rate_hz));
}

bool EditorState::sectionEnabled(std::size_t index) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return enabled_[index];
}

bool EditorState::rootPresent(std::size_t index, Lane lane) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return lane == Lane::kPole || zero_present_[index];
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
  if (!rootPresent(index, selected_lane_)) selected_lane_ = Lane::kPole;
  emit selectionChanged(selected_section_);
}

void EditorState::selectRoot(std::size_t index, Lane lane) {
  if (index >= trench::core::native::kSections || !enabled_[index] ||
      !rootPresent(index, lane)) {
    return;
  }
  if (index == selected_section_ && lane == selected_lane_) return;
  selected_section_ = index;
  selected_lane_ = lane;
  emit selectionChanged(selected_section_);
}

void EditorState::toggleSection(std::size_t index) {
  if (index >= trench::core::native::kSections) return;
  selected_section_ = index;
  selected_lane_ = Lane::kPole;
  enabled_[index] = !enabled_[index];
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::addZeroAtNyquist() {
  if (!enabled_[selected_section_] || zero_present_[selected_section_]) return;
  const auto& pole = resonant(corner_.sections[selected_section_].pole);
  corner_.sections[selected_section_].zero =
      Resonant{kNyquistHz, pole.bw_hz};
  zero_present_[selected_section_] = true;
  selected_lane_ = Lane::kZero;
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::setRoot(std::size_t section_index, Lane lane,
                          double frequency_hz, double bandwidth_hz) {
  if (section_index >= trench::core::native::kSections ||
      !enabled_[section_index] || !rootPresent(section_index, lane)) {
    return;
  }
  const Resonant wanted{
      std::clamp(frequency_hz, kLowHz, kNyquistHz),
      std::clamp(bandwidth_hz, kMinBandwidthHz, kMaxBandwidthHz)};
  auto& roots = lane == Lane::kPole ? corner_.sections[section_index].pole
                                    : corner_.sections[section_index].zero;
  if (resonant(roots) == wanted) return;
  roots = wanted;
  emit changed();
}
