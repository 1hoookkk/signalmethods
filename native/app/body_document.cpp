#include "body_document.hpp"

#include "trench/core/p2k.hpp"

#include <QUndoCommand>

#include <utility>

namespace {

void write_section(trench::core::PackedBody& body, std::size_t section,
                   const trench::core::PackedSection& words) {
  body.words[0][section] = words;
  body.words[4][section] = words;
}

class CornerEditCommand final : public QUndoCommand {
 public:
  CornerEditCommand(BodyDocument* document, BodyDocument::CornerSnapshot before,
                    BodyDocument::CornerSnapshot after)
      : document_(document), before_(before), after_(after) {}

  void redo() override { document_->applyCorner(after_); }
  void undo() override { document_->applyCorner(before_); }

 private:
  BodyDocument* document_;
  BodyDocument::CornerSnapshot before_;
  BodyDocument::CornerSnapshot after_;
};

class SectionEditCommand final : public QUndoCommand {
 public:
  SectionEditCommand(BodyDocument* document, std::size_t section,
                     trench::core::PackedSection before,
                     trench::core::PackedSection after)
      : document_(document), section_(section), before_(before), after_(after) {}

  void redo() override { document_->applySection(section_, after_); }
  void undo() override { document_->applySection(section_, before_); }

 private:
  BodyDocument* document_;
  std::size_t section_;
  trench::core::PackedSection before_;
  trench::core::PackedSection after_;
};

}  // namespace

BodyDocument::BodyDocument(trench::core::PackedBody body, double sample_rate_hz,
                           QObject* parent)
    : QObject(parent),
      body_(std::move(body)),
      sample_rate_hz_(sample_rate_hz),
      freedom_mask_(trench::core::p2k::kAllFree),
      undo_stack_(this) {}

const trench::core::PackedBody& BodyDocument::body() const noexcept { return body_; }

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
  write_section(body_, section, words);
  emit bodyChanged();
}

void BodyDocument::commitGesture(std::size_t section,
                                 const trench::core::PackedSection& before) {
  if (body_.words[0][section] == before) return;
  undo_stack_.push(
      new SectionEditCommand(this, section, before, body_.words[0][section]));
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

BodyDocument::CornerSnapshot BodyDocument::cornerSnapshot() const {
  CornerSnapshot out{};
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    out[section] = body_.words[0][section];
  }
  return out;
}

trench::core::p2k::CornerWords BodyDocument::seedWords() const {
  trench::core::p2k::CornerWords out{};
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    for (std::size_t word = 0; word < out[section].size(); ++word) {
      out[section][word] = body_.words[0][section][word];
    }
  }
  return out;
}

void BodyDocument::applyCorner(const CornerSnapshot& words) {
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    write_section(body_, section, words[section]);
  }
  emit bodyChanged();
}

void BodyDocument::applyFitStep(const trench::core::p2k::CornerWords& words) {
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    auto packed = body_.words[0][section];
    for (std::size_t word = 0; word < words[section].size(); ++word) {
      packed[word] = words[section][word];
    }
    write_section(body_, section, packed);
  }
  emit bodyChanged();
}

void BodyDocument::applyFitResult(const trench::core::p2k::StoredCorner& words) {
  for (std::size_t section = 0; section < trench::core::kLegacySectionCount; ++section) {
    trench::core::PackedSection packed{};
    for (std::size_t word = 0; word < packed.size(); ++word) {
      packed[word] = words[section][word];
    }
    write_section(body_, section, packed);
  }
  emit bodyChanged();
}

void BodyDocument::commitFit(const CornerSnapshot& before) {
  const auto after = cornerSnapshot();
  if (after == before) return;
  undo_stack_.push(new CornerEditCommand(this, before, after));
}
