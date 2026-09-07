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
    Explore boot;
    const bool had = open (quad, stars, libraryCount, file, &boot);
    pairA = boot.a; pairB = boot.b; pairT = boot.morph;
    frequency = boot.frequency; stress = boot.stress; octaves = boot.octaves;
    if (pairA < 0 || pairB < 0)
    {
        pairA = starNamed ("i"); pairB = starNamed ("u");
        pairT = 0.5; frequency = 0.0; stress = 1.0; octaves = 1.0;
    }
    if (! had || ! quad.complete())
        pinAll ({ starNamed (juce::String (juce::CharPointer_UTF8 ("\xc9\x91"))), starNamed (juce::String (juce::CharPointer_UTF8 ("\xc9\x99"))), starNamed ("i"), starNamed ("u") });
    history.clear(); future.clear();
    audio.onNote = [this] (int n) { note = n; changed(); };
    audio.onWheel = [this] (double v) { if (inPair()) sweep (v); else setPuck (v * 100.0, quad.q); };
    if (pairA >= 0 && pairB >= 0) auditioning = kPair;
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

Explore Session::explore() const { return { pairA, pairB, pairT, frequency, stress, octaves }; }

void Session::audition()
{
    if (editing >= 0 && quad.pins[(size_t) kCornerPin[editing]] >= 0)
    {
        auditioning = quad.pins[(size_t) kCornerPin[editing]];
        words = editWords();
        sounding = true;
        status = stars[(size_t) auditioning].name;
        playingLabel = juce::String::charToString (kCornerLetters[editing]) + "  " + status;
    }
    else if (auditioning == kPair && pairA >= 0 && pairB >= 0 && pairA < (int) stars.size() && pairB < (int) stars.size())
    {
        words = motherWordsAt (explore(), stars);
        sounding = true;
        status = stars[(size_t) pairA].name + " > " + stars[(size_t) pairB].name + "  " + juce::String (std::lround (pairT * 100.0));
        if (frequency > 0.0) status += "  freq " + juce::String (std::lround (frequency * 100.0));
        if (stress < 1.0) status += "  stress " + juce::String (std::lround (stress * 100.0));
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
        status = juce::String (std::lround (quad.morph)) + " " + juce::String (std::lround (quad.q));
        playingLabel = "pad " + juce::String (std::lround (quad.morph)) + " " + juce::String (std::lround (quad.q));
    }
    else
    {
        sounding = false;
        status = "";
        playingLabel = "";
        return;
    }
    heard = words;
    if (withAudio) audio.publish (flat (heard));
}

Session::Snapshot Session::snapshot() const
{
    Snapshot s;
    s.quad = quad;
    s.added.assign (stars.begin() + (long) libraryCount, stars.end());
    s.frequency = frequency; s.stress = stress; s.octaves = octaves;
    return s;
}

void Session::restore (const Snapshot& s)
{
    quad = s.quad;
    stars.resize (libraryCount);
    stars.insert (stars.end(), s.added.begin(), s.added.end());
    frequency = s.frequency; stress = s.stress; octaves = s.octaves;
    for (auto& p : quad.pins) if (p >= (int) stars.size()) p = -1;
    if (pairA >= (int) stars.size()) pairA = -1;
    if (pairB >= (int) stars.size()) pairB = -1;
    if (selected >= (int) stars.size()) selected = -1;
    if (editing >= 0 && quad.pins[(size_t) kCornerPin[editing]] < 0) editing = -1;
}

