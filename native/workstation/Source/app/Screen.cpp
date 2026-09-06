#include "Screen.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
const juce::Colour kPaper (0xffffffff), kInk (0xff1b1e22), kGrey (0xff8a9099), kFaint (0xffd9dde3);
const juce::Colour kWash (0xffeaf3fb), kLine (0xff9fc3e2), kDot (0xffc4d9ec), kVowel (0xff6f9fd0), kPath (0xff1b1e22), kGhost (0xffb7d3ea);

juce::Font sans (float height, bool bold = false) { return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Font serif (float height) { return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), height, juce::Font::plain)); }

double xOf (double hzValue, juce::Rectangle<int> r) { return r.getX() + r.getWidth() * std::log (hzValue / 20.0) / std::log (1000.0); }
double yOf (double db, juce::Rectangle<int> r) { return r.getBottom() - r.getHeight() * (db + 30.0) / 60.0; }

constexpr double kF2High = 4000.0, kF2Low = 300.0, kF1Low = 60.0, kF1High = 1500.0;
}

Screen::Screen (Session& s) : session (s), hz (curveHz())
{
    setSize (1480, 860);
    setWantsKeyboardFocus (true);
    layout();
    for (const auto& e : session.library) libraryFormants.push_back (formantsOf (e.q0));
    session.onChange = [this] { keepVisible(); repaint(); };
}

void Screen::layout()
{
    playKey = { 760, 22, 60, 22 }; sawKey = { 830, 22, 60, 22 }; noiseKey = { 900, 22, 110, 22 }; writeKey = { 1020, 22, 130, 22 };
    chart = { 110, 70, 970, 440 };
    qBar = { 70, 70, 14, 440 };
    response = { 110, 600, 970, 180 };
    libraryList = { 1180, 70, 260, 380 };
    stripList = { 1180, 490, 260, 300 };
    statusLine = { 60, 812, 1380, 24 };
}

juce::Point<float> Screen::vowelPoint (double f1, double f2) const
{
    const double x = chart.getX() + chart.getWidth() * std::log (kF2High / std::clamp (f2, kF2Low, kF2High)) / std::log (kF2High / kF2Low);
    const double y = chart.getY() + chart.getHeight() * std::log (std::clamp (f1, kF1Low, kF1High) / kF1Low) / std::log (kF1High / kF1Low);
    return { (float) x, (float) y };
}

juce::Point<float> Screen::chartPoint (const Words& words) const
{
    const auto f = formantsOf (words);
    if (f[0] > 0.0 && f[1] > 0.0) return vowelPoint (f[0], f[1]);
    if (f[0] > 0.0) return vowelPoint (f[0], kF2Low);
    return vowelPoint (kF1Low, kF2Low);
}

int Screen::nearestEntry (juce::Point<int> p) const
{
    int best = -1; double bestDistance = 10.0;
    for (int i = 0; i < (int) libraryFormants.size(); ++i)
    {
        const auto& f = libraryFormants[(size_t) i];
        const double d = vowelPoint (f[0] > 0.0 ? f[0] : kF1Low, f[1] > 0.0 ? f[1] : kF2Low).getDistanceFrom (p.toFloat());
        if (d < bestDistance) { bestDistance = d; best = i; }
    }
    return best;
}

