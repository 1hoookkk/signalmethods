#include "Screen.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
const juce::Colour kGround (0xffcccccc), kPaper (0xffffffff), kInk (0xff1d2126), kFrame (0xff6e6e6e);
const juce::Colour kBlue (0xff0072bd), kOrange (0xffd95319), kGold (0xffc48f00), kKey (0xffe2e2e2), kKeyOn (0xff2b2b2b), kGrid (0xffd6d6d6), kZero (0xffb0b0b0);

double xOf (double hzValue, juce::Rectangle<int> r)
{
    return r.getX() + r.getWidth() * std::log (hzValue / 20.0) / std::log (1000.0);
}

double yOf (double db, juce::Rectangle<int> r)
{
    return r.getBottom() - r.getHeight() * (db + 30.0) / 60.0;
}
}

Screen::Screen (Session& s) : session (s), hz (curveHz())
{
    setSize (1480, 860);
    setWantsKeyboardFocus (true);
    layout();
    session.onChange = [this] { repaint(); };
}

void Screen::layout()
{
    header = { 0, 0, 1480, 48 };
    playKey = { 880, 10, 90, 30 }; sawKey = { 980, 10, 70, 30 }; noiseKey = { 1060, 10, 150, 30 }; writeKey = { 1240, 10, 222, 30 };
    navigator = { 18, 58, 1444, 60 };
    squares = { 18, 130, 1100, 428 };
    square[0][0] = { 78, 156, 300, 190 }; square[1][0] = { 78, 356, 300, 190 };
    square[0][1] = { 718, 156, 300, 190 }; square[1][1] = { 718, 356, 300, 190 };
    pad = { 450, 190, 220, 320 };
    live = { 18, 570, 1100, 236 };
    libraryList = { 1140, 130, 322, 428 };
    columnList = { 1140, 570, 322, 236 };
    statusLine = { 18, 816, 1444, 28 };
}

juce::Rectangle<int> Screen::plotArea (juce::Rectangle<int> r) const { return r; }

void Screen::paintGrid (juce::Graphics& g, juce::Rectangle<int> r, bool) const
{
    g.setColour (kPaper); g.fillRect (r);
    g.setColour (kGrid);
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) std::round (xOf (f, r)), (float) r.getY(), (float) r.getBottom());
    g.setColour (kZero); g.drawHorizontalLine ((int) std::round (yOf (0.0, r)), (float) r.getX(), (float) r.getRight());
    g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (9.0f)));
    for (double f : { 100.0, 1000.0, 10000.0 })
        g.drawText (f < 1000 ? "100" : f < 10000 ? "1k" : "10k", (int) xOf (f, r) - 14, r.getBottom() + 2, 28, 12, juce::Justification::centred);
    g.drawText ("+30", r.getX() - 30, r.getY() - 6, 26, 12, juce::Justification::centredRight);
    g.drawText ("0", r.getX() - 30, (int) yOf (0.0, r) - 6, 26, 12, juce::Justification::centredRight);
    g.drawText ("-30", r.getX() - 30, r.getBottom() - 6, 26, 12, juce::Justification::centredRight);
}

void Screen::paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour) const
{
    const auto db = responseDb (words, hz);
    juce::Path p;
    for (size_t i = 0; i < hz.size(); ++i)
    {
        const float x = (float) xOf (hz[i], r), y = (float) std::clamp (yOf (db[i], r), (double) r.getY(), (double) r.getBottom());
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.4f));
}

void Screen::paintKey (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool enabled) const
{
    g.setColour (on ? kKeyOn : kKey); g.fillRect (r);
    g.setColour (kFrame); g.drawRect (r);
    g.setColour (on ? juce::Colours::white : (enabled ? kInk : kFrame));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (text, r, juce::Justification::centred);
}

