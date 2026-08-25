#include "body_document.hpp"

#include "trench/core/transpose.hpp"

#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/section_param.hpp"

#include <QUndoCommand>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

void write_section(trench::core::PackedBody& body, std::size_t corner,
                   std::size_t section, const trench::core::PackedSection& words) {
  body.words[corner][section] = words;
  body.words[corner + trench::core::kLegacyCornerCount][section] = words;
}

class CornerEditCommand final : public QUndoCommand {
 public:
  CornerEditCommand(BodyDocument* document, std::size_t corner,
                    BodyDocument::CornerSnapshot before,
                    BodyDocument::CornerSnapshot after)
      : document_(document), corner_(corner), before_(before), after_(after) {}

  void redo() override { document_->applyCorner(corner_, after_); }
  void undo() override { document_->applyCorner(corner_, before_); }

 private:
  BodyDocument* document_;
  std::size_t corner_;
  BodyDocument::CornerSnapshot before_;
  BodyDocument::CornerSnapshot after_;
};

class SpaceEditCommand final : public QUndoCommand {
 public:
  SpaceEditCommand(BodyDocument* document, trench::core::p2k::PerceptualSpace before,
                   trench::core::p2k::PerceptualSpace after)
      : document_(document), before_(std::move(before)), after_(std::move(after)) {}

  void redo() override { document_->applySpace(after_); }
  void undo() override { document_->applySpace(before_); }

 private:
  BodyDocument* document_;
  trench::core::p2k::PerceptualSpace before_;
  trench::core::p2k::PerceptualSpace after_;
};

class IntentEditCommand final : public QUndoCommand {
 public:
  IntentEditCommand(BodyDocument* document, std::size_t section,
                    std::optional<trench::core::p2k::Role> before,
                    std::optional<trench::core::p2k::Role> after)
      : document_(document), section_(section), before_(before), after_(after) {}

  void redo() override { document_->applyIntent(section_, after_); }
  void undo() override { document_->applyIntent(section_, before_); }

 private:
  BodyDocument* document_;
  std::size_t section_;
  std::optional<trench::core::p2k::Role> before_;
  std::optional<trench::core::p2k::Role> after_;
};

}  // namespace

BodyDocument::BodyDocument(trench::core::PackedBody body, double sample_rate_hz,
                           QObject* parent)
    : QObject(parent),
      body_(std::move(body)),
      sample_rate_hz_(sample_rate_hz),
      freedom_mask_(trench::core::p2k::kAllFree),
      grid_(trench::core::p2k::make_grid(space_)),
      undo_stack_(this) {}

const trench::core::PackedBody& BodyDocument::body() const noexcept { return body_; }

std::size_t BodyDocument::corner() const noexcept { return corner_; }

void BodyDocument::setCorner(std::size_t corner) {
  if (corner >= trench::core::kLegacyCornerCount) return;
  const auto morph = (corner & 1U) != 0U ? 1.0F : 0.0F;
  const auto q = (corner & 2U) != 0U ? 1.0F : 0.0F;
  const View wanted{morph, q, effectiveSemitones(morph, q)};
  if (wanted.morph != view_.morph || wanted.q != view_.q ||
      wanted.semitones != view_.semitones) {
    view_ = wanted;
    emit viewChanged();
  }
  if (corner == corner_) return;
  corner_ = corner;
  emit cornerChanged(corner_);
}

double BodyDocument::sampleRateHz() const noexcept { return sample_rate_hz_; }

QUndoStack* BodyDocument::undoStack() noexcept { return &undo_stack_; }

std::uint32_t BodyDocument::freedomMask() const noexcept { return freedom_mask_; }

void BodyDocument::toggleLane(std::size_t section, bool pole) {
  const auto bit = pole ? trench::core::p2k::pole_bit(section)
                        : trench::core::p2k::zero_bit(section);
  freedom_mask_ ^= bit;
  emit freedomMaskChanged(freedom_mask_);
}

void BodyDocument::applySection(std::size_t section,
                                const trench::core::PackedSection& words) {
  applySection(corner_, section, words);
}

void BodyDocument::applySection(std::size_t corner, std::size_t section,
                                const trench::core::PackedSection& words) {
  write_section(body_, corner, section, words);
  emit bodyChanged();
}

void BodyDocument::editSection(std::size_t section,
                               const trench::core::PackedSection& words) {
  auto rows = cornerSnapshot();
  rows[section] = words;
  trench::core::p2k::write_dc_unity_scales(rows);
  applyCorner(corner_, rows);
}

