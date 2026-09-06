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

juce::File tempStrip() { return juce::File::createTempFile ("strip").getSiblingFile (juce::Uuid().toString() + "_HEADSPACE.strip.json"); }

int entry (const hs::Session& s, const char* body, const char* side)
{
    for (int i = 0; i < (int) s.library.size(); ++i)
        if (s.library[(size_t) i].body == body && s.library[(size_t) i].side == side) return i;
    return -1;
}

void placeCorner (hs::Session& s, const char* body, const char* side)
{
    s.hear (entry (s, body, side));
    s.place();
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
        hs::Session s (root, tempStrip(), false);
        check (s.library.size() == 66, "library holds 66 factory anchors, two per body");
        bool admitted = true, files = true;
        for (const auto& e : s.library) { admitted = admitted && hs::admit (e.q0) && hs::admit (e.q1); files = files && hs::bodyFile (p2k, e.body).existsAsFile(); }
        check (admitted, "every factory corner has six active sections");
        check (files, "every body name maps to its preset file");
        check (s.library[0].name.endsWith ("M0") && s.library[1].name.endsWith ("M1"), "anchors are named by body and side");
        s.hear (5);
        check (same (s.words, s.library[5].q0) && s.librarySelected == 5, "hear plays the anchor exactly");
    }

    {
        hs::Session s (root, tempStrip(), false);
        placeCorner (s, "Talking Hedz", "M0"); placeCorner (s, "Deep Bouche", "M1");
        const auto& c = s.strip.anchors;
        check (s.strip.count() == 2 && c[0].name == "Talking Hedz M0" && c[1].name == "Deep Bouche M1", "place puts two factory anchors in the row");
        check (same (c[0].q0, s.library[(size_t) entry (s, "Talking Hedz", "M0")].q0) && same (c[0].q1, s.library[(size_t) entry (s, "Talking Hedz", "M0")].q1), "an anchor holds its own Q0 and Q1 states");
        check (s.strip.square == 1 && s.strip.morph == 100.0 && s.strip.selected == 2, "the mark stands on the placed anchor");
        check (same (s.words, c[1].q0), "at MORPH 100 the words are the right anchor exactly");
        s.key (key (juce::KeyPress::rightKey));
        s.key (key (juce::KeyPress::rightKey, true));
        check (std::abs (s.strip.morph - 100.0) < 1e-9, "walking past the last anchor clamps at 100");
        s.jump (1);
        s.key (key (juce::KeyPress::rightKey)); s.key (key (juce::KeyPress::rightKey, true));
        check (std::abs (s.strip.morph - 1.2) < 1e-9 && same (s.words, hs::lerp (hs::cornersOf (s.strip, 1), 0.012, 0.0)), "arrow and ctrl arrow move MORPH by 1 and 0.2 through the chip's lerp");
        s.key (key (juce::KeyPress::upKey));
        check (s.strip.q == 1.0, "up moves Q by 1");
        placeCorner (s, "Zoom Peaks", "M0");
        s.jump (1); s.walk (150.0, 0.0);
        check (s.strip.square == 2 && std::abs (s.strip.morph - 50.0) < 1e-9 && same (s.words, hs::lerp (hs::cornersOf (s.strip, 2), 0.5, 0.0)), "walking past 100 enters the next square");
        s.walk (500.0, 0.0);
        check (s.strip.square == 2 && s.strip.morph == 100.0 && same (s.words, s.strip.anchors[2].q0), "the last square clamps at its right anchor");
        s.walk (-500.0, -500.0);
        check (s.strip.square == 1 && s.strip.morph == 0.0 && s.strip.q == 0.0 && same (s.words, s.strip.anchors[0].q0), "walking back lands on the first anchor");
    }

    {
        hs::Session s (root, tempStrip(), false);
        placeCorner (s, "Talking Hedz", "M0"); placeCorner (s, "Deep Bouche", "M1");
        s.jump (1); s.walk (23.0, 0.0);
        const auto heard = s.words;
        const auto corners = hs::cornersOf (s.strip, 1);
        s.key (key ('S', true, 's'));
        const auto& c = s.strip.anchors;
        check (s.strip.count() == 3 && c[1].name == "C1" && c[0].name == "Talking Hedz M0" && c[2].name == "Deep Bouche M1", "keep inserts the capture between its parents");
        check (same (c[1].q0, heard) && same (c[1].q0, hs::lerp (corners, 0.23, 0.0)) && same (c[1].q1, hs::lerp (corners, 0.23, 1.0)), "the capture holds both rows at that MORPH");
        check (s.strip.square == 2 && s.strip.morph == 0.0 && same (s.words, heard), "after keeping, the mark stands on the capture and the sound is unchanged");
        check (c[1].origin.kind == "capture" && c[1].origin.parentA == "Talking Hedz M0" && c[1].origin.parentB == "Deep Bouche M1" && c[1].origin.morph == 23.0, "the capture records its parents and MORPH");
        check (s.status.startsWith ("C1 = Talking Hedz M0 -> Deep Bouche M1 at MORPH 23"), "status names the capture");
        s.jump (3); s.jump (2);
        check (same (s.words, heard), "recalling the capture plays identical words");

        const auto saved = s.strip;
        const auto reopened = hs::open (s.stripFile);
        bool equal = reopened.count() == 3 && reopened.square == saved.square && reopened.morph == saved.morph && reopened.q == saved.q && reopened.selected == saved.selected && reopened.captures == saved.captures;
        for (int k = 0; k < 3 && equal; ++k)
            equal = reopened.anchors[(size_t) k].name == saved.anchors[(size_t) k].name && reopened.anchors[(size_t) k].origin == saved.anchors[(size_t) k].origin
                 && same (reopened.anchors[(size_t) k].q0, saved.anchors[(size_t) k].q0) && same (reopened.anchors[(size_t) k].q1, saved.anchors[(size_t) k].q1);
        check (equal, "save then reopen restores order, names, origins and words");
        hs::Session again (root, s.stripFile, false);
        check (same (again.words, s.words) && again.strip.count() == 3, "a new session on the same file plays the same words");
    }

    {
        hs::Session s (root, tempStrip(), false);
        placeCorner (s, "Talking Hedz", "M0"); placeCorner (s, "Deep Bouche", "M1"); placeCorner (s, "Zoom Peaks", "M1");
        bool ok = true;
        for (int k = 1; k <= 2 && ok; ++k)
        {
            s.jump (k); s.walk (1.0, 0.0);
            const auto path = juce::File::createTempFile ("square.body240");
            s.write (path);
            const auto bytes = bytesOf (path);
            ok = bytes.size() == 240;
            const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
            for (int m = 0; m <= 100 && ok; m += 25)
                for (int q = 0; q <= 100 && ok; q += 25)
                {
                    s.setPosition (k, (double) m, (double) q);
                    const auto cw = body.interpolate_words (m / 100.0f, q / 100.0f, 0.0f);
                    for (size_t row = 0; row < hs::kRows; ++row) ok = ok && cw[row] == s.words[row];
                }
        }
        check (ok, "the written file reloaded through the plugin's lerp equals the live words on a 5 x 5 grid per square");
    }

    {
        hs::Session s (root, tempStrip(), false);
        std::vector<juce::String> bodies;
        for (const auto& e : s.library) if (std::find (bodies.begin(), bodies.end(), e.body) == bodies.end()) bodies.push_back (e.body);
        bool ok = bodies.size() == 33;
        for (const auto& body : bodies)
        {
            s.strip = hs::Strip();
            placeCorner (s, body.toRawUTF8(), "M0"); placeCorner (s, body.toRawUTF8(), "M1");
            const auto path = juce::File::createTempFile ("factory.body240");
            s.write (path);
            if (bytesOf (path) != bytesOf (hs::bodyFile (p2k, body))) { ok = false; std::printf ("      %s differs\n", body.toRawUTF8()); }
        }
        check (ok, "every factory body placed as two anchors writes its original bytes");
    }

    {
        hs::Session s (root, tempStrip(), false);
        auto w = s.library[0].q0;
        check (hs::admit (w), "a factory state is admitted");
        w[3] = trench::core::kIdentitySection;
        check (! hs::admit (w), "an identity section is refused");
        hs::Anchor c; c.name = "x"; c.q0 = w; c.q1 = s.library[0].q0;
        check (hs::insert (hs::Strip(), 1, c).count() == 0, "insert refuses a state with an identity section");
        bool real = false;
        for (const auto& e : s.library)
            for (const auto& row : e.q0)
            {
                const auto g = trench::core::geometry_from_words (row, trench::core::kP2kDatumHz);
                real = real || std::holds_alternative<trench::core::RealPair> (g.pole);
            }
        check (real, "real pole pairs are in the library and pass admission");
    }

    {
        hs::Session s (root, tempStrip(), false);
        const char* picks[] = { "Talking Hedz", "Deep Bouche", "Zoom Peaks", "Ooh To Eee", "Boland Bass" };
        for (auto* p : picks) placeCorner (s, p, "M0");
        check (s.strip.count() == 5 && s.strip.squares() == 4, "five anchors make four squares");
        s.jump (1);
        bool ok = s.strip.square == 1 && s.strip.morph == 0.0;
        for (int k = 1; k <= 4 && ok; ++k) { s.walk (100.0, 0.0); ok = s.strip.square == k && s.strip.morph == 100.0 && same (s.words, s.strip.anchors[(size_t) k].q0); }
        check (ok, "each morph of 100 lands on the next anchor exactly");
        s.walk (1.0, 0.0);
        check (s.strip.square == 4 && s.strip.morph == 100.0, "MORPH 100 of the last square is the last anchor");
        s.key (key (juce::KeyPress::homeKey)); const bool home = s.strip.square == 1 && s.strip.morph == 0.0 && s.strip.selected == 1;
        s.key (key (juce::KeyPress::endKey)); const bool end = s.strip.square == 4 && s.strip.morph == 100.0 && s.strip.selected == 5;
        s.key (key ('3', false, '3')); const bool three = s.strip.square == 2 && s.strip.morph == 100.0 && s.strip.selected == 3;
        check (home && end && three, "Home, End and the number keys jump to anchors");
    }

    {
        hs::Session s (root, tempStrip(), false);
        placeCorner (s, "Talking Hedz", "M0"); placeCorner (s, "Deep Bouche", "M1"); s.jump (1); s.walk (23.0, 0.0); s.keep();
        s.jump (3); s.key (key ('[', false, '[')); s.jump (1); s.key (key (juce::KeyPress::deleteKey));
        std::vector<juce::String> final;
        for (const auto& c : s.strip.anchors) final.push_back (c.name);
        check (final.size() == 2 && final[0] == "Deep Bouche M1" && final[1] == "C1" && s.history.size() == 5, "place, keep, move and delete record five edits");
        for (int i = 0; i < 5; ++i) s.key (key ('Z', true, 'z'));
        check (s.strip.count() == 0 && s.history.empty() && s.future.size() == 5 && hs::open (s.stripFile).count() == 0, "five undos return to an empty strip and the file follows");
        for (int i = 0; i < 5; ++i) s.key (key ('Y', true, 'y'));
        std::vector<juce::String> again;
        for (const auto& c : s.strip.anchors) again.push_back (c.name);
        check (again == final && same (s.words, s.strip.anchors[0].q0), "five redos restore the sequence");
        s.key (key ('Z', true, 'z'));
        check (s.strip.count() == 3 && s.strip.anchors[0].name == "Talking Hedz M0" && s.strip.anchors[2].name == "C1", "one undo restores the deleted anchor");
    }

    {
        hs::Session s (root, tempStrip(), false);
        const char* picks[] = { "Talking Hedz", "Deep Bouche", "Ooh To Eee", "Zoom Peaks" };
        for (auto* p : picks) placeCorner (s, p, "M0");
        s.jump (1); s.walk (23.0, 0.0); s.keep(); s.jump (3); s.walk (40.0, 30.0);
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
    }

    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
