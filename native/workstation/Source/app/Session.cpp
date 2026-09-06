#include "Session.h"
#include <algorithm>
#include <trench/core/body_from_audio.hpp>

namespace hs
{
namespace
{
std::array<std::uint16_t, 30> flat (const Words& w)
{
    std::array<std::uint16_t, 30> out {};
    for (size_t s = 0; s < kRows; ++s) for (size_t k = 0; k < kWords; ++k) out[s * kWords + k] = w[s][k];
    return out;
}
}

Session::Session (const juce::File& rootDir, const juce::File& quadFile, bool audioOn)
    : root (rootDir), file (quadFile), withAudio (audioOn)
{
    stars = loadVowels (root.getChildFile ("native/workstation/banks/Klatt 1980.bank.json"));
    stars.push_back (schwa());
    for (const auto& s : loadVowels (root.getChildFile ("native/workstation/banks/Hillenbrand 1995.bank.json"))) stars.push_back (s);
    for (const auto& s : loadBodies (root.getChildFile ("evidence/measured-bodies/ir_library"))) stars.push_back (s);
    libraryCount = stars.size();
    const bool had = open (quad, stars, libraryCount, file, &cube);
    if (! had || ! quad.complete())
        pinAll ({ starNamed (juce::String (juce::CharPointer_UTF8 ("\xc9\x91"))), starNamed (juce::String (juce::CharPointer_UTF8 ("\xc9\x99"))), starNamed ("i"), starNamed ("u") });
    history.clear(); future.clear();
    audio.onNote = [this] (int n) { note = n; changed(); };
    audition();
}

Session::~Session() { audio.stop(); }

void Session::changed() { if (onChange) onChange(); }

int Session::starNamed (const juce::String& name) const
{
    for (int i = 0; i < (int) stars.size(); ++i) if (stars[(size_t) i].name == name) return i;
    return -1;
}

juce::String Session::uniqueName (const juce::String& name) const
{
    if (starNamed (name) < 0) return name;
    for (int n = 2;; ++n) if (starNamed (name + " " + juce::String (n)) < 0) return name + " " + juce::String (n);
}

juce::String Session::pinName (int n) const
{
    const int p = quad.pins[(size_t) n];
    return p >= 0 && p < (int) stars.size() ? stars[(size_t) p].name : juce::String();
}

int Session::currentStar() const
{
    if (auditioning >= 0 || auditioning == kMade) return auditioning;
    return selected;
}

void Session::audition()
{
    if (editing >= 0 && (editingCube ? cube.pins[(size_t) editing] : quad.pins[(size_t) kCornerPin[editing]]) >= 0)
    {
        auditioning = editingCube ? cube.pins[(size_t) editing] : quad.pins[(size_t) kCornerPin[editing]];
        words = editWords();
        sounding = true;
        status = stars[(size_t) auditioning].name;
        playingLabel = (editingCube ? "cube " + juce::String (editing + 1) : juce::String::charToString (kCornerLetters[editing])) + "  " + status;
    }
    else if (auditioning == kCube)
    {
        sounding = cube.complete();
        if (! sounding)
        {
            playing = false;
            playingLabel = "";
            if (withAudio) audio.setPlaying (false);
            return;
        }
        words = cubeWordsAt (cube, stars);
        status = "";
        playingLabel = "cube " + juce::String (cube.x * 100.0, 0) + " " + juce::String (cube.y * 100.0, 0) + " at depth " + juce::String (cube.z * 100.0, 0);
    }
    else if (auditioning == kPair && pairA >= 0 && pairB >= 0 && pairA < (int) stars.size() && pairB < (int) stars.size())
    {
        Corners c { stars[(size_t) pairA].words, stars[(size_t) pairB].words, stars[(size_t) pairA].words, stars[(size_t) pairB].words };
        words = lerp (c, pairT, 0.0);
        sounding = true;
        status = stars[(size_t) pairA].name + " > " + stars[(size_t) pairB].name + "  " + juce::String (pairT * 100.0, 0);
        playingLabel = status;
    }
    else if (auditioning == kMade && madeLive)
    {
        words = made.words;
        sounding = true;
        status = made.name;
        playingLabel = made.parentA.isNotEmpty() ? made.parentA + " moved to " + formantName (made.words) : "made " + made.name;
    }
    else if (auditioning >= 0 && auditioning < (int) stars.size())
    {
        words = stars[(size_t) auditioning].words;
        sounding = true;
        status = stars[(size_t) auditioning].name;
        playingLabel = status;
    }
    else if (quad.complete())
    {
        words = wordsAt (quad, stars);
        sounding = true;
        status = juce::String (quad.morph, 0) + " " + juce::String (quad.q, 0);
        playingLabel = "pad " + status;
    }
    else
    {
        sounding = false;
        status = "";
        playingLabel = "";
        return;
    }
    heard = tracking ? transposed (words, std::pow (2.0, (note - 45) / 12.0)) : words;
    if (tracking) playingLabel += "   track " + noteName (440.0 * std::pow (2.0, (note - 69) / 12.0));
    if (withAudio) audio.publish (flat (heard));
}

Session::Snapshot Session::snapshot() const
{
    Snapshot s;
    s.quad = quad;
    s.cube = cube;
    s.added.assign (stars.begin() + (long) libraryCount, stars.end());
    return s;
}

void Session::restore (const Snapshot& s)
{
    quad = s.quad;
    cube = s.cube;
    stars.resize (libraryCount);
    stars.insert (stars.end(), s.added.begin(), s.added.end());
    for (auto& p : quad.pins) if (p >= (int) stars.size()) p = -1;
    for (auto& p : cube.pins) if (p >= (int) stars.size()) p = -1;
    if (selected >= (int) stars.size()) selected = -1;
    if (editing >= 0 && (editingCube ? cube.pins[(size_t) editing] : quad.pins[(size_t) kCornerPin[editing]]) < 0) editing = -1;
}

void Session::apply()
{
    save (quad, stars, libraryCount, file, &cube);
    audition();
    changed();
}

void Session::hover (int k)
{
    if (k < -1 || k >= (int) stars.size() || k == hovered) return;
    hovered = k;
    changed();
}

void Session::unhover() { hover (-1); }

void Session::select (int k)
{
    if (k < 0 || k >= (int) stars.size()) return;
    selected = k;
    editing = -1; editingCube = false;
    auditioning = k;
    audition();
    changed();
}

void Session::morphPair (int a, int b, double t)
{
    if (a < 0 || b < 0 || a >= (int) stars.size() || b >= (int) stars.size()) return;
    pairA = a; pairB = b; pairT = std::clamp (t, 0.0, 1.0);
    auditioning = kPair;
    audition();
    changed();
}

void Session::setMade (double f1, double f2)
{
    made = madeVowel (std::clamp (f1, 100.0, 1500.0), std::clamp (f2, 300.0, 4000.0));
    madeLive = true;
    auditioning = kMade;
    editing = -1;
    audition();
    changed();
}

void Session::setTransposed (int star, double f1)
{
    if (star < 0 || star >= (int) stars.size()) return;
    const auto f = formantsOf (stars[(size_t) star].words);
    if (f[0] <= 0.0 || f1 <= 0.0) return;
    made = stars[(size_t) star];
    made.kind = "made";
    made.words = transposed (stars[(size_t) star].words, f1 / f[0]);
    made.name = stars[(size_t) star].name + " " + formantName (made.words);
    made.body = made.name; made.corner = ""; made.parentA = stars[(size_t) star].name; made.parentB = "";
    made.morph = 0.0; made.q = 0.0;
    madeLive = true;
    auditioning = kMade;
    editing = -1;
    audition();
    changed();
}

void Session::pin (int n, int star)
{
    if (n < 0 || n > 3 || star < 0 || star >= (int) stars.size()) return;
    history.push_back (snapshot()); future.clear();
    quad.pins[(size_t) n] = star;
    auditioning = -1;
    apply();
}

void Session::pinCorner (int corner, int star)
{
    if (corner < 0 || corner > 3) return;
    if (inPair() || inMade() || auditioning == kCube || star == kMade || star == kCube) { keep(); star = selected; }
    pin (kCornerPin[corner], star);
}

bool Session::placeable() const
{
    return inPair() || inMade() || (auditioning == kCube && cube.complete()) || currentStar() >= 0 || (auditioning == -1 && quad.complete());
}

void Session::toCorner (int corner)
{
    if (! placeable()) return;
    if (auditioning == -1 && quad.complete()) { keep(); pinCorner (corner, selected); return; }
    pinCorner (corner, currentStar());
}

void Session::pinAll (const std::array<int, 4>& pins)
{
    for (int p : pins) if (p < 0 || p >= (int) stars.size()) return;
    history.push_back (snapshot()); future.clear();
    quad.pins = pins;
    auditioning = -1;
    apply();
}

int Session::addRead (const juce::File& wav)
{
    auto star = readWav (wav);
    if (! star) { status = "cannot read " + wav.getFileName(); changed(); return -1; }
    history.push_back (snapshot()); future.clear();
    star->name = uniqueName (star->name);
    stars.push_back (*star);
    selected = (int) stars.size() - 1;
    auditioning = selected;
    apply();
    return selected;
}

void Session::edit (int corner)
{
    if (editingCube) editing = -1;
    editingCube = false;
    if (corner < 0 || corner > 3 || corner == editing) { editing = -1; auditioning = -1; audition(); changed(); return; }
    const int star = quad.pins[(size_t) kCornerPin[corner]];
    if (star < 0) return;
    editing = corner;
    selected = star;
    auditioning = star;
    audition();
    changed();
}

void Session::editCube (int corner)
{
    if (corner < 0 || corner >= 8 || (editingCube && editing == corner))
    {
        setCubePoint (cube.x, cube.y, cube.z);
        return;
    }
    const int star = cube.pins[(size_t) corner];
    if (star < 0 || star >= (int) stars.size()) return;
    editingCube = true; editing = corner; selected = star;
    audition(); changed();
}

void Session::pinCube (int corner, int star)
{
    if (corner < 0 || corner >= 8) return;
    if (inMade() || inPair()) { keep(); star = selected; }
    if (star < 0 || star >= (int) stars.size()) return;
    history.push_back (snapshot()); future.clear();
    cube.pins[(size_t) corner] = star;
    editing = -1; editingCube = false; auditioning = star;
    apply();
}

void Session::setCubePoint (double x, double y, double z)
{
    cube.x = std::clamp (x, 0.0, 1.0); cube.y = std::clamp (y, 0.0, 1.0); cube.z = std::clamp (z, 0.0, 1.0);
    editing = -1; editingCube = false; auditioning = kCube;
    apply();
}

bool Session::takeSlice()
{
    if (! cube.complete()) return false;
    history.push_back (snapshot()); future.clear();
    const auto body = cubeBodyOf (cube, stars);
    for (int corner = 0; corner < 4; ++corner)
    {
        const auto sample = body.interpolate_words ((float) (corner & 1), (float) ((corner >> 1) & 1), (float) cube.z);
        Words captured;
        std::copy_n (sample.begin(), kRows, captured.begin());
        int pin = -1;
        for (int i = 0; i < (int) stars.size(); ++i) if (stars[(size_t) i].words == captured) { pin = i; break; }
        if (pin < 0)
        {
            Star star;
            star.kind = "capture"; star.words = captured; star.name = uniqueName (formantName (captured)); star.body = star.name;
            star.parentA = stars[(size_t) cube.pins[(size_t) corner]].name;
            star.parentB = stars[(size_t) cube.pins[(size_t) corner + 4]].name;
            star.morph = cube.z * 100.0;
            stars.push_back (star);
            pin = (int) stars.size() - 1;
        }
        quad.pins[(size_t) corner] = pin;
    }
    quad.morph = cube.x * 100.0; quad.q = cube.y * 100.0;
    editing = -1; editingCube = false; auditioning = -1;
    apply();
    return true;
}

void Session::beginRowEdit() { history.push_back (snapshot()); future.clear(); }

void Session::setRow (int corner, int row, Row r)
{
    if (corner < 0 || corner >= (editingCube ? 8 : 4) || row < 0 || row >= kRows) return;
    const int star = editingCube ? cube.pins[(size_t) corner] : quad.pins[(size_t) kCornerPin[corner]];
    if (star < 0 || star >= (int) stars.size()) return;
    setSectionWords (corner, row, rowWords (r, stars[(size_t) star].words[(size_t) row][4]));
}

void Session::setSection (int corner, int row, const Section& section, bool keepFifth)
{
    if (corner < 0 || corner >= (editingCube ? 8 : 4) || row < 0 || row >= kRows) return;
    const int star = editingCube ? cube.pins[(size_t) corner] : quad.pins[(size_t) kCornerPin[corner]];
    if (star < 0 || star >= (int) stars.size()) return;
    setSectionWords (corner, row, sectionWords (section, stars[(size_t) star].words[(size_t) row][4], keepFifth));
}

void Session::setSectionWords (int corner, int row, const trench::core::PackedSection& words)
{
    if (corner < 0 || corner >= (editingCube ? 8 : 4) || row < 0 || row >= kRows) return;
    int& anchor = editingCube ? cube.pins[(size_t) corner] : quad.pins[(size_t) kCornerPin[corner]];
    int star = anchor;
    if (star < 0 || star >= (int) stars.size()) return;
    if (stars[(size_t) star].kind != "capture")
    {
        Star s = stars[(size_t) star];
        s.kind = "capture"; s.parentA = s.name; s.parentB = ""; s.corner = ""; s.morph = 0.0; s.q = 0.0;
        stars.push_back (s);
        star = (int) stars.size() - 1;
        anchor = star;
    }
    auto& s = stars[(size_t) star];
    s.words[(size_t) row] = words;
    const auto name = formantName (s.words);
    if (s.name != name) { s.name = uniqueName (name); s.body = s.name; }
    editing = corner;
    selected = star;
    auditioning = star;
    apply();
}

Words Session::editWords() const
{
    if (editing < 0) return {};
    const int star = editingCube ? cube.pins[(size_t) editing] : quad.pins[(size_t) kCornerPin[editing]];
    return star >= 0 && star < (int) stars.size() ? stars[(size_t) star].words : Words {};
}

void Session::setPuck (double morph, double q)
{
    quad.morph = std::clamp (morph, 0.0, 100.0);
    quad.q = std::clamp (q, 0.0, 100.0);
    auditioning = -1;
    editing = -1;
    editingCube = false;
    apply();
}

void Session::nudge (double dm, double dq) { setPuck (quad.morph + dm, quad.q + dq); }

void Session::keep()
{
    const bool pair = inPair(), fromMade = inMade(), fromCube = auditioning == kCube;
    if (fromCube ? ! cube.complete() : (! pair && ! fromMade && ! quad.complete())) return;
    history.push_back (snapshot()); future.clear();
    Star s;
    s.kind = "capture";
    s.words = pair || fromCube ? words : fromMade ? made.words : wordsAt (quad, stars);
    s.parentA = pair ? stars[(size_t) pairA].name : fromMade ? (made.parentA.isNotEmpty() ? made.parentA : juce::String ("made")) : fromCube ? juce::String ("cube") : pinName (0);
    s.parentB = pair ? stars[(size_t) pairB].name : fromMade || fromCube ? juce::String() : pinName (1);
    s.morph = pair ? pairT * 100.0 : fromMade ? 0.0 : fromCube ? cube.x * 100.0 : quad.morph;
    s.q = pair || fromMade ? 0.0 : fromCube ? cube.y * 100.0 : quad.q;
    quad.captures += 1;
    s.name = uniqueName (fromMade ? made.name : formantName (s.words));
    s.body = s.name;
    stars.push_back (s);
    selected = (int) stars.size() - 1;
    auditioning = selected;
    apply();
}

void Session::removeAdded()
{
    if (selected < (int) libraryCount || selected >= (int) stars.size()) return;
    history.push_back (snapshot()); future.clear();
    stars.erase (stars.begin() + selected);
    for (auto& p : quad.pins) { if (p == selected) p = -1; else if (p > selected) --p; }
    for (auto& p : cube.pins) { if (p == selected) p = -1; else if (p > selected) --p; }
    editing = -1; editingCube = false;
    if (hovered == selected) hovered = -1; else if (hovered > selected) --hovered;
    selected = -1; auditioning = -1;
    apply();
}

void Session::undo()
{
    if (history.empty()) return;
    future.push_back (snapshot());
    restore (history.back());
    history.pop_back();
    hovered = -1; auditioning = -1; editing = -1;
    apply();
}

void Session::redo()
{
    if (future.empty()) return;
    history.push_back (snapshot());
    restore (future.back());
    future.pop_back();
    hovered = -1; auditioning = -1; editing = -1;
    apply();
}

juce::File Session::write (juce::File path)
{
    if (! quad.complete()) return {};
    if (path == juce::File())
    {
        const auto folder = root.getChildFile ("plugin/presets/user");
        folder.createDirectory();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
        path = folder.getChildFile ("headspace_" + stamp + ".body240");
        for (int n = 1; path.existsAsFile(); ++n) path = folder.getChildFile ("headspace_" + stamp + "_" + juce::String (n).paddedLeft ('0', 2) + ".body240");
    }
    status = writeBody (quad, stars, path) ? path.getFileName() : "cannot write";
    changed();
    return path;
}

void Session::setPlaying (bool on)
{
    if (on && withAudio && ! audio.isOpen())
    {
        if (! audio.start()) { status = "no audio device"; playing = false; changed(); return; }
    }
    playing = on;
    if (withAudio) audio.setPlaying (on);
    changed();
}

void Session::setNote (int midi)
{
    note = std::clamp (midi, 0, 127);
    if (withAudio) audio.setNote (note);
    if (tracking) audition();
    changed();
}

void Session::noteOn (int midi)
{
    note = std::clamp (midi, 0, 127);
    if (withAudio) audio.noteOn (note);
    if (tracking) audition();
    changed();
}

void Session::noteOff()
{
    if (withAudio) audio.noteOff();
}

void Session::setTracking (bool on)
{
    tracking = on;
    audition();
    changed();
}

bool Session::setLoop (const juce::File& wav)
{
    const auto clip = trench::core::audio::read_wav_mono (std::filesystem::path (wav.getFullPathName().toWideCharPointer()));
    if (! clip || clip->samples.size() < 2) { status = "cannot read " + wav.getFileName(); changed(); return false; }
    auto shared = std::make_shared<Audio::Clip>();
    shared->samples = clip->samples;
    shared->rate = clip->sample_rate_hz;
    loopName = wav.getFileNameWithoutExtension();
    if (withAudio) audio.setLoop (shared);
    setSource (2);
    return true;
}

void Session::noteIn (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) noteOn (m.getNoteNumber());
    else if (m.isNoteOff() && m.getNoteNumber() == note) noteOff();
}

