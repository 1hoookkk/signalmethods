#include "Workstation.h"
#include "Style.h"
#include <cmath>

namespace ws
{
void Workstation::setSurface (juce::Point<float> p)
{
    const auto s = spotAt (p);
    if (! s.valid()) return;
    spot = s;
    playBody = false;
    status = st.nameOf (s, open);
    redraw();
}

void Workstation::setPairT (juce::Point<float> p)
{
    pairFromPoint (p);
    redraw();
}

void Workstation::setWheel (juce::Point<float> p)
{
    body.morph = juce::jlimit (kWheelLow, kWheelHigh, (double) (p.x - L.square.getX()) / L.square.getWidth());
    body.q = juce::jlimit (kWheelLow, kWheelHigh, (double) (L.square.getBottom() - p.y) / L.square.getHeight());
    playBody = true;
    redraw();
}

void Workstation::scrubTo (float x)
{
    tl.playhead = L.tAt (x);
    if (const auto s = tl.pathAt (tl.playhead, st)) { spot = s; playBody = false; }
    redraw();
}

void Workstation::groupMean (int group)
{
    const int idx = lib.addGroupMean (group);
    status = idx >= 0 ? "captured " + lib.frames[(size_t) idx].name : "group is empty";
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
    auto chord = f.chord;
    auto& st = chord[(size_t) dragStage];
    if (mode == Mode::dragPole)
    {
        st.pole.on = true;
        st.pole.note = noteOf (hz);
        st.pole.width = widthOf (hz, 1.0 - std::pow (10.0, -juce::jlimit (0.0, 60.0, db + 30.0) / 20.0), kDatumHz);
        if (lockRow[(size_t) dragStage] && st.zero.on) st.zero.note = st.pole.note;
    }
    else
    {
        st.zero.on = true;
        st.zero.note = noteOf (hz);
        st.zero.width = widthOf (hz, 1.0 - std::pow (10.0, -juce::jlimit (0.0, 60.0, 30.0 - db) / 20.0), kDatumHz);
    }
    setChord (f, chord);
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
    auto chord = f.chord;
    chord[(size_t) row].pole.on = true;
    chord[(size_t) row].pole.note = noteOf (f1);
    chord[(size_t) row].pole.width = widthOf (f1, std::exp (-juce::MathConstants<double>::pi * b1 / kDatumHz), kDatumHz);
    setChord (f, chord);
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
        auto chord = f.chord;
        sharpen (chord, 0.25);
        setChord (f, chord);
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
    auto chord = f.chord;
    chord[kRows - 1].zero.on = true;
    chord[kRows - 1].zero.note = noteOf (20000.0);
    chord[kRows - 1].zero.width = widthOf (20000.0, 0.9995, kDatumHz);
    setChord (f, chord);
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
    f.capture = true;
    f.group = kGroups - 1;
    f.name = sound.file.getFileNameWithoutExtension().substring (0, 18) + " @" + juce::String (sound.slice, 2);
    setWords (f, *w, kDatumHz);
    lib.frames.push_back (f);
    {
        auto chord = f.chord;
        for (auto& stg : chord) if (stg.pole.on) { stg.pole.width = 0.25; stg.zero.on = true; stg.zero.note = stg.pole.note; stg.zero.width = 4.0; }
        setChord (f, chord);
        lib.frames.back() = f;
    }
    const Words placed = f.words;
    const int stub = st.addStub (f.name, placed, st.nearestNode (placed), kGroups - 1);
    st.stubs[(size_t) stub].frame = (int) lib.frames.size() - 1;
    Spot ps; ps.stub = stub;
    picked = ps;
    setRoom (Room::frames);
    status = f.name + " picked";
}

void Workstation::capture()
{
    if (! haveSound() || L.room != Room::frames) { status = "nothing to capture"; redraw(); return; }
    Spot at;
    if (pairLive()) at.node = pairT < 0.5 ? pairA : pairB;
    else if (spot) at = *spot;
    const juce::String name = pairLive() ? "cap " + pairName() + " " + juce::String (pairT, 2) : "cap " + st.nameOf (at, open);
    const int idx = captureSpot (at, live().words, name);
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
    else if (picked)
    {
        body.corner[(size_t) i] = frameFor (*picked);
        picked.reset();
    }
    else if (pairLive())
    {
        Spot at; at.node = pairT < 0.5 ? pairA : pairB;
        body.corner[(size_t) i] = captureSpot (at, live().words, "cap " + pairName() + " " + juce::String (pairT, 2));
    }
    else if (spot)
    {
        body.corner[(size_t) i] = frameFor (*spot);
    }
    else return;
    playBody = true;
    status = "";
    redraw();
}
}
