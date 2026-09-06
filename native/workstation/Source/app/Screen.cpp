#include "Screen.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
const juce::Colour kBack (0xff0a0e0b), kGrid (0xff141d16), kGreen (0xff5fe36c), kDim (0xff2c5a34), kText (0xff9fd3a6), kBox (0xff1a241c), kFill (0x4a2fb548);
const juce::Colour kVowelInk (0xff6fb8e8), kCaptureInk (0xffe0c060), kReadInk (0xffe89a5a);

juce::Font mono (float height) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain)); }

double xOf (double hzValue, juce::Rectangle<int> r) { return r.getX() + r.getWidth() * std::log (hzValue / 20.0) / std::log (1000.0); }
double yOf (double db, juce::Rectangle<int> r) { return r.getBottom() - r.getHeight() * (db + 30.0) / 60.0; }

juce::Colour inkOf (const Star& s)
{
    if (s.kind == "vowel") return kVowelInk;
    if (s.kind == "capture") return kCaptureInk;
    if (s.kind == "read") return kReadInk;
    return kGreen;
}
}

Screen::Screen (Session& s) : session (s), hz (curveHz())
{
    setSize (1120, 700);
    setWantsKeyboardFocus (true);
    layout();
    session.onChange = [this] { repaint(); };
}

void Screen::resized() { layout(); repaint(); }

void Screen::layout()
{
    const int w = std::max (600, getWidth()), h = std::max (400, getHeight());
    const int pickerW = std::max (300, w * 2 / 5);
    picker = { 0, 0, pickerW, h };
    chart = picker.reduced (44, 40).withTrimmedBottom (24);
    stage = { pickerW + 40, 44, w - pickerW - 80, h - 44 - 40 };
    const int bw = std::min (150, stage.getWidth() / 3);
    cornerBox[0] = { stage.getX() + 8, stage.getY() + 8, bw, 20 };
    cornerBox[1] = { stage.getRight() - 8 - bw, stage.getY() + 8, bw, 20 };
    cornerBox[2] = { stage.getX() + 8, stage.getBottom() - 28, bw, 20 };
    cornerBox[3] = { stage.getRight() - 8 - bw, stage.getBottom() - 28, bw, 20 };
    for (int n = 0; n < 4; ++n)
    {
        const bool right = n == 1 || n == 3;
        const auto box = cornerBox[(size_t) n];
        cornerTag[(size_t) n] = { right ? box.getX() - 44 : box.getRight() + 6, box.getY(), 38, 20 };
    }
    for (int i = 0; i < 3; ++i) keys[(size_t) i] = { stage.getX() + i * 56, h - 26, 50, 18 };
    const int tw = std::min (400, stage.getWidth() - 2 * bw - 40), th = kLine * (kRows + 1) + 8;
    table = { stage.getCentreX() - tw / 2, stage.getBottom() - 36 - th, tw, th };
}

juce::Rectangle<int> Screen::cell (int row, int column) const
{
    static const int widths[kColumns] = { 3, 7, 9, 9, 8 };
    int total = 0;
    for (int w : widths) total += w;
    int x = table.getX() + 6;
    for (int c = 0; c < column; ++c) x += (table.getWidth() - 12) * widths[c] / total;
    return { x, table.getY() + 4 + (row + 1) * kLine, (table.getWidth() - 12) * widths[column] / total, kLine };
}

juce::Point<float> Screen::cornerPoint (int corner) const
{
    const bool right = corner == 1 || corner == 3, bottom = corner >= 2;
    return { (float) (right ? stage.getRight() : stage.getX()), (float) (bottom ? stage.getBottom() : stage.getY()) };
}

juce::Point<float> Screen::puckPoint() const
{
    return { (float) (stage.getX() + session.quad.morph / 100.0 * stage.getWidth()), (float) (stage.getBottom() - session.quad.q / 100.0 * stage.getHeight()) };
}

juce::Point<float> Screen::chartPoint (double f1, double f2) const
{
    const double u = std::log (std::clamp (f1, kF1Low, kF1High) / kF1Low) / std::log (kF1High / kF1Low);
    const double v = std::log (std::clamp (f2, kF2Low, kF2High) / kF2Low) / std::log (kF2High / kF2Low);
    return { (float) (chart.getX() + u * chart.getWidth()), (float) (chart.getBottom() - v * chart.getHeight()) };
}

