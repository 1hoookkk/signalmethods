#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ws
{
inline juce::Font sans (float h) { return juce::Font (juce::FontOptions ("Arial", h, juce::Font::plain)); }
inline juce::Font mono (float h) { return juce::Font (juce::FontOptions ("Lucida Console", h, juce::Font::plain)); }
inline juce::Font faceOf (bool monospace, float h) { return monospace ? mono (h) : sans (h); }
inline const juce::Colour kGround (0xffcccccc), kPanel (0xffffffff), kFrame (0xff000000);
inline const juce::Colour kLine (0xffd6d6d6), kRule (0xffb0b0b0), kText (0xff000000), kDim (0xff5c5c5c);
inline const juce::Colour kKey (0xffe2e2e2), kKeyLine (0xff6e6e6e), kKeyOn (0xff2b2b2b), kKeyText (0xff000000), kKeyOnText (0xffffffff);
inline const juce::Colour kLive (0xffc48f00), kChosen (0xffd95319), kData (0xff0072bd);
inline juce::Rectangle<int> px (juce::Rectangle<float> r) { return r.toNearestInt(); }
inline const char* const kCornerNames[4] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };
}
