#include "Palette.h"
#include "Look.h"
#include "app/Library.h"
#include <cmath>

namespace hs
{
namespace
{
const char* const kNames[3] = { "Bodies", "Reads", "Captures" };
const char* const kKinds[3] = { "body", "read", "capture" };
}

Palette::Palette (Session& s, const plot::Curves& c) : session (s), curves (c) {}

bool Palette::onChart (const Star& s) { return s.kind == "vowel"; }

void Palette::layout (juce::Rectangle<int> r)
{
    area = r;
    const int side = std::max (80, std::min (area.getHeight() - 44, 260));
    chart = { area.getX() + 40, area.getY() + 22, side, side };
    keepKey = { chart.getRight() - 70, area.getY(), 70, 20 };
    picker = { chart.getRight() + 56, area.getY(), area.getRight() - chart.getRight() - 56, area.getHeight() };
    for (int i = 0; i < 3; ++i) tabs[(size_t) i] = { picker.getX() + i * (picker.getWidth() / 3), picker.getY(), picker.getWidth() / 3, 22 };
    dropZone = picker.withTrimmedTop (picker.getHeight() - 24);
    scroll = std::clamp (scroll, 0, std::max (0, (int) cards().size() * 34 - (picker.getHeight() - 26 - 24)));
}

std::vector<int> Palette::cards() const
{
    std::vector<int> out;
    for (int k = 0; k < (int) session.stars.size(); ++k)
        if (session.stars[(size_t) k].kind == kKinds[tab]) out.push_back (k);
    return out;
}

void Palette::scrollBy (int pixels) { scroll += pixels; layout (area); }

void Palette::showTab (int which) { tab = std::clamp (which, 0, 2); scroll = 0; layout (area); }

void Palette::followKind (const juce::String& kind)
{
    for (int i = 0; i < 3; ++i) if (kind == kKinds[i] && tab != i) showTab (i);
}

juce::Rectangle<int> Palette::card (int index) const
{
    return { picker.getX(), picker.getY() + 26 + index * 34 - scroll, picker.getWidth(), 32 };
}

int Palette::cardAt (juce::Point<int> p) const
{
    if (! picker.withTrimmedTop (26).withTrimmedBottom (24).contains (p)) return -1;
    const auto shown = cards();
    const int index = (p.y - picker.getY() - 26 + scroll) / 34;
    return index >= 0 && index < (int) shown.size() ? shown[(size_t) index] : -1;
}

int Palette::tabAt (juce::Point<int> p) const
{
    for (int i = 0; i < 3; ++i) if (tabs[(size_t) i].contains (p)) return i;
    return -1;
}

juce::Point<float> Palette::chartPoint (double f1, double f2) const
{
    const double u = std::log (std::clamp (f2, kF2Low, kF2High) / kF2Low) / std::log (kF2High / kF2Low);
    const double v = std::log (std::clamp (f1, kF1Low, kF1High) / kF1Low) / std::log (kF1High / kF1Low);
    return { (float) (chart.getRight() - u * chart.getWidth()), (float) (chart.getY() + v * chart.getHeight()) };
}

std::pair<double, double> Palette::formantsAt (juce::Point<int> p) const
{
    const double u = std::clamp ((chart.getRight() - p.x) / (double) chart.getWidth(), 0.0, 1.0);
    const double v = std::clamp ((p.y - chart.getY()) / (double) chart.getHeight(), 0.0, 1.0);
    return { kF1Low * std::pow (kF1High / kF1Low, v), kF2Low * std::pow (kF2High / kF2Low, u) };
}

int Palette::pointAt (juce::Point<int> p) const
{
    int best = -1;
    double bestD = 10.0;
    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        if (! onChart (session.stars[(size_t) k])) continue;
        const auto f = formantsOf (session.stars[(size_t) k].words);
        if (f[0] <= 0.0 || f[1] <= 0.0) continue;
        const double d = chartPoint (f[0], f[1]).getDistanceFrom (p.toFloat());
        if (d < bestD) { bestD = d; best = k; }
    }
    return best;
}