std::pair<double, double> Screen::formantsAt (juce::Point<int> p) const
{
    const double u = std::clamp ((p.x - chart.getX()) / (double) chart.getWidth(), 0.0, 1.0);
    const double v = std::clamp ((chart.getBottom() - p.y) / (double) chart.getHeight(), 0.0, 1.0);
    return { kF1Low * std::pow (kF1High / kF1Low, u), kF2Low * std::pow (kF2High / kF2Low, v) };
}

int Screen::cornerAt (juce::Point<int> p) const
{
    for (int n = 0; n < 4; ++n) if (cornerBox[(size_t) n].expanded (10, 16).contains (p)) return n;
    return -1;
}

int Screen::pointAt (juce::Point<int> p) const
{
    int best = -1;
    double bestD = 10.0;
    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        const auto f = formantsOf (session.stars[(size_t) k].words);
        if (f[0] <= 0.0 || f[1] <= 0.0) continue;
        const double d = chartPoint (f[0], f[1]).getDistanceFrom (p.toFloat());
        if (d < bestD) { bestD = d; best = k; }
    }
    return best;
}

int Screen::menuCount() const { return (int) session.stars.size(); }

juce::String Screen::menuItem (int i) const
{
    return i >= 0 && i < (int) session.stars.size() ? session.stars[(size_t) i].name : juce::String();
}

int Screen::menuItemAt (juce::Point<int> p) const
{
    if (! menu.open || ! menu.rect.contains (p)) return -1;
    const int i = (p.y - menu.rect.getY() + menu.scroll) / 18;
    return i >= 0 && i < menuCount() ? i : -1;
}

void Screen::openMenu (int target, juce::Rectangle<int> anchor)
{
    menu.open = true; menu.target = target; menu.scroll = 0;
    const int h = std::min (18 * menuCount(), std::max (90, getHeight() - anchor.getBottom() - 30));
    int y = anchor.getBottom() + 2;
    if (y + h > getHeight() - 8) y = std::max (8, anchor.getY() - h - 2);
    menu.rect = { anchor.getX(), y, std::max (anchor.getWidth(), 200), h };
    const int pinned = session.quad.pins[(size_t) Session::kCornerPin[target]];
    if (pinned > 4) menu.scroll = std::min (pinned * 18 - 36, std::max (0, 18 * menuCount() - h));
    repaint();
}

void Screen::paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width, bool fill) const
{
    const auto db = responseDb (words, hz);
    juce::Path p;
    for (size_t i = 0; i < hz.size(); ++i)
    {
        const float x = (float) xOf (hz[i], r), y = (float) std::clamp (yOf (db[i], r), (double) r.getY(), (double) r.getBottom());
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    if (fill)
    {
        juce::Path area (p);
        area.lineTo ((float) xOf (hz.back(), r), (float) r.getBottom());
        area.lineTo ((float) xOf (hz.front(), r), (float) r.getBottom());
        area.closeSubPath();
        g.setColour (kFill); g.fillPath (area);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (width));
}

void Screen::paintStage (juce::Graphics& g) const
{
    const auto r = stage;
    g.setColour (kGrid);
    for (double f = 100.0; f < 20000.0; f *= 10.0) g.fillRect ((int) std::round (xOf (f, r)), r.getY(), 1, r.getHeight());
    for (int db = -20; db <= 20; db += 10) g.fillRect (r.getX(), (int) std::round (yOf (db, r)), r.getWidth(), 1);
    for (int i = 1; i < 8; ++i) g.fillRect (r.getX() + i * r.getWidth() / 8, r.getY(), 1, r.getHeight());
    g.setColour (kDim); g.fillRect (r.getX(), (int) std::round (yOf (0.0, r)), r.getWidth(), 1);
    if (session.sounding) paintCurve (g, r, session.words, kGreen, 2.2f, true);
    if (session.quad.complete() && session.auditioning == -1)
    {
        const auto pk = puckPoint();
        g.setColour (kDim.withAlpha (0.9f));
        for (int n = 0; n < 4; ++n) { const auto c = cornerPoint (n); g.drawLine (pk.x, pk.y, c.x, c.y, 1.0f); }
        juce::Path diamond;
        diamond.addQuadrilateral (pk.x, pk.y - 9.0f, pk.x + 9.0f, pk.y, pk.x, pk.y + 9.0f, pk.x - 9.0f, pk.y);
        g.setColour (kBack); g.fillPath (diamond);
        g.setColour (kGreen); g.strokePath (diamond, juce::PathStrokeType (1.6f));
        g.fillEllipse (pk.x - 2.5f, pk.y - 2.5f, 5.0f, 5.0f);
    }
}

void Screen::paintCorners (juce::Graphics& g) const
{
    g.setFont (mono (11.0f));
    for (int n = 0; n < 4; ++n)
    {
        const auto c = cornerPoint (n);
        const bool right = n == 1 || n == 3, bottom = n >= 2;
        g.setColour (kGreen);
        g.fillRect ((int) c.x - (right ? 1 : 0), (int) c.y - (bottom ? 14 : 0), 1, 14);
        g.fillRect ((int) c.x - (right ? 10 : 0), (int) c.y - (bottom ? 1 : 0), 10, 1);
        const auto box = cornerBox[(size_t) n];
        const juce::String name = session.cornerName (n);
        g.setColour (kBox); g.fillRect (box);
        g.setColour (kText);
        g.drawText (name.isNotEmpty() ? name : juce::String ("choose"), box.reduced (6, 0).withTrimmedRight (14), juce::Justification::centredLeft);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), box.withTrimmedLeft (box.getWidth() - 16), juce::Justification::centred);
        g.setColour (session.editing == n ? kGreen : kDim);
        g.drawText ("rows", cornerTag[(size_t) n], right ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
    const char* names[] = { "PLAY", "SAW", "NOISE" };
    const bool on[] = { session.playing, session.source == 0, session.source == 1 };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (on[i] ? kGreen : kDim);
        g.drawText (names[i], keys[(size_t) i], juce::Justification::centredLeft);
    }
    if (session.status.isNotEmpty())
    {
        g.setColour (kDim);
        g.drawText (session.status, stage.getX() + 180, getHeight() - 26, stage.getWidth() - 180, 18, juce::Justification::centredRight);
    }
}