void Screen::paintList (juce::Graphics& g, juce::Rectangle<int> r, const juce::StringArray& rows, int selected, int scroll, bool focused) const
{
    g.setColour (kPaper); g.fillRect (r);
    g.setColour (focused ? kOrange : kFrame); g.drawRect (r, focused ? 2 : 1);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    const int visible = (r.getHeight() - 8) / kRow;
    for (int i = 0; i < visible; ++i)
    {
        const int row = scroll + i;
        if (row >= rows.size()) break;
        juce::Rectangle<int> line (r.getX() + 4, r.getY() + 4 + i * kRow, r.getWidth() - 8, kRow);
        if (row == selected) { g.setColour (kBlue); g.fillRect (line); g.setColour (juce::Colours::white); }
        else g.setColour (kInk);
        g.drawText (rows[row], line.reduced (4, 0), juce::Justification::centredLeft);
    }
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (kGround);
    const auto& s = session.strip;
    const int n = s.count();
    g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawText ("HEADSPACE", 18, 10, 300, 30, juce::Justification::centredLeft);
    paintKey (g, playKey, "PLAY", session.playing, true);
    paintKey (g, sawKey, "SAW", session.source == 0, true);
    paintKey (g, noiseKey, "PINK NOISE", session.source == 1, true);
    paintKey (g, writeKey, "WRITE BODY FILE", false, n >= 1);

    g.setColour (kPaper); g.fillRect (navigator); g.setColour (kFrame); g.drawRect (navigator);
    {
        const auto bar = navigator.reduced (24, 10);
        const int slots = std::max (n, 2);
        auto navX = [&] (double column) { return bar.getX() + (column - 1.0) / (slots - 1.0) * bar.getWidth(); };
        g.setColour (kBlue);
        for (int k = 1; k <= n; ++k) g.fillRect ((int) navX (k) - 1, bar.getY(), 3, bar.getHeight());
        if (n > 0)
        {
            const double x = n == 1 ? 1.0 : s.square + s.morph / 100.0;
            const double y = bar.getBottom() - s.q / 100.0 * bar.getHeight();
            g.setColour (kGold); g.fillRect ((int) navX (x) - 5, (int) y - 5, 10, 10);
        }
    }

    g.setColour (kPaper); g.fillRect (squares); g.setColour (kFrame); g.drawRect (squares);
    const int k = std::clamp (s.square, 1, s.squares());
    const int cols[2] = { k, std::min (k + 1, n) };
    g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("Q1", 30, square[0][0].getY() + 80, 40, 14, juce::Justification::centredLeft);
    g.drawText ("Q0", 30, square[1][0].getY() + 80, 40, 14, juce::Justification::centredLeft);
    for (int col = 0; col < 2; ++col)
    {
        for (int row = 0; row < 2; ++row)
        {
            const auto r = square[(size_t) row][(size_t) col];
            paintGrid (g, r, false);
            if (n > 0)
            {
                const auto& c = s.columns[(size_t) cols[col] - 1];
                paintCurve (g, r, row == 0 ? c.q1 : c.q0, kBlue);
            }
            const bool chosen = n > 0 && s.selected == cols[col];
            g.setColour (chosen ? kOrange : kInk); g.drawRect (r, chosen ? 2 : 1);
        }
        if (n > 0)
        {
            g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (s.columns[(size_t) cols[col] - 1].name, square[0][(size_t) col].getX(), squares.getY() + 6, 340, 16, juce::Justification::centredLeft);
        }
    }
    g.setColour (kPaper); g.fillRect (pad); g.setColour (kInk); g.drawRect (pad);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("MORPH", pad.getX(), pad.getBottom() + 4, pad.getWidth(), 12, juce::Justification::centred);
    g.drawText ("Q", pad.getX() - 16, pad.getCentreY() - 6, 12, 12, juce::Justification::centred);
    if (n > 0)
    {
        const float px = (float) (pad.getX() + s.morph / 100.0 * pad.getWidth()), py = (float) (pad.getBottom() - s.q / 100.0 * pad.getHeight());
        g.setColour (kGold); g.fillRect (px - 6.0f, py - 6.0f, 12.0f, 12.0f);
    }

    g.setColour (kPaper); g.fillRect (live); g.setColour (kFrame); g.drawRect (live);
    const auto liveGrid = live.reduced (60, 18);
    paintGrid (g, liveGrid, true);
    if (session.sounding) paintCurve (g, liveGrid, session.words, kBlue);

    juce::StringArray libraryRows;
    for (const auto& e : session.library) libraryRows.add (e.name);
    paintList (g, libraryList, libraryRows, session.librarySelected, libraryScroll, session.focus == Focus::library);
    juce::StringArray columnRows;
    for (int j = 0; j < n; ++j)
    {
        const auto& c = s.columns[(size_t) j];
        juce::String from = c.origin.body + " " + c.origin.corner;
        if (c.origin.kind == "capture") from = c.origin.parentA + " -> " + c.origin.parentB + " at " + juce::String (c.origin.morph, 1);
        columnRows.add (juce::String (j + 1) + "  " + c.name + "   " + from);
    }
    paintList (g, columnList, columnRows, s.selected - 1, columnScroll, session.focus == Focus::columns);

    g.setColour (kInk); g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (session.status, statusLine, juce::Justification::centredLeft);
}