void Session::setSource (int s)
{
    source = s;
    if (withAudio) audio.setSource (s);
    changed();
}

bool Session::key (const juce::KeyPress& k)
{
    const bool control = k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown();
    const double step = control ? 0.2 : 1.0;
    const int code = k.getKeyCode();
    const auto c = k.getTextCharacter();
    if (code == juce::KeyPress::rightKey) { nudge (step, 0.0); return true; }
    if (code == juce::KeyPress::leftKey) { nudge (-step, 0.0); return true; }
    if (code == juce::KeyPress::upKey) { nudge (0.0, step); return true; }
    if (code == juce::KeyPress::downKey) { nudge (0.0, -step); return true; }
    if (code == juce::KeyPress::spaceKey) { setPlaying (! playing); return true; }
    if (code == juce::KeyPress::deleteKey) { removeAdded(); return true; }
    if (code == juce::KeyPress::escapeKey && editing >= 0) { edit (editing); return true; }
    if (control && (c == 's' || c == 'S' || code == 'S')) { keep(); return true; }
    if (control && (c == 'z' || c == 'Z' || code == 'Z')) { undo(); return true; }
    if (control && (c == 'y' || c == 'Y' || code == 'Y')) { redo(); return true; }
    if (c == 'w' || c == 'W') { write(); return true; }
    if (c == 'n' || c == 'N') { setSource (1); return true; }
    if ((c == 'l' || c == 'L') && loopName.isNotEmpty()) { setSource (2); return true; }
    if (c == 's' || c == 'S') { setSource (0); return true; }
    if (c == 'k' || c == 'K') { setTracking (! tracking); return true; }
    if (c == '[') { setNote (note - 1); return true; }
    if (c == ']') { setNote (note + 1); return true; }
    if (code == juce::KeyPress::pageDownKey) { setNote (note - 12); return true; }
    if (code == juce::KeyPress::pageUpKey) { setNote (note + 12); return true; }
    for (int corner = 0; corner < 4; ++corner)
        if (c == (juce::juce_wchar) ('1' + corner) || c == (juce::juce_wchar) ('a' + corner) || c == (juce::juce_wchar) ('A' + corner)) { toCorner (corner); return true; }
    return false;
}
}
