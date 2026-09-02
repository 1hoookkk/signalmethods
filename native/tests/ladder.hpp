#pragma once

#include "editor_state.hpp"
#include "trench/core/native_body.hpp"

#include <array>
#include <cstddef>

inline void voiceLadder(EditorState& state, std::size_t corner, double root_hz = 64.0,
                        bool with_zeros = true) {
  constexpr std::array<double, 6> kPianoHz{64.0, 129.0, 388.0, 584.0, 777.0, 1430.0};
  constexpr std::array<double, 6> kPianoQ{72.0, 36.0, 34.0, 35.0, 34.0, 17.0};
  state.setEditingCorner(corner);
  for (std::size_t index = 0; index < trench::core::native::kSections; ++index) {
    const double hz = kPianoHz[index] * root_hz / 64.0;
    const double bw_hz = hz / kPianoQ[index];
    if (!state.sectionEnabledAt(corner, index)) state.toggleSection(index);
    state.setRoot(index, EditorState::Lane::kPole, hz, bw_hz);
    if (with_zeros && index + 1 < trench::core::native::kSections) {
      state.selectSection(index);
      state.addZeroAt(hz, 4.0 * bw_hz);
    }
  }
  state.selectSection(0);
}