void Screen::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    grabKeyboardFocus();
    if (playKey.contains (p)) { session.setPlaying (! session.playing); return; }
    if (sawKey.contains (p)) { session.setSource (0); return; }
    if (noiseKey.contains (p)) { session.setSource (1); return; }
    if (writeKey.contains (p)) { session.write(); return; }
    if (pad.contains (p)) { dragging = true; mouseDrag (e); return; }
    if (navigator.contains (p) && session.strip.count() > 0)
    {
        const auto bar = navigator.reduced (24, 10);
        const int n = session.strip.count(), slots = std::max (n, 2);
        const double x = 1.0 + (p.x - bar.getX()) / (double) bar.getWidth() * (slots - 1.0);
        const double xc = std::clamp (x, 1.0, (double) n);
        const int k = std::clamp ((int) std::floor (xc), 1, session.strip.squares());
        const double q = std::clamp ((bar.getBottom() - p.y) / (double) bar.getHeight(), 0.0, 1.0) * 100.0;
        session.strip.selected = (int) std::round (xc);
        session.setPosition (k, (xc - k) * 100.0, q);
        return;
    }
    if (libraryList.contains (p))
    {
        const int row = libraryScroll + (p.y - libraryList.getY() - 4) / kRow;
        if (row >= 0 && row < (int) session.library.size()) session.hear (row);
        return;
    }
    if (columnList.contains (p))
    {
        const int row = columnScroll + (p.y - columnList.getY() - 4) / kRow;
        if (row >= 0 && row < session.strip.count()) session.jump (row + 1);
        return;
    }
}

void Screen::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || session.strip.count() == 0) return;
    const auto p = e.getPosition();
    const double morph = (p.x - pad.getX()) / (double) pad.getWidth() * 100.0;
    const double q = (pad.getBottom() - p.y) / (double) pad.getHeight() * 100.0;
    session.setPosition (session.strip.square, morph, q);
}

void Screen::mouseUp (const juce::MouseEvent&) { dragging = false; }

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int lines = wheel.deltaY > 0 ? -3 : 3;
    if (libraryList.contains (e.getPosition()))
    {
        const int visible = (libraryList.getHeight() - 8) / kRow;
        libraryScroll = std::clamp (libraryScroll + lines, 0, std::max (0, (int) session.library.size() - visible));
        repaint();
    }
    else if (columnList.contains (e.getPosition()))
    {
        const int visible = (columnList.getHeight() - 8) / kRow;
        columnScroll = std::clamp (columnScroll + lines, 0, std::max (0, session.strip.count() - visible));
        repaint();
    }
}

bool Screen::keyPressed (const juce::KeyPress& k)
{
    const bool used = session.key (k);
    if (used)
    {
        const int visible = (libraryList.getHeight() - 8) / kRow;
        if (session.librarySelected < libraryScroll) libraryScroll = session.librarySelected;
        if (session.librarySelected >= libraryScroll + visible) libraryScroll = session.librarySelected - visible + 1;
        repaint();
    }
    return used;
}

juce::Image Screen::shot()
{
    juce::Image image (juce::Image::RGB, getWidth(), getHeight(), true);
    juce::Graphics g (image);
    paintEntireComponent (g, false);
    return image;
}
}
