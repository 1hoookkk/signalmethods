#include "Session.h"
#include <algorithm>

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
    stars = loadLibrary (root.getChildFile ("plugin/presets/p2k"));
    factoryCount = stars.size();
    for (const auto& s : stars) if (bodies.empty() || bodies.back() != s.body) bodies.push_back (s.body);
    const auto vowels = loadVowels (root.getChildFile ("native/workstation/banks/Klatt 1980.bank.json"));
    stars.insert (stars.end(), vowels.begin(), vowels.end());
    libraryCount = stars.size();
    const bool had = open (quad, stars, libraryCount, file);
    if (! had && ! quad.complete()) loadPreset ("Talking Hedz");
    history.clear(); future.clear();
    audition();
}

Session::~Session() { audio.stop(); }

void Session::changed() { if (onChange) onChange(); }

juce::String Session::pinName (int n) const
{
    const int p = quad.pins[(size_t) n];
    return p >= 0 && p < (int) stars.size() ? stars[(size_t) p].name : juce::String();
}

int Session::currentStar() const
{
    if (auditioning >= 0) return auditioning;
    return selected;
}

void Session::audition()
{
    if (auditioning == -2 && pairA >= 0 && pairB >= 0 && pairA < (int) stars.size() && pairB < (int) stars.size())
    {
        Corners c { stars[(size_t) pairA].words, stars[(size_t) pairB].words, stars[(size_t) pairA].words, stars[(size_t) pairB].words };
        words = lerp (c, pairT, 0.0);
        sounding = true;
        status = stars[(size_t) pairA].name + " > " + stars[(size_t) pairB].name + "  " + juce::String (pairT * 100.0, 0);
    }
    else if (auditioning >= 0 && auditioning < (int) stars.size())
    {
        words = stars[(size_t) auditioning].words;
        sounding = true;
        status = stars[(size_t) auditioning].name;
    }
    else if (quad.complete())
    {
        words = wordsAt (quad, stars);
        sounding = true;
        status = juce::String (quad.morph, 0) + " " + juce::String (quad.q, 0);
    }
    else
    {
        sounding = false;
        status = "";
        return;
    }
    if (withAudio) audio.publish (flat (words));
}

Session::Snapshot Session::snapshot() const
{
    Snapshot s;
    s.quad = quad;
    s.added.assign (stars.begin() + (long) libraryCount, stars.end());
    return s;
}

void Session::restore (const Snapshot& s)
{
    quad = s.quad;
    stars.resize (libraryCount);
    stars.insert (stars.end(), s.added.begin(), s.added.end());
    for (auto& p : quad.pins) if (p >= (int) stars.size()) p = -1;
    if (selected >= (int) stars.size()) selected = -1;
}

void Session::apply()
{
    save (quad, stars, libraryCount, file);
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
    auditioning = k;
    audition();
    changed();
}

void Session::morphPair (int a, int b, double t)
{
    if (a < 0 || b < 0 || a >= (int) stars.size() || b >= (int) stars.size()) return;
    pairA = a; pairB = b; pairT = std::clamp (t, 0.0, 1.0);
    auditioning = -2;
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
    if (inPair()) { keep(); star = selected; }
    pin (kCornerPin[corner], star);
}

void Session::pinAll (const std::array<int, 4>& pins)
{
    for (int p : pins) if (p < 0 || p >= (int) stars.size()) return;
    history.push_back (snapshot()); future.clear();
    quad.pins = pins;
    auditioning = -1;
    apply();
}

void Session::loadPreset (const juce::String& body)
{
    std::array<int, 4> pins { -1, -1, -1, -1 };
    for (int n = 0; n < 4; ++n)
        for (int k = 0; k < (int) factoryCount; ++k)
            if (stars[(size_t) k].body == body && stars[(size_t) k].corner == kPinNames[n]) pins[(size_t) n] = k;
    pinAll (pins);
}

int Session::addRead (const juce::File& wav)
{
    auto star = readWav (wav);
    if (! star) { status = "cannot read " + wav.getFileName(); changed(); return -1; }
    history.push_back (snapshot()); future.clear();
    stars.push_back (*star);
    selected = (int) stars.size() - 1;
    auditioning = selected;
    apply();
    return selected;
}

void Session::setPuck (double morph, double q)
{
    quad.morph = std::clamp (morph, 0.0, 100.0);
    quad.q = std::clamp (q, 0.0, 100.0);
    auditioning = -1;
    apply();
}

void Session::nudge (double dm, double dq) { setPuck (quad.morph + dm, quad.q + dq); }

void Session::keep()
{
    const bool pair = inPair();
    if (! pair && ! quad.complete()) return;
    history.push_back (snapshot()); future.clear();
    Star s;
    s.kind = "capture";
    s.words = pair ? words : wordsAt (quad, stars);
    s.parentA = pair ? stars[(size_t) pairA].name : pinName (0);
    s.parentB = pair ? stars[(size_t) pairB].name : pinName (1);
    s.morph = pair ? pairT * 100.0 : quad.morph; s.q = pair ? 0.0 : quad.q;
    quad.captures += 1;
    s.name = "C" + juce::String (quad.captures);
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
    hovered = -1; auditioning = -1;
    apply();
}

void Session::redo()
{
    if (future.empty()) return;
    history.push_back (snapshot());
    restore (future.back());
    future.pop_back();
    hovered = -1; auditioning = -1;
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
    const int target = currentStar();
    if (code == juce::KeyPress::rightKey) { nudge (step, 0.0); return true; }
    if (code == juce::KeyPress::leftKey) { nudge (-step, 0.0); return true; }
    if (code == juce::KeyPress::upKey) { nudge (0.0, step); return true; }
    if (code == juce::KeyPress::downKey) { nudge (0.0, -step); return true; }
    if (code == juce::KeyPress::spaceKey) { setPlaying (! playing); return true; }
    if (code == juce::KeyPress::deleteKey) { removeAdded(); return true; }
    if (control && (c == 's' || c == 'S' || code == 'S')) { keep(); return true; }
    if (control && (c == 'z' || c == 'Z' || code == 'Z')) { undo(); return true; }
    if (control && (c == 'y' || c == 'Y' || code == 'Y')) { redo(); return true; }
    if (c == 'w' || c == 'W') { write(); return true; }
    if (c == 'n' || c == 'N') { setSource (1); return true; }
    if (c == 's' || c == 'S') { setSource (0); return true; }
    for (int corner = 0; corner < 4; ++corner)
        if (c == kCornerLetters[corner] || c == (juce::juce_wchar) (kCornerLetters[corner] + 32) || c == (juce::juce_wchar) ('1' + corner))
        {
            if (inPair() || target >= 0) pinCorner (corner, target);
            return true;
        }
    return false;
}
}
