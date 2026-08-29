#include "editor_state.hpp"

#include "template_shelf.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

using Resonant = trench::core::native::Resonant;

constexpr double kAbsentRootBandwidthHz = 1.0e9;
constexpr Resonant kParkedRoot{EditorState::kNyquistHz, kAbsentRootBandwidthHz};
constexpr double kPivotCeilingHz = 15'000.0;

const Resonant& resonant(const trench::core::native::Roots& roots) {
  const auto* value = std::get_if<Resonant>(&roots);
  if (value == nullptr) throw std::logic_error("editor state contains non-resonant roots");
  return *value;
}

// THE INTERIOR IS LOG FREQUENCY AND LOG BANDWIDTH (Rossum 2019): corners 0/1
// are the Q0 morph pair and 2/3 the Q100 pair, so morph runs first and Q
// blends the two results.
Resonant blend(const std::array<Resonant, trench::core::native::kCorners>& roots,
               double morph, double q) {
  const auto along_morph = [&](std::size_t low, std::size_t high) {
    return std::pair{
        std::lerp(std::log2(roots[low].hz), std::log2(roots[high].hz), morph),
        std::lerp(std::log2(roots[low].bw_hz), std::log2(roots[high].bw_hz),
                  morph)};
  };
  const auto at_q0 = along_morph(0, 1);
  const auto at_q100 = along_morph(2, 3);
  return {std::exp2(std::lerp(at_q0.first, at_q100.first, q)),
          std::exp2(std::lerp(at_q0.second, at_q100.second, q))};
}

}  // namespace

EditorState::EditorState(QObject* parent) : QObject(parent) {
  // BOOT IS A FLAT EQ (Tyson 2026-08-28 "use a low shelf and 5 parametric
  // bells"): six live bands, pole and zero paired at each centre with equal
  // width, so the cascade opens at 0 dB and pulling a pole off its zero IS
  // the bell gain. Band 1 sits low and wide - the shelf seat.
  constexpr std::array<Resonant, trench::core::native::kSections> bands{{
      {80.0, 114.0}, {250.0, 250.0}, {700.0, 700.0},
      {2'000.0, 2'000.0}, {5'500.0, 5'500.0}, {12'000.0, 12'000.0},
  }};
  for (auto& state : corners_) {
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      state.corner.sections[index] = {bands[index], bands[index], true};
      state.enabled[index] = true;
      state.zero_present[index] = true;
    }
  }
}

EditorState::CornerState& EditorState::editing() noexcept {
  return corners_[editing_corner_];
}

const EditorState::CornerState& EditorState::editing() const noexcept {
  return corners_[editing_corner_];
}

const trench::core::native::Corner& EditorState::corner() const noexcept {
  return editing().corner;
}

const trench::core::native::Section& EditorState::section(std::size_t index) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return editing().corner.sections[index];
}

const trench::core::native::Section& EditorState::sectionAt(
    std::size_t corner, std::size_t index) const {
  if (corner >= trench::core::native::kCorners ||
      index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return corners_[corner].corner.sections[index];
}

bool EditorState::sectionEnabledAt(std::size_t corner, std::size_t index) const {
  if (corner >= trench::core::native::kCorners ||
      index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return corners_[corner].enabled[index];
}

bool EditorState::zeroPresentAt(std::size_t corner, std::size_t index) const {
  if (corner >= trench::core::native::kCorners ||
      index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return corners_[corner].zero_present[index];
}

trench::core::Cascade EditorState::cascade(double sample_rate_hz) const {
  trench::core::Cascade out{};
  for (auto& biquad : out) biquad = {1.0, 0.0, 0.0, 0.0, 0.0};
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const bool live = std::any_of(
        corners_.begin(), corners_.end(),
        [index](const CornerState& state) { return state.enabled[index]; });
    if (live) out[index] = interiorSectionBiquad(index, sample_rate_hz);
  }
  return out;
}

// A lane a corner does not carry stands at the rim, so it lerps out the way
// the hardware parks an absent root.
trench::core::Biquad EditorState::interiorSectionBiquad(
    std::size_t index, double sample_rate_hz) const {
  std::array<Resonant, trench::core::native::kCorners> poles{};
  std::array<Resonant, trench::core::native::kCorners> zeros{};
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    const auto& state = corners_[corner];
    const bool live = state.enabled[index];
    poles[corner] = live ? resonant(state.corner.sections[index].pole) : kParkedRoot;
    zeros[corner] = live && state.zero_present[index]
                        ? resonant(state.corner.sections[index].zero)
                        : kParkedRoot;
  }
  const trench::core::native::Section blended{blend(poles, morph_pos_, q_pos_),
                                              blend(zeros, morph_pos_, q_pos_),
                                              true};
  return trench::core::native::biquad(
      trench::core::native::design(blended, sample_rate_hz));
}

trench::core::Biquad EditorState::sectionBiquad(std::size_t index,
                                                double sample_rate_hz) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return trench::core::native::biquad(trench::core::native::design(
      editing().corner.sections[index], sample_rate_hz));
}