void BodyDocument::commitGesture(const CornerSnapshot& before) {
  const auto after = cornerSnapshot();
  if (after == before) return;
  undo_stack_.push(new CornerEditCommand(this, corner_, before, after));
}

const std::optional<std::vector<double>>& BodyDocument::target() const noexcept {
  return target_;
}

void BodyDocument::setTarget(std::vector<double> target) {
  target_ = std::move(target);
  emit targetChanged();
}

void BodyDocument::clearTarget() {
  if (!target_) return;
  target_.reset();
  emit targetChanged();
}

const trench::core::p2k::PerceptualSpace& BodyDocument::space() const noexcept {
  return space_;
}

const trench::core::p2k::Grid& BodyDocument::grid() const noexcept { return grid_; }

void BodyDocument::setSpace(const trench::core::p2k::PerceptualSpace& space) {
  undo_stack_.push(new SpaceEditCommand(this, space_, space));
}

void BodyDocument::applySpace(const trench::core::p2k::PerceptualSpace& space) {
  space_ = space;
  grid_ = trench::core::p2k::make_grid(space_);
  emit spaceChanged();
}

const trench::core::p2k::RoleIntent& BodyDocument::intent() const noexcept {
  return intent_;
}

void BodyDocument::setIntent(std::size_t section,
                             std::optional<trench::core::p2k::Role> role) {
  if (section >= intent_.size() || intent_[section] == role) return;
  undo_stack_.push(new IntentEditCommand(this, section, intent_[section], role));
}

void BodyDocument::applyIntent(std::size_t section,
                               std::optional<trench::core::p2k::Role> role) {
  intent_[section] = role;
}

BodyDocument::View BodyDocument::view() const noexcept { return view_; }

void BodyDocument::setView(float morph, float q) {
  const auto morph_clamped = std::clamp(morph, 0.0F, 1.0F);
  const auto q_clamped = std::clamp(q, 0.0F, 1.0F);
  const View wanted{morph_clamped, q_clamped,
                    effectiveSemitones(morph_clamped, q_clamped)};
  if (wanted.morph != view_.morph || wanted.q != view_.q ||
      wanted.semitones != view_.semitones) {
    view_ = wanted;
    emit viewChanged();
  }
  if (!atCorner()) return;
  setCorner((view_.morph > 0.5F ? 1U : 0U) | ((view_.q > 0.5F ? 1U : 0U) << 1U));
}

bool BodyDocument::atCorner() const noexcept {
  return (view_.morph == 0.0F || view_.morph == 1.0F) &&
         (view_.q == 0.0F || view_.q == 1.0F);
}

void BodyDocument::setTranspose(int semitones) {
  if (corner_semitones_[corner_] == semitones) return;
  corner_semitones_[corner_] = semitones;
  view_.semitones = effectiveSemitones(view_.morph, view_.q);
  emit viewChanged();
}

int BodyDocument::cornerTranspose(std::size_t corner) const noexcept {
  return corner < corner_semitones_.size() ? corner_semitones_[corner] : 0;
}

double BodyDocument::effectiveSemitones(float morph, float q) const {
  const auto m = static_cast<double>(morph);
  const auto blend = static_cast<double>(q);
  const auto low = corner_semitones_[0] * (1.0 - m) + corner_semitones_[1] * m;
  const auto high = corner_semitones_[2] * (1.0 - m) + corner_semitones_[3] * m;
  return low * (1.0 - blend) + high * blend;
}

trench::core::Cascade BodyDocument::viewCascade() const {
  const auto cascade = body_.interpolate_biquads(view_.morph, view_.q, 0.0F);
  if (view_.semitones == 0.0 && atCorner()) return cascade;
  return trench::core::unity_dc(trench::core::transpose_cascade(
      cascade, trench::core::ratio_of_semitones(view_.semitones), sample_rate_hz_));
}

std::vector<double> BodyDocument::viewResponseDb() const {
  if (view_.semitones != 0.0) {
    const auto cascade = viewCascade();
    std::vector<double> out;
    out.reserve(grid_.hz.size());
    for (const auto hz : grid_.hz) {
      out.push_back(trench::core::cascade_response_db(cascade, hz, sample_rate_hz_));
    }
    return out;
  }
  if (atCorner()) {
    trench::core::p2k::StoredCorner words{};
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      for (std::size_t word = 0; word < trench::core::p2k::kWordCount; ++word) {
        words[section][word] = body_.words[corner_][section][word];
      }
    }
    return trench::core::p2k::corner_response_db(words, grid_);
  }
  const auto bytes = body_.legacy_bytes();
  return trench::core::p2k::morph_response_db(bytes, view_.morph, view_.q, grid_);
}

