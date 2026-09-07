#include "Screen.h"
#include "Look.h"
#include <algorithm>
#include <cmath>

namespace hs
{
Screen::Screen (Session& s)
    : palette (s, curves), mother (s, curves), stage (s, curves), body (s, curves), engine (s, curves), keyboard (s), session (s)
{
    setSize (1120, 700);
    setWantsKeyboardFocus (true);
    tapOut.assign (16384, 0.0f);
    tapIn.assign (16384, 0.0f);
    layout();
    startTimer (30);
    session.onChange = [this] {
        if (session.selected >= 0 && session.selected < (int) session.stars.size()) palette.followKind (session.stars[(size_t) session.selected].kind);
        layout(); repaint();
    };
}

void Screen::resized() { layout(); repaint(); }

void Screen::layout()
{
    const int w = std::max (1000, getWidth()), h = std::max (640, getHeight());
    const juce::Rectangle<int> grid (16, 32, w - 32, h - 44);
    const int colX = grid.getX() + grid.getWidth() * 52 / 100, rowY = grid.getY() + grid.getHeight() * 60 / 100;
    mother.layout ({ grid.getX(), grid.getY(), colX - 12 - grid.getX(), rowY - 12 - grid.getY() });
    stage.layout ({ colX + 12, grid.getY(), grid.getRight() - colX - 12, rowY - 12 - grid.getY() });
    palette.layout ({ grid.getX(), rowY + 12, colX - 12 - grid.getX(), grid.getBottom() - rowY - 12 });
    bottom = { colX + 12, rowY + 12, grid.getRight() - colX - 12, grid.getBottom() - rowY - 12 };
    const int bodySide = std::max (80, std::min (bottom.getHeight() - 54, 176));
    body.layout ({ bottom.getX(), bottom.getY(), bodySide, bodySide });
    engine.layout ({ body.area.getRight() + 20, bottom.getY(), bottom.getRight() - body.area.getRight() - 20, bodySide });
    const int keyboardTop = body.area.getBottom() + 10;
    keyboard.layout ({ bottom.getX(), keyboardTop, bottom.getWidth(), std::max (44, bottom.getBottom() - keyboardTop) });
}

void Screen::timerCallback()
{
    if (! session.withAudio || ! session.audio.isOpen()) return;
    const int out = session.audio.pull (tapOut.data(), (int) tapOut.size());
    const int in = session.audio.pullInput (tapIn.data(), (int) tapIn.size());
    const int n = std::min (out, in);
    if (n <= 0) return;
    engine.feed (tapOut.data(), tapIn.data(), n);
    repaint (engine.area);
    repaint (stage.area);
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (Look::ground);
    g.setColour (Look::grid);
    for (const auto& r : { mother.area, stage.area, palette.area, bottom }) g.drawRect (r.expanded (6, 6));
    g.setFont (Look::font (13.0f));
    g.setColour (Look::dim);
    g.drawText ("HEADSPACE", juce::Rectangle<int> (16, 6, 200, 22), juce::Justification::centredLeft);
    const bool dropping = dragging == Drag::card || dragging == Drag::sound;
    mother.paint (g, dragPoint, dropping);
    stage.paint (g, dragging == Drag::peak || dragging == Drag::zero ? dragRow : -1, dragging == Drag::blade, dragging == Drag::carve);
    engine.paintLive (g, stage.magnitude);
    palette.paint (g, dragging == Drag::transpose ? dragStar : -1);
    body.paint (g, dropping ? body.cornerAt (dragPoint) : -1);
    engine.paint (g);
    keyboard.paint (g);
    paintGhost (g);
    menu.paint (g, session);
}

void Screen::paintGhost (juce::Graphics& g) const
{
    if (dragging == Drag::sound)
    {
        if (! session.sounding) return;
        juce::Rectangle<int> ghost (dragPoint.x - 36, dragPoint.y - 16, 72, 32);
        g.setColour (Look::panel.withAlpha (0.92f)); g.fillRect (ghost);
        g.setColour (Look::blue); g.drawRect (ghost);
        curves.draw (g, ghost.reduced (3, 3), session.heard, Look::blue, 1.0f, false);
        return;
    }
    if (dragging != Drag::card || dragStar < 0) return;
    const Words words = session.stars[(size_t) dragStar].words;
    const auto ink = plot::inkOf (session.stars[(size_t) dragStar]);
    juce::Rectangle<int> ghost (dragPoint.x - 36, dragPoint.y - 16, 72, 32);
    g.setColour (Look::panel.withAlpha (0.92f)); g.fillRect (ghost);
    g.setColour (ink); g.drawRect (ghost);
    curves.draw (g, ghost.reduced (3, 3), words, ink, 1.0f);
}

void Screen::mouseMove (const juce::MouseEvent& e)
{
    if (dragging != Drag::none) return;
    session.hover (palette.chart.contains (e.getPosition()) ? palette.pointAt (e.getPosition()) : -1);
}

void Screen::mouseExit (const juce::MouseEvent&) { if (dragging == Drag::none) session.unhover(); }

void Screen::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (isShowing()) grabKeyboardFocus();
    if (menu.open)
    {
        const int i = menu.itemAt (session, p);
        const int target = menu.target, kind = menu.kind;
        menu.open = false;
        if (i >= 0) { if (kind == 1) session.setPair (target, i); else session.pinCorner (target, i); }
        repaint();
        return;
    }
    if (const int i = engine.sourceAt (p); i >= 0)
    {
        if (i == 0) session.setPlaying (! session.playing);
        else if (i == 1) session.setSource (3);
        else if (i == 2) session.setSource (0);
        else if (i == 3) session.setSource (1);
        else if (i == 4) { if (session.loopName.isNotEmpty()) session.setSource (2); }
        else if (i == 5) session.setTracking (! session.fixedPitch);
        else session.write();
        return;
    }
    if (const int i = engine.routeAt (p); i >= 0)
    {
        const auto r = session.route (i);
        session.setRoute (i, ! r.on, r.depth);
        return;
    }
    if (engine.plot.contains (p)) { dragging = Drag::sound; dragOrigin = p; dragPoint = p; return; }
    if (keyboard.area.contains (p))
    {
        if (const int midi = keyboard.noteAt (p); midi >= 0) { dragging = Drag::keyboard; session.noteOn (midi); }
        return;
    }
    if (const int n = body.tagAt (p); n >= 0)
    {
        session.edit (n);
        if (p.x >= body.tag[(size_t) n].getRight() - 14) { palette.finding = true; palette.find.clear(); palette.showTab (0); }
        repaint();
        return;
    }
    if (body.area.contains (p) && session.quad.complete()) { dragging = Drag::puck; mouseDrag (e); return; }
    if (const int i = mother.tagAt (p); i >= 0)
    {
        session.editAnchor (i);
        if (p.x >= mother.tag[(size_t) i].getRight() - 14) { palette.finding = true; palette.find.clear(); palette.showTab (0); }
        repaint();
        return;
    }
    if (const int i = mother.cellAt (p); i >= 0)
    {
        session.editAnchor (i);
        return;
    }
    if (const int i = mother.railAt (p); i >= 0) { dragging = Drag::rail; dragRow = i; mouseDrag (e); return; }
    if (stage.carveKey.contains (p))
    {
        dragging = Drag::carve; dragOrigin = p; dragPoint = p;
        editStarted = false; stage.carve = 0.0;
        dragWords = stage.words();
        return;
    }
    if (const int row = stage.peakAt (p); row >= 0)
    {
        dragging = Drag::peak; dragRow = row; dragOrigin = p; dragPoint = p;
        editStarted = false; dragWords = stage.words();
        return;
    }
    if (const int row = stage.zeroAt (p); row >= 0)
    {
        dragging = Drag::zero; dragRow = row; dragOrigin = p; dragPoint = p;
        editStarted = false; dragWords = stage.words();
        return;
    }
    if (const float blade = stage.bladeX(); blade >= 0.0f && stage.magnitude.contains (p) && std::abs (p.x - blade) < 6.0f)
    {
        dragging = Drag::blade; dragOrigin = p; dragPoint = p;
        editStarted = false; dragWords = stage.words();
        return;
    }
    if (stage.modeKey.contains (p)) { stage.zerosMode = ! stage.zerosMode; repaint(); return; }
    if (stage.magnitude.contains (p) && (e.mods.isAltDown() || stage.zerosMode))
    {
        const auto words = stage.words();
        const double hzValue = plot::hzAt (p.x, stage.magnitude);
        int row = -1;
        double nearest = 1e9;
        for (int r = 0; r < kRows; ++r)
        {
            const auto s = sectionOf (words[(size_t) r]);
            if (s.zero) continue;
            const double d = s.pole ? std::abs (std::log (s.poleHz / hzValue)) : 1e8 + r;
            if (d < nearest) { nearest = d; row = r; }
        }
        if (row < 0) return;
        Section s = sectionOf (words[(size_t) row]);
        s.zero = true; s.zeroHz = hzValue; s.zeroRadius = 0.5;
        session.beginRowEdit();
        session.setSection (session.target(), row, s);
        return;
    }
    if (const int tab = palette.tabAt (p); tab >= 0) { palette.showTab (tab); repaint(); return; }
    if (palette.keepKey.contains (p)) { session.keep(); return; }
    if (const auto family = palette.headerAt (p); family.isNotEmpty()) { palette.toggle (family); repaint(); return; }
    if (const int k = palette.cardAt (p); k >= 0)
    {
        session.placeInTarget (k);
        dragging = Drag::card; dragStar = k; dragOrigin = p; dragPoint = p;
        return;
    }
    if (palette.chart.expanded (8, 8).contains (p))
    {
        if (const int k = palette.pointAt (p); k >= 0)
        {
            if (e.mods.isShiftDown())
            {
                dragging = Drag::transpose; dragStar = k; dragOrigin = p; dragPoint = p;
                session.setTransposed (k, formantsOf (session.stars[(size_t) k].words)[0]);
                return;
            }
            session.placeInTarget (k);
            dragging = Drag::card; dragStar = k; dragOrigin = p; dragPoint = p;
            return;
        }
        dragging = Drag::made;
        const auto f = palette.formantsAt (p);
        session.setMade (f.first, f.second);
    }
}

