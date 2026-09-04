#pragma once

#include "editor_state.hpp"

#include <QString>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace trench::app {

struct PoleState {
  double hz{};
  double bw_hz{};
  double r{};
  int bodies{};
};

struct PoleTemplate {
  QString family;
  QString type;
  double datum_hz{};
  std::vector<QString> bodies;
  std::vector<PoleState> states;
  bool ladder = false;
  double root_hz = 0.0;
  std::vector<double> ratios;
  std::vector<double> bw_fraction;
  std::vector<double> zero_bw_ratio;
  bool frame = false;
  std::array<std::array<std::uint16_t, 5>, 6> words{};
  [[nodiscard]] QString label() const { return family + QStringLiteral("  ") + type; }
};

enum class RowType { kEq, kLowPass, kHighPass, kPole };

struct TypeRow {
  RowType type{RowType::kEq};
  double hz{};
  double bw_hz{};
  double gain_db{};
};

struct Keyframe {
  QString group;
  QString name;
  QString source;
  QString mode;
  std::vector<TypeRow> rows;
};

inline constexpr double kTypeRowSeedGainDb = 12.0;
inline constexpr std::size_t kTypeRowMinStates = 3;

[[nodiscard]] std::vector<PoleTemplate> loadPoleTemplates();
[[nodiscard]] std::vector<PoleState> pickPoles(const PoleTemplate& tpl, std::size_t count);
[[nodiscard]] std::vector<TypeRow> typeRowsFor(const PoleTemplate& tpl);
void applyTypeRows(EditorState& state, const std::vector<TypeRow>& rows, std::size_t corner,
                   bool anchor = true);
void applyPoleTemplate(EditorState& state, const PoleTemplate& tpl, std::size_t corner,
                       bool anchor = true);
[[nodiscard]] std::vector<PoleTemplate> loadFrames();
[[nodiscard]] EditorState::CornerState frameCornerState(const PoleTemplate& frame);
void applyFrame(EditorState& state, const PoleTemplate& frame, std::size_t corner,
                bool anchor = true);
[[nodiscard]] std::vector<Keyframe> loadKeyframes();
void applyKeyframe(EditorState& state, const Keyframe& key, std::size_t corner,
                   bool anchor = true);
[[nodiscard]] PoleTemplate keyframeFrame(const Keyframe& key);
[[nodiscard]] std::array<std::uint16_t, 5> keyframeSlotWords(const Keyframe& key,
                                                             std::size_t slot);
[[nodiscard]] const std::vector<double>& responseGridHz();
[[nodiscard]] std::vector<double> responseDb(const PoleTemplate& tpl,
                                             const std::vector<double>& hz);
[[nodiscard]] std::vector<double> responseDb(const Keyframe& key,
                                             const std::vector<double>& hz);

}