double BodyDocument::viewPowerDb() const {
  const auto response = viewResponseDb();
  if (response.size() != grid_.weight.size() || grid_.weight_sum <= 0.0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  double power = 0.0;
  for (std::size_t index = 0; index < response.size(); ++index) {
    power += grid_.weight[index] * std::pow(10.0, response[index] / 10.0);
  }
  return 10.0 * std::log10(std::max(power / grid_.weight_sum, 1.0e-30));
}

double BodyDocument::targetScoreDb() const {
  if (!target_) return std::numeric_limits<double>::quiet_NaN();
  const auto model = viewResponseDb();
  std::vector<double> scratch(model.size(), 0.0);
  return std::sqrt(grid_.residual_var(*target_, model, scratch));
}

BodyDocument::CornerSnapshot BodyDocument::cornerSnapshot() const {
  CornerSnapshot out{};
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    out[section] = body_.words[corner_][section];
  }
  return out;
}

BodyDocument::CornerSnapshot BodyDocument::cornerSnapshot(std::size_t corner) const {
  CornerSnapshot out{};
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    out[section] = body_.words[corner][section];
  }
  return out;
}

std::size_t BodyDocument::seedSource() const {
  const auto is_identity = [this](std::size_t corner) {
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      if (body_.words[corner][section] != trench::core::kIdentitySection) return false;
    }
    return true;
  };
  if (!is_identity(corner_)) return corner_;
  for (const std::size_t candidate : {corner_ ^ 1U, corner_ ^ 2U, std::size_t{0}}) {
    if (!is_identity(candidate)) return candidate;
  }
  return corner_;
}

bool BodyDocument::seedIsInherited() const { return seedSource() != corner_; }

trench::core::p2k::CornerWords BodyDocument::seedWords() const {
  const std::size_t source = seedSource();
  trench::core::p2k::CornerWords out{};
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    for (std::size_t word = 0; word < out[section].size(); ++word) {
      out[section][word] = body_.words[source][section][word];
    }
  }
  return out;
}

void BodyDocument::applyCorner(const CornerSnapshot& words) {
  applyCorner(corner_, words);
}

void BodyDocument::applyCorner(std::size_t corner, const CornerSnapshot& words) {
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    write_section(body_, corner, section, words[section]);
  }
  emit bodyChanged();
}

void BodyDocument::applyFitStep(std::size_t corner,
                                const trench::core::p2k::CornerWords& words) {
  auto rows = cornerSnapshot(corner);
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    for (std::size_t word = 0; word < words[section].size(); ++word) {
      rows[section][word] = words[section][word];
    }
  }
  trench::core::p2k::write_dc_unity_scales(rows);
  applyCorner(corner, rows);
}

void BodyDocument::applyCharacter(double amount) {
  for (std::size_t corner = 0; corner < 2; ++corner) {
    CornerSnapshot narrowed{};
    for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
      narrowed[section] = trench::core::p2k::section_narrowed_toward_ceiling(
          body_.words[corner][section], amount, sample_rate_hz_);
    }
    applyCorner(corner + 2, narrowed);
  }
}

void BodyDocument::commitCharacter(const CornerSnapshot& before_low,
                                   const CornerSnapshot& before_high) {
  const auto after_low = cornerSnapshot(2);
  const auto after_high = cornerSnapshot(3);
  if (after_low == before_low && after_high == before_high) return;
  undo_stack_.beginMacro(QString());
  if (after_low != before_low) {
    undo_stack_.push(new CornerEditCommand(this, 2, before_low, after_low));
  }
  if (after_high != before_high) {
    undo_stack_.push(new CornerEditCommand(this, 3, before_high, after_high));
  }
  undo_stack_.endMacro();
}

void BodyDocument::commitFit(std::size_t corner, const CornerSnapshot& before) {
  CornerSnapshot after{};
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    after[section] = body_.words[corner][section];
  }
  if (after == before) return;
  undo_stack_.push(new CornerEditCommand(this, corner, before, after));
}