void Screen::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (! stage.magnitude.contains (p) || stage.peakAt (p) >= 0 || stage.zeroAt (p) >= 0) return;
    const auto words = stage.words();
    for (int row = 0; row + 1 < kRows; ++row)
    {
        const auto s = sectionOf (words[(size_t) row]);
        if (s.pole || s.zero) continue;
        session.beginRowEdit();
        session.setSection (session.target(), row, Stage::seed (plot::hzAt (p.x, stage.magnitude)));
        return;
    }
}

void Screen::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    dragPoint = p;
    if (dragging == Drag::carve)
    {
        const double amount = std::clamp ((p.x - dragOrigin.x) / 240.0, 0.0, 1.0);
        if (amount == stage.carve) return;
        stage.carve = amount;
        const auto words = carved (dragWords, amount);
        if (! editStarted) { session.beginRowEdit(); editStarted = true; }
        for (size_t r = 0; r + 1 < kRows; ++r)
            if (words[r] != stage.words()[r]) session.setSectionWords (session.target(), (int) r, words[r]);
        return;
    }
    if (dragging == Drag::peak || dragging == Drag::zero)
    {
        if (p == dragOrigin && ! editStarted) return;
        auto g = trench::core::geometry_from_words (dragWords[(size_t) dragRow], trench::core::kP2kDatumHz);
        const double hz = std::clamp (plot::hzAt (p.x, stage.magnitude), 20.0, 20000.0);
        const double dyDb = plot::dbAt (p.y, stage.magnitude) - plot::dbAt (dragOrigin.y, stage.magnitude);
        if (dragging == Drag::peak)
        {
            const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
            const double r0 = pole != nullptr ? pole->radius : 0.9;
            g.pole = trench::core::ConjugatePair { hz, std::clamp (1.0 - (1.0 - r0) * std::pow (10.0, -dyDb / 20.0), 0.05, 0.99995) };
        }
        else
        {
            const auto* zero = std::get_if<trench::core::ConjugatePair> (&g.zero);
            const double r0 = zero != nullptr ? zero->radius : 0.0;
            const bool floor = p.y >= stage.magnitude.getBottom() - 2;
            g.zero = trench::core::ConjugatePair { hz, floor ? 1.0 : std::clamp (1.0 - (1.0 - r0) * std::pow (10.0, dyDb / 20.0), 0.0, 0.9999) };
        }
        auto w = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
        w[4] = dragWords[(size_t) dragRow][4];
        if (w != stage.words()[(size_t) dragRow])
        {
            if (! editStarted) { session.beginRowEdit(); editStarted = true; }
            session.setSectionWords (session.target(), dragRow, w, false);
        }
        return;
    }
    if (dragging == Drag::blade)
    {
        Section s = sectionOf (dragWords[kRows - 1]);
        s.zero = true; s.zeroHz = plot::hzAt (p.x, stage.magnitude); s.zeroRadius = 1.0;
        const auto w = sectionWords (s, dragWords[kRows - 1][4]);
        if (w != stage.words()[kRows - 1])
        {
            if (! editStarted) { session.beginRowEdit(); editStarted = true; }
            session.setSectionWords (session.target(), kRows - 1, w);
        }
        return;
    }
    if (dragging == Drag::puck)
    {
        session.setPuck ((p.x - body.area.getX()) * 100.0 / body.area.getWidth(), (body.area.getBottom() - p.y) * 100.0 / body.area.getHeight());
        return;
    }
    if (dragging == Drag::rail)
    {
        const double v = mother.valueAt (dragRow, p);
        if (dragRow == 0) session.sweep (v);
        else if (dragRow == 1) session.setProbe (session.pairT, v, session.stress);
        else session.setProbe (session.pairT, session.frequency, v);
        return;
    }
    if (dragging == Drag::made)
    {
        const auto f = palette.formantsAt (p);
        session.setMade (f.first, f.second);
        return;
    }
    if (dragging == Drag::transpose)
    {
        if (dragStar >= 0) session.setTransposed (dragStar, palette.formantsAt (p).first);
        return;
    }
    if (dragging == Drag::keyboard)
    {
        if (const int midi = keyboard.noteAt (p); midi >= 0 && midi != session.note) session.noteOn (midi);
        return;
    }
    if (dragging == Drag::card || dragging == Drag::sound) repaint();
}

