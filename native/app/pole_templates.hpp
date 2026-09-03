#pragma once

#include <QString>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class EditorState;

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

[[nodiscard]] std::vector<PoleTemplate> loadPoleTemplates();
[[nodiscard]] std::vector<PoleState> pickPoles(const PoleTemplate& tpl, std::size_t count);
void applyPoleTemplate(EditorState& state, const PoleTemplate& tpl, std::size_t corner);
[[nodiscard]] std::vector<PoleTemplate> loadFrames();
void applyFrame(EditorState& state, const PoleTemplate& frame, std::size_t corner);

}
