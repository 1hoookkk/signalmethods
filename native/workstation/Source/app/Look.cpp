#include "Look.h"

namespace hs
{
const juce::Colour Look::ground (kGround), Look::panel (kPanel), Look::rule (kRule), Look::grid (kGrid);
const juce::Colour Look::ink (kInk), Look::text (kText), Look::dim (kDim), Look::faint (kFaint);
const juce::Colour Look::blue (kBlue), Look::orange (kOrange), Look::yellow (kYellow), Look::purple (kPurple), Look::green (kGreen);

Look::Look()
    : juce::LookAndFeel_V4 (ColourScheme (juce::Colour (kGround), juce::Colour (kPanel), juce::Colour (kPanel), juce::Colour (kRule), juce::Colour (kText),
                                          juce::Colour (kBlue), juce::Colour (kPanel), juce::Colour (kBlue), juce::Colour (kText)))
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (kGround));
    setColour (juce::DocumentWindow::textColourId, juce::Colour (kText));
}

juce::Font Look::font (float height) { return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height, juce::Font::plain)); }

void Look::axes (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (juce::Colour (kPanel)); g.fillRect (r);
    g.setColour (juce::Colour (kRule).withAlpha (0.55f)); g.drawRect (r);
}

void Look::handle (juce::Graphics& g, juce::Point<float> p, juce::Colour colour, bool square, bool lit)
{
    const float s = lit ? 6.0f : 5.0f;
    if (square)
    {
        g.setColour (juce::Colour (kPanel)); g.fillRect (p.x - s, p.y - s, 2.0f * s, 2.0f * s);
        g.setColour (colour); g.drawRect (p.x - s, p.y - s, 2.0f * s, 2.0f * s, lit ? 2.0f : 1.4f);
        return;
    }
    g.setColour (juce::Colour (kPanel)); g.fillEllipse (p.x - s, p.y - s, 2.0f * s, 2.0f * s);
    g.setColour (colour); g.drawEllipse (p.x - s, p.y - s, 2.0f * s, 2.0f * s, lit ? 2.2f : 1.6f);
}

void Look::underline (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour colour)
{
    g.setColour (colour);
    g.fillRect (r.getX(), r.getBottom() - 2, r.getWidth(), 2);
}
}