void Palette::paint (juce::Graphics& g, int transposingFrom) const
{
    g.setFont (Look::font (11.0f));
    for (int i = 0; i < 3; ++i)
    {
        const auto r = tabs[(size_t) i];
        g.setColour (tab == i ? Look::ink : Look::dim);
        g.drawText (kNames[i], r, juce::Justification::centredLeft);
        if (tab == i) Look::underline (g, r.withWidth (r.getWidth() - 12), Look::blue);
    }
    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (picker.withTrimmedTop (26).withTrimmedBottom (24));
        const auto shown = cards();
        for (int index = 0; index < (int) shown.size(); ++index)
        {
            const auto r = card (index);
            if (! r.intersects (picker)) continue;
            const int k = shown[(size_t) index];
            const auto& star = session.stars[(size_t) k];
            const bool selected = k == session.selected || k == session.auditioning;
            if (selected) { g.setColour (Look::panel); g.fillRect (r); }
            g.setColour (selected ? Look::ink : Look::text);
            g.setFont (Look::font (13.0f));
            g.drawText (star.name, r.reduced (6, 0).withTrimmedRight (92), juce::Justification::centredLeft);
            curves.draw (g, r.withTrimmedLeft (r.getWidth() - 88).reduced (3, 4), star.words, plot::inkOf (star), 1.0f);
            g.setColour (Look::grid); g.drawHorizontalLine (r.getBottom(), (float) r.getX(), (float) r.getRight());
        }
    }
    g.setFont (Look::font (10.0f));
    g.setColour (Look::faint); g.drawRect (dropZone);
    g.setColour (Look::dim);
    g.drawText ("drop .wav", dropZone, juce::Justification::centred);
    Look::glyph (g, Look::Glyph::keep, keepKey.withWidth (20).reduced (2), session.placeable() ? Look::ink : Look::faint);
    g.setColour (session.placeable() ? Look::ink : Look::faint);
    g.setFont (Look::font (11.0f));
    g.drawText ("Keep", keepKey.withTrimmedLeft (24), juce::Justification::centredLeft);
    Look::axes (g, chart);
    g.setFont (Look::font (10.0f));
    for (double f : { 3000.0, 2000.0, 1500.0, 1000.0, 700.0 })
    {
        const int x = (int) std::round (chartPoint (kF1Low, f).x);
        g.setColour (Look::grid); g.fillRect (x, chart.getY() + 1, 1, chart.getHeight() - 2);
        g.setColour (Look::dim); g.drawText (juce::String ((int) f), x - 20, chart.getBottom() + 4, 40, 12, juce::Justification::centred);
    }
    for (double f : { 300.0, 400.0, 500.0, 700.0, 1000.0 })
    {
        const int y = (int) std::round (chartPoint (f, kF2Low).y);
        g.setColour (Look::grid); g.fillRect (chart.getX() + 1, y, chart.getWidth() - 2, 1);
        g.setColour (Look::dim); g.drawText (juce::String ((int) f), chart.getX() - 38, y - 6, 34, 12, juce::Justification::centredRight);
    }
    const auto origin = chartPoint (kSchwaF1, kSchwaF2);
    g.setColour (Look::faint);
    g.drawLine (origin.x, (float) chart.getY(), origin.x, (float) chart.getBottom(), 1.0f);
    g.drawLine ((float) chart.getX(), origin.y, (float) chart.getRight(), origin.y, 1.0f);
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k < (int) session.stars.size(); ++k)
        {
            const auto& s = session.stars[(size_t) k];
            if (! onChart (s)) continue;
            const bool landmark = s.body == "Klatt 1980" || s.body == "neutral";
            if (landmark != (pass == 1)) continue;
            const auto f = formantsOf (s.words);
            if (f[0] <= 0.0 || f[1] <= 0.0) continue;
            const auto p = chartPoint (f[0], f[1]);
            const bool lit = k == session.selected || k == session.hovered || k == session.auditioning;
            bool pinned = false;
            for (int pin : session.quad.pins) pinned = pinned || pin == k;
            const auto ink = plot::inkOf (s);
            if (! landmark && ! lit && ! pinned)
            {
                g.setColour (ink.withAlpha (0.3f));
                g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
                continue;
            }
            g.setColour (lit ? ink : ink.withAlpha (pinned ? 0.9f : 0.6f));
            if (pinned) g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
            else g.drawEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f, lit ? 2.0f : 1.0f);
            g.setColour (lit ? ink : Look::text.withAlpha (0.8f));
            g.setFont (Look::font (landmark ? 12.0f : 10.0f));
            g.drawText (s.name, (int) p.x + 7, (int) p.y - 7, 100, 14, juce::Justification::centredLeft);
        }
    if (session.madeLive)
    {
        const auto f = formantsOf (session.made.words);
        const auto p = chartPoint (f[0], f[1]);
        if (transposingFrom >= 0 && transposingFrom < (int) session.stars.size())
        {
            const auto source = formantsOf (session.stars[(size_t) transposingFrom].words);
            const auto from = chartPoint (source[0], source[1]);
            g.setColour (Look::dim); g.drawLine (from.x, from.y, p.x, p.y, 1.0f);
        }
        g.setColour (session.inMade() ? Look::orange : Look::dim);
        g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
        g.drawLine (p.x - 9.0f, p.y, p.x + 9.0f, p.y, 1.0f);
        g.drawLine (p.x, p.y - 9.0f, p.x, p.y + 9.0f, 1.0f);
    }
}
}
