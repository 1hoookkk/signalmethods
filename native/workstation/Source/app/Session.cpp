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

Session::Session (const juce::File& rootDir, const juce::File& file, bool audioOn)
    : root (rootDir), stripFile (file), withAudio (audioOn)
{
    library = loadLibrary (root.getChildFile ("plugin/presets/p2k"));
    strip = open (stripFile);
    if (strip.count() == 0) hear (0); else audition();
}

Session::~Session() { audio.stop(); }

void Session::changed() { if (onChange) onChange(); }

void Session::audition()
{
    if (strip.count() == 0) { sounding = false; status = "place an anchor"; return; }
    words = wordsAt (strip);
    sounding = true;
    if (withAudio) audio.publish (flat (words));
    const int n = strip.count(), k = std::clamp (strip.square, 1, strip.squares());
    const auto& a = strip.anchors[(size_t) k - 1].name;
    const auto& b = strip.anchors[(size_t) std::min (k + 1, n) - 1].name;
    status = a + " -> " + b + "   MORPH " + juce::String (strip.morph, 1) + "   Q " + juce::String (strip.q, 1);
}

void Session::apply (Strip s, bool edit)
{
    if (edit) { history.push_back (strip); future.clear(); }
    strip = s;
    save (strip, stripFile);
    audition();
    changed();
}

void Session::hear (int k)
{
    if (k < 0 || k >= (int) library.size()) return;
    librarySelected = k;
    words = library[(size_t) k].q0;
    sounding = true;
    if (withAudio) audio.publish (flat (words));
    status = library[(size_t) k].name + "   library";
    changed();
}

void Session::place()
{
    auto s = strip;
    const int k = s.selected > 0 ? s.selected + 1 : s.count() + 1;
    s = insert (s, k, factoryAnchor (library, librarySelected));
    if (s.count() == strip.count()) return;
    s = jumpTo (s, k);
    s.q = 0.0;
    apply (s, true);
}

void Session::keep()
{
    if (strip.count() < 2) { status = "two anchors before a capture"; changed(); return; }
    apply (hs::keep (strip), true);
    const auto& c = strip.anchors[(size_t) strip.selected - 1];
    status = c.name + " = " + c.origin.parentA + " -> " + c.origin.parentB + " at MORPH " + juce::String (c.origin.morph, 1);
    changed();
}

void Session::walk (double dm, double dq)
{
    if (strip.count() == 0) return;
    apply (step (strip, dm, dq), false);
}

void Session::jump (int k)
{
    if (k < 1 || k > strip.count()) return;
    apply (jumpTo (strip, k), false);
}

void Session::setPosition (int square, double morph, double q)
{
    if (strip.count() == 0) return;
    auto s = strip;
    s.square = std::clamp (square, 1, s.squares());
    s.morph = std::clamp (morph, 0.0, 100.0);
    s.q = std::clamp (q, 0.0, 100.0);
    apply (s, false);
}

void Session::moveAnchor (int direction)
{
    if (strip.selected < 1) return;
    auto s = move (strip, strip.selected, direction);
    if (s.selected == strip.selected) return;
    s = jumpTo (s, s.selected);
    apply (s, true);
}

void Session::removeAnchor()
{
    if (strip.selected < 1) return;
    apply (remove (strip, strip.selected), true);
}

void Session::undo()
{
    if (history.empty()) return;
    future.push_back (strip);
    strip = history.back();
    history.pop_back();
    save (strip, stripFile);
    audition();
    changed();
}

void Session::redo()
{
    if (future.empty()) return;
    history.push_back (strip);
    strip = future.back();
    future.pop_back();
    save (strip, stripFile);
    audition();
    changed();
}

juce::File Session::write (juce::File path)
{
    if (strip.count() == 0) { status = "place an anchor first"; changed(); return {}; }
    if (path == juce::File())
    {
        const auto folder = root.getChildFile ("plugin/presets/user");
        folder.createDirectory();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
        path = folder.getChildFile ("headspace_" + stamp + ".body240");
        for (int n = 1; path.existsAsFile(); ++n) path = folder.getChildFile ("headspace_" + stamp + "_" + juce::String (n).paddedLeft ('0', 2) + ".body240");
    }
    status = writeBody (strip, path) ? path.getFullPathName() : "cannot write " + path.getFullPathName();
    changed();
    return path;
}

void Session::setPlaying (bool on)
{
    if (on && withAudio && ! audio.isOpen())
    {
        if (! audio.start()) { status = "no audio device   " + audio.error; playing = false; changed(); return; }
        status = audio.deviceInfo;
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
    const double nudge = control ? 0.2 : 1.0;
    const int code = k.getKeyCode();
    const auto c = k.getTextCharacter();
    if (code == juce::KeyPress::rightKey) { walk (nudge, 0.0); return true; }
    if (code == juce::KeyPress::leftKey) { walk (-nudge, 0.0); return true; }
    if (code == juce::KeyPress::upKey) { walk (0.0, nudge); return true; }
    if (code == juce::KeyPress::downKey) { walk (0.0, -nudge); return true; }
    if (code == juce::KeyPress::spaceKey) { setPlaying (! playing); return true; }
    if (code == juce::KeyPress::returnKey) { place(); return true; }
    if (code == juce::KeyPress::deleteKey) { removeAnchor(); return true; }
    if (code == juce::KeyPress::homeKey) { jump (1); return true; }
    if (code == juce::KeyPress::endKey) { jump (strip.count()); return true; }
    if (control && (c == 's' || c == 'S' || code == 'S')) { keep(); return true; }
    if (control && (c == 'z' || c == 'Z' || code == 'Z')) { undo(); return true; }
    if (control && (c == 'y' || c == 'Y' || code == 'Y')) { redo(); return true; }
    if (c == '[') { moveAnchor (-1); return true; }
    if (c == ']') { moveAnchor (1); return true; }
    if (c == 'w' || c == 'W') { write(); return true; }
    if (c >= '1' && c <= '9') { jump ((int) (c - '0')); return true; }
    return false;
}
}
