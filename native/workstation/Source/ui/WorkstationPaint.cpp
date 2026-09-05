#include "Workstation.h"
#include "Style.h"
#include "../render/SvgRenderer.h"
#include <cmath>

namespace ws
{
void Workstation::paint (juce::Graphics& g)
{
    layoutKeys();
    if (! gl) drawSvg (g, sceneToSvg (scene(), (float) getWidth(), (float) getHeight()), (float) getWidth(), (float) getHeight());
    g.setFont (mono (11.0f));
    paintChrome (g);
}

void Workstation::newOpenGLContextCreated() { renderer.create (ctx); }
void Workstation::openGLContextClosing() { renderer.destroy(); }

void Workstation::renderOpenGL()
{
    layoutKeys();
    renderer.draw (scene(), (float) getWidth(), (float) getHeight(), (float) ctx.getRenderingScale());
}

void Workstation::paintChrome (juce::Graphics& g)
{
    const auto b = current();
    if (L.room == Room::sound) paintSound (g);
    g.setColour (kLine);
    if (! L.tray.isEmpty()) g.drawVerticalLine ((int) L.tray.getRight() - 1, 0.0f, L.tl.getY());
    g.drawVerticalLine ((int) L.resp.getX(), 0.0f, L.tl.getY());
    g.drawHorizontalLine ((int) L.tl.getY(), 0.0f, (float) getWidth());
    if (L.room != Room::sound) g.drawHorizontalLine ((int) L.resp.getY(), L.resp.getX(), L.resp.getRight());
    if (! L.body.isEmpty()) g.drawHorizontalLine ((int) L.body.getBottom(), L.body.getX(), L.body.getRight());
    g.drawRect (px (L.field), 1);
    if (L.timelineOpen) g.drawRect (px (L.tlAx), 1);
    for (const auto& k : keys)
    {
        const auto r = px (k.box);
        g.setColour (k.on ? kChosen : kKey);
        g.fillRect (r);
        g.setColour (k.on ? kChosen : kKeyLine);
        g.drawRect (r, 1);
        g.setColour (k.on ? juce::Colours::black : kText);
        g.drawText (k.label, r.reduced (6, 0), juce::Justification::centredLeft);
    }
    if (L.room == Room::frames)
    {
        g.setColour (kDim);
        g.drawText ("across", (int) L.sortRow.getX() + 8, 5, 48, 14, juce::Justification::centredLeft);
        g.drawText ("up", (int) L.sortRow.getX() + 134, 5, 20, 14, juce::Justification::centredLeft);
        paintTray (g);
        paintBody (g);
        paintArma (g);
        if (pairMode && pairA >= 0 && pairB >= 0)
        {
            g.setColour (kChosen);
            g.drawText (juce::String (pairT, 3), (int) L.field.getX() + 8, (int) L.field.getY() + 4, 60, 12, juce::Justification::centredLeft);
            for (int a : { pairA, pairB })
            {
                const auto p = L.fromField (lib.anchors[(size_t) a].p);
                g.drawText (lib.frames[(size_t) lib.anchors[(size_t) a].frame].name, (int) p.x + 10, (int) p.y - 6, 220, 12, juce::Justification::centredLeft);
            }
        }
    }
    else if (L.room == Room::edit)
    {
        paintEditor (g);
        paintArma (g);
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<int> t ((int) L.body.getX() + 8 + (i & 1) * 212, (int) L.body.getY() + 48 + (i >> 1) * 80, 200, 40);
            g.setColour (kLine);
            g.drawRect (t, 1);
            g.drawHorizontalLine (t.getCentreY(), (float) t.getX(), (float) t.getRight());
            if (body.corner[(size_t) i] < 0) continue;
            g.setColour (i == editing ? kChosen : kDim);
            g.drawText (lib.frames[(size_t) body.corner[(size_t) i]].name, t.getX(), t.getY() - 14, 200, 12, juce::Justification::centredLeft);
        }
    }
    else paintArma (g);
    paintResponse (g, b);
    paintTimeline (g);
    g.setColour (kChosen);
    g.drawText (status, (int) L.field.getX() + 8, (int) L.field.getBottom() - 16, (int) L.field.getWidth() - 16, 12, juce::Justification::centredLeft);
}

