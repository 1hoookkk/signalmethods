#include "Screen.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
const juce::Colour kPaper (0xffffffff), kInk (0xff1b1e22), kGrey (0xff8a9099), kFaint (0xffd9dde3);
const juce::Colour kWash (0xffe9f3fb), kWashDeep (0xffd6e8f7), kLine (0xff9fc3e2), kGhost (0xffb7d3ea), kMark (0xff1b1e22);

juce::Font sans (float height, bool bold = false) { return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Font serif (float height) { return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), height, juce::Font::plain)); }

double xOf (double hzValue, juce::Rectangle<int> r) { return r.getX() + r.getWidth() * std::log (hzValue / 20.0) / std::log (1000.0); }
double yOf (double db, juce::Rectangle<int> r) { return r.getBottom() - r.getHeight() * (db + 30.0) / 60.0; }
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
    playKey = { 760, 22, 60, 22 }; sawKey = { 830, 22, 60, 22 }; noiseKey = { 900, 22, 110, 22 }; writeKey = { 1020, 22, 130, 22 };
    plane = { 60, 72, 1060, 380 };
    response = { 60, 500, 1060, 280 };
    libraryList = { 1180, 72, 260, 420 };
    columnList = { 1180, 520, 260, 260 };
    statusLine = { 60, 812, 1380, 24 };
}

double Screen::columnX (double column) const
{
    const int slots = std::max (session.strip.count(), 2);
    return plane.getX() + (column - 1.0) / (slots - 1.0) * plane.getWidth();
}

void Screen::paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width) const
{
    const auto db = responseDb (words, hz);
    juce::Path p;
    for (size_t i = 0; i < hz.size(); ++i)
    {
        const float x = (float) xOf (hz[i], r), y = (float) std::clamp (yOf (db[i], r), (double) r.getY(), (double) r.getBottom());
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (width));
}

void Screen::paintWord (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool enabled) const
{
    g.setFont (sans (12.0f));
    g.setColour (on ? kInk : (enabled ? kGrey : kFaint));
    g.drawText (text, r, juce::Justification::centred);
    if (on) { g.setColour (kInk); g.fillRect (r.getX() + 6, r.getBottom() - 1, r.getWidth() - 12, 1); }
}

void Screen::paintPlane (juce::Graphics& g) const
{
    const auto& s = session.strip;
    const int n = s.count();
    g.setColour (kWash); g.fillRect (plane);
    if (n >= 2)
    {
        const int k = std::clamp (s.square, 1, s.squares());
        const int x0 = (int) std::round (columnX (k)), x1 = (int) std::round (columnX (k + 1));
        g.setColour (kWashDeep); g.fillRect (x0, plane.getY(), x1 - x0, plane.getHeight());
    }
    g.setColour (kLine); g.drawRect (plane, 1);
    g.setFont (serif (11.0f)); g.setColour (kGrey);
    g.drawText ("Q 100", plane.getX() - 52, plane.getY() - 6, 44, 12, juce::Justification::centredRight);
    g.drawText ("Q 0", plane.getX() - 52, plane.getBottom() - 6, 44, 12, juce::Justification::centredRight);
    const int current = std::clamp (s.square, 1, s.squares());
    for (int k = 1; k <= n; ++k)
    {
        const int x = (int) std::round (columnX (k));
        g.setColour (s.selected == k ? kInk : kLine);
        g.fillRect (x, plane.getY(), s.selected == k ? 2 : 1, plane.getHeight());
        g.setFont (serif (11.0f)); g.setColour (kGrey);
        g.drawText (juce::String (k), x - 20, plane.getBottom() + 4, 40, 12, juce::Justification::centred);
        if (n <= 8 || s.selected == k || k == current || k == current + 1)
        {
            g.setFont (sans (11.0f)); g.setColour (s.selected == k ? kInk : kGrey);
            const auto just = k == 1 ? juce::Justification::centredLeft : k == n ? juce::Justification::centredRight : juce::Justification::centred;
            const int left = k == 1 ? x : k == n ? x - 180 : x - 90;
            g.drawText (s.columns[(size_t) k - 1].name, left, plane.getY() - 18, 180, 14, just);
        }
    }
    if (n > 0)
    {
        const double x = n == 1 ? 1.0 : s.square + s.morph / 100.0;
        const float px = (float) columnX (x), py = (float) (plane.getBottom() - s.q / 100.0 * plane.getHeight());
        g.setColour (kMark); g.fillEllipse (px - 5.0f, py - 5.0f, 10.0f, 10.0f);
        g.setColour (kPaper); g.drawEllipse (px - 5.0f, py - 5.0f, 10.0f, 10.0f, 1.5f);
        g.setFont (serif (11.0f)); g.setColour (kGrey);
        g.drawText ("MORPH " + juce::String (s.morph, 1), (int) px - 60, plane.getBottom() + 18, 120, 12, juce::Justification::centred);
    }
}