void Screen::paintPicker (juce::Graphics& g) const
{
    g.setColour (kGrid); g.fillRect (picker.getRight() - 1, 0, 1, getHeight());
    g.setFont (mono (10.0f));
    for (double f : { 200.0, 300.0, 500.0, 700.0, 1000.0 })
    {
        const int x = (int) std::round (chartPoint (f, kF2Low).x);
        g.setColour (kGrid); g.fillRect (x, chart.getY(), 1, chart.getHeight());
        g.setColour (kDim); g.drawText (juce::String ((int) f), x - 20, chart.getBottom() + 4, 40, 12, juce::Justification::centred);
    }
    for (double f : { 500.0, 700.0, 1000.0, 1500.0, 2000.0, 3000.0 })
    {
        const int y = (int) std::round (chartPoint (kF1Low, f).y);
        g.setColour (kGrid); g.fillRect (chart.getX(), y, chart.getWidth(), 1);
        g.setColour (kDim); g.drawText (juce::String ((int) f), chart.getX() - 42, y - 6, 38, 12, juce::Justification::centredRight);
    }
    g.setColour (kDim);
    g.drawText ("F1", chart.getRight() - 20, chart.getBottom() + 4, 20, 12, juce::Justification::centredRight);
    g.drawText ("F2", chart.getX() - 42, chart.getY() - 16, 38, 12, juce::Justification::centredRight);
    const auto origin = chartPoint (kSchwaF1, kSchwaF2);
    g.setColour (kDim.withAlpha (0.8f));
    g.drawLine (origin.x, (float) chart.getY(), origin.x, (float) chart.getBottom(), 1.0f);
    g.drawLine ((float) chart.getX(), origin.y, (float) chart.getRight(), origin.y, 1.0f);
    g.setFont (mono (11.0f));
    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        const auto& s = session.stars[(size_t) k];
        const auto f = formantsOf (s.words);
        if (f[0] <= 0.0 || f[1] <= 0.0) continue;
        const auto p = chartPoint (f[0], f[1]);
        const bool lit = k == session.selected || k == session.hovered || k == session.auditioning;
        bool pinned = false;
        for (int pin : session.quad.pins) pinned = pinned || pin == k;
        const auto ink = inkOf (s);
        g.setColour (lit ? ink : ink.withAlpha (pinned ? 0.9f : 0.55f));
        if (pinned) g.fillEllipse (p.x - 4.5f, p.y - 4.5f, 9.0f, 9.0f);
        else g.drawEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, lit ? 2.0f : 1.0f);
        g.setColour (lit ? ink : kText.withAlpha (0.8f));
        g.drawText (s.name, (int) p.x + 8, (int) p.y - 7, 90, 14, juce::Justification::centredLeft);
    }
    if (session.madeLive)
    {
        const auto f = formantsOf (session.made.words);
        const auto p = chartPoint (f[0], f[1]);
        g.setColour (session.inMade() ? kGreen : kDim);
        g.drawEllipse (p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f, 1.5f);
        g.drawLine (p.x - 10.0f, p.y, p.x + 10.0f, p.y, 1.0f);
        g.drawLine (p.x, p.y - 10.0f, p.x, p.y + 10.0f, 1.0f);
        g.drawText (session.made.name, (int) p.x + 10, (int) p.y + 6, 90, 14, juce::Justification::centredLeft);
    }
    if (dragging == Drag::card && dragStar >= 0)
    {
        const auto& s = session.stars[(size_t) dragStar];
        juce::Rectangle<int> ghost (dragPoint.x - 40, dragPoint.y - 18, 80, 36);
        g.setColour (kBack.withAlpha (0.9f)); g.fillRect (ghost);
        g.setColour (inkOf (s)); g.drawRect (ghost);
        paintCurve (g, ghost.reduced (3, 3), s.words, inkOf (s), 1.0f, false);
    }
}

