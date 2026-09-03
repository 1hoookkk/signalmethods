#pragma once

#include <QString>
#include <cstddef>
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
  [[nodiscard]] QString label() const { return family + QStringLiteral("  ") + type; }
};

[[nodiscard]] std::vector<PoleTemplate> loadPoleTemplates();
[[nodiscard]] std::vector<PoleState> pickPoles(const PoleTemplate& tpl, std::size_t count);
void applyPoleTemplate(EditorState& state, const PoleTemplate& tpl, std::size_t corner);

}
