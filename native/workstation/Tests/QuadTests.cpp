#include "Screen.h"
#include <cstdio>
#include <memory>

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

juce::File tempQuad() { return juce::File::createTempFile ("quad").getSiblingFile (juce::Uuid().toString() + "_HEADSPACE.quad.json"); }

int star (const hs::Session& s, const char* body, const char* corner)
{
    for (int i = 0; i < (int) s.stars.size(); ++i)
        if (s.stars[(size_t) i].body == body && s.stars[(size_t) i].corner == corner) return i;
    return -1;
}

void pinBody (hs::Session& s, const char* body)
{
    for (int n = 0; n < 4; ++n) s.pin (n, star (s, body, hs::kPinNames[n]));
}

juce::KeyPress key (int code, bool control = false, juce::juce_wchar text = 0)
{
    return juce::KeyPress (code, control ? juce::ModifierKeys::ctrlModifier : juce::ModifierKeys::noModifiers, text);
}

std::vector<std::uint8_t> bytesOf (const juce::File& f)
{
    juce::MemoryBlock mb;
    f.loadFileAsData (mb);
    return { (const std::uint8_t*) mb.getData(), (const std::uint8_t*) mb.getData() + mb.getSize() };
}

bool same (const hs::Words& a, const hs::Words& b) { return a == b; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File root (TRENCH_TABLE_STITCH_ROOT);
    const auto p2k = root.getChildFile ("plugin/presets/p2k");

    {
        hs::Session s (root, tempQuad(), false);
        check (s.stars.size() == 132 && s.libraryCount == 132, "the library holds 132 factory stars");
        bool admitted = true, files = true, formants = true;
        for (const auto& e : s.stars)
        {
            admitted = admitted && hs::admit (e.words);
            files = files && hs::bodyFile (p2k, e.body).existsAsFile();
            const auto f = hs::formantsOf (e.words);
            formants = formants && std::isfinite (f[0]) && std::isfinite (f[1]);
        }
        check (admitted, "every star has six active sections");
        check (files, "every body name maps to its preset file");
        check (formants, "every star has a finite place on the map");
        const auto f = hs::formantsOf (s.stars[(size_t) star (s, "Ooh To Eee", "M0 Q0")].words);
        check (f[0] > 100.0 && f[0] < f[1] && f[1] < 4000.0, "a vowel corner reads F1 below F2 inside the chart");
        check (s.quad.complete() && s.pinName (0) == "Talking Hedz M0 Q0" && s.pinName (3) == "Talking Hedz M1 Q1" && s.sounding, "a fresh session boots pinned to Talking Hedz and sounding");
        const auto boot = s.words;
        s.hover (5);
        check (same (s.words, boot) && s.hovered == 5, "hovering a star only names it, the sound stays");
        s.select (5);
        check (s.sounding && same (s.words, s.stars[5].words) && s.auditioning == 5, "clicking a star plays it exactly");
        s.setPuck (s.quad.morph, s.quad.q);
        check (same (s.words, boot) && s.auditioning == -1, "touching the pad returns to the quad");
    }

    {
        hs::Session s (root, tempQuad(), false);
        pinBody (s, "Talking Hedz");
        check (s.quad.complete() && s.pinName (0) == "Talking Hedz M0 Q0" && s.pinName (3) == "Talking Hedz M1 Q1", "four pins make a quad");
        check (same (s.words, s.stars[(size_t) s.quad.pins[0]].words), "at MORPH 0 Q 0 the words are the first pin exactly");
        s.setPuck (100.0, 0.0);
        check (same (s.words, s.stars[(size_t) s.quad.pins[1]].words), "at MORPH 100 Q 0 the words are the second pin exactly");
        s.setPuck (0.0, 100.0);
        check (same (s.words, s.stars[(size_t) s.quad.pins[2]].words), "at MORPH 0 Q 100 the words are the third pin exactly");
        s.setPuck (0.0, 0.0);
        s.key (key (juce::KeyPress::rightKey)); s.key (key (juce::KeyPress::rightKey, true)); s.key (key (juce::KeyPress::upKey));
        check (std::abs (s.quad.morph - 1.2) < 1e-9 && s.quad.q == 1.0, "arrows move the puck by 1 and 0.2");
        check (same (s.words, hs::lerp (hs::cornersOf (s.quad, s.stars), 0.012, 0.01)), "the puck plays the chip's lerp of the four pins");
        s.hover (star (s, "Deep Bouche", "M1 Q0"));
        s.key (key ('2', false, '2'));
        check (s.pinName (1) == "Deep Bouche M1 Q0", "a number key pins the hovered star");
        s.unhover();
        check (same (s.words, hs::lerp (hs::cornersOf (s.quad, s.stars), 0.012, 0.01)), "leaving the star returns to the puck");
    }

    {
        hs::Session s (root, tempQuad(), false);
        std::vector<juce::String> bodies;
        for (const auto& e : s.stars) if (std::find (bodies.begin(), bodies.end(), e.body) == bodies.end()) bodies.push_back (e.body);
        bool ok = bodies.size() == 33;
        for (const auto& body : bodies)
        {
            pinBody (s, body.toRawUTF8());
            const auto path = juce::File::createTempFile ("factory.body240");
            s.write (path);
            if (bytesOf (path) != bytesOf (hs::bodyFile (p2k, body))) { ok = false; std::printf ("      %s differs\n", body.toRawUTF8()); }
        }
        check (ok, "every factory body pinned at its four corners writes its original bytes");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.pin (0, star (s, "Talking Hedz", "M0 Q0")); s.pin (1, star (s, "Deep Bouche", "M1 Q0"));
        s.pin (2, star (s, "Ooh To Eee", "M0 Q1")); s.pin (3, star (s, "Zoom Peaks", "M1 Q1"));
        const auto path = juce::File::createTempFile ("quad.body240");
        s.write (path);
        const auto bytes = bytesOf (path);
        bool ok = bytes.size() == 240;
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
        for (int m = 0; m <= 100 && ok; m += 25)
            for (int q = 0; q <= 100 && ok; q += 25)
            {
                s.setPuck ((double) m, (double) q);
                const auto cw = body.interpolate_words (m / 100.0f, q / 100.0f, 0.0f);
                for (size_t row = 0; row < hs::kRows; ++row) ok = ok && cw[row] == s.words[row];
            }
        check (ok, "the written file reloaded through the plugin's lerp equals the puck's words on a 5 x 5 grid");
    }

    {
        hs::Session s (root, tempQuad(), false);
        pinBody (s, "Talking Hedz");
        s.setPuck (23.0, 40.0);
        const auto heard = s.words;
        s.key (key ('S', true, 's'));
        check (s.stars.size() == 133 && s.stars.back().name == "C1" && s.stars.back().kind == "capture", "keep adds the puck's sound as a new star");
        check (same (s.stars.back().words, heard) && s.selected == 132, "the capture holds the exact words and is selected");
        check (s.stars.back().parentA == "Talking Hedz M0 Q0" && s.stars.back().morph == 23.0 && s.stars.back().q == 40.0, "the capture records its pins and position");
        s.key (key ('1', false, '1'));
        check (s.pinName (0) == "C1" && same (hs::cornersOf (s.quad, s.stars)[0], heard), "a capture can be pinned like any star");
        s.setPuck (0.0, 0.0);
        check (same (s.words, heard), "the pinned capture plays back its words exactly");

        const auto saved = s.quad;
        hs::Session again (root, s.file, false);
        bool equal = again.stars.size() == 133 && again.quad.pins == saved.pins && again.quad.morph == saved.morph && again.quad.q == saved.q && again.quad.captures == saved.captures;
        equal = equal && same (again.stars.back().words, heard) && again.stars.back().name == "C1" && again.stars.back().parentA == "Talking Hedz M0 Q0";
        check (equal, "save then reopen restores the pins by name, the puck and the captures");
        check (same (again.words, s.words), "a new session on the same file plays the same words");
    }

    {
        hs::Session s (root, tempQuad(), false);
        pinBody (s, "Talking Hedz");
        s.setPuck (50.0, 50.0); s.keep();
        s.pin (2, star (s, "Zoom Peaks", "M0 Q1"));
        s.select (132); s.key (key (juce::KeyPress::deleteKey));
        check (s.stars.size() == 132 && s.history.size() == 7, "pins, a capture and a delete record seven edits");
        const auto pinsNow = s.quad.pins;
        for (int i = 0; i < 7; ++i) s.key (key ('Z', true, 'z'));
        check (s.quad.pins != pinsNow && s.pinName (0) == "Talking Hedz M0 Q0" && s.stars.size() == 132 && s.history.empty() && s.future.size() == 7, "seven undos return to the boot pins");
        for (int i = 0; i < 7; ++i) s.key (key ('Y', true, 'y'));
        check (s.quad.complete() && s.pinName (2) == "Zoom Peaks M0 Q1" && s.stars.size() == 132, "seven redos restore the quad");
        s.key (key ('Z', true, 'z'));
        check (s.stars.size() == 133 && s.stars.back().name == "C1", "one undo brings the deleted capture back");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int a = star (s, "Talking Hedz", "M0 Q0"), b = star (s, "Deep Bouche", "M1 Q0");
        s.select (a);
        s.morphPair (a, b, 0.23);
        hs::Corners c { s.stars[(size_t) a].words, s.stars[(size_t) b].words, s.stars[(size_t) a].words, s.stars[(size_t) b].words };
        check (s.inPair() && same (s.words, hs::lerp (c, 0.23, 0.0)), "dragging from one star toward another plays the chip's lerp between exactly those two");
        const auto heard = s.words;
        s.key (key ('2', false, '2'));
        check (s.stars.size() == 133 && s.stars.back().kind == "capture" && same (s.stars.back().words, heard) && s.pinName (1) == "C1", "a number key during the drag keeps the sound and pins it in one stroke");
        check (s.stars.back().parentA == "Talking Hedz M0 Q0" && s.stars.back().parentB == "Deep Bouche M1 Q0" && s.stars.back().morph == 23.0, "the kept star records both ends and the MORPH");
        s.morphPair (a, b, 1.0);
        check (same (s.words, s.stars[(size_t) b].words), "at the far star the drag is that star exactly");
    }

    {
        hs::Session s (root, tempQuad(), false);
        pinBody (s, "Zoom Peaks");
        const auto heat = hs::hotCells (hs::cornersOf (s.quad, s.stars), 9, hs::curveHz());
        double worst = -1e9;
        for (double h : heat) worst = std::max (worst, h);
        check (heat.size() == 81 && worst > 3.0, "Zoom Peaks lights up inside its own quad, as measured");
        pinBody (s, "Talking Hedz");
        const auto calm = hs::hotCells (hs::cornersOf (s.quad, s.stars), 9, hs::curveHz());
        bool finite = true;
        for (double h : calm) finite = finite && std::isfinite (h);
        check (finite, "the heat map is finite everywhere");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.pin (0, star (s, "Talking Hedz", "M0 Q0")); s.pin (1, star (s, "Deep Bouche", "M1 Q0"));
        s.pin (2, star (s, "Ooh To Eee", "M0 Q1")); s.pin (3, star (s, "Zoom Peaks", "M1 Q1"));
        s.setPuck (40.0, 30.0);
        hs::Screen screen (s);
        const auto image = screen.shot();
        const auto folder = root.getChildFile ("native/workstation/artifacts/shots");
        folder.createDirectory();
        const auto file = folder.getChildFile ("headspace.png");
        file.deleteFile();
        juce::PNGImageFormat png;
        juce::FileOutputStream out (file);
        const bool written = out.openedOk() && png.writeImageToStream (image, out);
        out.flush();
        check (written && file.getSize() > 20000 && image.getWidth() == 1480, "the screen renders to artifacts/shots/headspace.png without a window");
        check (juce::Desktop::getInstance().getNumComponents() == 0, "no window was opened");
        const auto p = screen.puckPoint (40.0, 30.0);
        check (screen.pad.toFloat().contains (p), "the puck sits inside the pad");
    }

    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
