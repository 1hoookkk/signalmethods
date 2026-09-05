#include "Workstation.h"
#include "Style.h"
#include "../render/SoftwareRenderer.h"
#include <cmath>

namespace ws
{
void Workstation::paint (juce::Graphics& g)
{
    if (gl) return;
    g.fillAll (kGround);
    drawScene (g, frame());
}

std::vector<Batch> Workstation::frame()
{
    layoutKeys();
    std::vector<Batch> out;
    Canvas panels (out);
    paintPanels (panels);
    for (auto& b : scene()) out.push_back (std::move (b));
    Canvas chrome (out);
    chrome.setFont (11.0f);
    paintChrome (chrome);
    return out;
}

void Workstation::paintPanels (Canvas& g)
{
    g.setColour (kPanel);
    if (! L.field.isEmpty()) g.fillRect (L.field);
    if (! L.outer.isEmpty()) g.fillRect (L.outer);
    if (! L.arma.isEmpty()) g.fillRect (L.arma.reduced (1.0f));
    g.fillRect (juce::Rectangle<float> (L.rx (20.0), L.ry (30.0), L.rx (20000.0) - L.rx (20.0), L.ry (-30.0) - L.ry (30.0)));
    if (L.timelineOpen) g.fillRect (L.tlAx);
    if (! L.cascade.isEmpty())
    {
        g.fillRect (L.cascade);
        for (int s = 0; s < kRows; ++s) g.fillRect (L.stageRect (s));
        for (int i = 0; i < 4; ++i) g.fillRect (L.thumbRect (i));
    }
}

void Workstation::newOpenGLContextCreated() { renderer.create (ctx); }
void Workstation::openGLContextClosing() { renderer.destroy(); }

void Workstation::renderOpenGL()
{
    renderer.draw (frame(), (float) getWidth(), (float) getHeight(), (float) ctx.getRenderingScale(), kGround);
}

void Workstation::paintChrome (Canvas& g)
{
    if (L.room == Room::sound) paintSound (g);
    g.setColour (kRule);
    if (! L.tray.isEmpty()) g.drawVerticalLine ((int) L.tray.getRight() - 1, 0.0f, L.tl.getY());
    g.drawVerticalLine ((int) L.resp.getX(), 0.0f, L.tl.getY());
    g.drawHorizontalLine ((int) L.tl.getY(), 0.0f, (float) getWidth());
    if (L.room != Room::sound) g.drawHorizontalLine ((int) L.resp.getY(), L.resp.getX(), L.resp.getRight());
    if (! L.body.isEmpty()) g.drawHorizontalLine ((int) L.body.getBottom(), L.body.getX(), L.body.getRight());
    g.setColour (kFrame);
    g.drawRect (px (L.field), 1);
    if (L.timelineOpen) g.drawRect (px (L.tlAx), 1);
    for (const auto& k : keys)
    {
        const auto r = px (k.box);
        g.setColour (k.on ? kKeyOn : kKey);
        g.fillRect (r);
        g.setColour (k.on ? kKeyOn : kKeyLine);
        g.drawRect (r, 1);
        g.setColour (k.on ? kKeyOnText : kKeyText);
        g.drawText (k.label, r.reduced (6, 0), juce::Justification::centredLeft);
    }
    if (L.room == Room::frames)
    {
        paintAxes (g);
        paintTray (g);
        paintBody (g);
        paintArma (g);
        if (pairLive())
        {
            g.setColour (kChosen);
            g.drawText (juce::String (pairT, 2), (int) L.field.getX() + 8, (int) L.field.getY() + 4, 60, 12, juce::Justification::centredLeft);
            for (int a : { pairA, pairB })
            {
                const auto p = view.project (st.nodes[(size_t) a].p);
                g.drawText (nodeName (a), (int) p.x + 10, (int) p.y - 6, 220, 12, juce::Justification::centredLeft);
            }
        }
        else if (picked)
        {
            g.setColour (kChosen);
            const auto p = view.project (st.positionOf (*picked));
            g.drawText (st.nameOf (*picked), (int) p.x + 10, (int) p.y - 6, 260, 12, juce::Justification::centredLeft);
        }
    }
    else if (L.room == Room::edit)
    {
        paintEditor (g);
        paintArma (g);
        for (int i = 0; i < 4; ++i)
        {
            const auto t = px (L.thumbRect (i));
            g.setColour (kFrame);
            g.drawRect (t, 1);
            g.setColour (kLine);
            g.drawHorizontalLine (t.getCentreY(), (float) t.getX(), (float) t.getRight());
            if (body.corner[(size_t) i] < 0) continue;
            g.setColour (i == editing ? kChosen : kDim);
            g.drawText (lib.frames[(size_t) body.corner[(size_t) i]].name, t.getX(), t.getY() - 14, 200, 12, juce::Justification::centredLeft);
        }
    }
    else paintArma (g);
    paintResponse (g);
    paintTimeline (g);
    g.setColour (kChosen);
    g.drawText (status, (int) L.field.getX() + 8, (int) L.field.getBottom() - 16, (int) L.field.getWidth() - 16, 12, juce::Justification::centredLeft);
}

void Workstation::paintTray (Canvas& g)
{
    const int maxRows = (int) ((L.tray.getHeight() - 8.0f) / 14.0f);
    trayScroll = juce::jlimit (0, std::max (0, (int) trayRows.size() - maxRows), trayScroll);
    for (int r = 0; r < maxRows && r + trayScroll < (int) trayRows.size(); ++r)
    {
        const auto& row = trayRows[(size_t) (r + trayScroll)];
        const float y = L.tray.getY() + 6.0f + r * 14.0f;
        if (row.header)
        {
            g.setColour (open[(size_t) row.group] ? kText : kDim);
            g.drawText (juce::String (open[(size_t) row.group] ? "- " : "+ ") + kGroupNames[row.group], (int) L.tray.getX() + 8, (int) y, (int) L.tray.getWidth() - 16, 14, juce::Justification::centredLeft);
            const juce::Rectangle<int> k ((int) L.tray.getRight() - 52, (int) y, 44, 14);
            g.setColour (kKey);
            g.fillRect (k);
            g.setColour (kKeyLine);
            g.drawRect (k, 1);
            g.setColour (kText);
            g.drawText ("MEAN", k.reduced (6, 0), juce::Justification::centredLeft);
            continue;
        }
        const auto& f = lib.frames[(size_t) row.frame];
        g.setColour (f.capture ? kLive : hueOf (f.m[0]));
        g.fillEllipse (L.tray.getX() + 17.0f, y + 4.5f, 5.0f, 5.0f);
        g.setColour (kText);
        g.drawText (f.name, (int) L.tray.getX() + 28, (int) y, (int) L.tray.getWidth() - 36, 14, juce::Justification::centredLeft);
    }
}

void Workstation::paintAxes (Canvas& g)
{
    const auto& v = view;
    const bool farX = v.farPlane (0), farY = v.farPlane (1);
    const double nearX = farX ? v.lo.x : v.hi.x, nearY = farY ? v.lo.y : v.hi.y, farXv = farX ? v.hi.x : v.lo.x;
    g.setColour (kDim);
    for (double hz : { 100.0, 1000.0, 10000.0 })
    {
        const auto p = v.project ({ std::log10 (hz / 20.0) / 3.0 - 0.5, nearY, v.lo.z });
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0), (int) p.x - 16, (int) p.y + 4, 32, 12, juce::Justification::centred);
    }
    for (double db : { -20.0, 0.0, 20.0 })
    {
        const auto p = v.project ({ nearX, db / 60.0, v.lo.z });
        g.drawText (juce::String ((int) db) + (db == 0.0 ? " dB" : ""), (int) p.x - 20, (int) p.y + 4, 40, 12, juce::Justification::centred);
    }
    for (const int f : st.usedFloors)
    {
        if (f < 0 || f >= kGroups) continue;
        const auto p = v.project ({ farXv, nearY, st.floorZ (f) });
        const bool left = v.project ({ farXv, nearY, 0.0 }).x < L.field.getCentreX();
        g.setColour (open[(size_t) f] ? kText : kDim);
        g.drawText (kGroupNames[f], left ? (int) p.x - 92 : (int) p.x + 8, (int) p.y - 6, 84, 12, left ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
}

void Workstation::paintResponse (Canvas& g)
{
    g.setColour (kFrame);
    g.drawRect (juce::Rectangle<int> ((int) L.rx (20.0), (int) L.ry (30.0), (int) (L.rx (20000.0) - L.rx (20.0)), (int) (L.ry (-30.0) - L.ry (30.0))), 1);
    g.setColour (kLine);
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) L.rx (f), L.ry (30.0), L.ry (-30.0));
    for (double d : { -20.0, 20.0 }) g.drawHorizontalLine ((int) L.ry (d), L.rx (20.0), L.rx (20000.0));
    g.setColour (kDim);
    g.drawHorizontalLine ((int) L.ry (0.0), L.rx (20.0), L.rx (20000.0));
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0), (int) L.rx (f) - 14, (int) L.ry (-30.0) + 4, 28, 12, juce::Justification::centred);
    for (double d : { 20.0, 0.0, -20.0 }) g.drawText (juce::String (d, 0), (int) L.resp.getX() + 6, (int) L.ry (d) - 6, 28, 12, juce::Justification::centredRight);
    g.drawText (juce::String ((int) std::round (fieldHz)) + " Hz", (int) L.resp.getRight() - 70, (int) L.resp.getY() + 8, 62, 12, juce::Justification::centredRight);
}

