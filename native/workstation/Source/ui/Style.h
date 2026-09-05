#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ws
{
inline juce::Font mono (float h) { return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::plain)); }
inline const juce::Colour kLine (0xff383838), kText (0xffc8c8c8), kDim (0xff7a7a7a), kKey (0xff161616), kKeyLine (0xff4a4a4a), kRule (0xff242424);
inline const juce::Colour kLive (0xffffe100), kChosen (0xff00e5ff), kData (0xffffffff);
inline juce::Rectangle<int> px (juce::Rectangle<float> r) { return r.toNearestInt(); }
inline const char* const kCornerNames[4] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };
}
