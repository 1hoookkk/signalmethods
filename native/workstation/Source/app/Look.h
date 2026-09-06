#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace hs
{
struct Look : public juce::LookAndFeel_V4
{
    Look();
    static constexpr juce::uint32 kGround = 0xfff0f0f0, kPanel = 0xffffffff, kRule = 0xff262626, kGrid = 0xffe3e3e3;
    static constexpr juce::uint32 kInk = 0xff141414, kText = 0xff262626, kDim = 0xff6f6f6f, kFaint = 0xffb8b8b8;
    static constexpr juce::uint32 kBlue = 0xff0072bd, kOrange = 0xffd95319, kYellow = 0xffedb120, kPurple = 0xff7e2f8e, kGreen = 0xff77ac30;
    static const juce::Colour ground, panel, rule, grid, ink, text, dim, faint, blue, orange, yellow, purple, green;
    static juce::Font font (float height);
    static void axes (juce::Graphics& g, juce::Rectangle<int> r);
    static void handle (juce::Graphics& g, juce::Point<float> p, juce::Colour colour, bool square, bool lit);
    static void underline (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour colour);
};
}
