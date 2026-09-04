#include "editor_state.hpp"

#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

using Resonant = trench::core::native::Resonant;
using RealRoots = trench::core::native::RealRoots;
using Roots = trench::core::native::Roots;

constexpr double kHiddenRootBandwidthHz = 1.0e9;
constexpr Resonant kHiddenRoot{EditorState::kNyquistHz, kHiddenRootBandwidthHz};
constexpr double kPivotCeilingHz = 15'000.0;
constexpr Resonant kFreshPole{1'000.0, 100.0};
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kCageWallHz = 20'277.05;
constexpr double kMinDecayHz = 1.0;
constexpr double kMaxDecayHz = 1.0e9;
constexpr double kCutStepDb = 20.0 * 0.30102999566398120;

Roots absentRoot() { return RealRoots{kInf, kInf}; }

bool isHiddenRoot(const Roots& roots) {
  const auto* tone = std::get_if<Resonant>(&roots);
  return tone != nullptr && *tone == kHiddenRoot;
}

double clampedDecayHz(double decay_hz) {
  const double magnitude = std::clamp(std::abs(decay_hz), kMinDecayHz, kMaxDecayHz);
  return decay_hz < 0.0 ? -magnitude : magnitude;
}

}

EditorState::EditorState(QObject* parent) : QObject(parent) {
  corners_ = blank().corners;
  render();
}

EditorState::Document EditorState::blank() {
  Document document;
  for (auto& state : document.corners) {
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      state.corner.sections[index] = {kHiddenRoot, kHiddenRoot, true};
      state.enabled[index] = false;
      state.zero_present[index] = false;
    }
  }
  return document;
}

EditorState::Document EditorState::documentFrom(const trench::core::native::Body& body) {
  Document document = blank();
  const auto editable = [](const Roots& roots) -> std::optional<Roots> {
    if (const auto* tone = std::get_if<Resonant>(&roots)) {
      if (!std::isfinite(tone->hz) || !std::isfinite(tone->bw_hz)) return std::nullopt;
      return Roots{*tone};
    }
    const auto& real = std::get<RealRoots>(roots);
    if (!std::isfinite(real.a_hz) || !std::isfinite(real.b_hz)) return std::nullopt;
    return Roots{real};
  };
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    auto& state = document.corners[corner];
    double loudest_db = -kInf;
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      const auto& section = body.corners[corner].sections[index];
      const auto pole = editable(section.pole);
      const auto zero = pole ? editable(section.zero) : std::nullopt;
      state.corner.sections[index] = {pole ? *pole : Roots{kHiddenRoot},
                                      zero ? *zero : Roots{kHiddenRoot},
                                      true};
      state.enabled[index] = pole.has_value();
      state.zero_present[index] = zero.has_value();
      if (pole) loudest_db = std::max(loudest_db, section.gain_db);
    }
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      if (!state.enabled[index]) continue;
      const double below = loudest_db - body.corners[corner].sections[index].gain_db;
      state.cut[index] = std::clamp(static_cast<int>(std::lround(below / kCutStepDb)), 0, kMaxCut);
    }
  }
  return document;
}

EditorState::CornerState& EditorState::editing() noexcept {
  return corners_[editing_corner_];
}

const EditorState::CornerState& EditorState::editing() const noexcept {
  return corners_[editing_corner_];
}

void EditorState::render() {
  packed_ = trench::core::native::export_p2k_body(body());
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    auto& words = packed_.words[corner];
    double ratio = 1.0;
    std::size_t voiced = 0;
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      if (words[index] == trench::core::kIdentitySection) continue;
      const auto [n, d] = trench::core::p2k::dc_terms(
          {words[index][0], words[index][1], words[index][2], words[index][3]});
      ratio *= d / (std::abs(n) < 1e-15 ? 1e-15 : n);
      ++voiced;
    }
    if (voiced == 0) continue;
    const double scale = std::pow(std::abs(ratio), 1.0 / static_cast<double>(voiced));
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      if (words[index] == trench::core::kIdentitySection) continue;
      words[index][4] = trench::core::encode_word(
          scale * std::exp2(-corners_[corner].cut[index]) / 4.0);
    }
    packed_.words[corner + trench::core::kLegacyCornerCount] = words;
  }
}

