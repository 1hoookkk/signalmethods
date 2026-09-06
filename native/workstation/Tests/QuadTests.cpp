#include "Screen.h"
#include <juce_audio_formats/juce_audio_formats.h>
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

juce::File voiceWav()
{
    const double rate = 44100.0;
    const int n = (int) rate;
    juce::AudioBuffer<float> buffer (1, n);
    auto* out = buffer.getWritePointer (0);
    const double formants[] = { 500.0, 1500.0, 2500.0 }, widths[] = { 60.0, 90.0, 120.0 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / rate, phase = std::fmod (t, 1.0 / 110.0);
        double v = 0.0;
        for (int f = 0; f < 3; ++f) v += std::exp (-3.141592653589793 * widths[f] * phase) * std::cos (2.0 * 3.141592653589793 * formants[f] * phase);
        out[i] = (float) (0.3 * v);
    }
    const auto file = juce::File::createTempFile ("voice.wav");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (new juce::FileOutputStream (file), rate, 1, 16, {}, 0));
    writer->writeFromAudioSampleBuffer (buffer, 0, n);
    writer.reset();
    return file;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File root (TRENCH_TABLE_STITCH_ROOT);
    const auto p2k = root.getChildFile ("plugin/presets/p2k");

    {
        hs::Session s (root, tempQuad(), false);
        check (s.factoryCount == 132 && s.libraryCount == 144 && s.stars.size() == 144 && s.bodies.size() == 33, "the palette holds 132 factory corners and 12 Klatt vowels");
        bool admitted = true, files = true;
        for (const auto& e : s.stars) { admitted = admitted && hs::admit (e.words); if (e.kind == "factory") files = files && hs::bodyFile (p2k, e.body).existsAsFile(); }
        check (admitted && files, "every palette entry is admitted and every body maps to its file");
        check (s.quad.complete() && s.cornerName (0) == "Talking Hedz M0 Q1" && s.cornerName (2) == "Talking Hedz M0 Q0" && s.sounding, "a fresh session boots on Talking Hedz, A top-left is M0 Q1, C bottom-left is M0 Q0");
        const auto boot = s.words;
        s.hover (5);
        check (same (s.words, boot) && s.hovered == 5, "hover only names a card, the sound stays");
        s.select (5);
        check (s.sounding && same (s.words, s.stars[5].words) && s.auditioning == 5, "clicking a card plays it exactly");
        s.setPuck (s.quad.morph, s.quad.q);
        check (same (s.words, boot) && s.auditioning == -1, "touching the stage returns to the four corners");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.loadPreset ("Zoom Peaks");
        check (s.pinName (0) == "Zoom Peaks M0 Q0" && s.pinName (3) == "Zoom Peaks M1 Q1", "PRESET loads a body into the four corners");
        check (same (s.words, s.stars[(size_t) s.quad.pins[0]].words), "at MORPH 0 Q 0 the words are M0 Q0 exactly");
        s.setPuck (100.0, 100.0);
        check (same (s.words, s.stars[(size_t) s.quad.pins[3]].words), "at MORPH 100 Q 100 the words are M1 Q1 exactly");
        s.setPuck (0.0, 0.0);
        s.key (key (juce::KeyPress::rightKey)); s.key (key (juce::KeyPress::rightKey, true)); s.key (key (juce::KeyPress::upKey));
        check (std::abs (s.quad.morph - 1.2) < 1e-9 && s.quad.q == 1.0 && same (s.words, hs::lerp (hs::cornersOf (s.quad, s.stars), 0.012, 0.01)), "arrows move the puck and the words are the chip's lerp");
        const int deep = star (s, "Deep Bouche", "M1 Q0");
        s.select (deep);
        s.key (key ('B', false, 'b'));
        check (s.cornerName (1) == "Deep Bouche M1 Q0" && s.pinName (3) == "Deep Bouche M1 Q0", "the B key puts the playing card in corner B, which is M1 Q1");
        s.select (deep);
        s.pinCorner (2, deep);
        check (s.cornerName (2) == "Deep Bouche M1 Q0" && s.pinName (0) == "Deep Bouche M1 Q0", "corner C is M0 Q0");
    }

    {
        hs::Session s (root, tempQuad(), false);
        bool ok = true;
        for (const auto& body : s.bodies)
        {
            s.loadPreset (body);
            const auto path = juce::File::createTempFile ("factory.body240");
            s.write (path);
            if (bytesOf (path) != bytesOf (hs::bodyFile (p2k, body))) { ok = false; std::printf ("      %s differs\n", body.toRawUTF8()); }
        }
        check (ok, "every factory body loaded as a preset writes its original bytes");
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
        s.setPuck (23.0, 40.0);
        const auto heard = s.words;
        s.key (key ('S', true, 's'));
        check (s.stars.size() == 145 && s.stars.back().name == "C1" && s.stars.back().kind == "capture" && same (s.stars.back().words, heard), "Ctrl+S keeps the puck's sound as a card");
        check (s.stars.back().parentA == "Talking Hedz M0 Q0" && s.stars.back().morph == 23.0 && s.stars.back().q == 40.0 && s.auditioning == 144, "the capture records its corners and position and is playing");
        s.key (key ('A', false, 'a'));
        check (s.cornerName (0) == "C1" && same (hs::cornersOf (s.quad, s.stars)[2], heard), "the A key puts the capture in corner A");
        const auto voice = voiceWav();
        const int k = s.addRead (voice);
        check (k == 145 && s.stars.back().kind == "read" && hs::admit (s.stars.back().words), "a dropped wav becomes a card with six active sections");
        const auto f = hs::formantsOf (s.stars.back().words);
        std::printf ("      read formants %.0f %.0f %.0f %.0f\n", f[0], f[1], f[2], f[3]);
        for (const auto& row : s.stars.back().words)
        {
            const auto g = trench::core::geometry_from_words (row, trench::core::kP2kDatumHz);
            if (const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole)) std::printf ("      pole %.0f Hz r %.4f\n", pole->hz, pole->radius);
        }
        check (f[0] > 300.0 && f[0] < 900.0, "the read finds the first formant of a synthetic voice near 500 Hz");
        s.setPuck (s.quad.morph, s.quad.q);
        hs::Session again (root, s.file, false);
        check (again.stars.size() == 146 && again.stars[144].kind == "capture" && again.stars[145].kind == "read" && again.cornerName (0) == "C1" && same (again.words, s.words) && same (again.stars[145].words, s.stars[145].words), "save then reopen restores captures, reads and corners by name");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int a = star (s, "Talking Hedz", "M0 Q0"), b = star (s, "Deep Bouche", "M1 Q0");
        s.select (a);
        s.morphPair (a, b, 0.23);
        hs::Corners c { s.stars[(size_t) a].words, s.stars[(size_t) b].words, s.stars[(size_t) a].words, s.stars[(size_t) b].words };
        check (s.inPair() && same (s.words, hs::lerp (c, 0.23, 0.0)), "sliding along a rail plays the chip's lerp between two neighbouring cards");
        const auto heard = s.words;
        s.key (key ('D', false, 'd'));
        check (s.stars.size() == 145 && same (s.stars.back().words, heard) && s.cornerName (3) == "C1", "a corner key during the slide keeps the sound and puts it in that corner");
        s.morphPair (a, b, 1.0);
        check (same (s.words, s.stars[(size_t) b].words), "at the far card the slide is that card exactly");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPuck (50.0, 50.0); s.keep();
        s.pin (2, star (s, "Zoom Peaks", "M0 Q1"));
        s.select (144); s.key (key (juce::KeyPress::deleteKey));
        const size_t edits = s.history.size();
        check (s.stars.size() == 144 && edits == 3, "a capture, a pin and a delete record three edits");
        for (size_t i = 0; i < edits; ++i) s.key (key ('Z', true, 'z'));
        check (s.pinName (2) == "Talking Hedz M0 Q1" && s.stars.size() == 144 && s.history.empty() && s.future.size() == edits, "undo returns to the boot corners");
        for (size_t i = 0; i < edits; ++i) s.key (key ('Y', true, 'y'));
        check (s.pinName (2) == "Zoom Peaks M0 Q1" && s.stars.size() == 144, "redo restores the edits");
        s.key (key ('Z', true, 'z'));
        check (s.stars.size() == 145 && s.stars.back().name == "C1", "one undo brings the deleted capture back");
    }

    {
        auto at = [] (const trench::core::PackedSection& w, double hz) { const std::array<trench::core::Biquad, 1> one { trench::core::section_words_to_biquad (w) }; return trench::core::cascade_response_db (one, hz, trench::core::kP2kDatumHz); };
        bool rising = true;
        double last = 0.0;
        for (int f = 0; f < hs::kFreqCodes; ++f) { const double hz = hs::rowHz (hs::rowWords ({ hs::RowType::peak, f, 0 }, 0xE000)); rising = rising && hz > last; last = hz; }
        const double low = hs::rowHz (hs::rowWords ({ hs::RowType::peak, 0, 0 }, 0xE000)), high = last;
        std::printf ("      F 0 = %.1f Hz, F 127 = %.0f Hz\n", low, high);
        check (rising && low > 70.0 && low < 95.0 && high > 11000.0 && high < 12500.0, "the row grammar's frequency code climbs from about 81 Hz to about 11.6 kHz at the P2K datum");
        const auto flat = hs::rowWords ({ hs::RowType::peak, 64, 0 }, 0xE000);
        const auto up = hs::rowWords ({ hs::RowType::peak, 64, 8 }, 0xE000), down = hs::rowWords ({ hs::RowType::peak, 64, -8 }, 0xE000);
        const double note = hs::rowHz (flat);
        std::printf ("      F 64 = %.1f Hz, gain +8 = %.2f dB, -8 = %.2f dB, at 100 Hz %.2f dB\n", note, at (up, note), at (down, note), at (up, 100.0));
        check (std::abs (at (flat, note)) < 0.05 && std::abs (1200.0 * std::log2 (hs::rowHz (up) / note)) < 10.0, "gain zero is flat and gain moves the note by less than 10 cents, as the hardware words do");
        check (at (up, note) > 5.5 && at (up, note) < 6.5 && at (down, note) < -5.5 && at (down, note) > -6.5 && std::abs (at (up, 100.0)) < 0.2, "gain +8 is a 6 dB peak at the note, -8 a 6 dB dip, and the skirt stays flat");
        const auto notch = hs::rowWords ({ hs::RowType::notch, 64, 0 }, 0xE000);
        check (at (notch, note) < -30.0 && std::abs (at (notch, 100.0)) < 1.0, "a notch row puts the zero on the circle at the note");
        bool inverse = true;
        for (int f = 0; f < hs::kFreqCodes && inverse; f += 9)
            for (int g = hs::kGainMin; g <= hs::kGainMax && inverse; g += 7)
            {
                const auto w = hs::rowWords ({ hs::RowType::peak, f, g }, 0xE000);
                inverse = hs::rowWords (hs::rowOf (w), 0xE000) == w;
            }
        check (inverse && hs::rowOf (hs::rowWords ({ hs::RowType::rest, 0, 0 }, 0xE000)).type == hs::RowType::rest && hs::rowOf (notch).type == hs::RowType::notch, "every grammar row reads back as itself");
        hs::Words padded {};
        for (size_t r = 0; r < hs::kRows; ++r) padded[r] = r < 4 ? hs::rowWords ({ hs::RowType::peak, (int) (20 + 20 * r), 6 }, 0xE000) : trench::core::kIdentitySection;
        check (hs::admit (padded), "a four-section frame padded with rest rows is admitted");
        check (hs::noteName (440.0) == "A4" && hs::noteName (0.0) == "rest", "rows are named by note");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int factory = s.quad.pins[(size_t) hs::Session::kCornerPin[0]];
        const auto before = s.stars[(size_t) factory].words;
        s.edit (0);
        check (s.editing == 0 && s.auditioning == factory && same (s.words, before), "opening corner A's rows plays that corner exactly");
        s.beginRowEdit();
        s.setRow (0, 1, { hs::RowType::peak, 70, 10 });
        const int edited = s.quad.pins[(size_t) hs::Session::kCornerPin[0]];
        bool untouched = true;
        for (size_t r = 0; r < hs::kRows; ++r) if (r != 1) untouched = untouched && s.stars[(size_t) edited].words[r] == before[r];
        check (edited != factory && s.stars[(size_t) edited].kind == "capture" && s.stars[(size_t) edited].name == "C1" && s.cornerName (0) == "C1", "editing a factory corner makes a capture and puts it in that corner");
        check (untouched && s.stars[(size_t) edited].words[1] == hs::rowWords ({ hs::RowType::peak, 70, 10 }, before[1][4]) && same (s.stars[(size_t) factory].words, before), "only the edited row changes, its fifth word is kept, and the factory card is untouched");
        s.setRow (0, 1, { hs::RowType::peak, 71, 10 });
        check (s.quad.pins[(size_t) hs::Session::kCornerPin[0]] == edited && s.stars.size() == 145 && same (s.words, s.stars[(size_t) edited].words), "a second edit stays in the same capture and is what plays");
        const auto path = juce::File::createTempFile ("edited.body240");
        s.write (path);
        const auto bytes = bytesOf (path);
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
        check (body.words[2][1] == hs::rowWords ({ hs::RowType::peak, 71, 10 }, before[1][4]), "W writes the edited row into the M0 Q1 corner of the 240 bytes");
        s.undo();
        check (s.editing == -1 && s.cornerName (0) == "Talking Hedz M0 Q1" && s.stars.size() == 144, "one undo removes the edit and its capture");
        s.edit (2); s.setPuck (10.0, 10.0);
        check (s.editing == -1 && s.auditioning == -1, "touching the stage closes the rows");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.loadPreset ("Zoom Peaks");
        const auto heat = hs::hotCells (hs::cornersOf (s.quad, s.stars), 9, hs::curveHz());
        double worst = -1e9;
        for (double h : heat) worst = std::max (worst, h);
        check (heat.size() == 81 && worst > 3.0, "Zoom Peaks stacks peaks inside its own body, as measured");
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
        check (written && file.getSize() > 20000 && image.getWidth() == 1120, "the screen renders to artifacts/shots/headspace.png without a window");
        check (juce::Desktop::getInstance().getNumComponents() == 0, "no window was opened");
        check (screen.stage.toFloat().contains (screen.puckPoint()), "the puck sits inside the stage");
        screen.setSize (900, 560);
        check (screen.stage.getWidth() > 400 && screen.cornerBox[3].getRight() <= 900, "the layout follows the window size");
        screen.setSize (1120, 700);
        s.edit (0);
        const auto rows = screen.shot();
        const auto rowsFile = folder.getChildFile ("headspace_rows.png");
        rowsFile.deleteFile();
        juce::FileOutputStream rowsOut (rowsFile);
        const bool rowsWritten = rowsOut.openedOk() && png.writeImageToStream (rows, rowsOut);
        rowsOut.flush();
        check (rowsWritten && screen.stage.contains (screen.table) && screen.cell (5, 4).getBottom() <= screen.table.getBottom(), "the six rows render inside the stage to artifacts/shots/headspace_rows.png");
    }

    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
