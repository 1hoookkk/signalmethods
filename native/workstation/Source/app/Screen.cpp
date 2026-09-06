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
    const int railW = w < 1000 ? 100 : 124;
    rails[0] = { 0, 0, railW, h };
    rails[1] = { w - railW, 0, railW, h };
    stage = { railW + 48, 44, w - 2 * railW - 96, h - 44 - 40 };
    presetBox = { w / 2 - 110, 10, 220, 20 };
    const int bw = std::min (150, stage.getWidth() / 3);
    cornerBox[0] = { stage.getX() + 8, stage.getY() + 22, bw, 20 };
    cornerBox[1] = { stage.getRight() - 8 - bw, stage.getY() + 22, bw, 20 };
    cornerBox[2] = { stage.getX() + 8, stage.getBottom() - 42, bw, 20 };
    cornerBox[3] = { stage.getRight() - 8 - bw, stage.getBottom() - 42, bw, 20 };
    for (int i = 0; i < 3; ++i) keys[(size_t) i] = { stage.getX() + i * 56, h - 26, 50, 18 };
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

int Screen::cornerAt (juce::Point<int> p) const
{
    for (int n = 0; n < 4; ++n) if (cornerBox[(size_t) n].expanded (10, 16).contains (p)) return n;
    return -1;
}

std::vector<int> Screen::railStars (int rail) const
{
    std::vector<int> out;
    const int n = (int) session.stars.size();
    if (rail == 0) for (int k = 0; k < (int) session.factoryCount; ++k) out.push_back (k);
    else for (int k = (int) session.factoryCount; k < n; ++k) out.push_back (k);
    return out;
}

juce::Rectangle<int> Screen::cardRect (int rail, int index) const
{
    const auto r = rails[(size_t) rail];
    return { r.getX() + 14, r.getY() + 20 + index * kCard - scroll[(size_t) rail], r.getWidth() - 28, 40 };
}

int Screen::cardAt (juce::Point<int> p, int rail) const
{
    if (! rails[(size_t) rail].contains (p)) return -1;
    const auto list = railStars (rail);
    for (int i = 0; i < (int) list.size(); ++i) if (cardRect (rail, i).expanded (0, 12).contains (p)) return list[(size_t) i];
    return -1;
}

int Screen::menuCount() const { return menu.target == 4 ? (int) session.bodies.size() : (int) session.stars.size(); }

juce::String Screen::menuItem (int i) const
{
    if (menu.target == 4) return i >= 0 && i < (int) session.bodies.size() ? session.bodies[(size_t) i] : juce::String();
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
    if (target < 4)
    {
        const int pinned = session.quad.pins[(size_t) Session::kCornerPin[target]];
        if (pinned > 4) menu.scroll = std::min (pinned * 18 - 36, std::max (0, 18 * menuCount() - h));
    }
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
        const juce::String label = juce::String::charToString ((juce::juce_wchar) Session::kCornerLetters[n]) + (name.isNotEmpty() ? "  READY" : "  EMPTY");
        g.setColour (kGreen);
        g.drawText (label, right ? box.getRight() - 120 : box.getX(), bottom ? box.getBottom() + 4 : box.getY() - 18, 120, 14, right ? juce::Justification::centredRight : juce::Justification::centredLeft);
        g.setColour (kBox); g.fillRect (box);
        g.setColour (kText);
        g.drawText (name.isNotEmpty() ? name : juce::String ("choose"), box.reduced (6, 0).withTrimmedRight (14), juce::Justification::centredLeft);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), box.withTrimmedLeft (box.getWidth() - 16), juce::Justification::centred);
    }
    g.setColour (kBox); g.fillRect (presetBox);
    g.setColour (kText);
    g.drawText ("PRESET", presetBox.reduced (6, 0), juce::Justification::centredLeft);
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), presetBox.withTrimmedLeft (presetBox.getWidth() - 16), juce::Justification::centred);
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