bool EditorState::sectionEnabled(std::size_t index) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return editing().enabled[index];
}

bool EditorState::rootPresent(std::size_t index, Lane lane) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return lane == Lane::kPole || editing().zero_present[index];
}

std::size_t EditorState::selectedSection() const noexcept {
  return selected_section_;
}

EditorState::Lane EditorState::selectedLane() const noexcept {
  return selected_lane_;
}

std::size_t EditorState::editingCorner() const noexcept {
  return editing_corner_;
}

double EditorState::morphPos() const noexcept { return morph_pos_; }

double EditorState::qPos() const noexcept { return q_pos_; }

void EditorState::selectSection(std::size_t index) {
  if (index >= trench::core::native::kSections || index == selected_section_) return;
  selected_section_ = index;
  if (!rootPresent(index, selected_lane_)) selected_lane_ = Lane::kPole;
  emit selectionChanged(selected_section_);
}

void EditorState::selectRoot(std::size_t index, Lane lane) {
  if (index >= trench::core::native::kSections || !editing().enabled[index] ||
      !rootPresent(index, lane)) {
    return;
  }
  if (index == selected_section_ && lane == selected_lane_) return;
  selected_section_ = index;
  selected_lane_ = lane;
  emit selectionChanged(selected_section_);
}

void EditorState::setEditingCorner(std::size_t index) {
  if (index >= trench::core::native::kCorners || index == editing_corner_) return;
  editing_corner_ = index;
  if (!rootPresent(selected_section_, selected_lane_)) selected_lane_ = Lane::kPole;
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::setPadPosition(double morph01, double q01) {
  const double morph = std::clamp(morph01, 0.0, 1.0);
  const double q = std::clamp(q01, 0.0, 1.0);
  if (morph == morph_pos_ && q == q_pos_) return;
  morph_pos_ = morph;
  q_pos_ = q;
  // RIDE INTO A CORNER, EDIT THAT CORNER (Tyson 2026-08-29 "auto swap to a
  // corner when you morph pretty much all the way").
  const bool hi_m = morph_pos_ > 0.88, lo_m = morph_pos_ < 0.12;
  const bool hi_q = q_pos_ > 0.88, lo_q = q_pos_ < 0.12;
  if ((lo_m || hi_m) && (lo_q || hi_q)) {
    const std::size_t corner = (hi_q ? 2u : 0u) + (hi_m ? 1u : 0u);
    if (corner != editing_corner_) setEditingCorner(corner);
  }
  emit changed();
}

void EditorState::toggleSection(std::size_t index) {
  if (index >= trench::core::native::kSections) return;
  selected_section_ = index;
  selected_lane_ = Lane::kPole;
  editing().enabled[index] = !editing().enabled[index];
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::addZeroAt(double hz, double bw_hz) {
  auto& state = editing();
  if (!state.enabled[selected_section_] || state.zero_present[selected_section_]) {
    return;
  }
  state.corner.sections[selected_section_].zero =
      Resonant{std::clamp(hz, kLowHz, kNyquistHz),
               std::clamp(bw_hz, kMinBandwidthHz, kMaxBandwidthHz)};
  state.zero_present[selected_section_] = true;
  selected_lane_ = Lane::kZero;
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::removeZero() {
  auto& state = editing();
  if (!state.zero_present[selected_section_]) return;
  state.corner.sections[selected_section_].zero = kParkedRoot;
  state.zero_present[selected_section_] = false;
  selected_lane_ = Lane::kPole;
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::loadTemplate(const trench::app::TemplateEntry& entry) {
  auto& state = editing();
  std::size_t first_present = trench::core::native::kSections;
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const auto& pole = entry.poles[index];
    state.corner.sections[index] = {
        pole.present ? Resonant{pole.hz, pole.bw_hz} : kParkedRoot,
        kParkedRoot, true};
    state.enabled[index] = pole.present;
    state.zero_present[index] = false;
    if (pole.present && first_present == trench::core::native::kSections) {
      first_present = index;
    }
  }
  selected_section_ =
      first_present == trench::core::native::kSections ? 0 : first_present;
  selected_lane_ = Lane::kPole;
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::loadPoles(const std::vector<std::pair<double, double>>& poles) {
  auto& state = editing();
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const bool carried = index < poles.size();
    state.corner.sections[index] = {
        carried ? Resonant{std::clamp(poles[index].first, kLowHz, kNyquistHz),
                           std::clamp(poles[index].second, kMinBandwidthHz,
                                      kMaxBandwidthHz)}
                : kParkedRoot,
        kParkedRoot, true};
    state.enabled[index] = carried;
    state.zero_present[index] = false;
  }
  selected_section_ = 0;
  selected_lane_ = Lane::kPole;
  emit changed();
  emit selectionChanged(selected_section_);
}

void EditorState::setRoot(std::size_t section_index, Lane lane,
                          double frequency_hz, double bandwidth_hz) {
  auto& state = editing();
  if (section_index >= trench::core::native::kSections ||
      !state.enabled[section_index] || !rootPresent(section_index, lane)) {
    return;
  }
  const Resonant wanted{
      std::clamp(frequency_hz, kLowHz, kNyquistHz),
      std::clamp(bandwidth_hz, kMinBandwidthHz, kMaxBandwidthHz)};
  auto& roots = lane == Lane::kPole ? state.corner.sections[section_index].pole
                                    : state.corner.sections[section_index].zero;
  if (resonant(roots) == wanted) return;
  roots = wanted;
  emit changed();
}

void EditorState::applyAffine(double semitones, double tract, double character,
                              double exaggerate) {
  const bool moves_frequency = semitones != 0.0 || tract != 1.0;
  const bool moves_bandwidth = character != 1.0 || exaggerate != 1.0;
  if (!moves_frequency && !moves_bandwidth) return;

  auto& state = editing();
  const auto authored = [&state](std::size_t index, Lane lane) -> Resonant* {
    if (lane == Lane::kZero && !state.zero_present[index]) return nullptr;
    auto& roots = lane == Lane::kPole ? state.corner.sections[index].pole
                                      : state.corner.sections[index].zero;
    auto* value = std::get_if<Resonant>(&roots);
    if (value == nullptr || value->bw_hz > kMaxBandwidthHz) return nullptr;
    return value;
  };
  const auto forEachRoot = [&authored](auto&& apply) {
    for (std::size_t index = 0; index < trench::core::native::kSections;
         ++index) {
      if (auto* pole = authored(index, Lane::kPole)) apply(*pole, Lane::kPole);
      if (auto* zero = authored(index, Lane::kZero)) apply(*zero, Lane::kZero);
    }
  };
  const auto polePivot = [&state, &authored](auto&& measure) {
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t index = 0; index < trench::core::native::kSections;
         ++index) {
      if (!state.enabled[index]) continue;
      const auto* pole = authored(index, Lane::kPole);
      if (pole == nullptr || pole->hz >= kPivotCeilingHz) continue;
      sum += std::log(measure(*pole));
      ++count;
    }
    return count == 0 ? 0.0 : std::exp(sum / static_cast<double>(count));
  };

  if (semitones != 0.0) {
    const double shift = std::exp2(semitones / 12.0);
    forEachRoot([shift](Resonant& root, Lane) {
      root.hz = std::clamp(root.hz * shift, kLowHz, kNyquistHz);
    });
  }
  if (tract != 1.0) {
    const double pivot = polePivot([](const Resonant& root) { return root.hz; });
    if (pivot > 0.0) {
      forEachRoot([pivot, tract](Resonant& root, Lane) {
        root.hz = std::clamp(pivot * std::pow(root.hz / pivot, tract), kLowHz,
                             kNyquistHz);
      });
    }
  }
  if (character != 1.0) {
    forEachRoot([character](Resonant& root, Lane lane) {
      if (lane != Lane::kPole) return;
      root.bw_hz = std::clamp(root.bw_hz * character, kMinBandwidthHz,
                              kMaxBandwidthHz);
    });
  }
  if (exaggerate != 1.0) {
    const double pivot =
        polePivot([](const Resonant& root) { return root.bw_hz; });
    if (pivot > 0.0) {
      forEachRoot([pivot, exaggerate](Resonant& root, Lane lane) {
        if (lane != Lane::kPole) return;
        root.bw_hz =
            std::clamp(pivot * std::pow(root.bw_hz / pivot, exaggerate),
                       kMinBandwidthHz, kMaxBandwidthHz);
      });
    }
  }
  emit changed();
}