void Screen::paintResponse (juce::Graphics& g) const
{
    const auto r = response;
    g.setColour (kFaint);
    for (double f : { 100.0, 1000.0, 10000.0 }) g.fillRect ((int) std::round (xOf (f, r)), r.getY(), 1, r.getHeight());
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1); g.fillRect (r.getX(), r.getBottom(), r.getWidth(), 1);
    g.setColour (kGrey); g.fillRect (r.getX(), (int) std::round (yOf (0.0, r)), r.getWidth(), 1);
    g.setFont (serif (11.0f)); g.setColour (kGrey);
    for (double f : { 100.0, 1000.0, 10000.0 })
        g.drawText (f < 1000 ? "100 Hz" : f < 10000 ? "1 kHz" : "10 kHz", (int) xOf (f, r) - 30, r.getBottom() + 6, 60, 12, juce::Justification::centred);
    g.drawText ("+30 dB", r.getX() - 56, r.getY() - 6, 48, 12, juce::Justification::centredRight);
    g.drawText ("0", r.getX() - 56, (int) yOf (0.0, r) - 6, 48, 12, juce::Justification::centredRight);
    g.drawText ("-30", r.getX() - 56, r.getBottom() - 6, 48, 12, juce::Justification::centredRight);
    const auto& s = session.strip;
    if (s.count() > 0)
        for (const auto& corner : cornersOf (s)) paintCurve (g, r, corner, kGhost, 1.0f);
    if (session.sounding) paintCurve (g, r, session.words, kInk, 1.6f);
}