void Screen::paintTable (juce::Graphics& g) const
{
    if (session.editing < 0) return;
    const auto words = session.editWords();
    g.setColour (kBack.withAlpha (0.92f)); g.fillRect (table);
    g.setColour (kDim); g.drawRect (table);
    g.setFont (mono (10.5f));
    const char* heads[kColumns] = { "", "TYPE", "NOTE", "HZ", "GAIN" };
    g.setColour (kDim);
    for (int c = 0; c < kColumns; ++c) g.drawText (heads[c], cell (-1, c), juce::Justification::centredLeft);
    for (int r = 0; r < kRows; ++r)
    {
        const auto& w = words[(size_t) r];
        const auto row = rowOf (w);
        const double hzValue = rowHz (w), db = rowDb (w);
        const bool rest = row.type == RowType::rest;
        const bool lit = dragging == Drag::row && dragRow == r;
        g.setColour (lit ? kGreen : rest ? kDim : kText);
        g.drawText (juce::String (r + 1), cell (r, 0), juce::Justification::centredLeft);
        g.drawText (rest ? "rest" : row.type == RowType::notch ? "notch" : "peak", cell (r, 1), juce::Justification::centredLeft);
        g.drawText (noteName (hzValue), cell (r, 2), juce::Justification::centredLeft);
        g.drawText (rest ? "" : juce::String (hzValue, hzValue < 1000.0 ? 1 : 0), cell (r, 3), juce::Justification::centredLeft);
        g.drawText (rest ? "" : juce::String (db, 1) + " dB", cell (r, 4), juce::Justification::centredLeft);
    }
}

void Screen::paintMenu (juce::Graphics& g) const
{
    if (! menu.open) return;
    g.setColour (kBox); g.fillRect (menu.rect);
    g.setColour (kDim); g.drawRect (menu.rect);
    g.setFont (mono (11.0f));
    const int first = menu.scroll / 18, last = std::min (menuCount(), first + menu.rect.getHeight() / 18 + 2);
    g.saveState();
    g.reduceClipRegion (menu.rect);
    for (int i = first; i < last; ++i)
    {
        juce::Rectangle<int> line (menu.rect.getX(), menu.rect.getY() + i * 18 - menu.scroll, menu.rect.getWidth(), 18);
        const bool current = session.quad.pins[(size_t) Session::kCornerPin[menu.target]] == i;
        if (current) { g.setColour (kDim); g.fillRect (line); }
        g.setColour (inkOf (session.stars[(size_t) i]));
        g.drawText (menuItem (i), line.reduced (8, 0), juce::Justification::centredLeft);
    }
    g.restoreState();
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (kBack);
    paintStage (g);
    paintCorners (g);
    paintPicker (g);
    paintTable (g);
    paintMenu (g);
}

void Screen::mouseMove (const juce::MouseEvent& e)
{
    if (dragging != Drag::none) return;
    session.hover (chart.contains (e.getPosition()) ? pointAt (e.getPosition()) : -1);
}

void Screen::mouseExit (const juce::MouseEvent&) { if (dragging == Drag::none) session.unhover(); }