int Screen::nearestAnchor (juce::Point<int> p) const
{
    int best = 0; double bestDistance = 12.0;
    for (int k = 1; k <= session.strip.count(); ++k)
    {
        const double d = chartPoint (session.strip.anchors[(size_t) k - 1].q0).getDistanceFrom (p.toFloat());
        if (d < bestDistance) { bestDistance = d; best = k; }
    }
    return best;
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

void Screen::paintChart (juce::Graphics& g) const
{
    static const char* vowels[] = { "Ooh To Eee", "Eeh To Aah", "Multi Q Vox", "Talking Hedz", "Ubu Orator", "Deep Bouche" };
    const auto& s = session.strip;
    const int n = s.count();

    juce::Path shape;
    const auto a = vowelPoint (200.0, 2600.0), b = vowelPoint (200.0, 500.0), c = vowelPoint (850.0, 880.0), d = vowelPoint (850.0, 1800.0);
    shape.startNewSubPath (a); shape.lineTo (b); shape.lineTo (c); shape.lineTo (d); shape.closeSubPath();
    g.setColour (kWash); g.fillPath (shape);
    g.setColour (kLine); g.strokePath (shape, juce::PathStrokeType (1.0f));
    g.setFont (serif (10.0f)); g.setColour (kGrey);
    g.drawText ("2600", (int) a.x - 46, (int) a.y - 15, 40, 12, juce::Justification::centredRight);
    g.drawText ("200", (int) a.x - 46, (int) a.y - 2, 40, 12, juce::Justification::centredRight);
    g.drawText ("500", (int) b.x + 6, (int) b.y - 15, 40, 12, juce::Justification::centredLeft);
    g.drawText ("200", (int) b.x + 6, (int) b.y - 2, 40, 12, juce::Justification::centredLeft);
    g.drawText ("1800", (int) d.x - 20, (int) d.y + 5, 40, 12, juce::Justification::centred);
    g.drawText ("850", (int) d.x - 50, (int) d.y - 6, 40, 12, juce::Justification::centredRight);
    g.drawText ("880", (int) c.x - 20, (int) c.y + 5, 40, 12, juce::Justification::centred);
    g.drawText ("850", (int) c.x + 10, (int) c.y - 6, 40, 12, juce::Justification::centredLeft);
    g.drawText ("F2", chart.getX() - 4, chart.getY() - 16, 30, 12, juce::Justification::centredLeft);
    g.drawText ("F1", chart.getX() - 46, chart.getY() + 4, 40, 12, juce::Justification::centredRight);

    for (int i = 0; i < (int) libraryFormants.size(); ++i)
    {
        const auto& f = libraryFormants[(size_t) i];
        const auto& e = session.library[(size_t) i];
        const auto p = vowelPoint (f[0] > 0.0 ? f[0] : kF1Low, f[1] > 0.0 ? f[1] : kF2Low);
        bool vowel = false;
        for (auto* v : vowels) vowel = vowel || e.body == v;
        const bool chosen = i == session.librarySelected;
        g.setColour (chosen ? kInk : vowel ? kVowel : kDot);
        const float radius = vowel ? 3.5f : 2.5f;
        g.fillEllipse (p.x - radius, p.y - radius, 2.0f * radius, 2.0f * radius);
        if (chosen)
        {
            g.setColour (kInk); g.drawEllipse (p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f, 1.0f);
            g.setFont (sans (11.0f));
            g.drawText (e.name, (int) p.x + 12, (int) p.y - 8, 200, 16, juce::Justification::centredLeft);
        }
    }

    const int current = std::clamp (s.square, 1, s.squares());
    for (int k = 1; k < n; ++k)
    {
        const auto p0 = chartPoint (s.anchors[(size_t) k - 1].q0), p1 = chartPoint (s.anchors[(size_t) k].q0);
        g.setColour (k == current ? kPath : kGrey);
        g.drawLine (p0.x, p0.y, p1.x, p1.y, k == current ? 2.0f : 1.0f);
    }
    for (int k = 1; k <= n; ++k)
    {
        const auto& anchor = s.anchors[(size_t) k - 1];
        const auto p = chartPoint (anchor.q0), pq = chartPoint (anchor.q1);
        g.setColour (kPaper); g.fillEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
        g.setColour (s.selected == k ? kInk : kGrey);
        g.drawEllipse (p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f, s.selected == k ? 2.0f : 1.2f);
        g.drawEllipse (pq.x - 3.0f, pq.y - 3.0f, 6.0f, 6.0f, 1.0f);
        g.setFont (serif (11.0f)); g.setColour (s.selected == k ? kInk : kGrey);
        g.drawText (juce::String (k), (int) p.x - 20, (int) p.y - 24, 40, 12, juce::Justification::centred);
        if (k == current || k == current + 1 || k == s.selected)
        {
            g.setFont (sans (11.0f));
            g.drawText (anchor.name, (int) p.x + 10, (int) p.y + 4, 220, 14, juce::Justification::centredLeft);
        }
    }
    if (n > 0 && session.sounding)
    {
        const auto p = chartPoint (session.words);
        g.setColour (kInk); g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (kPaper); g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
    }

    g.setColour (kFaint); g.fillRect (qBar);
    if (n > 0)
    {
        const float y = (float) (qBar.getBottom() - s.q / 100.0 * qBar.getHeight());
        g.setColour (kInk); g.fillRect ((float) qBar.getX() - 2.0f, y - 1.5f, (float) qBar.getWidth() + 4.0f, 3.0f);
    }
    g.setFont (serif (10.0f)); g.setColour (kGrey);
    g.drawText ("Q", qBar.getX() - 3, qBar.getY() - 16, 20, 12, juce::Justification::centred);

    if (session.sounding)
    {
        const auto f = formantsOf (session.words);
        juce::String line;
        const char* names[] = { "F1", "F2", "F3", "F4" };
        for (int i = 0; i < 4; ++i) if (f[(size_t) i] > 0.0) line += juce::String (names[i]) + " " + juce::String ((int) std::round (f[(size_t) i])) + " Hz      ";
        if (n > 0) line += "MORPH " + juce::String (s.morph, 1) + "      Q " + juce::String (s.q, 1);
        g.setFont (serif (12.0f)); g.setColour (kInk);
        g.drawText (line, chart.getX(), chart.getBottom() + 22, chart.getWidth(), 14, juce::Justification::centredLeft);
    }
}

void Screen::paintResponse (juce::Graphics& g) const
{
    const auto r = response;
    g.setColour (kFaint);
    for (double f : { 100.0, 1000.0, 10000.0 }) g.fillRect ((int) std::round (xOf (f, r)), r.getY(), 1, r.getHeight());
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1); g.fillRect (r.getX(), r.getBottom(), r.getWidth(), 1);
    g.setColour (kGrey); g.fillRect (r.getX(), (int) std::round (yOf (0.0, r)), r.getWidth(), 1);
    g.setFont (serif (10.0f)); g.setColour (kGrey);
    for (double f : { 100.0, 1000.0, 10000.0 })
        g.drawText (f < 1000 ? "100 Hz" : f < 10000 ? "1 kHz" : "10 kHz", (int) xOf (f, r) - 30, r.getBottom() + 5, 60, 12, juce::Justification::centred);
    g.drawText ("+30", r.getX() - 40, r.getY() - 6, 32, 12, juce::Justification::centredRight);
    g.drawText ("0", r.getX() - 40, (int) yOf (0.0, r) - 6, 32, 12, juce::Justification::centredRight);
    g.drawText ("-30", r.getX() - 40, r.getBottom() - 6, 32, 12, juce::Justification::centredRight);
    const auto& s = session.strip;
    if (s.count() > 0)
    {
        const auto c = cornersOf (s);
        paintCurve (g, r, lerp (c, 0.0, s.q / 100.0), kGhost, 1.0f);
        paintCurve (g, r, lerp (c, 1.0, s.q / 100.0), kGhost, 1.0f);
    }
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
        if (row == selected) { g.setColour (kWash); g.fillRect (line); }
        g.setFont (sans (11.0f)); g.setColour (row == selected ? kInk : kGrey);
        g.drawText (rows[row], line.reduced (6, 0), juce::Justification::centredLeft);
        if (row < notes.size() && notes[row].isNotEmpty())
        {
            g.setFont (serif (10.0f)); g.setColour (kGrey);
            g.drawText (notes[row], line.reduced (6, 0), juce::Justification::centredRight);
        }
    }
    if (rows.size() > visible)
    {
        g.setFont (serif (10.0f)); g.setColour (kGrey);
        g.drawText (juce::String (scroll + 1) + " to " + juce::String (std::min (rows.size(), scroll + visible)) + " of " + juce::String (rows.size()), r.getX(), r.getBottom() + 2, r.getWidth(), 12, juce::Justification::centredRight);
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
    paintChart (g);
    paintResponse (g);

    juce::StringArray libraryRows, libraryNotes;
    for (const auto& e : session.library) { libraryRows.add (e.body); libraryNotes.add (e.side); }
    paintList (g, libraryList, "Library", libraryRows, libraryNotes, session.librarySelected, libraryScroll);
    juce::StringArray stripRows, stripNotes;
    for (int j = 0; j < n; ++j)
    {
        const auto& a = s.anchors[(size_t) j];
        stripRows.add (juce::String (j + 1) + "   " + a.name);
        stripNotes.add (a.origin.kind == "capture" ? a.origin.parentA + " > " + a.origin.parentB + " " + juce::String (a.origin.morph, 0) : juce::String());
    }
    paintList (g, stripList, "Strip", stripRows, stripNotes, s.selected - 1, stripScroll);

    g.setFont (serif (12.0f)); g.setColour (kGrey);
    g.drawText (session.status, statusLine, juce::Justification::centredLeft);
}

