#include "Screen.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
const juce::Colour kPaper (0xffffffff), kInk (0xff1b1e22), kGrey (0xff8a9099), kFaint (0xffd9dde3);
const juce::Colour kWash (0xffeaf3fb), kLine (0xff9fc3e2), kDot (0xffc4d9ec), kVowel (0xff6f9fd0), kLetter (0xff5b8fc4), kGhost (0xffb7d3ea);
const juce::Colour kHot (0xffd94a3a), kCapture (0xffc48f00);

struct Reference { const char* letter; double f1, f2; };
const Reference kKlatt[] = {
    { "i", 310.0, 2020.0 }, { "\xc9\xaa", 400.0, 1800.0 }, { "e", 480.0, 1720.0 }, { "\xc9\x9b", 530.0, 1680.0 },
    { "\xc3\xa6", 620.0, 1660.0 }, { "\xc9\x91", 700.0, 1220.0 }, { "\xc9\x94", 600.0, 990.0 }, { "\xca\x8c", 620.0, 1220.0 },
    { "o", 540.0, 1100.0 }, { "\xca\x8a", 450.0, 1100.0 }, { "u", 350.0, 1250.0 }, { "\xc9\x9d", 470.0, 1270.0 } };

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
    session.onChange = [this] { repaint(); };
}

void Screen::layout()
{
    playKey = { 760, 22, 60, 22 }; sawKey = { 830, 22, 60, 22 }; noiseKey = { 900, 22, 110, 22 }; writeKey = { 1020, 22, 130, 22 };
    for (int n = 0; n < 4; ++n) chips[(size_t) n] = { 60 + n * 345, 54, 330, 22 };
    map = { 110, 110, 700, 420 };
    pad = { 900, 110, 300, 300 };
    response = { 900, 480, 300, 200 };
    statusLine = { 60, 812, 1380, 24 };
}

juce::Point<float> Screen::vowelPoint (double f1, double f2) const
{
    const double x = map.getX() + map.getWidth() * std::log (kF2High / std::clamp (f2, kF2Low, kF2High)) / std::log (kF2High / kF2Low);
    const double y = map.getY() + map.getHeight() * std::log (std::clamp (f1, kF1Low, kF1High) / kF1Low) / std::log (kF1High / kF1Low);
    return { (float) x, (float) y };
}

juce::Point<float> Screen::starPoint (int k) const
{
    if (k < 0 || k >= (int) session.stars.size()) return {};
    if (k < (int) points.size()) return points[(size_t) k];
    const auto f = formantsOf (session.stars[(size_t) k].words);
    return vowelPoint (f[0] > 0.0 ? f[0] : kF1Low, f[1] > 0.0 ? f[1] : kF2Low);
}

juce::Point<float> Screen::padCorner (int n) const
{
    const float x = (float) (n == 1 || n == 3 ? pad.getRight() : pad.getX());
    const float y = (float) (n >= 2 ? pad.getY() : pad.getBottom());
    return { x, y };
}

juce::Point<float> Screen::puckPoint (double morph, double q) const
{
    return { (float) (pad.getX() + morph / 100.0 * pad.getWidth()), (float) (pad.getBottom() - q / 100.0 * pad.getHeight()) };
}

int Screen::cornerAt (juce::Point<int> p) const
{
    for (int n = 0; n < 4; ++n) if (padCorner (n).getDistanceFrom (p.toFloat()) < 26.0f) return n;
    return -1;
}

int Screen::nearestStar (juce::Point<float> p, float within) const
{
    int best = -1; float bestDistance = within;
    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        const float d = starPoint (k).getDistanceFrom (p);
        if (d < bestDistance) { bestDistance = d; best = k; }
    }
    return best;
}

