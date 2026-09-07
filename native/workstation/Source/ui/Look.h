#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace hs
{
struct Look : public juce::LookAndFeel_V4
{
    Look();
    static constexpr juce::uint32 kGround = 0xff161718, kPanel = 0xff1f2124, kRule = 0xffa8abb0, kGrid = 0xff2e3136;
    static constexpr juce::uint32 kInk = 0xffeceef0, kText = 0xffd2d5d9, kDim = 0xff8a8e94, kFaint = 0xff45494f;
    static constexpr juce::uint32 kBlue = 0xff4fa3e6, kOrange = 0xfff08a45, kYellow = 0xfff0c24a, kPurple = 0xffb98ad6, kGreen = 0xff96cf5a;
    static const juce::Colour ground, panel, rule, grid, ink, text, dim, faint, blue, orange, yellow, purple, green;
    static juce::Font font (float height);
    static void axes (juce::Graphics& g, juce::Rectangle<int> r);
    static void handle (juce::Graphics& g, juce::Point<float> p, juce::Colour colour, bool square, bool lit);
    static void underline (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour colour);
    enum class Glyph { play, saw, noise, loop, write, caret, arrow, keep, use, count };
    static bool hasGlyph (Glyph which);
    static void glyph (juce::Graphics& g, Glyph which, juce::Rectangle<int> r, juce::Colour colour);
};
}