void Session::apply()
{
    const Explore state = explore();
    save (quad, stars, libraryCount, file, &state);
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
    editing = -1;
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

void Session::setPair (int which, int star)
{
    if (which < 0 || which > 1) return;
    if (star == kMade || inMade() || (star < 0 && inPair())) { keep(); star = selected; }
    if (star < 0 || star >= (int) stars.size()) return;
    history.push_back (snapshot()); future.clear();
    (which == 0 ? pairA : pairB) = star;
    if (pairA >= 0 && pairB >= 0) auditioning = kPair;
    editing = -1;
    apply();
}

void Session::sweep (double t)
{
    if (pairA < 0 || pairB < 0) return;
    morphPair (pairA, pairB, t);
}

void Session::setProbe (double morph, double frequency, double stress)
{
    pairT = std::clamp (morph, 0.0, 1.0);
    this->frequency = std::clamp (frequency, 0.0, 1.0);
    this->stress = std::clamp (stress, 0.0, 1.0);
    if (pairA < 0 || pairB < 0) return;
    auditioning = kPair;
    editing = -1;
    audition();
    changed();
}

void Session::setOctaves (double value)
{
    history.push_back (snapshot()); future.clear();
    octaves = std::clamp (value, -3.0, 3.0);
    apply();
}

bool Session::bake()
{
    if (pairA < 0 || pairB < 0 || pairA >= (int) stars.size() || pairB >= (int) stars.size()) return false;
    history.push_back (snapshot()); future.clear();
    const auto body = motherBodyOf (explore(), stars);
    for (int pin = 0; pin < 4; ++pin)
    {
        const auto sample = body.interpolate_words ((float) (pin & 1), (float) ((pin >> 1) & 1), (float) stress);
        Words baked;
        std::copy_n (sample.begin(), kRows, baked.begin());
        int star = -1;
        for (int i = 0; i < (int) stars.size(); ++i) if (stars[(size_t) i].words == baked) { star = i; break; }
        if (star < 0)
        {
            Star fresh;
            fresh.kind = "capture"; fresh.words = baked;
            fresh.name = uniqueName (formantName (baked)); fresh.body = fresh.name;
            fresh.parentA = stars[(size_t) pairA].name;
            fresh.parentB = stars[(size_t) pairB].name;
            fresh.morph = (pin & 1) * 100.0;
            fresh.q = ((pin >> 1) & 1) * 100.0;
            stars.push_back (fresh);
            star = (int) stars.size() - 1;
        }
        quad.pins[(size_t) pin] = star;
    }
    quad.morph = pairT * 100.0; quad.q = frequency * 100.0;
    editing = -1; auditioning = -1;
    apply();
    return true;
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
    if (star == kMade || (star < 0 && (inPair() || inMade()))) { keep(); star = selected; }
    pin (kCornerPin[corner], star);
}

bool Session::placeable() const
{
    return inPair() || inMade() || currentStar() >= 0 || (auditioning == -1 && quad.complete());
}

void Session::toCorner (int corner)
{
    if (! placeable()) return;
    working = std::clamp (corner, 0, 3);
    const bool wasPair = inPair();
    const double t = pairT;
    int star = currentStar();
    if (auditioning == -1 || wasPair || inMade()) { keep(); star = selected; }
    pinCorner (corner, star);
    if (wasPair) setProbe (t, frequency, stress);
}

void Session::toColumn (int column)
{
    if (! placeable()) return;
    if (auditioning == -1 || inPair() || inMade()) keep();
    const int star = currentStar();
    if (star < 0 || star >= (int) stars.size()) return;
    history.push_back (snapshot()); future.clear();
    const int side = column == 0 ? 0 : 1;
    quad.pins[(size_t) side] = star;
    quad.pins[(size_t) (side + 2)] = star;
    working = side;
    auditioning = -1;
    apply();
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
    if (corner < 0 || corner > 3) { editing = -1; auditioning = -1; audition(); changed(); return; }
    const int star = quad.pins[(size_t) kCornerPin[corner]];
    if (star < 0) return;
    editing = corner;
    working = corner;
    selected = star;
    auditioning = star;
    audition();
    changed();
}

void Session::beginRowEdit() { history.push_back (snapshot()); future.clear(); }

void Session::setRow (int corner, int row, Row r)
{
    if (corner < 0 || corner >= 4 || row < 0 || row >= kRows) return;
    const int star = quad.pins[(size_t) kCornerPin[corner]];
    if (star < 0 || star >= (int) stars.size()) return;
    setSectionWords (corner, row, rowWords (r, stars[(size_t) star].words[(size_t) row][4]));
}

void Session::setSection (int corner, int row, const Section& section, bool keepFifth)
{
    if (corner < 0 || corner >= 4 || row < 0 || row >= kRows) return;
    const int star = quad.pins[(size_t) kCornerPin[corner]];
    if (star < 0 || star >= (int) stars.size()) return;
    setSectionWords (corner, row, sectionWords (section, stars[(size_t) star].words[(size_t) row][4], keepFifth), keepFifth);
}

void Session::setSectionWords (int corner, int row, const trench::core::PackedSection& words, bool balance)
{
    if (corner < 0 || corner >= 4 || row < 0 || row >= kRows) return;
    int& anchor = quad.pins[(size_t) kCornerPin[corner]];
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
    if (balance) unityDc (s.words);
    const auto name = formantName (s.words);
    if (s.name != name) { s.name = uniqueName (name); s.body = s.name; }
    editing = corner;
    selected = star;
    auditioning = star;
    apply();
}

Words Session::editWords() const
{
    const int star = quad.pins[(size_t) kCornerPin[editing < 0 ? working : editing]];
    return star >= 0 && star < (int) stars.size() ? stars[(size_t) star].words : Words {};
}

void Session::setPuck (double morph, double q)
{
    quad.morph = std::clamp (morph, 0.0, 100.0);
    quad.q = std::clamp (q, 0.0, 100.0);
    auditioning = -1;
    editing = -1;
    apply();
}

void Session::nudge (double dm, double dq) { setPuck (quad.morph + dm, quad.q + dq); }

void Session::keep()
{
    const bool pair = inPair(), fromMade = inMade();
    if (! pair && ! fromMade && ! quad.complete()) return;
    history.push_back (snapshot()); future.clear();
    Star s;
    s.kind = "capture";
    s.words = pair ? words : fromMade ? made.words : wordsAt (quad, stars);
    s.parentA = pair ? stars[(size_t) pairA].name : fromMade ? (made.parentA.isNotEmpty() ? made.parentA : juce::String ("made")) : pinName (0);
    s.parentB = pair ? stars[(size_t) pairB].name : fromMade ? juce::String() : pinName (1);
    s.morph = pair ? pairT * 100.0 : fromMade ? 0.0 : quad.morph;
    s.q = pair ? frequency * 100.0 : fromMade ? 0.0 : quad.q;
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
    editing = -1;
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
    changed();
}

void Session::noteOn (int midi)
{
    note = std::clamp (midi, 0, 127);
    if (withAudio && heldNote >= 0 && heldNote != note) audio.noteOff (heldNote);
    heldNote = note;
    if (withAudio) audio.noteOn (note);
    changed();
}

void Session::noteOff()
{
    if (withAudio && heldNote >= 0) audio.noteOff (heldNote);
    heldNote = -1;
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
    else if (m.isNoteOff() && m.getNoteNumber() == heldNote) noteOff();
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
    if (c == 'p' || c == 'P') { setSource (3); return true; }
    if ((c == 'l' || c == 'L') && loopName.isNotEmpty()) { setSource (2); return true; }
    if (c == 's' || c == 'S') { setSource (0); return true; }
    if (c == '[') { setNote (note - 1); return true; }
    if (c == ']') { setNote (note + 1); return true; }
    if (code == juce::KeyPress::pageDownKey) { setNote (note - 12); return true; }
    if (code == juce::KeyPress::pageUpKey) { setNote (note + 12); return true; }
    for (int corner = 0; corner < 4; ++corner)
    {
        if (k.getModifiers().isShiftDown() && (code == '1' + corner || code == 'A' + corner)) { toColumn (corner & 1); return true; }
        if (c == (juce::juce_wchar) ('1' + corner) || c == (juce::juce_wchar) ('a' + corner) || c == (juce::juce_wchar) ('A' + corner)) { toCorner (corner); return true; }
    }
    return false;
}
}
