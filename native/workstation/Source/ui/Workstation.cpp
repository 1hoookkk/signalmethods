#include "Workstation.h"
#include "Style.h"
#include <cmath>

namespace ws
{
Workstation::Workstation (Library& library, bool useGL, bool withAudio) : lib (library), gl (useGL)
{
    if (withAudio) audio = std::make_unique<Audio>();
    setOpaque (true);
    setSize (1280, 800);
    L.duration = tl.duration;
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
    lib.axisX = 6;
    lib.axisY = 7;
    lib.sort();
    pairMode = true; pairA = 52; pairB = 53;
    pairTo (1.62);
    tl.keys = { { 0.5, pairPoint (-0.5), kData }, { 3.0, pairPoint (0.6), kData }, { 6.5, pairPoint (2.2), kData } };
    tl.playhead = 2.1;
    body.corner = { 52, 53, 54, 55 };
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
    if (playBody && body.ready()) audio->setWords (body.wheelWords (lib.frames));
    else if (current()) audio->setWords (live().words);
}

void Workstation::timerCallback()
{
    if (audio != nullptr && audio->isPlaying() && L.room == Room::sound && ! tl.playing)
    {
        sound.slice = audio->playhead();
        redraw();
        return;
    }
    if (! tl.playing) { stopTimer(); return; }
    double t = playFrom + (juce::Time::getMillisecondCounterHiRes() - playT0) / 1000.0;
    if (t >= tl.duration)
    {
        if (tl.loop) { playT0 = juce::Time::getMillisecondCounterHiRes(); playFrom = 0.0; t = 0.0; }
        else { tl.playing = false; stopTimer(); t = tl.duration; }
    }
    tl.playhead = t;
    if (const auto p = tl.pathAt (t)) { if (pairLive()) pairFromPoint (*p); else probe = p; }
    redraw();
}

bool Workstation::visible (int i) const
{
    const auto& f = lib.frames[(size_t) lib.anchors[(size_t) i].frame];
    if (! f.capture && ! open[(size_t) f.group]) return false;
    if (pairMode && pairB >= 0 && i != pairA && i != pairB) return false;
    return true;
}

std::optional<Blend> Workstation::current() const
{
    if (pairMode && pairA >= 0 && pairB >= 0) return Blend { { pairA, pairB, pairA }, { 1.0 - pairT, pairT, 0.0 } };
    return probe ? lib.blendAt ((*probe)[0], (*probe)[1]) : std::nullopt;
}

Words Workstation::playingWords() const { return live().words; }

Morph Workstation::live() const
{
    Morph m;
    if (L.room == Room::sound) { if (const auto w = sound.frameAt (sound.slice)) m.words = *w; return m; }
    if (playBody && body.ready()) return body.wheelMorph (lib.frames);
    if (pairLive()) return pairMorph (lib.frames[(size_t) lib.anchors[(size_t) pairA].frame].words, lib.frames[(size_t) lib.anchors[(size_t) pairB].frame].words, pairT);
    if (const auto b = current()) m.words = lib.wordsOf (*b);
    return m;
}

bool Workstation::pairLive() const { return pairMode && pairA >= 0 && pairB >= 0; }

std::array<double, 2> Workstation::pairPoint (double t) const
{
    const auto& pa = lib.anchors[(size_t) pairA].p;
    const auto& pb = lib.anchors[(size_t) pairB].p;
    return { pa[0] + (pb[0] - pa[0]) * t, pa[1] + (pb[1] - pa[1]) * t };
}

juce::String Workstation::pairName() const
{
    const auto a = lib.frames[(size_t) lib.anchors[(size_t) pairA].frame].name, b = lib.frames[(size_t) lib.anchors[(size_t) pairB].frame].name;
    const juce::String dot = juce::CharPointer_UTF8 (" \xc2\xb7 ");
    const int cut = a.indexOf (dot);
    if (cut > 0 && b.startsWith (a.substring (0, cut + 3))) return a.substring (0, cut) + " " + a.substring (cut + 3) + " > " + b.substring (cut + 3);
    return a + " > " + b;
}

void Workstation::pairTo (double t)
{
    pairT = juce::jlimit (kPushLow, kPushHigh, t);
    probe = pairPoint (pairT);
    playBody = false;
    status = pairT > 1.0 ? pairName() + "  " + juce::String (pairT, 2) + "  caricature" : pairT < 0.0 ? pairName() + "  " + juce::String (pairT, 2) + "  anti" : juce::String();
}

void Workstation::pairFromPoint (std::array<double, 2> p)
{
    const auto& pa = lib.anchors[(size_t) pairA].p;
    const auto& pb = lib.anchors[(size_t) pairB].p;
    const double dx = pb[0] - pa[0], dy = pb[1] - pa[1], len2 = std::max (1e-12, dx * dx + dy * dy);
    pairTo (((p[0] - pa[0]) * dx + (p[1] - pa[1]) * dy) / len2);
}
}
