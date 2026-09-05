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
        ctx.setComponentPaintingEnabled (true);
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
    pairMode = true; pairA = 52; pairB = 53; pairT = 0.35;
    const auto& pa = lib.anchors[(size_t) pairA].p;
    const auto& pb = lib.anchors[(size_t) pairB].p;
    tl.keys = { { 0.5, { 0.3, 0.35 }, kData }, { 3.0, { 0.55, 0.6 }, kData }, { 6.5, { 0.4, 0.75 }, kData } };
    tl.playhead = 2.1;
    probe = std::array<double, 2> { pa[0] + (pb[0] - pa[0]) * pairT, pa[1] + (pb[1] - pa[1]) * pairT };
    body.corner = { 52, 53, 54, 55 };
    body.rowOn[4] = false;
    body.morph = 0.35;
    body.q = 0.6;
    playBody = true;
    openEditor (1);
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
    else if (const auto b = current()) audio->setWords (lib.wordsOf (*b));
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
    if (const auto p = tl.pathAt (t)) probe = p;
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

Words Workstation::playingWords() const
{
    if (L.room == Room::sound) { if (const auto w = sound.frameAt (sound.slice)) return *w; return Words {}; }
    if (playBody && body.ready()) return body.wheelWords (lib.frames);
    if (const auto b = current()) return lib.wordsOf (*b);
    return Words {};
}
}