void Screen::refreshHeat()
{
    if (! session.quad.complete()) { heat.clear(); heatPins = { -2, -2, -2, -2 }; return; }
    if (heatPins == session.quad.pins && ! heat.empty()) return;
    heatPins = session.quad.pins;
    heat = hotCells (cornersOf (session.quad, session.stars), kHeat, hz);
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

void Screen::paintMap (juce::Graphics& g)
{
    static const char* vowels[] = { "Ooh To Eee", "Eeh To Aah", "Multi Q Vox", "Talking Hedz", "Ubu Orator", "Deep Bouche" };
    if (points.size() != session.libraryCount)
    {
        points.clear();
        for (size_t k = 0; k < session.libraryCount; ++k)
        {
            const auto f = formantsOf (session.stars[k].words);
            points.push_back (vowelPoint (f[0] > 0.0 ? f[0] : kF1Low, f[1] > 0.0 ? f[1] : kF2Low));
        }
    }
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
    g.drawText ("F2", map.getX() - 4, map.getY() - 16, 30, 12, juce::Justification::centredLeft);
    g.drawText ("F1", map.getX() - 46, map.getY() + 4, 40, 12, juce::Justification::centredRight);

    g.setFont (serif (15.0f)); g.setColour (kLetter);
    for (const auto& r : kKlatt)
    {
        const auto p = vowelPoint (r.f1, r.f2);
        g.drawText (juce::String (juce::CharPointer_UTF8 (r.letter)), (int) p.x - 10, (int) p.y - 10, 20, 20, juce::Justification::centred);
    }

    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        const auto& s = session.stars[(size_t) k];
        const auto p = starPoint (k);
        bool vowel = false;
        for (auto* v : vowels) vowel = vowel || s.body == v;
        const bool capture = s.kind == "capture";
        const bool lit = k == session.selected;
        g.setColour (capture ? kCapture : lit ? kInk : vowel ? kVowel : kDot);
        const float radius = capture ? 4.0f : vowel ? 3.5f : 2.5f;
        if (capture) g.fillRect (p.x - radius, p.y - radius, 2.0f * radius, 2.0f * radius);
        else g.fillEllipse (p.x - radius, p.y - radius, 2.0f * radius, 2.0f * radius);
    }
    for (int n = 0; n < 4; ++n)
    {
        const int k = session.quad.pins[(size_t) n];
        if (k < 0) continue;
        const auto p = starPoint (k);
        g.setColour (kInk); g.drawEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f, 1.4f);
        g.setFont (serif (10.0f));
        g.drawText (juce::String (n + 1), (int) p.x - 20, (int) p.y - 22, 40, 12, juce::Justification::centred);
    }
    if (session.hovered >= 0)
    {
        const auto p = starPoint (session.hovered);
        g.setColour (kGrey); g.drawEllipse (p.x - 9.0f, p.y - 9.0f, 18.0f, 18.0f, 1.0f);
        g.setFont (sans (11.0f)); g.setColour (kInk);
        g.drawText (session.stars[(size_t) session.hovered].name, (int) p.x + 12, (int) p.y - 8, 220, 16, juce::Justification::centredLeft);
    }
    if (session.selected >= 0 && session.selected != session.hovered)
    {
        const auto p = starPoint (session.selected);
        g.setFont (sans (11.0f)); g.setColour (kInk);
        g.drawText (session.stars[(size_t) session.selected].name, (int) p.x + 12, (int) p.y - 8, 220, 16, juce::Justification::centredLeft);
    }
    if (session.inPair())
    {
        const auto a = starPoint (session.pairA), b = starPoint (session.pairB);
        g.setColour (kInk); g.drawLine (a.x, a.y, b.x, b.y, 1.2f);
        g.setFont (sans (11.0f));
        g.drawText (session.stars[(size_t) session.pairB].name, (int) b.x + 12, (int) b.y - 8, 220, 16, juce::Justification::centredLeft);
    }
    if (session.sounding && (session.auditioning == -1 || session.inPair()) && (session.quad.complete() || session.inPair()))
    {
        const auto f = formantsOf (session.words);
        const auto p = vowelPoint (f[0] > 0.0 ? f[0] : kF1Low, f[1] > 0.0 ? f[1] : kF2Low);
        g.setColour (kInk); g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (kPaper); g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
    }
}