void Screen::paintList (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::StringArray& rows, const juce::StringArray& notes, int selected, int scroll) const
{
    g.setFont (sans (12.0f)); g.setColour (kGrey);
    g.drawText (title, r.getX(), r.getY() - 20, r.getWidth(), 14, juce::Justification::centredLeft);
    g.setColour (kFaint); g.fillRect (r.getX(), r.getY() - 4, r.getWidth(), 1);
    const int visible = r.getHeight() / kRow;
    for (int i = 0; i < visible; ++i)
    {
        const int row = scroll + i;
        if (row >= rows.size()) break;
        juce::Rectangle<int> line (r.getX(), r.getY() + i * kRow, r.getWidth(), kRow);
        if (row == selected) { g.setColour (kWashDeep); g.fillRect (line); }
        g.setFont (sans (11.0f)); g.setColour (row == selected ? kInk : kGrey);
        g.drawText (rows[row], line.reduced (6, 0), juce::Justification::centredLeft);
        if (row < notes.size() && notes[row].isNotEmpty())
        {
            g.setFont (serif (10.0f)); g.setColour (kGrey);
            g.drawText (notes[row], line.reduced (6, 0), juce::Justification::centredRight);
        }
    }
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (kPaper);
    const auto& s = session.strip;
    const int n = s.count();
    g.setFont (sans (13.0f, true)); g.setColour (kInk);
    g.drawText ("HEADSPACE", 60, 22, 200, 22, juce::Justification::centredLeft);
    paintWord (g, playKey, "PLAY", session.playing, true);
    paintWord (g, sawKey, "SAW", session.source == 0, true);
    paintWord (g, noiseKey, "PINK NOISE", session.source == 1, true);
    paintWord (g, writeKey, "WRITE BODY FILE", false, n >= 1);
    paintPlane (g);
    paintResponse (g);

    juce::StringArray libraryRows, libraryNotes;
    for (const auto& e : session.library) { libraryRows.add (e.body); libraryNotes.add (e.corner); }
    paintList (g, libraryList, "Library", libraryRows, libraryNotes, session.librarySelected, libraryScroll);
    juce::StringArray columnRows, columnNotes;
    for (int j = 0; j < n; ++j)
    {
        const auto& c = s.columns[(size_t) j];
        columnRows.add (juce::String (j + 1) + "   " + c.name);
        columnNotes.add (c.origin.kind == "capture" ? c.origin.parentA + " > " + c.origin.parentB + " " + juce::String (c.origin.morph, 0) : juce::String());
    }
    paintList (g, columnList, "Strip", columnRows, columnNotes, s.selected - 1, columnScroll);

    g.setFont (serif (12.0f)); g.setColour (kGrey);
    g.drawText (session.status, statusLine, juce::Justification::centredLeft);
}

void Screen::placeAt (juce::Point<int> p, bool select)
{
    const int n = session.strip.count();
    if (n == 0) return;
    const int slots = std::max (n, 2);
    const double x = 1.0 + (p.x - plane.getX()) / (double) plane.getWidth() * (slots - 1.0);
    const double xc = std::clamp (x, 1.0, (double) n);
    const int k = std::clamp ((int) std::floor (xc), 1, session.strip.squares());
    const double q = std::clamp ((plane.getBottom() - p.y) / (double) plane.getHeight(), 0.0, 1.0) * 100.0;
    if (select) session.strip.selected = (int) std::round (xc);
    session.setPosition (k, (xc - k) * 100.0, q);
}

void Screen::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    grabKeyboardFocus();
    if (playKey.contains (p)) { session.setPlaying (! session.playing); return; }
    if (sawKey.contains (p)) { session.setSource (0); return; }
    if (noiseKey.contains (p)) { session.setSource (1); return; }
    if (writeKey.contains (p)) { session.write(); return; }
    if (plane.expanded (0, 24).contains (p)) { dragging = true; placeAt (p, true); return; }
    if (libraryList.contains (p))
    {
        const int row = libraryScroll + (p.y - libraryList.getY()) / kRow;
        if (row >= 0 && row < (int) session.library.size()) session.hear (row);
        return;
    }
    if (columnList.contains (p))
    {
        const int row = columnScroll + (p.y - columnList.getY()) / kRow;
        if (row >= 0 && row < session.strip.count()) session.jump (row + 1);
        return;
    }
}

void Screen::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging) placeAt (e.getPosition(), false);
}

void Screen::mouseUp (const juce::MouseEvent&) { dragging = false; }

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int lines = wheel.deltaY > 0 ? -3 : 3;
    if (libraryList.contains (e.getPosition()))
    {
        libraryScroll = std::clamp (libraryScroll + lines, 0, std::max (0, (int) session.library.size() - libraryList.getHeight() / kRow));
        repaint();
    }
    else if (columnList.contains (e.getPosition()))
    {
        columnScroll = std::clamp (columnScroll + lines, 0, std::max (0, session.strip.count() - columnList.getHeight() / kRow));
        repaint();
    }
}

bool Screen::keyPressed (const juce::KeyPress& k)
{
    const bool used = session.key (k);
    if (used)
    {
        const int visible = libraryList.getHeight() / kRow;
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