void EditorState::commit() {
  render();
  emit changed();
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

EditorState::Document EditorState::document() const {
  return {corners_, editing_corner_, morph_pos_, q_pos_, morph_axis_, q_axis_};
}

trench::core::native::Body EditorState::body() const {
  trench::core::native::Body out{};
  for (std::size_t corner = 0; corner < trench::core::native::kCorners; ++corner) {
    const CornerState& state = corners_[corner];
    auto& target = out.corners[corner];
    for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
      const bool live = state.enabled[index];
      const auto& authored = state.corner.sections[index];
      target.sections[index] = {
          live ? authored.pole : absentRoot(),
          live && state.zero_present[index] ? authored.zero : absentRoot(),
          true};
    }
  }
  return out;
}

const trench::core::PackedBody& EditorState::packed() const noexcept {
  return packed_;
}

const std::optional<trench::core::PackedBody>& EditorState::sourceWords() const noexcept {
  return source_words_;
}

trench::audio::AuditionView EditorState::view() const {
  return {packed_, static_cast<float>(morph_pos_), static_cast<float>(q_pos_), 0.0};
}

trench::audio::AuditionView EditorState::soloView(std::size_t section, bool pole_only) const {
  trench::core::PackedBody solo = packed_;
  for (auto& corner : solo.words) {
    for (std::size_t index = 0; index < trench::core::kSectionCount; ++index) {
      if (index != section) corner[index] = trench::core::kIdentitySection;
    }
    if (pole_only) {
      corner[section][0] = trench::core::kIdentitySection[0];
      corner[section][1] = trench::core::kIdentitySection[1];
    }
  }
  return {solo, static_cast<float>(morph_pos_), static_cast<float>(q_pos_), 0.0};
}

trench::core::Cascade EditorState::cascade(double sample_rate_hz) const {
  return trench::audio::design_audition(view(), sample_rate_hz);
}