void Screen::paintPad (juce::Graphics& g)
{
    const auto& q = session.quad;
    refreshHeat();
    g.setColour (kWash); g.fillRect (pad);
    if (! heat.empty())
        for (int j = 0; j < kHeat; ++j)
            for (int i = 0; i < kHeat; ++i)
            {
                const double above = heat[(size_t) (j * kHeat + i)];
                if (above <= 3.0) continue;
                const float alpha = (float) std::clamp ((above - 3.0) / 12.0, 0.08, 0.5);
                const float cw = pad.getWidth() / (float) kHeat, ch = pad.getHeight() / (float) kHeat;
                g.setColour (kHot.withAlpha (alpha));
                g.fillRect ((float) pad.getX() + i * cw, (float) pad.getBottom() - (j + 1) * ch, cw, ch);
            }
    g.setColour (kFaint);
    g.fillRect (pad.getCentreX(), pad.getY(), 1, pad.getHeight()); g.fillRect (pad.getX(), pad.getCentreY(), pad.getWidth(), 1);
    g.setColour (kInk); g.drawRect (pad);
    g.setFont (serif (10.0f)); g.setColour (kGrey);
    g.drawText ("MORPH", pad.getX(), pad.getBottom() + 18, pad.getWidth(), 12, juce::Justification::centred);
    g.drawText ("Q", pad.getX() - 30, pad.getCentreY() - 6, 20, 12, juce::Justification::centred);
    for (int n = 0; n < 4; ++n)
    {
        const auto c = padCorner (n);
        const bool set = q.pins[(size_t) n] >= 0;
        g.setColour (kPaper); g.fillEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);
        g.setColour (set ? kInk : kFaint); g.drawEllipse (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f, 1.4f);
        g.setFont (serif (10.0f)); g.setColour (kInk);
        g.drawText (juce::String (n + 1), (int) c.x - 9, (int) c.y - 6, 18, 12, juce::Justification::centred);
        const bool left = n == 0 || n == 2, top = n >= 2;
        g.setFont (sans (10.0f)); g.setColour (set ? kInk : kGrey);
        g.drawText (set ? session.pinName (n) : juce::String (kPinNames[n]), left ? (int) c.x - 12 : (int) c.x - 168, top ? (int) c.y - 30 : (int) c.y + 14, 180, 14, left ? juce::Justification::centredLeft : juce::Justification::centredRight);
    }
    if (q.complete())
    {
        const auto pk = puckPoint (q.morph, q.q);
        g.setColour (kInk); g.fillEllipse (pk.x - 6.0f, pk.y - 6.0f, 12.0f, 12.0f);
        g.setColour (kPaper); g.drawEllipse (pk.x - 6.0f, pk.y - 6.0f, 12.0f, 12.0f, 1.5f);
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
    if (session.quad.complete() && session.auditioning < 0)
    {
        const auto c = cornersOf (session.quad, session.stars);
        paintCurve (g, r, lerp (c, 0.0, session.quad.q / 100.0), kGhost, 1.0f);
        paintCurve (g, r, lerp (c, 1.0, session.quad.q / 100.0), kGhost, 1.0f);
    }
    if (session.sounding) paintCurve (g, r, session.words, kInk, 1.6f);
    if (session.sounding)
    {
        const double peak = peakDb (session.words, hz);
        const auto f = formantsOf (session.words);
        juce::String line;
        const char* names[] = { "F1", "F2", "F3", "F4" };
        for (int i = 0; i < 4; ++i) if (f[(size_t) i] > 0.0) line += juce::String (names[i]) + " " + juce::String ((int) std::round (f[(size_t) i])) + "     ";
        g.setFont (serif (11.0f)); g.setColour (kInk);
        g.drawText (line, r.getX(), r.getBottom() + 22, r.getWidth() + 200, 14, juce::Justification::centredLeft);
        g.setColour (peak > 33.0 ? kHot : kGrey);
        g.drawText ("peak " + juce::String (peak > 0 ? "+" : "") + juce::String (peak, 1) + " dB", r.getX(), r.getBottom() + 38, 200, 12, juce::Justification::centredLeft);
    }
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (kPaper);
    g.setFont (sans (13.0f, true)); g.setColour (kInk);
    g.drawText ("HEADSPACE", 60, 22, 200, 22, juce::Justification::centredLeft);
    paintWord (g, playKey, "PLAY", session.playing, true);
    paintWord (g, sawKey, "SAW", session.source == 0, true);
    paintWord (g, noiseKey, "PINK NOISE", session.source == 1, true);
    paintWord (g, writeKey, "WRITE BODY FILE", false, session.quad.complete());
    for (int n = 0; n < 4; ++n)
    {
        const auto r = chips[(size_t) n];
        const bool set = session.quad.pins[(size_t) n] >= 0;
        g.setColour (session.selected >= 0 ? kGrey : kFaint); g.drawRect (r);
        g.setFont (serif (10.0f)); g.setColour (kGrey);
        g.drawText (juce::String (n + 1) + "   " + kPinNames[n], r.reduced (8, 0), juce::Justification::centredLeft);
        g.setFont (sans (11.0f)); g.setColour (set ? kInk : kFaint);
        g.drawText (set ? session.pinName (n) : juce::String ("empty"), r.reduced (8, 0).withTrimmedLeft (78), juce::Justification::centredLeft);
    }
    paintMap (g);
    paintPad (g);
    paintResponse (g);
    g.setFont (serif (12.0f)); g.setColour (kGrey);
    juce::String hint = session.status;
    if (session.selected >= 0 && session.auditioning == session.selected) hint = session.stars[(size_t) session.selected].name + "   press 1 to 4 or click a corner to put it there, or drag toward another star";
    g.drawText (hint, statusLine, juce::Justification::centredLeft);
}

