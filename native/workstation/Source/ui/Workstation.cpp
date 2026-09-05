#include "Workstation.h"
#include "Style.h"
#include <cmath>

namespace ws
{
Workstation::Workstation (Library& library, Stitch& stitch, bool useGL, bool withAudio) : lib (library), st (stitch), gl (useGL)
{
    if (withAudio) audio = std::make_unique<Audio>();
    setOpaque (true);
    setSize (1280, 800);
    L.duration = tl.duration;
    view.hi.z = 1.15;
    view.lo.z = -0.1;
    if (gl)
    {
        ctx.setRenderer (this);
        juce::OpenGLPixelFormat fmt;
        fmt.multisamplingLevel = 4;
        ctx.setPixelFormat (fmt);
        ctx.setMultisamplingEnabled (true);
        ctx.setComponentPaintingEnabled (false);
        ctx.setContinuousRepainting (false);
        ctx.attachTo (*this);
    }
}

Workstation::~Workstation()
{
    if (gl) ctx.detach();
}

void Workstation::setRoom (Room r)
{
    L.room = r;
    if (r != Room::edit) editing = -1;
    if (r == Room::sound)
    {
        if (wavs.empty()) wavs = Sound::scan (workspace);
        if (sound.mono->empty() && ! wavs.empty()) loadWav (0);
    }
    redraw();
}

void Workstation::demo()
{
    int face = -1;
    for (int i = 0; i < (int) st.faces.size(); ++i)
        if (st.faces[(size_t) i].name.startsWith ("Talking Hedz")) { face = i; break; }
    if (face < 0 && ! st.faces.empty()) face = 0;
    if (face < 0) return;
    const auto& f = st.faces[(size_t) face];
    openFace = face;
    pairMode = true; pairA = f.nodes[0]; pairB = f.nodes[1];
    pairTo (1.62);
    Spot k0, k1, k2;
    k0.face = face; k0.m = 0.2; k0.q = 0.2;
    k1.face = face; k1.m = 0.8; k1.q = 0.5;
    k2.node = f.nodes[3];
    tl.keys = { { 0.5, k0 }, { 3.0, k1 }, { 6.5, k2 } };
    tl.playhead = 2.1;
    for (int i = 0; i < 4; ++i) { Spot c; c.node = f.nodes[(size_t) i]; body.corner[(size_t) i] = frameFor (c); }
    body.rowOn[4] = false;
    body.morph = 1.4;
    body.q = 0.6;
    playBody = true;
    openEditor (1);
}

void Workstation::demoPair (double t)
{
    setRoom (Room::frames);
    pairTo (t);
    redraw();
}

void Workstation::demoShelf()
{
    setRoom (Room::frames);
    pairMode = false; pairA = pairB = -1;
    openFace = -1;
    spot.reset();
    picked.reset();
    playBody = false;
    status = "";
    redraw();
}

void Workstation::demoSound (int wavIndex, double at, double a, double b)
{
    setRoom (Room::sound);
    loadWav (wavIndex);
    sound.slice = at;
    sound.regionA = a;
    sound.regionB = b;
}

bool Workstation::exportBody (const juce::File& file)
{
    const bool ok = body.exportTo (lib.frames, file);
    status = ok ? "wrote " + file.getFileName() : "body needs four corners";
    redraw();
    return ok;
}

void Workstation::redraw()
{
    feedAudio();
    if (gl) ctx.triggerRepaint();
    repaint();
}

void Workstation::feedAudio()
{
    if (audio == nullptr) return;
    if (L.room == Room::sound)
    {
        if (const auto w = sound.frameAt (sound.slice)) audio->setWords (*w);
        return;
    }
    if (haveSound()) audio->setWords (live().words);
}

void Workstation::timerCallback()
{
    if (audio != nullptr && audio->isPlaying() && L.room == Room::sound && ! tl.playing)
    {
        sound.slice = audio->playhead();
        redraw();
        return;
    }
    if (audio != nullptr && audio->isPlaying() && ! tl.playing && ! (sweep[0] || sweep[1] || sweep[2])) { repaint(); if (gl) ctx.triggerRepaint(); return; }
    if (sweep[0] || sweep[1] || sweep[2])
    {
        sweepT += 1.0 / 60.0;
        if (! spot || ! spot->free) { Spot fs; fs.free = true; fs.z = freeZ; spot = fs; }
        if (sweep[0]) spot->x = 0.5 * std::sin (sweepT * 0.35);
        if (sweep[1]) spot->y = 0.5 * std::sin (sweepT * 0.23 + 1.0);
        if (sweep[2]) { spot->z = 0.5 + 0.5 * std::sin (sweepT * 0.17 + 2.0); freeZ = spot->z; }
        playBody = false;
        status = st.nameOf (*spot, open);
        if (! tl.playing) { redraw(); return; }
    }
    if (! tl.playing) { if (! (sweep[0] || sweep[1] || sweep[2]) && ! (audio != nullptr && audio->isPlaying())) stopTimer(); return; }
    double t = playFrom + (juce::Time::getMillisecondCounterHiRes() - playT0) / 1000.0;
    if (t >= tl.duration)
    {
        if (tl.loop) { playT0 = juce::Time::getMillisecondCounterHiRes(); playFrom = 0.0; t = 0.0; }
        else { tl.playing = false; stopTimer(); t = tl.duration; }
    }
    tl.playhead = t;
    if (const auto s = tl.pathAt (t, st)) { spot = s; playBody = false; }
    redraw();
}

Workstation::Probe Workstation::probe() const
{
    Probe pr;
    pr.status = status;
    pr.sounding = haveSound();
    pr.listening = audio != nullptr && audio->isPlaying();
    if (spot) { pr.free = spot->free; pr.x = spot->x; pr.y = spot->y; pr.z = spot->z; }
    return pr;
}

juce::Point<float> Workstation::clearPoint() const
{
    const auto f = L.field;
    for (int ring = 0; ring < 40; ++ring)
        for (int k = 0; k < 16; ++k)
        {
            const float a = (float) k / 16.0f * juce::MathConstants<float>::twoPi;
            const juce::Point<float> p (f.getCentreX() + std::cos (a) * ring * 6.0f, f.getCentreY() + 40.0f + std::sin (a) * ring * 6.0f);
            if (anchorAt (p) < 0 && anchorAt (p + juce::Point<float> (60.0f, 0.0f)) < 0 && anchorAt (p + juce::Point<float> (0.0f, -40.0f)) < 0)
            {
                double x = 0.0, y = 0.0;
                if (view.unproject (p, freeZ, x, y) && x > view.lo.x && x < view.hi.x && y > view.lo.y && y < view.hi.y) return p;
            }
        }
    return { f.getCentreX(), f.getCentreY() };
}

juce::Rectangle<float> Workstation::keyBox (const juce::String& id) const
{
    for (const auto& k : keys) if (k.id == id) return k.box;
    return {};
}

void Workstation::gesture (juce::Point<float> p, int phase, bool shift, bool right)
{
    juce::ModifierKeys mods;
    if (shift) mods = mods.withFlags (juce::ModifierKeys::shiftModifier);
    mods = mods.withFlags (right ? juce::ModifierKeys::rightButtonModifier : juce::ModifierKeys::leftButtonModifier);
    const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), p, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, this, this, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, phase != 0);
    if (phase == 0) mouseDown (e);
    else if (phase == 1) mouseDrag (e);
    else mouseUp (e);
}