void Workstation::paintBody (Canvas& g)
{
    g.setColour (kRule);
    g.drawRect (px (L.outer), 1);
    g.setColour (kFrame);
    g.drawRect (px (L.square), 1);
    g.setColour (body.outside() ? kChosen : kDim);
    g.drawText ("M " + juce::String (body.morph, 2) + "   Q " + juce::String (body.q, 2), (int) L.outer.getX(), (int) L.outer.getBottom() + 6, (int) L.outer.getWidth(), 12, juce::Justification::centred);
    g.setColour (kDim);
    g.drawText ("rows", (int) L.outer.getX(), (int) L.outer.getBottom() + 30, 40, 14, juce::Justification::centredLeft);
    for (int i = 0; i < 4; ++i)
        if (body.corner[(size_t) i] >= 0)
            g.drawText (lib.frames[(size_t) body.corner[(size_t) i]].name, (int) L.outer.getX() + 152, (int) L.outer.getBottom() + 54 + i * 13, (int) L.body.getRight() - (int) L.outer.getX() - 160, 12, juce::Justification::centredLeft);
}

void Workstation::paintArma (Canvas& g)
{
    g.setColour (kDim);
    for (double db : { 20.0, 40.0, 60.0 }) { const auto p = L.armaXY (20.0, 1.0 - std::pow (10.0, -db / 20.0)); g.drawText (db >= 60.0 ? juce::String ((int) db) + " dB" : juce::String ((int) db), (int) p.x - 44, (int) p.y - 6, 40, 12, juce::Justification::centredRight); }
    for (int o = 0; o <= 10; o += 2) { const double hz = 20.0 * std::pow (2.0, o); const auto p = L.armaXY (hz, 0.9995); g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + "k" : juce::String (hz, 0), (int) p.x - 16, (int) p.y - 15, 32, 12, juce::Justification::centred); }
    if (! haveSound()) return;
    const auto rows = geometryOf (playingWords());
    g.setColour (kText);
    for (int s = 0; s < kRows; ++s)
        if (rows[(size_t) s].pole) { const auto p = L.armaXY (rows[(size_t) s].pHz, rows[(size_t) s].pR); g.drawText (juce::String (s + 1), (int) p.x + 6, (int) p.y - 13, 12, 12, juce::Justification::centredLeft); }
}