void Screen::mouseMove (const juce::MouseEvent& e)
{
    if (dragging) return;
    if (map.expanded (30, 30).contains (e.getPosition())) session.hover (nearestStar (e.position, 10.0f));
    else session.unhover();
}

void Screen::mouseExit (const juce::MouseEvent&) { session.unhover(); }

void Screen::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    grabKeyboardFocus();
    if (playKey.contains (p)) { session.setPlaying (! session.playing); return; }
    if (sawKey.contains (p)) { session.setSource (0); return; }
    if (noiseKey.contains (p)) { session.setSource (1); return; }
    if (writeKey.contains (p)) { session.write(); return; }
    for (int n = 0; n < 4; ++n)
        if (chips[(size_t) n].contains (p)) { if (session.selected >= 0) session.pin (n, session.selected); return; }
    if (const int n = cornerAt (p); n >= 0 && session.selected >= 0 && session.auditioning == session.selected) { session.pin (n, session.selected); return; }
    if (pad.expanded (12, 12).contains (p) && session.quad.complete())
    {
        dragging = true;
        mouseDrag (e);
        return;
    }
    if (map.expanded (30, 30).contains (p))
    {
        if (const int k = nearestStar (e.position, 12.0f); k >= 0) { session.select (k); pairing = true; pairFrom = k; }
        return;
    }
}

void Screen::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragging) { session.setPuck ((p.x - pad.getX()) * 100.0 / pad.getWidth(), (pad.getBottom() - p.y) * 100.0 / pad.getHeight()); return; }
    if (! pairing || pairFrom < 0) return;
    int b = -1; float best = 1e9f;
    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        if (k == pairFrom) continue;
        const float d = starPoint (k).getDistanceFrom (e.position);
        if (d < best) { best = d; b = k; }
    }
    if (b < 0) return;
    const auto a = starPoint (pairFrom), target = starPoint (b);
    const float span = target.getDistanceFrom (a);
    if (span < 1.0f) return;
    session.morphPair (pairFrom, b, a.getDistanceFrom (e.position) / span);
}

void Screen::mouseUp (const juce::MouseEvent&) { dragging = false; pairing = false; pairFrom = -1; }

bool Screen::keyPressed (const juce::KeyPress& k)
{
    const bool used = session.key (k);
    if (used) repaint();
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