void Workstation::paintTray (juce::Graphics& g)
{
    const int maxRows = (int) ((L.tray.getHeight() - 8.0f) / 14.0f);
    trayScroll = juce::jlimit (0, std::max (0, (int) trayRows.size() - maxRows), trayScroll);
    if (pickFor >= 0) { g.setColour (kChosen); g.drawRect (px (L.tray).reduced (1), 1); }
    for (int r = 0; r < maxRows && r + trayScroll < (int) trayRows.size(); ++r)
    {
        const auto& row = trayRows[(size_t) (r + trayScroll)];
        const float y = L.tray.getY() + 6.0f + r * 14.0f;
        if (row.header)
        {
            g.setColour (open[(size_t) row.group] ? kText : kDim);
            g.drawText (juce::String (open[(size_t) row.group] ? "- " : "+ ") + kGroupNames[row.group], (int) L.tray.getX() + 8, (int) y, (int) L.tray.getWidth() - 16, 14, juce::Justification::centredLeft);
            continue;
        }
        const auto& f = lib.frames[(size_t) row.frame];
        g.setColour (f.capture ? kLive : hueOf (f.m[0]));
        g.fillEllipse (L.tray.getX() + 17.0f, y + 4.5f, 5.0f, 5.0f);
        g.setColour (kText);
        g.drawText (f.name, (int) L.tray.getX() + 28, (int) y, (int) L.tray.getWidth() - 36, 14, juce::Justification::centredLeft);
    }
}

void Workstation::paintResponse (juce::Graphics& g, const std::optional<Blend>& b)
{
    g.setColour (kLine);
    g.drawRect (juce::Rectangle<int> ((int) L.rx (20.0), (int) L.ry (30.0), (int) (L.rx (20000.0) - L.rx (20.0)), (int) (L.ry (-30.0) - L.ry (30.0))), 1);
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) L.rx (f), L.ry (30.0), L.ry (-30.0));
    for (double d : { -20.0, 20.0 }) g.drawHorizontalLine ((int) L.ry (d), L.rx (20.0), L.rx (20000.0));
    g.setColour (kDim);
    g.drawHorizontalLine ((int) L.ry (0.0), L.rx (20.0), L.rx (20000.0));
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0), (int) L.rx (f) - 14, (int) L.ry (-30.0) + 4, 28, 12, juce::Justification::centred);
    for (double d : { 20.0, 0.0, -20.0 }) g.drawText (juce::String (d, 0), (int) L.resp.getX() + 6, (int) L.ry (d) - 6, 28, 12, juce::Justification::centredRight);
    g.drawText (juce::String ((int) std::round (fieldHz)) + " Hz", (int) L.resp.getRight() - 70, (int) L.resp.getY() + 8, 62, 12, juce::Justification::centredRight);
    if (L.room == Room::frames && b && ! playBody)
    {
        int y = (int) L.resp.getY() - 44;
        for (int i = 0; i < 3; ++i)
        {
            if (b->w[(size_t) i] <= 0.0) continue;
            const auto& f = lib.frames[(size_t) lib.anchors[(size_t) b->anchors[(size_t) i]].frame];
            g.setColour (hueOf (f.m[0]));
            g.fillEllipse (L.resp.getX() + 9.0f, (float) y + 4.0f, 5.0f, 5.0f);
            g.setColour (kText);
            g.drawText (juce::String ((int) std::round (b->w[(size_t) i] * 100)) + "%  " + f.name, (int) L.resp.getX() + 20, y, (int) L.resp.getWidth() - 24, 13, juce::Justification::centredLeft);
            y += 13;
        }
    }
}

void Workstation::paintBody (juce::Graphics& g)
{
    g.setColour (kLine);
    g.drawRect (px (L.square), 1);
    g.setColour (kDim);
    g.drawText ("M " + juce::String (body.morph, 2) + "   Q " + juce::String (body.q, 2), (int) L.square.getX(), (int) L.square.getBottom() + 6, (int) L.square.getWidth(), 12, juce::Justification::centred);
    g.drawText ("rows", (int) L.square.getRight() + 64, (int) L.square.getY() + 10, 40, 12, juce::Justification::centredLeft);
    for (int i = 0; i < 4; ++i)
        if (body.corner[(size_t) i] >= 0)
            g.drawText (lib.frames[(size_t) body.corner[(size_t) i]].name, (int) L.square.getRight() + 64, (int) L.square.getY() + 120 + i * 13, (int) L.body.getRight() - (int) L.square.getRight() - 72, 12, juce::Justification::centredLeft);
}

void Workstation::paintArma (juce::Graphics& g)
{
    g.setColour (kDim);
    for (double db : { 20.0, 40.0, 60.0 }) { const auto p = L.armaXY (20.0, 1.0 - std::pow (10.0, -db / 20.0)); g.drawText (juce::String ((int) db) + " dB", (int) p.x - 44, (int) p.y - 6, 40, 12, juce::Justification::centredRight); }
    for (int o = 0; o <= 10; o += 2) { const double hz = 20.0 * std::pow (2.0, o); const auto p = L.armaXY (hz, 0.9995); g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + "k" : juce::String (hz, 0), (int) p.x - 16, (int) p.y - 15, 32, 12, juce::Justification::centred); }
    const bool haveWords = L.room == Room::sound ? ! sound.mono->empty() : (current().has_value() || (playBody && body.ready()));
    if (! haveWords) return;
    const auto rows = geometryOf (playingWords());
    g.setColour (kText);
    for (int s = 0; s < kRows; ++s)
        if (rows[(size_t) s].pole) { const auto p = L.armaXY (rows[(size_t) s].pHz, rows[(size_t) s].pR); g.drawText (juce::String (s + 1), (int) p.x + 6, (int) p.y - 13, 12, 12, juce::Justification::centredLeft); }
}