bool Workstation::floorOpen (int floor) const { return floor < 0 || floor >= kGroups || open[(size_t) floor]; }

bool Workstation::haveSound() const
{
    if (L.room == Room::sound) return ! sound.mono->empty();
    return (playBody && body.ready()) || pairLive() || spot.has_value();
}

Words Workstation::playingWords() const { return live().words; }

Morph Workstation::live() const
{
    Morph m;
    if (L.room == Room::sound) { if (const auto w = sound.frameAt (sound.slice)) m.words = *w; return m; }
    if (playBody && body.ready()) return body.wheelMorph (lib.frames);
    if (pairLive())
    {
        const auto led = leadTo (decompile (st.wordsOf (pairA), kDatumHz), decompile (st.wordsOf (pairB), kDatumHz));
        return pairMorph (compile (led.a, kDatumHz), compile (led.b, kDatumHz), pairT);
    }
    if (spot) return st.soundAt (*spot, open);
    return m;
}

bool Workstation::pairLive() const { return pairMode && pairA >= 0 && pairB >= 0; }

Vec3 Workstation::pairPoint (double t) const
{
    const auto& a = st.nodes[(size_t) pairA].p;
    const auto& b = st.nodes[(size_t) pairB].p;
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

juce::String Workstation::nodeName (int node) const
{
    if (node < 0 || node >= (int) st.nodes.size()) return {};
    const auto& n = st.nodes[(size_t) node];
    if (n.frame >= 0 && n.frame < (int) lib.frames.size()) return lib.frames[(size_t) n.frame].name;
    for (const auto& f : st.faces)
        for (int i = 0; i < (int) f.nodes.size(); ++i)
            if (f.nodes[(size_t) i] == node) return f.name + (f.nodes.size() == 4 ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + kCornerNames[i] : " c" + juce::String (i));
    return "node " + juce::String (node);
}

juce::String Workstation::pairName() const
{
    const auto a = nodeName (pairA), b = nodeName (pairB);
    const juce::String dot = juce::CharPointer_UTF8 (" \xc2\xb7 ");
    const int cut = a.indexOf (dot);
    if (cut > 0 && b.startsWith (a.substring (0, cut + 3))) return a.substring (0, cut) + " " + a.substring (cut + 3) + " > " + b.substring (cut + 3);
    return a + " > " + b;
}

void Workstation::pairTo (double t)
{
    pairT = juce::jlimit (kPushLow, kPushHigh, t);
    playBody = false;
    status = pairT > 1.0 ? pairName() + "  " + juce::String (pairT, 2) + "  caricature" : pairT < 0.0 ? pairName() + "  " + juce::String (pairT, 2) + "  anti" : pairName() + "  " + juce::String (pairT, 2);
}

void Workstation::pairFromPoint (juce::Point<float> p)
{
    const auto a = view.project (pairPoint (0.0)), b = view.project (pairPoint (1.0));
    const double dx = b.x - a.x, dy = b.y - a.y, len2 = std::max (1e-6, dx * dx + dy * dy);
    pairTo (((p.x - a.x) * dx + (p.y - a.y) * dy) / len2);
}

int Workstation::frameFor (const Spot& s)
{
    if (s.node >= 0 && s.node < (int) st.nodes.size())
    {
        auto& n = st.nodes[(size_t) s.node];
        if (n.frame < 0) n.frame = lib.addNamed (st.wordsOf (s.node), nodeName (s.node), juce::jlimit (0, kGroups - 1, n.floor), false);
        return n.frame;
    }
    if (s.stub >= 0 && s.stub < (int) st.stubs.size())
    {
        auto& b = st.stubs[(size_t) s.stub];
        if (b.frame < 0)
        {
            for (int i = 0; i < (int) lib.frames.size(); ++i) if (lib.frames[(size_t) i].name == b.name) { b.frame = i; break; }
            if (b.frame < 0) b.frame = lib.addNamed (b.words, b.name, juce::jlimit (0, kGroups - 1, b.floor), true);
        }
        return b.frame;
    }
    return captureSpot (s, st.soundAt (s, open).words, "cap " + st.nameOf (s, open));
}

int Workstation::captureSpot (const Spot& s, const Words& words, const juce::String& name)
{
    const int idx = lib.addNamed (words, name, kGroups - 1, true);
    const int stub = st.addStub (name, words, s.node >= 0 ? s.node : st.nearestNode (words), kGroups - 1);
    st.stubs[(size_t) stub].frame = idx;
    return idx;
}
}
