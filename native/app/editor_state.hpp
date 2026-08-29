#pragma once

#include "trench/audio/audition.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/packed_body.hpp"

#include <QObject>

#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace trench::app {
struct TemplateEntry;
}

class EditorState final : public QObject {
  Q_OBJECT

 public:
  enum class Lane { kPole, kZero };

  struct CornerState {
    trench::core::native::Corner corner{};
    std::array<bool, trench::core::native::kSections> enabled{};
    std::array<bool, trench::core::native::kSections> zero_present{};
    bool operator==(const CornerState&) const = default;
  };

  struct Document {
    std::array<CornerState, trench::core::native::kCorners> corners{};
    std::size_t editing_corner{};
    double morph{};
    double q{};
    bool operator==(const Document&) const = default;
  };

  static constexpr double kDatumHz = 44'100.0;
  static constexpr double kLowHz = 20.0;
  static constexpr double kNyquistHz = kDatumHz * 0.5;
  static constexpr double kHighHz = kNyquistHz;
  static constexpr double kMinBandwidthHz = 1.0;
  static constexpr double kMaxBandwidthHz = 20'000.0;

  explicit EditorState(QObject* parent = nullptr);

  [[nodiscard]] static Document blank();

  [[nodiscard]] const trench::core::native::Corner& corner() const noexcept;
  [[nodiscard]] const trench::core::native::Section& section(std::size_t index) const;
  [[nodiscard]] const trench::core::native::Section& sectionAt(
      std::size_t corner, std::size_t index) const;
  [[nodiscard]] bool sectionEnabledAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] bool zeroPresentAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] Document document() const;
  [[nodiscard]] trench::core::native::Body body() const;
  [[nodiscard]] const trench::core::PackedBody& packed() const noexcept;
  [[nodiscard]] trench::audio::AuditionView view() const;
  [[nodiscard]] trench::core::Cascade cascade(double sample_rate_hz = kDatumHz) const;
  [[nodiscard]] trench::core::Biquad sectionBiquad(std::size_t index) const;
  [[nodiscard]] bool sectionEnabled(std::size_t index) const;
  [[nodiscard]] bool rootPresent(std::size_t index, Lane lane) const;
  [[nodiscard]] std::size_t selectedSection() const noexcept;
  [[nodiscard]] Lane selectedLane() const noexcept;
  [[nodiscard]] std::size_t editingCorner() const noexcept;
  [[nodiscard]] double morphPos() const noexcept;
  [[nodiscard]] double qPos() const noexcept;

  void setDocument(const Document& document);
  void undo();
  void redo();
  [[nodiscard]] bool canUndo() const noexcept;
  [[nodiscard]] bool canRedo() const noexcept;
  void beginUndoGroup();
  void endUndoGroup();
  void selectSection(std::size_t index);
  void selectRoot(std::size_t index, Lane lane);
  void setEditingCorner(std::size_t index);
  void setPadPosition(double morph01, double q01);
  void toggleSection(std::size_t index);
  struct ZeroHabits {
    std::size_t skirts{};
    std::size_t trims{};
  };
  ZeroHabits applyZeroHabits();
  void addZeroAt(double hz, double bw_hz);
  void removeZero();
  void loadTemplate(const trench::app::TemplateEntry& entry);
  void loadPoles(const std::vector<std::pair<double, double>>& poles,
                 const std::vector<std::optional<std::pair<double, double>>>& zeros = {});
  void setRoot(std::size_t section, Lane lane, double frequency_hz,
               double bandwidth_hz);
  void applyAffine(double semitones, double tract, double character,
                   double exaggerate);

 signals:
  void changed();
  void selectionChanged(std::size_t section);

 private:
  [[nodiscard]] CornerState& editing() noexcept;
  [[nodiscard]] const CornerState& editing() const noexcept;
  void render();
  void commit();
  void remember();
  void restore(const Document& document);

  std::vector<Document> undo_;
  std::vector<Document> redo_;
  int group_depth_{};
  bool group_recorded_{};
  std::array<CornerState, trench::core::native::kCorners> corners_{};
  std::size_t editing_corner_{};
  double morph_pos_{};
  double q_pos_{};
  std::size_t selected_section_{};
  Lane selected_lane_{Lane::kPole};
  trench::core::PackedBody packed_{};
};