void Workstation::paintEditor (Canvas& g)
{
    if (editing < 0 || body.corner[(size_t) editing] < 0) return;
    const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    const auto gridOf = [&] (juce::Rectangle<float> r, bool hzLabels, bool dbLabels)
    {
        g.setColour (kFrame);
        g.drawRect (px (r), 1);
        g.setColour (kLine);
        const bool full = r.getHeight() > 100.0f;
        for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) L.sx (r, hz), r.getY(), r.getBottom());
        if (full) for (double hz : { 50.0, 200.0, 500.0, 2000.0, 5000.0 }) g.drawVerticalLine ((int) L.sx (r, hz), r.getY(), r.getBottom());
        if (full) for (double db : { -20.0, -10.0, 10.0, 20.0 }) g.drawHorizontalLine ((int) L.sy (r, db), r.getX(), r.getRight());
        g.setColour (kDim);
        g.drawHorizontalLine ((int) L.sy (r, 0.0), r.getX(), r.getRight());
        if (hzLabels) for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0), (int) L.sx (r, hz) - 14, (int) r.getBottom() + 2, 28, 12, juce::Justification::centred);
        if (dbLabels) for (double db : { 20.0, 0.0, -20.0 }) g.drawText (juce::String (db, 0), (int) r.getX() - 34, (int) L.sy (r, db) - 6, 30, 12, juce::Justification::centredRight);
    };
    const auto cr = L.cascade;
    gridOf (cr, true, true);
    g.setColour (kChosen);
    g.drawText (f.name, (int) cr.getX(), (int) cr.getY() - 14, (int) cr.getWidth(), 12, juce::Justification::centredLeft);
    {
        juce::String chordLine;
        for (const auto& st : f.chord) if (st.pole.on) chordLine += (chordLine.isEmpty() ? "" : "  ") + noteName (st.pole.note);
        g.setColour (kDim);
        g.drawText (chordLine, (int) cr.getRight() + 80, (int) cr.getY() + 24, 400, 12, juce::Justification::centredLeft);
    }
    openBar = { cr.getRight() + 80.0f, cr.getY() + 4.0f, 160.0f, 10.0f };
    g.setColour (kDim);
    g.drawText ("open", (int) openBar.getX() - 40, (int) openBar.getY() - 2, 36, 14, juce::Justification::centredRight);
    g.setColour (kPanel);
    g.fillRect (openBar);
    g.setColour (kKeyLine);
    g.drawRect (px (openBar), 1);
    g.setColour (kChosen);
    g.fillRect (openBar.getX() + 1.0f, openBar.getY() + 1.0f, (openBar.getWidth() - 2.0f) * (float) openAmount, openBar.getHeight() - 2.0f);
    g.setColour (kDim);
    g.drawText (juce::String ((int) std::round (250.0 * std::pow (900.0 / 250.0, openAmount))) + " Hz", (int) openBar.getRight() + 4, (int) openBar.getY() - 2, 56, 14, juce::Justification::centredLeft);
    g.setFont (11.0f, true);
    for (int s = 0; s < kRows; ++s)
    {
        const auto r = L.stageRect (s);
        gridOf (r, s >= 3, s % 3 == 0);
        const auto& st = f.chord[(size_t) s];
        const int x0 = (int) r.getX(), y0 = (int) r.getBottom() + 17;
        g.setColour (st.pole.on ? kText : kDim);
        g.drawText (st.pole.on ? "pole " + noteName (st.pole.note).paddedRight (' ', 8) + " w " + juce::String (st.pole.width, 1) + " st" : "pole off", x0, y0, (int) r.getWidth(), 13, juce::Justification::centredLeft);
        g.setColour (st.zero.on ? kText : kDim);
        g.drawText (st.zero.on ? "zero " + noteName (st.zero.note).paddedRight (' ', 8) + " w " + juce::String (st.zero.width, 1) + " st" : "zero off", x0, y0 + 14, (int) r.getWidth(), 13, juce::Justification::centredLeft);
        g.setColour (kDim);
        g.drawText (juce::String (st.gainDb, 1) + " dB", x0, y0 + 28, (int) r.getWidth() - 120, 13, juce::Justification::centredLeft);
    }
    g.setFont (11.0f);
}

