#pragma once

#include "trench/audio/audition.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/packed_body.hpp"

#include <QObject>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class EditorState final : public QObject {
  Q_OBJECT

 public:
  enum class Lane { kPole, kZero };

  struct CornerState {
    trench::core::native::Corner corner{};
    std::array<bool, trench::core::native::kSections> enabled{};
    std::array<bool, trench::core::native::kSections> zero_present{};
    std::array<int, trench::core::native::kSections> cut{};
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
  static constexpr int kMaxCut = 3;

  explicit EditorState(QObject* parent = nullptr);

  [[nodiscard]] static Document blank();
  [[nodiscard]] static Document documentFrom(const trench::core::native::Body& body);

  [[nodiscard]] const trench::core::native::Corner& corner() const noexcept;
  [[nodiscard]] const trench::core::native::Section& section(std::size_t index) const;
  [[nodiscard]] const trench::core::native::Section& sectionAt(
      std::size_t corner, std::size_t index) const;
  [[nodiscard]] bool sectionEnabledAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] bool zeroPresentAt(std::size_t corner, std::size_t index) const;
  [[nodiscard]] Document document() const;
  [[nodiscard]] trench::core::native::Body body() const;
  [[nodiscard]] const trench::core::PackedBody& packed() const noexcept;
  [[nodiscard]] const std::optional<trench::core::PackedBody>& sourceWords() const noexcept;
  [[nodiscard]] trench::audio::AuditionView view() const;
  [[nodiscard]] trench::audio::AuditionView soloView(std::size_t section,
                                                     bool pole_only = false) const;
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
  void setSourceWords(const trench::core::PackedBody& words);
  void clearSourceWords();
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
  void toggleSectionAt(std::size_t corner, std::size_t index);
  void addZeroAt(double hz, double bw_hz);
  void removeZero();
  void setRoot(std::size_t section, Lane lane, double frequency_hz,
               double bandwidth_hz);
  void setRootAt(std::size_t corner, std::size_t section, Lane lane, double frequency_hz,
                 double bandwidth_hz);
  void setZeroAt(std::size_t corner, std::size_t section, double frequency_hz,
                 double bandwidth_hz);
  void setRealRootAt(std::size_t corner, std::size_t index, Lane lane, double a_hz,
                     double b_hz);
  void setWordsAt(std::size_t corner, std::size_t section, Lane lane, std::uint16_t mag_word,
                  std::uint16_t rsq_word);
  void removeZeroAt(std::size_t corner, std::size_t section);
  void setCutAt(std::size_t corner, std::size_t section, int cut);
  [[nodiscard]] int cutAt(std::size_t corner, std::size_t section) const;
  void applyAffine(double semitones, double tract, double character,
                   double exaggerate);
  void copyCornerFrom(std::size_t source);
  void copyCornerTo(std::size_t target);
  void sharpenPoles(double radius_step);

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
  std::optional<trench::core::PackedBody> source_words_{};
};
