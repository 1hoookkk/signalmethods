#include "Screen.h"
#include "Look.h"
#include <cmath>

namespace hs
{
Screen::Screen (Session& s)
    : palette (s, curves), mother (s, curves), stage (s, curves), body (s, curves), engine (s, curves), keyboard (s), session (s)
{
    setSize (1120, 700);
    setWantsKeyboardFocus (true);
    layout();
    session.onChange = [this] {
        if (session.selected >= 0 && session.selected < (int) session.stars.size()) palette.followKind (session.stars[(size_t) session.selected].kind);
        layout(); repaint();
    };
}

void Screen::resized() { layout(); repaint(); }

void Screen::layout()
{
    const int w = std::max (1000, getWidth()), h = std::max (640, getHeight());
    const juce::Rectangle<int> grid (20, 36, w - 40, h - 56);
    const int colX = grid.getX() + grid.getWidth() * 58 / 100, rowY = grid.getY() + grid.getHeight() * 60 / 100;
    mother.layout ({ grid.getX(), grid.getY(), colX - 16 - grid.getX(), rowY - 16 - grid.getY() });
    stage.layout ({ colX + 16, grid.getY(), grid.getRight() - colX - 16, rowY - 16 - grid.getY() });
    palette.layout ({ grid.getX(), rowY + 16, colX - 16 - grid.getX(), grid.getBottom() - rowY - 16 });
    bottom = { colX + 16, rowY + 16, grid.getRight() - colX - 16, grid.getBottom() - rowY - 16 };
    const int keyboardH = std::clamp (bottom.getHeight() / 3, 60, 90);
    const int bodySide = std::max (80, std::min (bottom.getHeight() - keyboardH - 10, 200));
    body.layout ({ bottom.getX(), bottom.getY(), bodySide, bodySide });
    engine.layout ({ body.area.getRight() + 20, bottom.getY(), bottom.getRight() - body.area.getRight() - 20, bodySide });
    keyboard.layout ({ bottom.getX(), bottom.getBottom() - keyboardH, bottom.getWidth(), keyboardH });
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (Look::ground);
    g.setFont (Look::font (13.0f));
    g.setColour (Look::dim);
    g.drawText ("HEADSPACE", juce::Rectangle<int> (20, 8, 200, 22), juce::Justification::centredLeft);
    const bool dropping = dragging == Drag::card || dragging == Drag::slice;
    mother.paint (g, dragPoint, dropping);
    stage.paint (g, dragging == Drag::peak || dragging == Drag::zero ? dragRow : -1, dragging == Drag::blade, dragging == Drag::carve);
    palette.paint (g, dragging == Drag::transpose ? dragStar : -1);
    body.paint (g, dropping ? body.cornerAt (dragPoint) : -1);
    engine.paint (g);
    keyboard.paint (g);
    paintGhost (g);
    menu.paint (g, session);
}

void Screen::paintGhost (juce::Graphics& g) const
{
    if (dragging != Drag::card && dragging != Drag::slice) return;
    if (dragging == Drag::card && dragStar < 0) return;
    const Words words = dragging == Drag::card ? session.stars[(size_t) dragStar].words : session.heard;
    const auto ink = dragging == Drag::card ? plot::inkOf (session.stars[(size_t) dragStar]) : Look::blue;
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
        const int target = menu.target;
        const bool cube = menu.cube;
        menu.open = false;
        if (i >= 0) { if (cube) session.pinCube (target, i); else session.pinCorner (target, i); }
        repaint();
        return;
    }
    if (const int i = engine.sourceAt (p); i >= 0)
    {
        if (i == 0) session.setPlaying (! session.playing);
        else if (i == 3) { if (session.loopName.isNotEmpty()) session.setSource (2); }
        else session.setSource (i == 1 ? 0 : 1);
        return;
    }
    if (const int i = engine.cornerKeyAt (p); i >= 0) { session.toCorner (i); return; }
    if (engine.writeKey.contains (p)) { session.write(); return; }
    if (keyboard.area.contains (p))
    {
        if (const int midi = keyboard.noteAt (p); midi >= 0) { dragging = Drag::keyboard; session.noteOn (midi); }
        return;
    }
    if (const int n = body.tagAt (p); n >= 0)
    {
        if (p.x >= body.tag[(size_t) n].getRight() - 14) menu.show (session, n, false, body.tag[(size_t) n], getLocalBounds());
        else session.edit (n);
        repaint();
        return;
    }
    if (body.area.contains (p) && session.quad.complete()) { dragging = Drag::puck; mouseDrag (e); return; }
    if (const int i = mother.tagAt (p); i >= 0)
    {
        if (p.x >= mother.tag[(size_t) i].getRight() - 14) menu.show (session, Mother::pinOf (i), true, mother.tag[(size_t) i], getLocalBounds());
        else session.editCube (Mother::pinOf (i));
        repaint();
        return;
    }
    if (mother.bakeKey.contains (p)) { session.takeSlice(); return; }
    if (mother.depth.expanded (0, 8).contains (p)) { dragging = Drag::depth; mouseDrag (e); return; }
    if (const int side = mother.sideAt (p); side >= 0)
    {
        if (! session.cube.complete()) return;
        if (mother.probePoint (side).getDistanceFrom (p.toFloat()) < 10.0f && e.mods.isShiftDown())
        {
            dragging = Drag::slice; dragOrigin = p; dragPoint = p;
            if (session.auditioning != Session::kCube) session.setCubePoint (session.cube.x, session.cube.y, session.cube.z);
            return;
        }
        dragging = Drag::probe; dragSide = side;
        mouseDrag (e);
        return;
    }
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
    if (stage.magnitude.contains (p) && e.mods.isAltDown())
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
    if (const int k = palette.cardAt (p); k >= 0)
    {
        session.select (k);
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
            session.select (k);
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
        const auto words = Stage::solved (dragWords, dragRow, dragging == Drag::zero, plot::hzAt (p.x, stage.magnitude), plot::dbAt (p.y, stage.magnitude));
        if (words[(size_t) dragRow] != stage.words()[(size_t) dragRow])
        {
            if (! editStarted) { session.beginRowEdit(); editStarted = true; }
            session.setSectionWords (session.target(), dragRow, words[(size_t) dragRow]);
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
    if (dragging == Drag::probe)
    {
        const auto f = mother.face[(size_t) dragSide];
        session.setCubePoint ((p.x - f.getX()) / (double) f.getWidth(), (f.getBottom() - p.y) / (double) f.getHeight(), session.cube.z);
        return;
    }
    if (dragging == Drag::depth)
    {
        session.setCubePoint (session.cube.x, session.cube.y, (p.x - mother.depth.getX()) / (double) mother.depth.getWidth());
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
    if (dragging == Drag::card || dragging == Drag::slice) repaint();
}

void Screen::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragging == Drag::card && dragStar >= 0)
    {
        if (const int n = body.cornerAt (p); n >= 0) session.pinCorner (n, dragStar);
        else if (const int pin = mother.pinAt (p); pin >= 0) session.pinCube (pin, dragStar);
    }
    if (dragging == Drag::slice)
    {
        if (const int n = body.cornerAt (p); n >= 0) session.toCorner (n);
    }
    if (dragging == Drag::keyboard) session.noteOff();
    dragging = Drag::none; dragStar = -1; dragRow = -1;
    editStarted = false;
    repaint();
}

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto p = e.getPosition();
    const int step = wheel.deltaY > 0 ? -1 : 1;
    if (menu.open && menu.rect.contains (p)) { menu.wheel (session, step); repaint(); return; }
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

bool Screen::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const bool plain = ! m.isCommandDown() && ! m.isCtrlDown() && ! m.isAltDown();
    if (k.getKeyCode() == 'H' && plain) { stage.showHardware = ! stage.showHardware; repaint(); return true; }
    if (menu.open && k.getKeyCode() == juce::KeyPress::escapeKey) { menu.open = false; repaint(); return true; }
    if (k.getKeyCode() == juce::KeyPress::returnKey) { session.toCorner (session.working); return true; }
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
    const juce::Point<int> p (x, y);
    const int corner = body.cornerAt (p), pin = mother.pinAt (p);
    const bool asLoop = corner < 0 && pin < 0 && ! palette.area.contains (p);
    for (const auto& f : files)
    {
        if (! f.endsWithIgnoreCase (".wav")) continue;
        if (asLoop) { session.setLoop (juce::File (f)); continue; }
        const int k = session.addRead (juce::File (f));
        if (k < 0) continue;
        if (corner >= 0) session.pinCorner (corner, k);
        else if (pin >= 0) session.pinCube (pin, k);
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
