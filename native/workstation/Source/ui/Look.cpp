#include "Look.h"
#include <map>

namespace hs
{
namespace
{
const char* const kSvg[(int) Look::Glyph::count] = {
    "<svg viewBox='0 0 16 16'><path d='M4 6 L8 10 L12 6' fill='none' stroke='#000' stroke-width='1.4' stroke-linejoin='round'/></svg>",
    "<svg viewBox='0 0 16 16'><path d='M8 3 L8 13 M3 8 L13 8' fill='none' stroke='#000' stroke-width='1.6' stroke-linecap='round'/></svg>",
    "<svg viewBox='0 0 16 16'><path d='M3 8.5 L6.5 12 L13 4' fill='none' stroke='#000' stroke-width='1.8' stroke-linejoin='round' stroke-linecap='round'/></svg>" };

juce::Drawable* tinted (Look::Glyph which, juce::Colour colour)
{
    static std::map<std::pair<int, juce::uint32>, std::unique_ptr<juce::Drawable>> cache;
    const auto key = std::make_pair ((int) which, colour.getARGB());
    if (const auto it = cache.find (key); it != cache.end()) return it->second.get();
    std::unique_ptr<juce::Drawable> drawable = juce::Drawable::createFromSVGString (juce::String (kSvg[(int) which]));
    if (drawable == nullptr) return nullptr;
    drawable->replaceColour (juce::Colours::black, colour);
    return (cache[key] = std::move (drawable)).get();
}
}

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

bool Look::hasGlyph (Glyph which) { return (int) which >= 0 && which < Glyph::count && tinted (which, juce::Colour (kInk)) != nullptr; }

void Look::glyph (juce::Graphics& g, Glyph which, juce::Rectangle<int> r, juce::Colour colour)
{
    if (auto* d = tinted (which, colour)) d->drawWithin (g, r.toFloat(), juce::RectanglePlacement::centred, 1.0f);
}
}