void Screen::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragging == Drag::card && dragStar >= 0)
    {
        if (const int n = body.cornerAt (p); n >= 0) { session.pinCorner (n, dragStar); if (e.mods.isShiftDown()) session.pinCorner (n ^ 2, dragStar); }
        else if (const int i = mother.cellAt (p); i >= 0) session.setPair (i, dragStar);
    }
    if (dragging == Drag::sound)
    {
        if (const int n = body.cornerAt (p); n >= 0) { if (e.mods.isShiftDown()) session.toColumn (n & 1); else session.toCorner (n); }
        else if (const int i = mother.cellAt (p); i >= 0) { session.keep(); session.setPair (i, session.selected); }
    }
    if (dragging == Drag::keyboard) session.noteOff();
    if (editStarted && (dragging == Drag::peak || dragging == Drag::zero || dragging == Drag::blade || dragging == Drag::carve)) session.relevel();
    dragging = Drag::none; dragStar = -1; dragRow = -1;
    editStarted = false;
    repaint();
}

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto p = e.getPosition();
    const int step = wheel.deltaY > 0 ? -1 : 1;
    if (menu.open && menu.rect.contains (p)) { menu.wheel (session, step); repaint(); return; }
    if (const int i = engine.depthAt (p); i >= 0)
    {
        const auto r = session.route (i);
        session.setRoute (i, r.on, r.depth + (wheel.deltaY > 0 ? 0.1 : -0.1));
        return;
    }
    if (mother.rail[1].expanded (0, 6).contains (p) || mother.onOctaves (p)) { session.setOctaves (session.octaves + (wheel.deltaY > 0 ? 0.25 : -0.25)); return; }
    if (const int row = stage.peakAt (p); row >= 0)
    {
        Section s = sectionOf (stage.words()[(size_t) row]);
        const double width = std::max (0.05, widthSt (s.poleHz, s.poleRadius) * std::pow (1.06, wheel.deltaY > 0 ? -1.0 : 1.0));
        s.poleRadius = radiusForWidth (s.poleHz, width);
        const auto now = juce::Time::currentTimeMillis();
        if (now - wheelTime > 800) session.beginRowEdit();
        wheelTime = now;
        session.setSection (session.target(), row, s);
        return;
    }
    if (palette.picker.contains (p)) { palette.scrollBy (step * 68); repaint(); }
}