trench::core::Biquad EditorState::sectionBiquad(std::size_t index) const {
  if (index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return cascade(kDatumHz)[index];
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

void EditorState::setDocument(const Document& document) {
  if (document.editing_corner >= trench::core::native::kCorners) {
    throw std::out_of_range("document editing corner is outside the four corners");
  }
  remember();
  source_words_.reset();
  restore(document);
}

void EditorState::setSourceWords(const trench::core::PackedBody& words) {
  source_words_ = words;
  emit changed();
}

void EditorState::clearSourceWords() {
  if (!source_words_) return;
  source_words_.reset();
  emit changed();
}

void EditorState::restore(const Document& document) {
  corners_ = document.corners;
  editing_corner_ = std::min(document.editing_corner, trench::core::native::kCorners - 1);
  morph_pos_ = std::clamp(document.morph, 0.0, 1.0);
  q_pos_ = std::clamp(document.q, 0.0, 1.0);
  morph_axis_ = document.morph_axis;
  q_axis_ = document.q_axis;
  if (!rootPresent(selected_section_, selected_lane_)) selected_lane_ = Lane::kPole;
  commit();
  emit selectionChanged(selected_section_);
}

void EditorState::remember() {
  if (group_depth_ > 0) {
    if (group_recorded_) return;
    group_recorded_ = true;
  }
  undo_.push_back(document());
  if (undo_.size() > 200) undo_.erase(undo_.begin());
  redo_.clear();
}

void EditorState::undo() {
  if (undo_.empty()) return;
  redo_.push_back(document());
  const Document previous = undo_.back();
  undo_.pop_back();
  restore(previous);
}

void EditorState::redo() {
  if (redo_.empty()) return;
  undo_.push_back(document());
  const Document next = redo_.back();
  redo_.pop_back();
  restore(next);
}

bool EditorState::canUndo() const noexcept { return !undo_.empty(); }

bool EditorState::canRedo() const noexcept { return !redo_.empty(); }

void EditorState::beginUndoGroup() {
  if (group_depth_++ == 0) group_recorded_ = false;
}

void EditorState::endUndoGroup() {
  if (group_depth_ > 0) --group_depth_;
}

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
  const bool hi_m = morph_pos_ > 0.88, lo_m = morph_pos_ < 0.12;
  const bool hi_q = q_pos_ > 0.88, lo_q = q_pos_ < 0.12;
  if ((lo_m || hi_m) && (lo_q || hi_q)) {
    const std::size_t corner = (hi_q ? 2u : 0u) + (hi_m ? 1u : 0u);
    if (corner != editing_corner_) setEditingCorner(corner);
  }
  emit changed();
}

void EditorState::toggleSection(std::size_t index) {
  toggleSectionAt(editing_corner_, index);
}

void EditorState::toggleSectionAt(std::size_t corner, std::size_t index) {
  if (corner >= trench::core::native::kCorners ||
      index >= trench::core::native::kSections) {
    return;
  }
  remember();
  selected_section_ = index;
  selected_lane_ = Lane::kPole;
  auto& state = corners_[corner];
  state.enabled[index] = !state.enabled[index];
  if (state.enabled[index] && isHiddenRoot(state.corner.sections[index].pole)) {
    state.corner.sections[index].pole = kFreshPole;
  }
  if (state.enabled[index] && index + 1 == trench::core::native::kSections &&
      !state.zero_present[index]) {
    state.corner.sections[index].zero = Resonant{kCageWallHz, 0.0};
    state.zero_present[index] = true;
  }
  commit();
  emit selectionChanged(selected_section_);
}

void EditorState::addZeroAt(double hz, double bw_hz) {
  auto& state = editing();
  if (!state.enabled[selected_section_] || state.zero_present[selected_section_]) {
    return;
  }
  remember();
  state.corner.sections[selected_section_].zero =
      Resonant{std::clamp(hz, kLowHz, kNyquistHz),
               std::clamp(bw_hz, kMinBandwidthHz, kMaxBandwidthHz)};
  state.zero_present[selected_section_] = true;
  selected_lane_ = Lane::kZero;
  commit();
  emit selectionChanged(selected_section_);
}

void EditorState::removeZero() {
  removeZeroAt(editing_corner_, selected_section_);
}

void EditorState::removeZeroAt(std::size_t corner, std::size_t section_index) {
  if (corner >= trench::core::native::kCorners ||
      section_index >= trench::core::native::kSections) {
    return;
  }
  auto& state = corners_[corner];
  if (!state.zero_present[section_index]) return;
  remember();
  state.corner.sections[section_index].zero = kHiddenRoot;
  state.zero_present[section_index] = false;
  if (corner == editing_corner_ && section_index == selected_section_) {
    selected_lane_ = Lane::kPole;
  }
  commit();
  emit selectionChanged(selected_section_);
}


void EditorState::setWordsAt(std::size_t corner, std::size_t section_index, Lane lane,
                             std::uint16_t mag_word, std::uint16_t rsq_word) {
  if (corner >= trench::core::native::kCorners ||
      section_index >= trench::core::native::kSections) {
    return;
  }
  auto& state = corners_[corner];
  if (!state.enabled[section_index]) return;
  const auto [p, q] = trench::core::p2k::pq(mag_word, rsq_word);
  const Roots wanted = trench::core::native::roots_from_coefficients(p, q, kDatumHz);
  auto& roots = lane == Lane::kPole ? state.corner.sections[section_index].pole
                                    : state.corner.sections[section_index].zero;
  const bool present = lane == Lane::kPole || state.zero_present[section_index];
  if (present && roots == wanted) return;
  remember();
  roots = wanted;
  if (lane == Lane::kZero) state.zero_present[section_index] = true;
  commit();
}

void EditorState::setCutAt(std::size_t corner, std::size_t section_index, int cut) {
  if (corner >= trench::core::native::kCorners ||
      section_index >= trench::core::native::kSections) {
    return;
  }
  auto& state = corners_[corner];
  const int wanted = std::clamp(cut, 0, kMaxCut);
  if (state.cut[section_index] == wanted) return;
  remember();
  state.cut[section_index] = wanted;
  commit();
}

int EditorState::cutAt(std::size_t corner, std::size_t section_index) const {
  if (corner >= trench::core::native::kCorners ||
      section_index >= trench::core::native::kSections) {
    throw std::out_of_range("section index is outside the six-section state");
  }
  return corners_[corner].cut[section_index];
}

void EditorState::setRoot(std::size_t section_index, Lane lane,
                          double frequency_hz, double bandwidth_hz) {
  setRootAt(editing_corner_, section_index, lane, frequency_hz, bandwidth_hz);
}

void EditorState::setRootAt(std::size_t corner, std::size_t section_index, Lane lane,
                            double frequency_hz, double bandwidth_hz) {
  if (corner >= trench::core::native::kCorners ||
      section_index >= trench::core::native::kSections) {
    return;
  }
  auto& state = corners_[corner];
  if (!state.enabled[section_index] ||
      (lane == Lane::kZero && !state.zero_present[section_index])) {
    return;
  }
  const Resonant wanted{
      std::clamp(frequency_hz, kLowHz, kNyquistHz),
      std::clamp(bandwidth_hz, kMinBandwidthHz, kMaxBandwidthHz)};
  auto& roots = lane == Lane::kPole ? state.corner.sections[section_index].pole
                                    : state.corner.sections[section_index].zero;
  if (roots == Roots{wanted}) return;
  remember();
  roots = wanted;
  commit();
}

void EditorState::setZeroAt(std::size_t corner, std::size_t section_index,
                            double frequency_hz, double bandwidth_hz) {
  if (corner >= trench::core::native::kCorners ||
      section_index >= trench::core::native::kSections) {
    return;
  }
  auto& state = corners_[corner];
  if (!state.enabled[section_index]) return;
  const Resonant wanted{std::clamp(frequency_hz, kLowHz, kNyquistHz),
                        std::clamp(bandwidth_hz, kMinBandwidthHz, kMaxBandwidthHz)};
  if (state.zero_present[section_index] &&
      state.corner.sections[section_index].zero == Roots{wanted}) {
    return;
  }
  remember();
  state.corner.sections[section_index].zero = wanted;
  state.zero_present[section_index] = true;
  commit();
}

void EditorState::setRealRootAt(std::size_t corner, std::size_t section_index, Lane lane,
                                double a_hz, double b_hz) {
  if (corner >= trench::core::native::kCorners ||
      section_index >= trench::core::native::kSections) {
    return;
  }
  auto& state = corners_[corner];
  if (!state.enabled[section_index] ||
      (lane == Lane::kZero && !state.zero_present[section_index])) {
    return;
  }
  const RealRoots wanted{clampedDecayHz(a_hz), clampedDecayHz(b_hz)};
  auto& roots = lane == Lane::kPole ? state.corner.sections[section_index].pole
                                    : state.corner.sections[section_index].zero;
  if (roots == Roots{wanted}) return;
  remember();
  roots = wanted;
  commit();
}

void EditorState::applyAffine(double semitones, double tract, double character,
                              double exaggerate) {
  applyAffineAt(editing_corner_, semitones, tract, character, exaggerate);
}

void EditorState::applyAffineAt(std::size_t corner, double semitones, double tract,
                                double character, double exaggerate) {
  if (corner >= trench::core::native::kCorners) return;
  const bool moves_frequency = semitones != 0.0 || tract != 1.0;
  const bool moves_bandwidth = character != 1.0 || exaggerate != 1.0;
  if (!moves_frequency && !moves_bandwidth) return;
  remember();

  auto& state = corners_[corner];
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
  commit();
}

void EditorState::copyCornerFrom(std::size_t source) {
  if (source >= trench::core::native::kCorners || source == editing_corner_) return;
  if (corners_[source] == editing()) return;
  remember();
  editing() = corners_[source];
  commit();
}

void EditorState::copyCornerTo(std::size_t target) {
  if (target >= trench::core::native::kCorners || target == editing_corner_) return;
  if (corners_[target] == editing()) return;
  remember();
  corners_[target] = editing();
  commit();
}

std::optional<double> EditorState::poleHzAt(std::size_t corner, std::size_t index) const {
  if (corner >= kCorners || index >= kSections) return std::nullopt;
  const CornerState& state = corners_[corner];
  if (!state.enabled[index]) return std::nullopt;
  const auto* tone = std::get_if<Resonant>(&state.corner.sections[index].pole);
  if (tone == nullptr) return std::nullopt;
  if (!std::isfinite(tone->hz) || !(tone->hz > 0.0)) return std::nullopt;
  if (!std::isfinite(tone->bw_hz) || tone->bw_hz > kMaxBandwidthHz) return std::nullopt;
  return tone->hz;
}

EditorState::AnchorPlan EditorState::planAnchorsAgainst(
    std::size_t corner,
    const std::array<std::optional<double>, kSections>& incoming_pole_hz,
    bool include_q) const {
  AnchorPlan plan;
  plan.slot_for_row.fill(kSections);
  if (corner >= kCorners) {
    for (std::size_t row = 0; row < kSections; ++row) plan.slot_for_row[row] = row;
    return plan;
  }
  std::array<std::array<std::optional<double>, kSections>, 2> partner_hz{};
  for (std::size_t slot = 0; slot < kSections; ++slot) {
    partner_hz[0][slot] = poleHzAt(corner ^ 1u, slot);
    if (include_q) partner_hz[1][slot] = poleHzAt(corner ^ 2u, slot);
  }
  std::array<bool, kSections> row_taken{};
  std::array<bool, kSections> slot_taken{};
  for (std::size_t slot = 0; slot < kSections; ++slot) {
    if (!partner_hz[0][slot] && !partner_hz[1][slot]) continue;
    std::size_t best = kSections;
    double best_cost = std::numeric_limits<double>::infinity();
    for (std::size_t row = 0; row < kSections; ++row) {
      if (row_taken[row] || !incoming_pole_hz[row]) continue;
      if (!(*incoming_pole_hz[row] > 0.0)) continue;
      double total = 0.0;
      std::size_t terms = 0;
      for (const auto& held : partner_hz) {
        if (!held[slot]) continue;
        total += std::abs(std::log2(*incoming_pole_hz[row] / *held[slot]));
        ++terms;
      }
      const double cost = total / static_cast<double>(terms);
      if (!(cost <= kAnchorOctaves) || !(cost < best_cost)) continue;
      best_cost = cost;
      best = row;
    }
    if (best >= kSections) continue;
    plan.slot_for_row[best] = slot;
    row_taken[best] = true;
    slot_taken[slot] = true;
    ++plan.paired;
  }
  std::vector<std::size_t> loose;
  for (std::size_t row = 0; row < kSections; ++row) {
    if (row_taken[row] || !incoming_pole_hz[row]) continue;
    loose.push_back(row);
  }
  std::stable_sort(loose.begin(), loose.end(), [&](std::size_t left, std::size_t right) {
    return *incoming_pole_hz[left] < *incoming_pole_hz[right];
  });
  std::vector<std::size_t> free_slots;
  for (std::size_t slot = 0; slot < kSections; ++slot) {
    if (!slot_taken[slot]) free_slots.push_back(slot);
  }
  std::size_t next = 0;
  for (const std::size_t row : loose) plan.slot_for_row[row] = free_slots[next++];
  for (std::size_t row = 0; row < kSections; ++row) {
    if (plan.slot_for_row[row] < kSections) continue;
    plan.slot_for_row[row] = free_slots[next++];
  }
  return plan;
}

EditorState::AnchorPlan EditorState::planAnchors(
    std::size_t corner,
    const std::array<std::optional<double>, kSections>& incoming_pole_hz) const {
  return planAnchorsAgainst(corner, incoming_pole_hz, false);
}

EditorState::AnchorPlan EditorState::planSquareAnchors(
    std::size_t corner,
    const std::array<std::optional<double>, kSections>& incoming_pole_hz) const {
  return planAnchorsAgainst(corner, incoming_pole_hz, true);
}

std::pair<QString, QString> EditorState::axisNames() const {
  return {morph_axis_, q_axis_};
}

void EditorState::setAxisNames(const QString& morph, const QString& q) {
  if (morph_axis_ == morph && q_axis_ == q) return;
  remember();
  morph_axis_ = morph;
  q_axis_ = q;
  commit();
}

void EditorState::anchorCornerToPartner(std::size_t corner) {
  anchorCornerAgainst(corner, false);
}

void EditorState::anchorCornerToSquare(std::size_t corner) {
  anchorCornerAgainst(corner, true);
}

void EditorState::anchorSquare() {
  beginUndoGroup();
  for (std::size_t corner = 1; corner < kCorners; ++corner) {
    anchorCornerAgainst(corner, true);
  }
  endUndoGroup();
}

void EditorState::anchorCornerAgainst(std::size_t corner, bool include_q) {
  if (corner >= kCorners) return;
  bool anchored = false;
  for (std::size_t slot = 0; slot < kSections; ++slot) {
    if (poleHzAt(corner ^ 1u, slot)) anchored = true;
    if (include_q && poleHzAt(corner ^ 2u, slot)) anchored = true;
  }
  if (!anchored) return;
  std::array<std::optional<double>, kSections> incoming{};
  for (std::size_t row = 0; row < kSections; ++row) incoming[row] = poleHzAt(corner, row);
  const AnchorPlan plan = planAnchorsAgainst(corner, incoming, include_q);
  bool identity = true;
  for (std::size_t row = 0; row < kSections; ++row) {
    if (plan.slot_for_row[row] != row) identity = false;
  }
  if (identity) return;
  beginUndoGroup();
  remember();
  const CornerState held = corners_[corner];
  CornerState& state = corners_[corner];
  for (std::size_t row = 0; row < kSections; ++row) {
    const std::size_t slot = plan.slot_for_row[row];
    state.corner.sections[slot] = held.corner.sections[row];
    state.enabled[slot] = held.enabled[row];
    state.zero_present[slot] = held.zero_present[row];
    state.cut[slot] = held.cut[row];
  }
  commit();
  endUndoGroup();
}

void EditorState::sharpenPoles(double radius_step) {
  sharpenPolesAt(editing_corner_, radius_step);
}

void EditorState::sharpenPolesAt(std::size_t corner, double radius_step) {
  if (corner >= trench::core::native::kCorners || radius_step == 0.0) return;
  auto& state = corners_[corner];
  bool moved = false;
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    if (!state.enabled[index]) continue;
    auto* pole = std::get_if<Resonant>(&state.corner.sections[index].pole);
    if (pole == nullptr || pole->bw_hz > kMaxBandwidthHz) continue;
    const double radius = std::exp(-std::numbers::pi * pole->bw_hz / kDatumHz);
    const double raised = std::clamp(radius + radius_step, 0.0, 0.999999);
    const double bw = std::clamp(-std::log(raised) * kDatumHz / std::numbers::pi,
                                 kMinBandwidthHz, kMaxBandwidthHz);
    if (bw == pole->bw_hz) continue;
    if (!moved) remember();
    moved = true;
    pole->bw_hz = bw;
  }
  if (moved) commit();
}
