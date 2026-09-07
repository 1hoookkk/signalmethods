#include "Stage.h"
#include "Look.h"
#include <cmath>

namespace hs
{
namespace
{
juce::String hex (std::uint16_t w) { return juce::String::toHexString ((int) w).paddedLeft ('0', 4).toUpperCase(); }
}

Stage::Stage (Session& s, const plot::Curves& c) : session (s), curves (c) {}

void Stage::layout (juce::Rectangle<int> r)
{
    area = r;
    const int plotWidth = area.getWidth() - 48, plotHeight = area.getHeight() - 60;
    const int unit = std::max (1, std::min (plotWidth / 3, plotHeight / 2));
    magnitude = { area.getX() + 40, area.getY() + 30, 3 * unit, 2 * unit };
    carveKey = { area.getRight() - 96, area.getY(), 96, 22 };
    modeKey = { carveKey.getX() - 70, carveKey.getY(), 60, carveKey.getHeight() };
}

Words Stage::words() const { return session.editable() ? session.editWords() : session.words; }

Words Stage::solved (const Words& words, int row, bool zero, double hz, double targetDb)
{
    Section s = sectionOf (words[(size_t) row]);
    if (zero) { s.zero = true; s.zeroHz = hz; } else { s.pole = true; s.poleHz = hz; }
    auto with = [&] (double radius) {
        Words w = words;
        if (zero) s.zeroRadius = radius; else s.poleRadius = radius;
        w[(size_t) row] = sectionWords (s, words[(size_t) row][4]);
        unityDc (w);
        return w;
    };
    if (zero && targetDb <= -29.9) return with (1.0);
    double lo = 0.05, hi = zero ? 1.0 : 0.99999;
    for (int i = 0; i < 40; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        const bool below = responseDb (with (mid), { hz })[0] < targetDb;
        if (zero == below) hi = mid; else lo = mid;
    }
    return with (0.5 * (lo + hi));
}

Section Stage::seed (double hz)
{
    Section s;
    s.pole = true; s.poleHz = hz; s.poleRadius = radiusForWidth (hz, 2.0);
    s.zero = true; s.zeroHz = hz; s.zeroRadius = radiusForWidth (hz, 32.0);
    return s;
}

juce::Point<float> Stage::peakPoint (int row) const
{
    const auto w = words();
    const double frequency = sectionOf (w[(size_t) row]).poleHz;
    const double db = responseDb (w, { frequency })[0];
    return { (float) plot::xOf (frequency, magnitude), (float) std::clamp (plot::yOf (db, magnitude), (double) magnitude.getY(), (double) magnitude.getBottom()) };
}

juce::Point<float> Stage::zeroPoint (int row) const
{
    const auto w = words();
    const double frequency = sectionOf (w[(size_t) row]).zeroHz;
    const double db = responseDb (w, { frequency })[0];
    return { (float) plot::xOf (frequency, magnitude), (float) std::clamp (plot::yOf (db, magnitude), (double) magnitude.getY(), (double) magnitude.getBottom()) };
}

int Stage::peakAt (juce::Point<int> p) const
{
    if (! session.editable()) return -1;
    if (zerosMode) return -1;
    if (! magnitude.expanded (8).contains (p)) return -1;
    const auto w = words();
    int best = -1;
    float distance = 10.0f;
    for (int row = 0; row < kRows; ++row)
    {
        if (! sectionOf (w[(size_t) row]).pole) continue;
        const float d = peakPoint (row).getDistanceFrom (p.toFloat());
        if (d < distance) { best = row; distance = d; }
    }
    return best;
}

int Stage::zeroAt (juce::Point<int> p) const
{
    if (! session.editable()) return -1;
    if (! zerosMode) return -1;
    if (! magnitude.expanded (8).contains (p)) return -1;
    const auto w = words();
    int best = -1;
    float distance = 10.0f;
    for (int row = 0; row < kRows; ++row)
    {
        const auto s = sectionOf (w[(size_t) row]);
        if (! s.zero || s.zeroRadius > 0.999) continue;
        const float d = zeroPoint (row).getDistanceFrom (p.toFloat());
        if (d < distance) { best = row; distance = d; }
    }
    return best;
}

float Stage::bladeX() const
{
    const auto s = sectionOf (words()[kRows - 1]);
    return s.zero && s.zeroRadius > 0.999 ? (float) plot::xOf (s.zeroHz, magnitude) : -1.0f;
}

double Stage::cascadeDb (int row) const
{
    const auto w = words();
    const auto s = sectionOf (w[(size_t) row]);
    const double frequency = s.pole ? s.poleHz : s.zero ? s.zeroHz : 0.0;
    return frequency > 0.0 ? responseDb (w, { frequency })[0] : 0.0;
}

void Stage::paint (juce::Graphics& g, int litRow, bool bladeLit, bool carving) const
{
    const int corner = session.target();
    g.setFont (Look::font (11.0f));
    g.setColour (Look::dim);
    const int star = session.editStar();
    const juce::String title = ! session.editable() ? session.playingLabel
                             : session.anchorTarget >= 0 && star >= 0 && star < (int) session.stars.size() ? session.stars[(size_t) star].name
                             : juce::String::charToString (Session::kCornerLetters[corner]) + "  " + session.cornerName (corner);
    g.drawText (title, area.withHeight (18), juce::Justification::centredLeft);
    g.setFont (Look::font (10.0f));
    {
        const juce::String carveWord = "CARVE  " + juce::String (carve * 100.0, 0);
        const int width = juce::GlyphArrangement::getStringWidthInt (Look::font (10.0f), carveWord);
        g.setColour (carving ? Look::ink : Look::dim);
        g.drawText (carveWord, carveKey, juce::Justification::centredRight);
        if (carving) Look::underline (g, carveKey.withTrimmedLeft (carveKey.getWidth() - width), Look::orange);
        const juce::String modeWord = zerosMode ? "ZEROS" : "POLES";
        const int modeWidth = juce::GlyphArrangement::getStringWidthInt (Look::font (10.0f), modeWord);
        g.setColour (Look::ink);
        g.drawText (modeWord, modeKey, juce::Justification::centredRight);
        Look::underline (g, modeKey.withTrimmedLeft (modeKey.getWidth() - modeWidth), zerosMode ? Look::orange : Look::blue);
    }
    const auto r = magnitude;
    Look::axes (g, r);
    g.setFont (Look::font (10.0f));
    for (int db = -30; db <= 30; db += 10)
    {
        const int y = (int) std::round (plot::yOf (db, r));
        if (db != -30 && db != 30) { g.setColour (db == 0 ? Look::faint : Look::grid); g.drawHorizontalLine (y, (float) r.getX() + 1, (float) r.getRight() - 1); }
        g.setColour (Look::dim);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), r.getX() - 38, y - 7, 30, 14, juce::Justification::centredRight);
    }
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        const int x = (int) std::round (plot::xOf (f, r));
        g.setColour (Look::grid); g.drawVerticalLine (x, (float) r.getY() + 1, (float) r.getBottom() - 1);
    }
    for (double f : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
    {
        const int x = (int) std::round (plot::xOf (f, r));
        g.setColour (Look::dim);
        g.drawText (f < 1000.0 ? juce::String ((int) f) : juce::String ((int) (f / 1000.0)) + "k", x - 18, r.getBottom() + 4, 36, 14, juce::Justification::centred);
    }
    const auto w = words();
    curves.draw (g, r, w, Look::blue, 2.4f, false);
    if (const float blade = bladeX(); blade >= 0.0f && session.editable())
    {
        g.setColour (bladeLit ? Look::orange : Look::orange.withAlpha (0.6f));
        g.drawLine (blade, (float) r.getY() + 1, blade, (float) r.getBottom() - 1, bladeLit ? 2.0f : 1.2f);
        juce::Path grip;
        grip.addTriangle (blade - 6.0f, (float) r.getY() + 1, blade + 6.0f, (float) r.getY() + 1, blade, (float) r.getY() + 10);
        g.fillPath (grip);
    }
    for (int row = 0; row < kRows && session.editable(); ++row)
    {
        const auto s = sectionOf (w[(size_t) row]);
        const bool lit = litRow == row;
        if (s.zero && s.zeroRadius <= 0.999)
        {
            const auto z = zeroPoint (row);
            Look::handle (g, z, zerosMode ? Look::orange : Look::orange.withAlpha (0.35f), true, lit);
            if (! s.pole) { g.setColour (Look::text); g.drawText (juce::String (row + 1), (int) z.x + 8, (int) z.y - 16, 16, 14, juce::Justification::centredLeft); }
        }
        if (s.pole)
        {
            const auto p = peakPoint (row);
            Look::handle (g, p, zerosMode ? Look::blue.withAlpha (0.35f) : Look::blue, false, lit);
            g.setColour (Look::text); g.drawText (juce::String (row + 1), (int) p.x + 8, (int) p.y - 17, 16, 14, juce::Justification::centredLeft);
        }
    }
    g.setColour (Look::rule.withAlpha (0.55f)); g.drawRect (r);
    if (! showHardware) return;
    g.setFont (Look::font (10.0f));
    for (int row = 0; row < kRows; ++row)
    {
        const auto& words5 = w[(size_t) row];
        const auto s = sectionOf (words5);
        const auto line = juce::String (row + 1) + "  " + hex (words5[0]) + " " + hex (words5[1]) + "  " + hex (words5[2]) + " " + hex (words5[3]) + "  " + hex (words5[4])
            + (s.pole ? "   " + juce::String (s.poleHz, 0) + " Hz r " + juce::String (s.poleRadius, 4) : juce::String())
            + (s.zero ? "   z " + juce::String (s.zeroHz, 0) + " Hz r " + juce::String (s.zeroRadius, 4) : juce::String());
        g.setColour (Look::text);
        g.drawText (line, r.getX() + 6, r.getBottom() - 14 * (kRows - row) - 4, r.getWidth() - 12, 14, juce::Justification::centredLeft);
    }
}
}