void Workstation::paintSound (Canvas& g)
{
    if (sound.spectrogram.isValid())
    {
        g.drawImageAt (sound.spectrogram, (int) L.field.getX() + 1, (int) L.field.getY() + 1);
        const float sx = L.field.getX() + (float) (sound.seconds > 0.0 ? sound.slice / sound.seconds : 0.0) * L.field.getWidth();
        if (sound.seconds > 0.0 && std::abs (sound.regionB - sound.regionA) > 1e-6 && (sound.regionA > 0.0 || sound.regionB < sound.seconds))
        {
            const float ra = L.field.getX() + (float) (std::min (sound.regionA, sound.regionB) / sound.seconds) * L.field.getWidth();
            const float rb = L.field.getX() + (float) (std::max (sound.regionA, sound.regionB) / sound.seconds) * L.field.getWidth();
            g.setColour (kGround.withAlpha (0.6f));
            g.fillRect (L.field.getX() + 1.0f, L.field.getY() + 1.0f, ra - L.field.getX() - 1.0f, L.field.getHeight() - 2.0f);
            g.fillRect (rb, L.field.getY() + 1.0f, L.field.getRight() - rb - 1.0f, L.field.getHeight() - 2.0f);
            g.setColour (kChosen);
            g.drawVerticalLine ((int) ra, L.field.getY(), L.field.getBottom());
            g.drawVerticalLine ((int) rb, L.field.getY(), L.field.getBottom());
        }
        g.setColour (kLive);
        g.drawVerticalLine ((int) sx, L.field.getY(), L.field.getBottom());
    }
    g.setColour (kText);
    for (double hz : { 100.0, 1000.0, 10000.0 })
    {
        const float y = L.field.getY() + (float) (1.0 - std::log10 (hz / 20.0) / 3.0) * L.field.getHeight();
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0), (int) L.field.getRight() - 40, (int) y - 6, 34, 12, juce::Justification::centredRight);
    }
    g.setColour (kDim);
    for (int t = 0; t <= (int) sound.seconds; ++t)
        g.drawText (juce::String (t), (int) (L.field.getX() + (sound.seconds > 0.0 ? t / sound.seconds : 0.0) * L.field.getWidth()) - 6, (int) L.field.getBottom() + 2, 20, 12, juce::Justification::centredLeft);
    g.setColour (kLive);
    g.drawText (juce::String (sound.slice, 3) + " s", (int) L.field.getRight() - 90, (int) L.field.getY() + 4, 84, 14, juce::Justification::centredRight);
    if (const auto w = sound.frameAt (sound.slice))
    {
        const auto rows = geometryOf (*w);
        int y = (int) L.field.getY() + 6;
        g.setColour (kData);
        g.setFont (11.0f, true);
        for (int s = 0; s < kRows; ++s)
            if (rows[(size_t) s].pole && rows[(size_t) s].pR > 0.0)
            {
                g.drawText (juce::String (s + 1) + "  " + juce::String ((int) std::round (rows[(size_t) s].pHz)) + " Hz  r " + juce::String (rows[(size_t) s].pR, 3), (int) L.field.getX() + 8, y, 200, 13, juce::Justification::centredLeft);
                y += 13;
            }
        g.setFont (11.0f);
    }
    const int maxRows = (int) ((L.tray.getHeight() - 8.0f) / 14.0f);
    wavScroll = juce::jlimit (0, std::max (0, (int) wavs.size() - maxRows), wavScroll);
    for (int r = 0; r < maxRows && r + wavScroll < (int) wavs.size(); ++r)
    {
        const auto& f = wavs[(size_t) (r + wavScroll)];
        g.setColour (f == sound.file ? kChosen : kText);
        g.drawText (f.getFileNameWithoutExtension(), (int) L.tray.getX() + 8, (int) (L.tray.getY() + 6.0f + r * 14.0f), (int) L.tray.getWidth() - 16, 14, juce::Justification::centredLeft);
    }
}

void Workstation::paintTimeline (Canvas& g)
{
    g.setColour (kDim);
    if (! L.timelineOpen) return;
    for (int t = 0; t <= (int) tl.duration; ++t) g.drawText (juce::String (t), (int) L.tx (t) - 8, (int) L.tlAx.getBottom() + 4, 20, 12, juce::Justification::centred);
    g.drawText (juce::String (tl.playhead, 2) + " s", (int) L.tlAx.getX() + 4, (int) L.tlAx.getY() - 14, 58, 12, juce::Justification::centredLeft);
}
}