void Workstation::paintEditor (juce::Graphics& g)
{
    if (editing < 0 || body.corner[(size_t) editing] < 0) return;
    const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    const auto gridOf = [&] (juce::Rectangle<float> r, bool hzLabels, bool dbLabels)
    {
        g.setColour (kLine);
        g.drawRect (px (r), 1);
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
    g.drawText (f.name, (int) cr.getX(), (int) cr.getY() - 14, (int) cr.getWidth() / 2, 12, juce::Justification::centredLeft);
    openBar = { cr.getRight() - 220.0f, cr.getY() - 13.0f, 160.0f, 10.0f };
    g.setColour (kDim);
    g.drawText ("open", (int) openBar.getX() - 40, (int) openBar.getY() - 2, 36, 14, juce::Justification::centredRight);
    g.setColour (juce::Colours::black);
    g.fillRect (openBar);
    g.setColour (kKeyLine);
    g.drawRect (px (openBar), 1);
    g.setColour (kChosen);
    g.fillRect (openBar.getX() + 1.0f, openBar.getY() + 1.0f, (openBar.getWidth() - 2.0f) * (float) openAmount, openBar.getHeight() - 2.0f);
    g.setColour (kDim);
    g.drawText (juce::String ((int) std::round (250.0 * std::pow (900.0 / 250.0, openAmount))) + " Hz", (int) openBar.getRight() + 4, (int) openBar.getY() - 2, 56, 14, juce::Justification::centredLeft);
    for (int s = 0; s < kRows; ++s)
    {
        const auto r = L.stageRect (s);
        gridOf (r, s == kRows - 1, s == 0);
        const auto& gm = f.rows[(size_t) s];
        const int x0 = (int) L.field.getX() + 8, y0 = (int) r.getY() + 2;
        g.setColour (gm.pole ? kText : kDim);
        g.drawText (gm.pole ? "pole  " + juce::String ((int) std::round (gm.pHz)).paddedLeft (' ', 6) + " Hz   r " + juce::String (gm.pR, 3) + "   " + juce::String (resDb (gm.pR), 1) + " dB" : "pole  real", x0 + 40, y0, 240, 13, juce::Justification::centredLeft);
        g.setColour (gm.zero ? kText : kDim);
        g.drawText (gm.zero ? "zero  " + juce::String ((int) std::round (gm.zHz)).paddedLeft (' ', 6) + " Hz   r " + juce::String (gm.zR, 3) : "zero  real", x0 + 40, y0 + 14, 240, 13, juce::Justification::centredLeft);
    }
}

void Workstation::paintSound (juce::Graphics& g)
{
    if (sound.spectrogram.isValid())
    {
        g.drawImageAt (sound.spectrogram, (int) L.field.getX() + 1, (int) L.field.getY() + 1);
        const float sx = L.field.getX() + (float) (sound.seconds > 0.0 ? sound.slice / sound.seconds : 0.0) * L.field.getWidth();
        if (sound.seconds > 0.0 && std::abs (sound.regionB - sound.regionA) > 1e-6 && (sound.regionA > 0.0 || sound.regionB < sound.seconds))
        {
            const float ra = L.field.getX() + (float) (std::min (sound.regionA, sound.regionB) / sound.seconds) * L.field.getWidth();
            const float rb = L.field.getX() + (float) (std::max (sound.regionA, sound.regionB) / sound.seconds) * L.field.getWidth();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
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
        for (int s = 0; s < kRows; ++s)
            if (rows[(size_t) s].pole && rows[(size_t) s].pR > 0.0)
            {
                g.drawText (juce::String (s + 1) + "  " + juce::String ((int) std::round (rows[(size_t) s].pHz)) + " Hz  r " + juce::String (rows[(size_t) s].pR, 3), (int) L.field.getX() + 8, y, 200, 13, juce::Justification::centredLeft);
                y += 13;
            }
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

void Workstation::paintTimeline (juce::Graphics& g)
{
    g.setColour (kDim);
    if (! L.timelineOpen) return;
    for (int t = 0; t <= (int) tl.duration; ++t) g.drawText (juce::String (t), (int) L.tx (t) - 8, (int) L.tlAx.getBottom() + 4, 20, 12, juce::Justification::centred);
    g.drawText (juce::String (tl.playhead, 2) + " s", (int) L.tlAx.getX() + 4, (int) L.tlAx.getY() - 14, 58, 12, juce::Justification::centredLeft);
}
}