void Screen::dragAlong (juce::Point<int> p)
{
    const auto& s = session.strip;
    if (s.count() < 2) return;
    const int k = std::clamp (s.square, 1, s.squares());
    const auto p0 = chartPoint (s.anchors[(size_t) k - 1].q0), p1 = chartPoint (s.anchors[(size_t) k].q0);
    const auto d = p1 - p0;
    const double length2 = d.x * d.x + d.y * d.y;
    if (length2 < 1.0) return;
    const double t = std::clamp (((p.x - p0.x) * d.x + (p.y - p0.y) * d.y) / length2, 0.0, 1.0);
    session.setPosition (k, t * 100.0, s.q);
}

void Screen::keepVisible()
{
    const int visible = libraryList.getHeight() / kRow;
    if (session.librarySelected < libraryScroll) libraryScroll = session.librarySelected;
    if (session.librarySelected >= libraryScroll + visible) libraryScroll = session.librarySelected - visible + 1;
    const int rows = stripList.getHeight() / kRow, sel = session.strip.selected - 1;
    if (sel >= 0 && sel < stripScroll) stripScroll = sel;
    if (sel >= stripScroll + rows) stripScroll = sel - rows + 1;
}

void Screen::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    grabKeyboardFocus();
    if (playKey.contains (p)) { session.setPlaying (! session.playing); return; }
    if (sawKey.contains (p)) { session.setSource (0); return; }
    if (noiseKey.contains (p)) { session.setSource (1); return; }
    if (writeKey.contains (p)) { session.write(); return; }
    if (qBar.expanded (8, 0).contains (p)) { dragging = Drag::q; mouseDrag (e); return; }
    if (chart.expanded (20, 20).contains (p))
    {
        if (const int k = nearestAnchor (p); k > 0) { session.jump (k); dragging = Drag::path; return; }
        if (const int i = nearestEntry (p); i >= 0) { session.hear (i); return; }
        if (session.strip.count() >= 2) { dragging = Drag::path; dragAlong (p); }
        return;
    }
    if (libraryList.contains (p))
    {
        const int row = libraryScroll + (p.y - libraryList.getY()) / kRow;
        if (row >= 0 && row < (int) session.library.size()) session.hear (row);
        return;
    }
    if (stripList.contains (p))
    {
        const int row = stripScroll + (p.y - stripList.getY()) / kRow;
        if (row >= 0 && row < session.strip.count()) session.jump (row + 1);
        return;
    }
}

void Screen::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragging == Drag::path) dragAlong (p);
    else if (dragging == Drag::q && session.strip.count() > 0)
    {
        const double q = std::clamp ((qBar.getBottom() - p.y) / (double) qBar.getHeight(), 0.0, 1.0) * 100.0;
        session.setPosition (session.strip.square, session.strip.morph, q);
    }
}

void Screen::mouseUp (const juce::MouseEvent&) { dragging = Drag::none; }

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int lines = wheel.deltaY > 0 ? -3 : 3;
    if (libraryList.contains (e.getPosition()))
    {
        libraryScroll = std::clamp (libraryScroll + lines, 0, std::max (0, (int) session.library.size() - libraryList.getHeight() / kRow));
        repaint();
    }
    else if (stripList.contains (e.getPosition()))
    {
        stripScroll = std::clamp (stripScroll + lines, 0, std::max (0, session.strip.count() - stripList.getHeight() / kRow));
        repaint();
    }
}

bool Screen::keyPressed (const juce::KeyPress& k)
{
    const bool used = session.key (k);
    if (used) { keepVisible(); repaint(); }
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