bool Screen::keyStateChanged (bool)
{
    static const char* const row = "zsxdcvgbhnjm,l.";
    bool any = false;
    for (int i = 0; row[i] != 0; ++i)
    {
        const int code = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) row[i]);
        const bool down = juce::KeyPress::isKeyCurrentlyDown (code);
        const int midi = session.keyOctave + i;
        if (down && ! heldKeys.count (midi)) continue;
        if (! down && heldKeys.count (midi)) { heldKeys.erase (midi); session.keyNoteOff (midi); any = true; }
    }
    if (any) repaint();
    return any;
}

bool Screen::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const bool plain = ! m.isCommandDown() && ! m.isCtrlDown() && ! m.isAltDown();
    const bool ctrl = m.isCommandDown() || m.isCtrlDown();
    if (k.getKeyCode() == 'H' && ctrl) { stage.showHardware = ! stage.showHardware; repaint(); return true; }
    if (k.getKeyCode() == 'G' && ctrl && onSpectrogram) { onSpectrogram(); return true; }
    if (menu.open && k.getKeyCode() == juce::KeyPress::escapeKey) { menu.open = false; repaint(); return true; }
    if (palette.finding)
    {
        const auto ch = k.getTextCharacter();
        if (k.getKeyCode() == juce::KeyPress::escapeKey) { palette.finding = false; palette.find.clear(); }
        else if (k.getKeyCode() == juce::KeyPress::backspaceKey) palette.find = palette.find.dropLastCharacters (1);
        else if (k.getKeyCode() == juce::KeyPress::returnKey) { const auto shown = palette.cards(); if (! shown.empty()) session.placeInTarget (shown[0]); palette.finding = false; palette.find.clear(); palette.layout (palette.area); }
        else if (ch >= 32 && ch < 127) palette.find += juce::String::charToString (ch);
        palette.layout (palette.area);
        repaint();
        return true;
    }
    if (k.getTextCharacter() == '/' && plain) { palette.finding = true; palette.find.clear(); palette.showTab (0); repaint(); return true; }
    if (k.getKeyCode() == juce::KeyPress::returnKey) { session.toCorner (session.working); return true; }
    const int heldBefore = session.heldNote;
    const bool used = session.key (k);
    if (used && session.heldNote != heldBefore && session.heldNote >= 0 && plain) heldKeys.insert (session.heldNote);
    if (used) repaint();
    return used;
}

namespace
{
bool audible (const juce::String& f) { return f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".aif"); }
}

bool Screen::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files) if (audible (f)) return true;
    return false;
}

void Screen::filesDropped (const juce::StringArray& files, int x, int y)
{
    const juce::Point<int> p (x, y);
    const int corner = body.cornerAt (p), end = mother.cellAt (p);
    const bool asLoop = corner < 0 && end < 0 && ! palette.area.contains (p);
    const bool asFit = juce::ModifierKeys::getCurrentModifiers().isAltDown();
    for (const auto& f : files)
    {
        if (! audible (f)) continue;
        if (asLoop) { session.setLoop (juce::File (f)); continue; }
        const int k = asFit ? session.addFit (juce::File (f)) : session.addRead (juce::File (f));
        if (k < 0) continue;
        if (corner >= 0) session.pinCorner (corner, k);
        else if (end >= 0) session.setPair (end, k);
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