void Screen::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    grabKeyboardFocus();
    if (menu.open)
    {
        const int i = menuItemAt (p);
        const int target = menu.target;
        menu.open = false;
        if (i >= 0) session.pinCorner (target, i);
        repaint();
        return;
    }
    for (int i = 0; i < 3; ++i)
        if (keys[(size_t) i].contains (p))
        {
            if (i == 0) session.setPlaying (! session.playing); else session.setSource (i == 1 ? 0 : 1);
            return;
        }
    for (int n = 0; n < 4; ++n) if (cornerBox[(size_t) n].contains (p)) { openMenu (n, cornerBox[(size_t) n]); return; }
    for (int n = 0; n < 4; ++n) if (cornerTag[(size_t) n].contains (p)) { session.edit (n); return; }
    if (session.editing >= 0 && table.contains (p))
    {
        for (int r = 0; r < kRows; ++r)
            for (int c = 1; c < kColumns; ++c)
                if (cell (r, c).contains (p))
                {
                    const auto words = session.editWords();
                    Row row = rowOf (words[(size_t) r]);
                    session.beginRowEdit();
                    if (c == 1)
                    {
                        row.type = row.type == RowType::rest ? RowType::peak : row.type == RowType::peak ? RowType::notch : RowType::rest;
                        if (row.type == RowType::peak && r == kRows - 1 && rowHz (words[(size_t) r]) <= 0.0) { row.type = RowType::notch; row.f = kFreqCodes - 1; row.g = 0; }
                        session.setRow (session.editing, r, row);
                        return;
                    }
                    if (row.type == RowType::rest) { row.type = RowType::peak; session.setRow (session.editing, r, row); }
                    dragging = Drag::row; dragRow = r; dragColumn = c; dragOrigin = p; dragPoint = p; dragBase = row;
                    return;
                }
        return;
    }
    if (chart.expanded (8, 8).contains (p))
    {
        if (const int k = pointAt (p); k >= 0)
        {
            session.select (k);
            dragging = Drag::card; dragStar = k; dragOrigin = p; dragPoint = p;
            return;
        }
        dragging = Drag::made;
        const auto f = formantsAt (p);
        session.setMade (f.first, f.second);
        return;
    }
    if (stage.expanded (12, 12).contains (p) && session.quad.complete())
    {
        dragging = Drag::puck;
        mouseDrag (e);
    }
}

void Screen::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    dragPoint = p;
    if (dragging == Drag::row)
    {
        Row row = dragBase;
        const int steps = (p.x - dragOrigin.x) / 4;
        if (dragColumn == 4) row.g = std::clamp (dragBase.g + steps, kGainMin, kGainMax);
        else row.f = std::clamp (dragBase.f + steps, 0, kFreqCodes - 1);
        session.setRow (session.editing, dragRow, row);
        return;
    }
    if (dragging == Drag::puck)
    {
        session.setPuck ((p.x - stage.getX()) * 100.0 / stage.getWidth(), (stage.getBottom() - p.y) * 100.0 / stage.getHeight());
        return;
    }
    if (dragging == Drag::made)
    {
        const auto f = formantsAt (p);
        session.setMade (f.first, f.second);
        return;
    }
    if (dragging == Drag::card) repaint();
}

void Screen::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragging == Drag::card)
    {
        if (const int n = cornerAt (p); n >= 0 && dragStar >= 0) session.pinCorner (n, dragStar);
    }
    dragging = Drag::none; dragStar = -1; dragRow = -1; dragColumn = -1;
    repaint();
}

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (menu.open && menu.rect.contains (e.getPosition()))
    {
        menu.scroll = std::clamp (menu.scroll + (wheel.deltaY > 0 ? -54 : 54), 0, std::max (0, 18 * menuCount() - menu.rect.getHeight()));
        repaint();
    }
}

bool Screen::keyPressed (const juce::KeyPress& k)
{
    if (menu.open && k.getKeyCode() == juce::KeyPress::escapeKey) { menu.open = false; repaint(); return true; }
    const bool used = session.key (k);
    if (used) repaint();
    return used;
}

bool Screen::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files) if (f.endsWithIgnoreCase (".wav")) return true;
    return false;
}

void Screen::filesDropped (const juce::StringArray& files, int x, int y)
{
    const int corner = cornerAt ({ x, y });
    for (const auto& f : files)
    {
        if (! f.endsWithIgnoreCase (".wav")) continue;
        const int k = session.addRead (juce::File (f));
        if (k >= 0 && corner >= 0) session.pinCorner (corner, k);
    }
}

juce::Image Screen::shot()
{
    juce::Image image (juce::Image::RGB, getWidth(), getHeight(), true);
    juce::Graphics g (image);
    paintEntireComponent (g, false);
    return image;
}
}
