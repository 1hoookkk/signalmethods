#include "Workstation.h"
#include "../render/SvgRenderer.h"
#include <cmath>

namespace ws
{
namespace
{
juce::Font mono (float h) { return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::plain)); }
const juce::Colour kLine (0xff383838), kText (0xffc8c8c8), kDim (0xff7a7a7a), kKey (0xff161616), kKeyLine (0xff4a4a4a);
const juce::Colour kLive (0xffffe100), kChosen (0xff00e5ff), kData (0xffffffff);
juce::Rectangle<int> px (juce::Rectangle<float> r) { return r.toNearestInt(); }
const char* const kCornerNames[4] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };
}

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

void Workstation::buildTray()
{
    trayRows.clear();
    if (L.room == Room::sound) return;
    for (int g = 0; g < kGroups; ++g)
    {
        trayRows.push_back ({ -1, g, true });
        if (! open[(size_t) g]) continue;
        for (int i = 0; i < (int) lib.frames.size(); ++i)
            if (lib.frames[(size_t) i].group == g) trayRows.push_back ({ i, g, false });
    }
}

void Workstation::layoutKeys()
{
    L.timelineOpen = ! tl.keys.empty() || tl.playing;
    L.compute ((float) getWidth(), (float) getHeight());
    buildTray();
    keys.clear();
    const float kh = 14.0f;
    keys.push_back ({ "room0", "FRAMES", { 8.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::frames });
    keys.push_back ({ "room1", "EDIT", { 72.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::edit });
    keys.push_back ({ "room2", "SOUND", { 136.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::sound });
    const bool audioOn = audio != nullptr && audio->isPlaying();
    keys.push_back ({ "listen", audioOn ? "STOP" : "LISTEN", { L.resp.getX() + 8.0f, L.resp.getY() + 6.0f, 60.0f, kh }, audioOn });
    keys.push_back ({ "wet", "FILTER", { L.resp.getX() + 72.0f, L.resp.getY() + 6.0f, 60.0f, kh }, wet });
    if (L.room == Room::frames)
    {
        keys.push_back ({ "axx", kMeasureNames[lib.axisX], { L.sortRow.getX() + 56.0f, 5.0f, 70.0f, kh }, false });
        keys.push_back ({ "axy", kMeasureNames[lib.axisY], { L.sortRow.getX() + 156.0f, 5.0f, 70.0f, kh }, false });
        keys.push_back ({ "sort", "SORT", { L.sortRow.getX() + 232.0f, 5.0f, 52.0f, kh }, false });
        keys.push_back ({ "pair", "PAIR", { L.field.getRight() - 56.0f, 5.0f, 56.0f, kh }, pairMode });
        keys.push_back ({ "capture", "CAPTURE", { L.field.getRight() - 128.0f, 5.0f, 68.0f, kh }, false });
        const auto sq = L.square;
        for (int i = 0; i < 4; ++i)
        {
            const float x = (i & 1) ? sq.getRight() + 8.0f : sq.getX() - 56.0f, y = (i & 2) ? sq.getY() : sq.getBottom() - kh;
            keys.push_back ({ "corner" + juce::String (i), kCornerNames[i], { x, y, 48.0f, kh }, body.corner[(size_t) i] >= 0 });
        }
        const float bx = sq.getRight() + 64.0f;
        for (int s = 0; s < kRows; ++s) keys.push_back ({ "row" + juce::String (s), juce::String (s + 1), { bx + s * 24.0f, sq.getY() + 24.0f, 20.0f, kh }, body.rowOn[(size_t) s] });
        keys.push_back ({ "playbody", "BODY", { bx, sq.getY() + 48.0f, 68.0f, kh }, playBody });
        keys.push_back ({ "edit", "EDIT", { bx + 72.0f, sq.getY() + 48.0f, 68.0f, kh }, false });
        keys.push_back ({ "export", "EXPORT", { bx, sq.getY() + 72.0f, 68.0f, kh }, false });
        keys.push_back ({ "copy", "COPY", { bx + 72.0f, sq.getY() + 72.0f, 68.0f, kh }, copyFrom >= 0 });
        keys.push_back ({ "sharpen", "SHARPEN", { bx, sq.getY() + 96.0f, 68.0f, kh }, false });
        keys.push_back ({ "unity", "UNITY", { bx + 72.0f, sq.getY() + 96.0f, 68.0f, kh }, body.unity });
    }
    else if (L.room == Room::edit)
    {
        for (int s = 0; s < kRows; ++s)
        {
            keys.push_back ({ "row" + juce::String (s), juce::String (s + 1), { L.field.getX() + 8.0f, L.stageRect (s).getY() + 2.0f, 20.0f, kh }, body.rowOn[(size_t) s] });
            keys.push_back ({ "lock" + juce::String (s), "LOCK", { L.field.getX() + 236.0f, L.stageRect (s).getY() + 16.0f, 48.0f, kh }, lockRow[(size_t) s] });
        }
        keys.push_back ({ "ceiling", "CEILING", { L.field.getX() + 236.0f, L.stageRect (kRows - 1).getY() + 34.0f, 60.0f, kh }, false });
        for (int i = 0; i < 4; ++i) keys.push_back ({ "goto" + juce::String (i), kCornerNames[i], { L.body.getX() + 8.0f + (i & 1) * 212.0f, L.body.getY() + 16.0f + (i >> 1) * 80.0f, 52.0f, kh }, editing == i });
    }
    else
    {
        keys.push_back ({ "speech", "SPEECH", { L.sortRow.getX() + 8.0f, 5.0f, 60.0f, kh }, sound.speech });
        keys.push_back ({ "bells", "BELLS", { L.sortRow.getX() + 72.0f, 5.0f, 56.0f, kh }, ! sound.speech });
        keys.push_back ({ "frame", "FRAME", { L.field.getRight() - 60.0f, 5.0f, 60.0f, kh }, false });
    }
    if (L.timelineOpen)
    {
        keys.push_back ({ "play", tl.playing ? "STOP" : "PLAY", { L.tlAx.getRight() - 188.0f, L.tl.getY() + 3.0f, 60.0f, kh }, tl.playing });
        keys.push_back ({ "loop", "LOOP", { L.tlAx.getRight() - 124.0f, L.tl.getY() + 3.0f, 60.0f, kh }, tl.loop });
    }
    keys.push_back ({ "addkey", "+ KEY", { L.tlAx.getRight() - 60.0f, L.tl.getY() + 3.0f, 60.0f, kh }, false });
}

void Workstation::setProbe (juce::Point<float> p)
{
    probe = L.toField (p);
    playBody = false;
    status = current() ? "" : "outside the anchors";
    redraw();
}

void Workstation::setPairT (juce::Point<float> p)
{
    const auto a = L.fromField (lib.anchors[(size_t) pairA].p), b = L.fromField (lib.anchors[(size_t) pairB].p);
    const float dx = b.x - a.x, dy = b.y - a.y, len2 = std::max (1e-6f, dx * dx + dy * dy);
    pairT = juce::jlimit (0.0, 1.0, (double) (((p.x - a.x) * dx + (p.y - a.y) * dy) / len2));
    const auto& pa = lib.anchors[(size_t) pairA].p;
    const auto& pb = lib.anchors[(size_t) pairB].p;
    probe = std::array<double, 2> { pa[0] + (pb[0] - pa[0]) * pairT, pa[1] + (pb[1] - pa[1]) * pairT };
    playBody = false;
    redraw();
}

void Workstation::setWheel (juce::Point<float> p)
{
    body.morph = juce::jlimit (0.0, 1.0, (double) (p.x - L.square.getX()) / L.square.getWidth());
    body.q = juce::jlimit (0.0, 1.0, (double) (L.square.getBottom() - p.y) / L.square.getHeight());
    playBody = true;
    redraw();
}

void Workstation::scrubTo (float x)
{
    tl.playhead = L.tAt (x);
    if (const auto p = tl.pathAt (tl.playhead)) { probe = p; playBody = false; }
    redraw();
}

void Workstation::pickHz (float x)
{
    const double t = juce::jlimit (0.0, 1.0, (double) (x - L.rx (20.0)) / (L.rx (20000.0) - L.rx (20.0)));
    fieldHz = 20.0 * std::pow (1000.0, t);
    redraw();
}

void Workstation::openEditor (int corner)
{
    if (body.corner[(size_t) corner] < 0) { status = "corner is empty"; redraw(); return; }
    const auto& src = lib.frames[(size_t) body.corner[(size_t) corner]];
    if (! src.capture)
    {
        Frame f = src;
        f.capture = true;
        f.name = "edit " + src.name;
        f.group = kGroups - 1;
        lib.frames.push_back (f);
        body.corner[(size_t) corner] = (int) lib.frames.size() - 1;
    }
    editing = corner;
    editCorner = corner;
    playBody = true;
    L.room = Room::edit;
    status = "";
    redraw();
}

void Workstation::dragHandle (juce::Point<float> p)
{
    auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    const auto r = L.stageRect (dragStage);
    const double hz = L.hzAt (r, p.x), db = L.dbAt (r, p.y);
    if (mode == Mode::dragPole)
    {
        setPole (f.words, dragStage, hz, 1.0 - std::pow (10.0, -juce::jlimit (0.0, 60.0, db + 30.0) / 20.0));
        if (lockRow[(size_t) dragStage] && f.rows[(size_t) dragStage].zero) setZero (f.words, dragStage, hz, f.rows[(size_t) dragStage].zR);
    }
    else setZero (f.words, dragStage, hz, 1.0 - std::pow (10.0, -juce::jlimit (0.0, 60.0, 30.0 - db) / 20.0));
    measure (f);
    redraw();
}

int Workstation::f1Row (const Frame& f) const
{
    int best = -1;
    double lo = 1e9;
    for (int s = 0; s < kRows; ++s)
        if (f.rows[(size_t) s].pole && f.rows[(size_t) s].pR > 0.85 && f.rows[(size_t) s].pHz < lo) { lo = f.rows[(size_t) s].pHz; best = s; }
    return best;
}

void Workstation::setOpen (float x)
{
    openAmount = juce::jlimit (0.0, 1.0, (double) (x - openBar.getX()) / openBar.getWidth());
    auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    const int row = f1Row (f);
    if (row < 0) { status = "no F1 pole in this frame"; redraw(); return; }
    const double f1 = 250.0 * std::pow (900.0 / 250.0, openAmount), b1 = 60.0 + 60.0 * openAmount;
    setPole (f.words, row, f1, std::exp (-juce::MathConstants<double>::pi * b1 / kDatumHz));
    measure (f);
    status = "";
    redraw();
}

void Workstation::sharpenQ()
{
    if (body.corner[0] < 0 || body.corner[1] < 0) { status = "fill M0 Q0 and M1 Q0 first"; redraw(); return; }
    for (int i = 0; i < 2; ++i)
    {
        Frame f = lib.frames[(size_t) body.corner[(size_t) i]];
        f.capture = true;
        f.group = kGroups - 1;
        f.name = "sharp " + f.name.replace (" Q0", " Q1");
        sharpen (f.words, 0.25);
        measure (f);
        lib.frames.push_back (f);
        body.corner[(size_t) (2 + i)] = (int) lib.frames.size() - 1;
    }
    playBody = true;
    redraw();
}

void Workstation::ceiling()
{
    if (editing < 0) return;
    auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    setZero (f.words, kRows - 1, 20000.0, 0.9995);
    measure (f);
    redraw();
}

void Workstation::loadWav (int index)
{
    if (index < 0 || index >= (int) wavs.size()) return;
    if (! sound.load (wavs[(size_t) index])) { status = "could not read " + wavs[(size_t) index].getFileName(); redraw(); return; }
    L.compute ((float) getWidth(), (float) getHeight());
    sound.render ((int) L.field.getWidth() - 2, (int) L.field.getHeight() - 2);
    if (audio) audio->setClip (sound.mono, sound.sampleRate);
    status = "";
    redraw();
}

void Workstation::setSlice (float x)
{
    if (sound.mono->empty()) return;
    sound.slice = juce::jlimit (0.0, sound.seconds, (double) (x - L.field.getX()) / L.field.getWidth() * sound.seconds);
    redraw();
}

void Workstation::setRegion (float x, bool start)
{
    if (sound.mono->empty()) return;
    const double t = juce::jlimit (0.0, sound.seconds, (double) (x - L.field.getX()) / L.field.getWidth() * sound.seconds);
    if (start) { sound.regionA = t; sound.regionB = t; }
    else sound.regionB = t;
    if (audio) audio->setRegion (std::min (sound.regionA, sound.regionB), std::max (sound.regionA, sound.regionB));
    redraw();
}

void Workstation::frameFromSlice()
{
    const auto w = sound.frameAt (sound.slice);
    if (! w) { status = "no frame at this slice"; redraw(); return; }
    Frame f;
    f.words = *w;
    f.capture = true;
    f.group = kGroups - 1;
    f.name = sound.file.getFileNameWithoutExtension().substring (0, 18) + " @" + juce::String (sound.slice, 2);
    measure (f);
    lib.frames.push_back (f);
    lib.anchors.push_back ({ (int) lib.frames.size() - 1, lib.coordOf (f) });
    lib.retriangulate();
    pickFor = (int) lib.anchors.size() - 1;
    setRoom (Room::frames);
    status = f.name + " picked";
}

void Workstation::capture()
{
    const auto b = current();
    if (! b) { status = "outside the anchors"; redraw(); return; }
    const int idx = lib.addCapture (lib.wordsOf (*b), *probe);
    status = "captured " + lib.frames[(size_t) idx].name;
    redraw();
}

void Workstation::assignCorner (int i)
{
    editCorner = i;
    if (copyFrom >= 0 && body.corner[(size_t) copyFrom] >= 0)
    {
        Frame f = lib.frames[(size_t) body.corner[(size_t) copyFrom]];
        f.capture = true;
        f.group = kGroups - 1;
        f.name = "copy " + f.name;
        lib.frames.push_back (f);
        body.corner[(size_t) i] = (int) lib.frames.size() - 1;
        copyFrom = -1;
    }
    else if (pickFor >= 0 && pickFor < (int) lib.anchors.size())
    {
        body.corner[(size_t) i] = lib.anchors[(size_t) pickFor].frame;
        pickFor = -1;
    }
    else if (pairMode && pairA >= 0 && pairB >= 0)
    {
        body.corner[(size_t) i] = lib.addCapture (lib.wordsOf (*current()), *probe);
    }
    else if (const auto b = current())
    {
        int single = -1;
        for (int k = 0; k < 3; ++k) if (b->w[(size_t) k] > 0.999) single = lib.anchors[(size_t) b->anchors[(size_t) k]].frame;
        body.corner[(size_t) i] = single >= 0 ? single : lib.addCapture (lib.wordsOf (*b), *probe);
    }
    else return;
    playBody = true;
    status = "";
    redraw();
}

void Workstation::press (const juce::String& id)
{
    if (id == "room0") setRoom (Room::frames);
    else if (id == "room1") { if (body.corner[(size_t) editCorner] >= 0) openEditor (editCorner); else status = "pick a corner first"; }
    else if (id == "room2") setRoom (Room::sound);
    else if (id == "axx") lib.axisX = (lib.axisX + 1) % kMeasures;
    else if (id == "axy") lib.axisY = (lib.axisY + 1) % kMeasures;
    else if (id == "sort") lib.sort();
    else if (id == "pair") { pairMode = ! pairMode; pairA = pairB = -1; pairT = 0.0; status = pairMode ? "pick two anchors" : ""; }
    else if (id == "capture") capture();
    else if (id.startsWith ("corner")) assignCorner (id.substring (6).getIntValue());
    else if (id.startsWith ("goto")) openEditor (id.substring (4).getIntValue());
    else if (id.startsWith ("row")) { const int s = id.substring (3).getIntValue(); body.rowOn[(size_t) s] = ! body.rowOn[(size_t) s]; }
    else if (id == "playbody") playBody = ! playBody;
    else if (id == "edit") { if (body.corner[(size_t) editCorner] >= 0) openEditor (editCorner); else status = "pick a corner first"; }
    else if (id == "export") exportBody (exportDir.getChildFile ("ws_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".body240"));
    else if (id == "copy") { copyFrom = copyFrom >= 0 ? -1 : editCorner; status = copyFrom >= 0 ? "click the corner to paste into" : ""; }
    else if (id == "sharpen") sharpenQ();
    else if (id == "unity") body.unity = ! body.unity;
    else if (id == "ceiling") ceiling();
    else if (id.startsWith ("lock")) { const int s = id.substring (4).getIntValue(); lockRow[(size_t) s] = ! lockRow[(size_t) s]; }
    else if (id == "speech") sound.speech = true;
    else if (id == "bells") sound.speech = false;
    else if (id == "frame") frameFromSlice();
    else if (id == "listen")
    {
        if (audio == nullptr) status = "no audio device";
        else if (L.room == Room::sound && sound.mono->empty()) status = "load a sound first";
        else { audio->setPlaying (! audio->isPlaying()); if (audio->isPlaying()) startTimerHz (30); else if (! tl.playing) stopTimer(); }
    }
    else if (id == "wet") { wet = ! wet; if (audio) audio->setWet (wet); }
    else if (id == "play")
    {
        tl.playing = ! tl.playing;
        if (tl.playing) { playT0 = juce::Time::getMillisecondCounterHiRes(); playFrom = tl.playhead >= tl.duration ? 0.0 : tl.playhead; startTimerHz (60); }
        else if (audio == nullptr || ! audio->isPlaying()) stopTimer();
    }
    else if (id == "loop") tl.loop = ! tl.loop;
    else if (id == "addkey")
    {
        if (! probe) probe = std::array<double, 2> { 0.5, 0.5 };
        tl.keys.push_back ({ tl.playhead, *probe, kData });
    }
    redraw();
}

int Workstation::anchorAt (juce::Point<float> p) const
{
    if (L.room != Room::frames || ! L.field.contains (p)) return -1;
    for (int i = (int) lib.anchors.size() - 1; i >= 0; --i)
        if (visible (i) && L.fromField (lib.anchors[(size_t) i].p).getDistanceFrom (p) < 6.0f) return i;
    return -1;
}

int Workstation::trayAt (juce::Point<float> p) const
{
    if (! L.tray.contains (p)) return -1;
    const int row = (int) ((p.y - L.tray.getY() - 6.0f) / 14.0f) + trayScroll;
    return row >= 0 && row < (int) trayRows.size() ? row : -1;
}

int Workstation::keyAt (juce::Point<float> p) const
{
    if (! L.timelineOpen) return -1;
    for (int i = 0; i < (int) tl.keys.size(); ++i)
        if (juce::Point<float> (L.tx (tl.keys[(size_t) i].t), L.tlAx.getCentreY()).getDistanceFrom (p) < 8.0f) return i;
    return -1;
}

void Workstation::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    for (const auto& k : keys)
        if (k.box.contains (p)) { press (k.id); return; }
    if (L.room == Room::sound)
    {
        if (L.tray.contains (p))
        {
            const int row = (int) ((p.y - L.tray.getY() - 6.0f) / 14.0f) + wavScroll;
            if (row >= 0 && row < (int) wavs.size()) loadWav (row);
            return;
        }
        if (p.y >= L.field.getBottom() && p.y <= L.field.getBottom() + 16.0f && p.x >= L.field.getX() && p.x <= L.field.getRight()) { mode = Mode::region; setRegion (p.x, true); return; }
        if (L.field.contains (p)) { mode = Mode::slice; setSlice (p.x); return; }
        if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
        return;
    }
    if (L.room == Room::edit)
    {
        if (openBar.contains (p)) { mode = Mode::open; setOpen (p.x); return; }
        if (L.field.contains (p) && body.corner[(size_t) editing] >= 0)
        {
            const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
            for (int s = 0; s < kRows; ++s)
            {
                const auto r = L.stageRect (s);
                if (! r.contains (p)) continue;
                const auto& g = f.rows[(size_t) s];
                dragStage = s;
                if (g.zero && juce::Point<float> (L.sx (r, g.zHz), L.sy (r, sectionDb (f.words, s, g.zHz))).getDistanceFrom (p) < 10.0f) { mode = Mode::dragZero; return; }
                mode = Mode::dragPole;
                dragHandle (p);
                return;
            }
        }
        if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
        return;
    }
    if (const int t = trayAt (p); t >= 0)
    {
        const auto& row = trayRows[(size_t) t];
        if (row.header) { open[(size_t) row.group] = ! open[(size_t) row.group]; redraw(); return; }
        if (pickFor >= 0 && pickFor < (int) lib.anchors.size()) { lib.anchors[(size_t) pickFor].frame = row.frame; pickFor = -1; lib.retriangulate(); redraw(); return; }
        mode = Mode::dragFrame; dragFrame = row.frame; dragPos = p; dragStart = p; return;
    }
    if (const int a = anchorAt (p); a >= 0)
    {
        if (pairMode)
        {
            if (pairA < 0) { pairA = a; status = "pick the far anchor"; }
            else if (pairB < 0 && a != pairA) { pairB = a; pairT = 0.0; probe = lib.anchors[(size_t) pairA].p; status = ""; }
            else { pairA = a; pairB = -1; status = "pick the far anchor"; }
            playBody = false;
            redraw();
            return;
        }
        mode = Mode::dragAnchor; dragAnchor = a; dragPos = p; dragStart = p; return;
    }
    if (const int k = keyAt (p); k >= 0) { mode = Mode::dragKey; dragKey = k; dragStart = p; return; }
    if (L.timelineOpen && L.tlAx.contains (p)) { mode = Mode::scrub; scrubTo (p.x); return; }
    if (L.square.contains (p) && body.ready()) { mode = Mode::wheel; setWheel (p); return; }
    if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
    if (pairMode && pairA >= 0 && pairB >= 0 && L.field.contains (p)) { mode = Mode::pair; setPairT (p); return; }
    if (L.field.contains (p)) { mode = Mode::probe; setProbe (p); return; }
}

void Workstation::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    switch (mode)
    {
        case Mode::probe: setProbe (p); break;
        case Mode::scrub: scrubTo (p.x); break;
        case Mode::pickHz: pickHz (p.x); break;
        case Mode::pair: setPairT (p); break;
        case Mode::wheel: setWheel (p); break;
        case Mode::dragPole:
        case Mode::dragZero: dragHandle (p); break;
        case Mode::open: setOpen (p.x); break;
        case Mode::slice: setSlice (p.x); break;
        case Mode::region: setRegion (p.x, false); break;
        case Mode::dragFrame:
        case Mode::dragAnchor: dragPos = p; redraw(); break;
        case Mode::dragKey: tl.keys[(size_t) dragKey].t = L.tAt (p.x); redraw(); break;
        case Mode::none: break;
    }
}

void Workstation::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const bool moved = p.getDistanceFrom (dragStart) > 6.0f;
    if (mode == Mode::dragFrame && L.field.contains (p) && moved)
    {
        lib.anchors.push_back ({ dragFrame, L.toField (p) });
        lib.retriangulate();
    }
    else if (mode == Mode::dragAnchor && moved)
    {
        auto& a = lib.anchors[(size_t) dragAnchor];
        int cornerHit = -1;
        for (const auto& k : keys) if (k.id.startsWith ("corner") && k.box.contains (p)) cornerHit = k.id.substring (6).getIntValue();
        if (cornerHit >= 0) { body.corner[(size_t) cornerHit] = a.frame; editCorner = cornerHit; playBody = true; }
        else if (L.timelineOpen && L.tlAx.contains (p)) tl.keys.push_back ({ L.tAt (p.x), a.p, kData });
        else if (L.field.contains (p)) { a.p = L.toField (p); lib.retriangulate(); }
        else { lib.anchors.erase (lib.anchors.begin() + dragAnchor); lib.retriangulate(); }
    }
    else if (mode == Mode::dragAnchor && ! moved)
    {
        pickFor = pickFor == dragAnchor ? -1 : dragAnchor;
        status = pickFor >= 0 ? "picked, click a corner" : "";
    }
    else if (mode == Mode::dragKey && ! moved)
    {
        probe = tl.keys[(size_t) dragKey].p;
        tl.playhead = tl.keys[(size_t) dragKey].t;
        playBody = false;
    }
    mode = Mode::none;
    dragFrame = dragAnchor = dragKey = -1;
    redraw();
}

void Workstation::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position;
    if (L.tray.contains (p))
    {
        if (L.room == Room::sound) wavScroll = std::max (0, wavScroll - (int) std::round (w.deltaY * 30));
        else trayScroll = std::max (0, trayScroll - (int) std::round (w.deltaY * 30));
        redraw();
    }
    else if (L.room == Room::frames && L.field.contains (p))
    {
        const auto before = L.toField (p);
        L.zoom = juce::jlimit (0.5, 6.0, L.zoom * (w.deltaY > 0 ? 1.1 : 0.9));
        const auto after = L.fromField (before);
        L.pan[0] += p.x - after.x;
        L.pan[1] += p.y - after.y;
        redraw();
    }
}

std::vector<Batch> Workstation::scene() const
{
    std::vector<Batch> out;
    const auto ks = tl.sorted();
    const auto b = current();
    const bool haveWords = L.room == Room::sound ? ! sound.mono->empty() : (b.has_value() || (playBody && body.ready()));
    if (L.room == Room::frames)
    {
        Batch grid { Batch::lines, false, {} };
        for (int i = 1; i < 4; ++i)
        {
            const auto a = L.fromField ({ i / 4.0, 0.0 }), c = L.fromField ({ i / 4.0, 1.0 }), d = L.fromField ({ 0.0, i / 4.0 }), f = L.fromField ({ 1.0, i / 4.0 });
            grid.v.push_back (vertex (a, kLine, 1.0f)); grid.v.push_back (vertex (c, kLine, 1.0f));
            grid.v.push_back (vertex (d, kLine, 1.0f)); grid.v.push_back (vertex (f, kLine, 1.0f));
        }
        out.push_back (grid);
        Batch links { Batch::lines, false, {} };
        for (size_t i = 0; i + 1 < ks.size(); ++i) { links.v.push_back (vertex (L.fromField (ks[i].p), kDim, 1.0f)); links.v.push_back (vertex (L.fromField (ks[i + 1].p), kDim, 1.0f)); }
        if (pairMode && pairA >= 0 && pairB >= 0)
        {
            links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) pairA].p), kChosen, 1.5f));
            links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) pairB].p), kChosen, 1.5f));
        }
        else if (b && ! playBody)
            for (int i = 0; i < 3; ++i) { links.v.push_back (vertex (L.fromField (*probe), kChosen, 1.0f)); links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) b->anchors[(size_t) i]].p), kChosen, 1.0f)); }
        out.push_back (links);
        Batch pts { Batch::points, true, {} };
        for (int i = 0; i < (int) lib.anchors.size(); ++i)
        {
            if (! visible (i)) continue;
            const auto& a = lib.anchors[(size_t) i];
            const auto& f = lib.frames[(size_t) a.frame];
            const bool chosen = i == pairA || i == pairB || i == pickFor;
            pts.v.push_back (vertex (L.fromField (a.p), chosen ? kChosen : hueOf (f.m[0]), chosen || i == dragAnchor ? 12.0f : 8.0f));
        }
        Batch squares { Batch::points, false, {} };
        for (const auto& k : ks) squares.v.push_back (vertex (L.fromField (k.p), kData, 7.0f));
        if (probe && ! playBody) pts.v.push_back (vertex (L.fromField (*probe), kLive, 9.0f));
        if (mode == Mode::dragFrame) pts.v.push_back (vertex (dragPos, hueOf (lib.frames[(size_t) dragFrame].m[0]), 12.0f));
        if (mode == Mode::dragAnchor) pts.v.push_back (vertex (dragPos, kData, 12.0f));
        out.push_back (squares);
        out.push_back (pts);
        if (playBody && body.ready())
        {
            const auto sq = L.square;
            Batch sqLines { Batch::lines, false, {} };
            sqLines.v.push_back (vertex ({ sq.getCentreX(), sq.getY() }, kLine, 1.0f)); sqLines.v.push_back (vertex ({ sq.getCentreX(), sq.getBottom() }, kLine, 1.0f));
            sqLines.v.push_back (vertex ({ sq.getX(), sq.getCentreY() }, kLine, 1.0f)); sqLines.v.push_back (vertex ({ sq.getRight(), sq.getCentreY() }, kLine, 1.0f));
            out.push_back (sqLines);
        }
        if (body.ready())
        {
            const auto sq = L.square;
            Batch wheel { Batch::points, true, {} };
            for (int i = 0; i < 4; ++i)
                wheel.v.push_back (vertex ({ i & 1 ? sq.getRight() : sq.getX(), i & 2 ? sq.getY() : sq.getBottom() }, hueOf (lib.frames[(size_t) body.corner[(size_t) i]].m[0]), 8.0f));
            wheel.v.push_back (vertex ({ sq.getX() + (float) body.morph * sq.getWidth(), sq.getBottom() - (float) body.q * sq.getHeight() }, kLive, 9.0f));
            out.push_back (wheel);
        }
    }
    if (L.room == Room::edit)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (body.corner[(size_t) i] < 0) continue;
            const juce::Rectangle<float> t (L.body.getX() + 8.0f + (i & 1) * 212.0f, L.body.getY() + 48.0f + (i >> 1) * 80.0f, 200.0f, 40.0f);
            Batch thumb { Batch::strip, false, {} };
            const auto cv = curveOf (lib.frames[(size_t) body.corner[(size_t) i]].words);
            for (int k = 0; k < kCurvePoints; ++k)
                thumb.v.push_back (vertex ({ t.getX() + k / float (kCurvePoints - 1) * t.getWidth(), t.getY() + (float) ((30.0 - juce::jlimit (-30.0, 30.0, cv[(size_t) k])) / 60.0) * t.getHeight() }, i == editing ? kChosen : kDim, 1.0f));
            out.push_back (thumb);
        }
    }
    if (L.room == Room::edit && editing >= 0 && body.corner[(size_t) editing] >= 0)
    {
        const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
        Batch curves { Batch::strip, false, {} };
        const auto cr = L.cascade;
        const auto cv = curveOf (f.words);
        for (int i = 0; i < kCurvePoints; ++i) curves.v.push_back (vertex ({ L.sx (cr, 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.sy (cr, juce::jlimit (-30.0, 30.0, cv[(size_t) i])) }, kData, 1.5f));
        out.push_back (curves);
        Batch handles { Batch::points, false, {} };
        Batch zeroHandles { Batch::points, true, {} };
        for (int s = 0; s < kRows; ++s)
        {
            const auto r = L.stageRect (s);
            const auto& g = f.rows[(size_t) s];
            const bool on = body.rowOn[(size_t) s];
            Batch sc { Batch::strip, false, {} };
            for (int i = 0; i < kCurvePoints; ++i)
            {
                const double hz = 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1));
                sc.v.push_back (vertex ({ L.sx (r, hz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, hz))) }, on ? kData : kDim, 1.2f));
            }
            out.push_back (sc);
            if (g.pole) handles.v.push_back (vertex ({ L.sx (r, g.pHz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, g.pHz))) }, kData, 7.0f));
            if (g.zero) zeroHandles.v.push_back (vertex ({ L.sx (r, g.zHz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, g.zHz))) }, kChosen, 6.0f));
        }
        out.push_back (handles);
        out.push_back (zeroHandles);
    }
    if (L.room == Room::sound && ! sound.mono->empty())
    {
        Batch spec { Batch::strip, false, {} };
        const auto mag = sound.sliceMagnitude (sound.slice);
        for (int i = 0; i < kCurvePoints; ++i) spec.v.push_back (vertex ({ L.rx (20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.ry (juce::jlimit (-30.0, 30.0, (double) mag[(size_t) i] + 30.0)) }, kDim, 1.0f));
        out.push_back (spec);
    }
    Batch marker { Batch::lines, false, {} };
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (30.0) }, kLine, 1.0f));
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (-30.0) }, kLine, 1.0f));
    out.push_back (marker);
    if (haveWords)
    {
        Batch curve { Batch::strip, false, {} };
        const auto cv = curveOf (playingWords());
        for (int i = 0; i < kCurvePoints; ++i) curve.v.push_back (vertex ({ L.rx (20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.ry (juce::jlimit (-30.0, 30.0, cv[(size_t) i])) }, L.room == Room::sound ? kChosen : kData, 1.5f));
        out.push_back (curve);
    }
    {
        Batch rings { Batch::lines, false, {} };
        for (double db : { 20.0, 40.0, 60.0 })
        {
            const double rr = 1.0 - std::pow (10.0, -db / 20.0);
            juce::Point<float> prev;
            for (int i = 0; i <= 40; ++i)
            {
                const auto p = L.armaXY (20.0 * std::pow (2.0, 10.0 * i / 40.0), rr);
                if (i > 0) { rings.v.push_back (vertex (prev, kLine, 1.0f)); rings.v.push_back (vertex (p, kLine, 1.0f)); }
                prev = p;
            }
        }
        for (int o = 0; o <= 10; o += 2) { rings.v.push_back (vertex (L.armaXY (20.0 * std::pow (2.0, o), 0.0), kLine, 1.0f)); rings.v.push_back (vertex (L.armaXY (20.0 * std::pow (2.0, o), 0.999), kLine, 1.0f)); }
        out.push_back (rings);
        if (haveWords)
        {
            const auto rows = geometryOf (playingWords());
            Batch glides { Batch::lines, false, {} };
            Batch poles { Batch::points, false, {} };
            Batch zeros { Batch::points, true, {} };
            std::vector<std::pair<int, double>> parents;
            if (L.room == Room::sound) {}
            else if (playBody && body.ready()) { const auto w = body.weights(); for (int i = 0; i < 4; ++i) parents.push_back ({ body.corner[(size_t) i], w[(size_t) i] }); }
            else if (b) for (int i = 0; i < 3; ++i) parents.push_back ({ lib.anchors[(size_t) b->anchors[(size_t) i]].frame, b->w[(size_t) i] });
            for (int s = 0; s < kRows; ++s)
            {
                const auto& r = rows[(size_t) s];
                if (! r.pole) continue;
                const auto here = L.armaXY (r.pHz, r.pR);
                for (const auto& [frameIdx, weight] : parents)
                {
                    if (weight < 0.08) continue;
                    const auto& pr = lib.frames[(size_t) frameIdx].rows[(size_t) s];
                    if (! pr.pole) continue;
                    glides.v.push_back (vertex (here, kChosen, 1.0f));
                    glides.v.push_back (vertex (L.armaXY (pr.pHz, pr.pR), kChosen, 1.0f));
                }
                poles.v.push_back (vertex (here, kData, 8.0f));
                if (r.zero && r.zR > 0.01) zeros.v.push_back (vertex (L.armaXY (r.zHz, r.zR), kChosen, 7.0f));
            }
            out.push_back (glides);
            out.push_back (poles);
            out.push_back (zeros);
        }
    }
    if (L.timelineOpen)
    {
        Batch tline { Batch::lines, false, {} };
        for (size_t i = 0; i + 1 < ks.size(); ++i) { tline.v.push_back (vertex ({ L.tx (ks[i].t), L.tlAx.getCentreY() }, kDim, 1.0f)); tline.v.push_back (vertex ({ L.tx (ks[i + 1].t), L.tlAx.getCentreY() }, kDim, 1.0f)); }
        tline.v.push_back (vertex ({ L.tx (tl.playhead), L.tlAx.getY() }, kLive, 1.0f));
        tline.v.push_back (vertex ({ L.tx (tl.playhead), L.tlAx.getBottom() }, kLive, 1.0f));
        out.push_back (tline);
        Batch tpts { Batch::points, false, {} };
        for (const auto& k : ks) tpts.v.push_back (vertex ({ L.tx (k.t), L.tlAx.getCentreY() }, kData, 8.0f));
        out.push_back (tpts);
    }
    return out;
}

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