void Screen::paintRails (juce::Graphics& g) const
{
    for (int rail = 0; rail < 2; ++rail)
    {
        const auto list = railStars (rail);
        g.setColour (kGrid); g.fillRect (rails[(size_t) rail].getX() + (rail == 0 ? rails[(size_t) rail].getWidth() - 1 : 0), 0, 1, getHeight());
        for (int i = 0; i < (int) list.size(); ++i)
        {
            const auto r = cardRect (rail, i);
            if (r.getBottom() < -20 || r.getY() > getHeight() + 20) continue;
            const int k = list[(size_t) i];
            const auto& s = session.stars[(size_t) k];
            const bool lit = k == session.selected || k == session.hovered || k == session.auditioning;
            bool pinned = false;
            for (int p : session.quad.pins) pinned = pinned || p == k;
            const auto ink = inkOf (s);
            g.setColour (kBack); g.fillRect (r);
            g.setColour (lit ? ink : pinned ? ink.withAlpha (0.8f) : ink.withAlpha (0.35f)); g.drawRect (r, lit ? 2 : 1);
            paintCurve (g, r.reduced (3, 3), s.words, lit ? ink : ink.withAlpha (0.7f), 1.0f, false);
            g.setFont (mono (9.5f)); g.setColour (lit ? ink : kText.withAlpha (0.7f));
            g.drawText (s.name, r.getX() - 8, r.getY() - 14, r.getWidth() + 16, 12, juce::Justification::centred);
            if (pinned)
            {
                juce::String tags;
                for (int c = 0; c < 4; ++c) if (session.quad.pins[(size_t) Session::kCornerPin[c]] == k) tags += juce::String::charToString ((juce::juce_wchar) Session::kCornerLetters[c]);
                g.setColour (ink); g.drawText (tags, r.getRight() - 30, r.getY() + 2, 26, 12, juce::Justification::centredRight);
            }
        }
    }
    if (dragging == Drag::card && dragStar >= 0)
    {
        const auto& s = session.stars[(size_t) dragStar];
        juce::Rectangle<int> ghost (dragPoint.x - 40, dragPoint.y - 18, 80, 36);
        g.setColour (kBack.withAlpha (0.9f)); g.fillRect (ghost);
        g.setColour (inkOf (s)); g.drawRect (ghost);
        paintCurve (g, ghost.reduced (3, 3), s.words, inkOf (s), 1.0f, false);
    }
    if (dragging == Drag::rail && session.inPair())
    {
        g.setFont (mono (10.0f)); g.setColour (kGreen);
        g.drawText (juce::String (session.pairT * 100.0, 0), dragPoint.x + 12, dragPoint.y - 6, 40, 12, juce::Justification::centredLeft);
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
        const bool current = menu.target < 4 ? session.quad.pins[(size_t) Session::kCornerPin[menu.target]] == i : false;
        if (current) { g.setColour (kDim); g.fillRect (line); }
        g.setColour (menu.target == 4 ? kText : inkOf (session.stars[(size_t) i]));
        g.drawText (menuItem (i), line.reduced (8, 0), juce::Justification::centredLeft);
    }
    g.restoreState();
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (kBack);
    paintStage (g);
    paintCorners (g);
    paintRails (g);
    paintMenu (g);
}

void Screen::mouseMove (const juce::MouseEvent& e)
{
    if (dragging != Drag::none) return;
    const auto p = e.getPosition();
    int k = -1;
    for (int rail = 0; rail < 2 && k < 0; ++rail) k = cardAt (p, rail);
    session.hover (k);
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
        if (i >= 0)
        {
            if (target == 4) session.loadPreset (session.bodies[(size_t) i]);
            else session.pinCorner (target, i);
        }
        repaint();
        return;
    }
    for (int i = 0; i < 3; ++i)
        if (keys[(size_t) i].contains (p))
        {
            if (i == 0) session.setPlaying (! session.playing); else session.setSource (i == 1 ? 0 : 1);
            return;
        }
    if (presetBox.contains (p)) { openMenu (4, presetBox); return; }
    for (int n = 0; n < 4; ++n) if (cornerBox[(size_t) n].contains (p)) { openMenu (n, cornerBox[(size_t) n]); return; }
    for (int rail = 0; rail < 2; ++rail)
        if (const int k = cardAt (p, rail); k >= 0)
        {
            session.select (k);
            dragging = Drag::rail; dragStar = k; dragRail = rail; dragOrigin = p; dragPoint = p;
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
    if (dragging == Drag::puck)
    {
        session.setPuck ((p.x - stage.getX()) * 100.0 / stage.getWidth(), (stage.getBottom() - p.y) * 100.0 / stage.getHeight());
        return;
    }
    if (dragging == Drag::rail || dragging == Drag::card)
    {
        const auto rail = rails[(size_t) dragRail];
        if (rail.expanded (30, 0).contains (p) && dragging == Drag::rail)
        {
            const auto list = railStars (dragRail);
            int from = -1;
            for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i] == dragStar) from = i;
            if (from < 0) return;
            const int dir = p.y < dragOrigin.y ? -1 : 1;
            const int to = from + dir;
            if (to < 0 || to >= (int) list.size()) return;
            const float span = (float) std::abs (cardRect (dragRail, to).getCentreY() - cardRect (dragRail, from).getCentreY());
            const float travel = (float) std::abs (p.y - dragOrigin.y);
            session.morphPair (dragStar, list[(size_t) to], span > 1.0f ? travel / span : 0.0);
            repaint();
            return;
        }
        dragging = Drag::card;
        repaint();
    }
}

void Screen::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragging == Drag::card)
    {
        if (const int n = cornerAt (p); n >= 0 && dragStar >= 0) session.pinCorner (n, dragStar);
    }
    dragging = Drag::none; dragStar = -1; dragRail = -1;
    repaint();
}

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int lines = wheel.deltaY > 0 ? -kCard : kCard;
    if (menu.open && menu.rect.contains (e.getPosition()))
    {
        menu.scroll = std::clamp (menu.scroll + (wheel.deltaY > 0 ? -54 : 54), 0, std::max (0, 18 * menuCount() - menu.rect.getHeight()));
        repaint();
        return;
    }
    for (int rail = 0; rail < 2; ++rail)
        if (rails[(size_t) rail].contains (e.getPosition()))
        {
            const int total = (int) railStars (rail).size() * kCard;
            scroll[(size_t) rail] = std::clamp (scroll[(size_t) rail] + lines, 0, std::max (0, total - getHeight() + 40));
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
