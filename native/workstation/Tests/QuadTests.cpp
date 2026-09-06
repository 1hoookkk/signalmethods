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

juce::KeyPress key (int code, bool control = false, juce::juce_wchar text = 0)
{
    return juce::KeyPress (code, control ? juce::ModifierKeys::ctrlModifier : juce::ModifierKeys::noModifiers, text);
}

juce::MouseEvent mouse (hs::Screen& screen, juce::Point<float> position, juce::Point<float> origin)
{
    return { juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys::leftButtonModifier,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &screen, &screen, juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, position != origin };
}

std::vector<std::uint8_t> bytesOf (const juce::File& f)
{
    juce::MemoryBlock mb;
    f.loadFileAsData (mb);
    return { (const std::uint8_t*) mb.getData(), (const std::uint8_t*) mb.getData() + mb.getSize() };
}

bool same (const hs::Words& a, const hs::Words& b) { return a == b; }

juce::String ipa (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); }

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

double at (const trench::core::PackedSection& w, double hz)
{
    const std::array<trench::core::Biquad, 1> one { trench::core::section_words_to_biquad (w) };
    return trench::core::cascade_response_db (one, hz, trench::core::kP2kDatumHz);
}
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File root (TRENCH_TABLE_STITCH_ROOT);
    const auto p2k = root.getChildFile ("plugin/presets/p2k");

    {
        hs::Session s (root, tempQuad(), false);
        check (s.libraryCount == 13 && s.stars.size() == 13, "the picker holds the 12 Klatt vowels and schwa, nothing else");
        bool named = true, factory = false, zeros = true;
        for (const auto& e : s.stars)
        {
            named = named && e.name.isNotEmpty() && e.name.length() <= 2 && ! e.name.containsAnyOf ("0123456789");
            factory = factory || e.kind == "factory";
            bool hasZero = false;
            for (const auto& row : e.words) hasZero = hasZero || row[1] < 0xFF00;
            zeros = zeros && hasZero;
        }
        check (named && ! factory, "every vowel is named by its symbol alone and no E-mu preset is on the surface");
        check (zeros, "every vowel carries zeros, not poles alone");
        check (s.quad.complete() && s.cornerName (0) == "i" && s.cornerName (1) == "u" && s.cornerName (2) == ipa ("\xc9\x91") && s.cornerName (3) == ipa ("\xc9\x99") && s.sounding, "a fresh session boots with i, u, a and schwa in the four corners");
        const auto boot = s.words;
        s.hover (5);
        check (same (s.words, boot) && s.hovered == 5, "hover only names a point, the sound stays");
        s.select (5);
        check (s.sounding && same (s.words, s.stars[5].words) && s.auditioning == 5, "clicking a vowel plays it exactly");
        s.setPuck (s.quad.morph, s.quad.q);
        check (same (s.words, boot) && s.auditioning == -1, "touching the stage returns to the four corners");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setMade (700.0, 1100.0);
        const auto f = hs::formantsOf (s.words);
        std::printf ("      made 700/1100 reads %.0f %.0f %.0f %.0f\n", f[0], f[1], f[2], f[3]);
        check (s.inMade() && s.madeLive && std::abs (f[0] - 700.0) < 5.0 && std::abs (f[1] - 1100.0) < 8.0 && s.status == "700/1100", "clicking empty chart makes a vowel at that F1 and F2 and plays it");
        check (hs::rowOf (s.words[5]).type == hs::RowType::notch && hs::rowHz (s.words[5]) > 11000.0, "a made vowel carries the high safety notch in row 6");
        s.key (key ('S', true, 's'));
        check (s.stars.size() == 14 && s.stars.back().kind == "capture" && s.stars.back().name == "700/1100" && same (s.stars.back().words, hs::madeVowel (700.0, 1100.0).words), "Ctrl+S keeps the made vowel as a point named by its formants");
        s.setMade (700.0, 1100.0);
        s.key (key ('2', false, '2'));
        check (s.stars.size() == 15 && s.stars.back().name == "700/1100 2" && s.cornerName (1) == "700/1100 2", "the 2 key keeps the made vowel and puts it in the top-right corner, never a counter name");
        const int i = s.starNamed ("i");
        s.select (i);
        s.key (key ('3', false, '3'));
        check (s.cornerName (2) == "i" && s.pinName (0) == "i", "the 3 key puts the playing vowel in the bottom-left corner, which is M0 Q0");
        s.pinCorner (0, s.starNamed ("u"));
        check (s.cornerName (0) == "u" && s.pinName (2) == "u", "placing by drag lands in M0 Q1 for the top-left corner");
        s.select (s.starNamed ("e"));
        s.key (key ('D', false, 'd'));
        check (s.cornerName (3) == "e", "the D key puts the playing vowel in the bottom-right corner");
        s.select (s.starNamed ("o"));
        s.toCorner (1);
        check (s.cornerName (1) == "o" && s.placeable(), "the to-corner button does what the key does");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const char* names[8] = { "i", "e", "u", "o", "\xc9\x91", "\xc3\xa6", "\xc9\x99", "\xca\x8c" };
        for (int n = 0; n < 8; ++n) s.pinCube (n, s.starNamed (ipa (names[n])));
        check (s.cube.complete() && s.auditioning == s.starNamed (ipa (names[7])), "eight picks fill the cube and the last pick plays");
        s.setCubePoint (0.3, 0.6, 0.25);
        const auto body = hs::cubeBodyOf (s.cube, s.stars);
        const auto cw = body.interpolate_words (0.3f, 0.6f, 0.25f);
        bool ok = s.auditioning == hs::Session::kCube && s.sounding;
        for (size_t row = 0; row < hs::kRows; ++row) ok = ok && cw[row] == s.words[row];
        check (ok, "the cube point plays the chip's three-axis lerp of the eight corners");
        s.setCubePoint (0.0, 0.0, 0.0);
        check (same (s.words, s.stars[(size_t) s.starNamed ("i")].words), "at the cube's origin the words are corner 1 exactly");
        s.setCubePoint (1.0, 1.0, 1.0);
        check (same (s.words, s.stars[(size_t) s.starNamed (ipa ("\xca\x8c"))].words), "at the cube's far corner the words are corner 8 exactly");
        s.setCubePoint (0.5, 0.5, 0.5);
        const auto heard = s.words;
        s.key (key ('1', false, '1'));
        check (s.stars.size() == 14 && same (s.stars.back().words, heard) && s.cornerName (0) == s.stars.back().name && s.stars.back().parentA == "cube", "a corner key at a cube point keeps the slice sound and puts it in that corner");
        s.setCubePoint (0.5, 0.5, 0.75);
        check (s.takeSlice() && s.quad.morph == 50.0 && s.quad.q == 50.0, "taking the slice puts the plane's four corners into the body at the point's MORPH and Q");
        const auto slice = hs::cubeBodyOf (s.cube, s.stars);
        bool corners = true;
        for (int corner = 0; corner < 4 && corners; ++corner)
        {
            const auto expect = slice.interpolate_words ((float) (corner & 1), (float) ((corner >> 1) & 1), 0.75f);
            const auto got = hs::cornersOf (s.quad, s.stars)[(size_t) corner];
            for (size_t row = 0; row < hs::kRows; ++row) corners = corners && expect[row] == got[row];
        }
        check (corners, "each body corner is the chip's lerp along Z between the cube's front and back corners");
        hs::Session again (root, s.file, false);
        check (again.cube.complete() && again.cube.z == 0.75 && again.cube.pins == s.cube.pins, "save then reopen restores the cube's eight corners and depth");
    }

    {
        hs::Session s (root, tempQuad(), false);
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
        s.setPuck (0.0, 0.0);
        check (same (s.words, s.stars[(size_t) s.quad.pins[0]].words), "at MORPH 0 Q 0 the words are the bottom-left corner exactly");
        s.setPuck (100.0, 100.0);
        check (same (s.words, s.stars[(size_t) s.quad.pins[3]].words), "at MORPH 100 Q 100 the words are the top-right corner exactly");
        s.setPuck (0.0, 0.0);
        s.key (key (juce::KeyPress::rightKey)); s.key (key (juce::KeyPress::rightKey, true)); s.key (key (juce::KeyPress::upKey));
        check (std::abs (s.quad.morph - 1.2) < 1e-9 && s.quad.q == 1.0 && same (s.words, hs::lerp (hs::cornersOf (s.quad, s.stars), 0.012, 0.01)), "arrows move the puck and the words are the chip's lerp");
    }

    {
        const auto library = hs::loadLibrary (p2k);
        bool ok = library.size() == 132;
        for (size_t b = 0; b < library.size() && ok; b += 4)
        {
            hs::Corners c;
            for (size_t n = 0; n < 4; ++n) c[n] = library[b + n].words;
            const auto bytes = hs::bytesOf (c);
            const auto original = bytesOf (hs::bodyFile (p2k, library[b].body));
            ok = original.size() == bytes.size() && std::equal (bytes.begin(), bytes.end(), original.begin());
        }
        check (ok, "the pack of four factory corners is the factory's own 240 bytes, for every body");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPuck (23.0, 40.0);
        const auto heard = s.words;
        s.key (key ('S', true, 's'));
        const auto name = hs::formantName (heard);
        check (s.stars.size() == 14 && s.stars.back().name == name && s.stars.back().kind == "capture" && same (s.stars.back().words, heard) && name.containsChar ('/'), "Ctrl+S keeps the puck's sound as a point named by its formants");
        check (s.stars.back().parentA == ipa ("\xc9\x91") && s.stars.back().morph == 23.0 && s.stars.back().q == 40.0 && s.auditioning == 13, "the capture records its corners and position and is playing");
        s.key (key ('1', false, '1'));
        check (s.cornerName (0) == name && same (hs::cornersOf (s.quad, s.stars)[2], heard), "the 1 key puts the capture in the top-left corner");
        const auto voice = voiceWav();
        const int k = s.addRead (voice);
        check (k == 14 && s.stars.back().kind == "read" && s.stars.back().name == voice.getFileNameWithoutExtension(), "a dropped wav becomes a point named by the file");
        const auto f = hs::formantsOf (s.stars.back().words);
        std::printf ("      read formants %.0f %.0f %.0f %.0f\n", f[0], f[1], f[2], f[3]);
        check (f[0] > 300.0 && f[0] < 900.0, "the read finds the first formant of a synthetic voice near 500 Hz");
        {
            const auto& read = s.stars.back().words;
            bool paired = true;
            for (size_t r = 0; r + 1 < hs::kRows && paired; ++r)
            {
                const auto geometry = trench::core::geometry_from_words (read[r], trench::core::kP2kDatumHz);
                const auto* pole = std::get_if<trench::core::ConjugatePair> (&geometry.pole);
                const auto* zero = std::get_if<trench::core::ConjugatePair> (&geometry.zero);
                if (pole == nullptr || pole->radius < 0.05) continue;
                paired = zero != nullptr && std::abs (zero->hz - pole->hz) < pole->hz * 0.02 && zero->radius < pole->radius;
            }
            check (paired && hs::rowOf (read[5]).type == hs::RowType::notch && hs::rowHz (read[5]) > 11000.0, "a read pairs each pole with a zero on the same angle and pins row 6 to the ceiling notch");
        }
        s.setPuck (s.quad.morph, s.quad.q);
        hs::Session again (root, s.file, false);
        check (again.stars.size() == 15 && again.stars[13].kind == "capture" && again.stars[14].kind == "read" && again.cornerName (0) == name && same (again.words, s.words) && same (again.stars[14].words, s.stars[14].words), "save then reopen restores captures, reads and corners by name");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int a = s.starNamed ("i"), b = s.starNamed ("u");
        s.select (a);
        s.morphPair (a, b, 0.23);
        hs::Corners c { s.stars[(size_t) a].words, s.stars[(size_t) b].words, s.stars[(size_t) a].words, s.stars[(size_t) b].words };
        check (s.inPair() && same (s.words, hs::lerp (c, 0.23, 0.0)), "a pair plays the chip's lerp between two vowels");
        const auto heard = s.words;
        s.key (key ('4', false, '4'));
        check (s.stars.size() == 14 && same (s.stars.back().words, heard) && s.cornerName (3) == hs::formantName (heard), "a corner key during a pair keeps the sound and puts it in that corner");
    }

    {
        hs::Session s (root, tempQuad(), false);
        check (s.note == 45 && hs::noteName (440.0 * std::pow (2.0, (s.note - 69) / 12.0)) == "A2", "the saw starts on A2, 110 Hz");
        s.noteIn (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
        check (s.note == 60, "a MIDI note sets the saw's pitch");
        s.noteIn (juce::MidiMessage::noteOff (1, 60));
        check (s.note == 60, "note off leaves the drone where it is");
        s.key (key (']', false, ']')); s.key (key (juce::KeyPress::pageDownKey));
        check (s.note == 49, "] steps a semitone up and Page Down an octave down");
        const auto loop = voiceWav();
        check (s.setLoop (loop) && s.source == 2 && s.loopName == loop.getFileNameWithoutExtension(), "a wav dropped on the stage becomes the loop and plays through the cascade");
        s.key (key ('S', false, 's')); s.key (key ('L', false, 'l'));
        check (s.source == 2, "L returns to the loop");
        check (! s.setLoop (juce::File ("C:/nowhere/none.wav")) && s.status.startsWith ("cannot read"), "a missing wav says so");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPuck (50.0, 50.0); s.keep();
        s.pin (2, s.starNamed ("e"));
        s.select (13); s.key (key (juce::KeyPress::deleteKey));
        const size_t edits = s.history.size();
        check (s.stars.size() == 13 && edits == 3, "a capture, a pin and a delete record three edits");
        for (size_t i = 0; i < edits; ++i) s.key (key ('Z', true, 'z'));
        check (s.pinName (2) == "i" && s.stars.size() == 13 && s.history.empty() && s.future.size() == edits, "undo returns to the boot corners");
        for (size_t i = 0; i < edits; ++i) s.key (key ('Y', true, 'y'));
        check (s.pinName (2) == "e" && s.stars.size() == 13, "redo restores the edits");
        s.key (key ('Z', true, 'z'));
        check (s.stars.size() == 14 && s.stars.back().kind == "capture", "one undo brings the deleted capture back");
    }

    {
        bool rising = true;
        double last = 0.0;
        for (int f = 0; f < hs::kFreqCodes; ++f) { const double hzValue = hs::rowHz (hs::rowWords ({ hs::RowType::peak, f, 0 }, 0xE000)); rising = rising && hzValue > last; last = hzValue; }
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
        const int vowel = s.quad.pins[(size_t) hs::Session::kCornerPin[0]];
        const auto before = s.stars[(size_t) vowel].words;
        s.edit (0);
        check (s.editing == 0 && s.auditioning == vowel && same (s.words, before), "opening a corner's rows plays that corner exactly");
        s.beginRowEdit();
        s.setRow (0, 1, { hs::RowType::peak, 70, 10 });
        const int edited = s.quad.pins[(size_t) hs::Session::kCornerPin[0]];
        bool untouched = true;
        for (size_t r = 0; r < hs::kRows; ++r) if (r != 1) untouched = untouched && s.stars[(size_t) edited].words[r] == before[r];
        check (edited != vowel && s.stars[(size_t) edited].kind == "capture" && s.stars[(size_t) edited].name == hs::formantName (s.stars[(size_t) edited].words) && s.cornerName (0) == s.stars[(size_t) edited].name, "editing a vowel makes a capture named by its formants and puts it in that corner");
        check (untouched && s.stars[(size_t) edited].words[1] == hs::rowWords ({ hs::RowType::peak, 70, 10 }, before[1][4]) && same (s.stars[(size_t) vowel].words, before), "only the edited row changes, its fifth word is kept, and the vowel is untouched");
        s.setRow (0, 1, { hs::RowType::peak, 71, 10 });
        check (s.quad.pins[(size_t) hs::Session::kCornerPin[0]] == edited && s.stars.size() == 14 && same (s.words, s.stars[(size_t) edited].words), "a second edit stays in the same capture and is what plays");
        const auto path = juce::File::createTempFile ("edited.body240");
        s.write (path);
        const auto bytes = bytesOf (path);
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
        check (body.words[2][1] == hs::rowWords ({ hs::RowType::peak, 71, 10 }, before[1][4]), "W writes the edited row into the M0 Q1 corner of the 240 bytes");
        s.undo();
        check (s.editing == -1 && s.cornerName (0) == "i" && s.stars.size() == 13, "one undo removes the edit and its capture");
        s.edit (2); s.setPuck (10.0, 10.0);
        check (s.editing == -1 && s.auditioning == -1, "touching the stage closes the rows");
    }

    {
        hs::Session s (root, tempQuad(), false);
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
        const auto f = screen.formantsAt (screen.chartPoint (700.0, 1100.0).toInt());
        check (std::abs (f.first - 700.0) < 6.0 && std::abs (f.second - 1100.0) < 10.0 && screen.chart.toFloat().contains (screen.chartPoint (hs::kSchwaF1, hs::kSchwaF2)), "the chart maps F1 and F2 both ways and schwa sits inside it");
        check (screen.pointAt (screen.chartPoint (hs::kSchwaF1, hs::kSchwaF2).toInt()) == s.starNamed (ipa ("\xc9\x99")), "schwa is found at the chart's origin");
        const auto u = hs::formantsOf (s.stars[(size_t) s.starNamed ("u")].words), iy = hs::formantsOf (s.stars[(size_t) s.starNamed ("i")].words);
        std::printf ("      u reads %.0f %.0f, i reads %.0f %.0f\n", u[0], u[1], iy[0], iy[1]);
        check (u[0] > 250.0 && u[0] < 400.0 && u[1] > 1150.0 && u[1] < 1350.0 && iy[1] > 1900.0, "the Klatt vowels sit at their own Table II F1 and F2, the wide tilt row is not a formant");
        screen.setSize (900, 560);
        check (screen.stage.getWidth() > 300 && screen.stage.contains (screen.picker) && screen.stage.contains (screen.chart) && ! screen.picker.intersects (screen.chart), "the layout follows the window size and the palette never overlaps the vowel space");
        bool hudPinned = screen.hud.getRight() == 900 - 20 && screen.hud.getBottom() == 560 - 20 && ! screen.hud.intersects (screen.stage) && ! screen.hud.intersects (screen.keyboard);
        for (int n = 0; n < 4; ++n) hudPinned = hudPinned && screen.hud.contains (screen.cornerBox[(size_t) n]) && screen.cornerBox[(size_t) n].contains (screen.cornerPlot[(size_t) n]);
        hudPinned = hudPinned && screen.cornerBox[0].getRight() == screen.cornerBox[1].getX() && screen.cornerBox[0].getBottom() == screen.cornerBox[2].getY() && screen.cornerBox[3].getX() > screen.cornerBox[2].getX() && screen.hud.contains (screen.writeKey);
        check (hudPinned, "the 2x2 body sits in the bottom-right corner with A B over C D and the write key, clear of the room and the keyboard");
        check (screen.cornerAt (screen.cornerPlot[2].getCentre()) == 2 && screen.cornerAt (screen.chart.getCentre()) < 0, "a drop on the third cell lands in corner C");
        check (screen.keyboard.getRight() <= screen.stage.getRight() && screen.keys[0].getY() > screen.stage.getBottom() && screen.toKeys[3].getRight() <= screen.stage.getRight(), "the audition strip and the to-corner buttons run under the room and beside the body");
        screen.setSize (1120, 700);
        for (int room = 0; room < 4; ++room)
        {
            screen.showView ((hs::Screen::View) room);
            const char* files[4] = { "headspace.png", "headspace_cube.png", "headspace_rows.png", "headspace_perform.png" };
            if (room == 0 || room == 2) continue;
            const auto roomFile = folder.getChildFile (files[room]);
            roomFile.deleteFile();
            juce::FileOutputStream roomOut (roomFile);
            check (roomOut.openedOk() && png.writeImageToStream (screen.shot(), roomOut), room == 1 ? "the cube room renders to artifacts/shots/headspace_cube.png" : "the perform room renders to artifacts/shots/headspace_perform.png");
            roomOut.flush();
        }
        check (screen.view == hs::Screen::View::perform && screen.stage.contains (screen.morph) && screen.morph.getWidth() == screen.morph.getHeight() && screen.morph.toFloat().contains (screen.puckPoint()), "the perform room is a square pad inside the room with the puck on it");
        screen.showView (hs::Screen::View::cube);
        check (screen.stage.contains (screen.cubeArea) && screen.stage.contains (screen.depth) && screen.stage.contains (screen.cubeBox[0]) && screen.stage.contains (screen.cubeBox[7]), "the cube, its eight name boxes and the depth rail sit inside the room");
        screen.showView (hs::Screen::View::picker);
        s.edit (0);
        check (screen.view == hs::Screen::View::stage, "opening a corner's rows goes to the stage room");
        const auto rows = screen.shot();
        const auto rowsFile = folder.getChildFile ("headspace_rows.png");
        rowsFile.deleteFile();
        juce::FileOutputStream rowsOut (rowsFile);
        const bool rowsWritten = rowsOut.openedOk() && png.writeImageToStream (rows, rowsOut);
        rowsOut.flush();
        check (rowsWritten && screen.stage.contains (screen.table) && screen.cell (5, 4).getBottom() <= screen.table.getBottom(), "the six rows render inside the stage to artifacts/shots/headspace_rows.png");
        screen.keyPressed (key ('H', false, 'h'));
        const auto rawFile = folder.getChildFile ("headspace_rows_raw.png");
        rawFile.deleteFile();
        juce::FileOutputStream rawOut (rawFile);
        check (rawOut.openedOk() && png.writeImageToStream (screen.shot(), rawOut), "the H view renders raw Hz and radius words headlessly");
        rawOut.flush();
    }

    {
        hs::Session s (root, tempQuad(), false);
        hs::Screen screen (s);
        s.edit (0);
        s.beginRowEdit();
        s.setRow (0, 0, { hs::RowType::peak, 50, 10 });
        const auto before = s.editWords();
        const auto history = s.history.size();
        const auto start = screen.peakPoint (0).roundToInt().toFloat();
        check (screen.peakAt (start.toInt()) == 0, "a formant handle is hit on the magnitude response");
        screen.mouseDown (mouse (screen, start, start));
        screen.mouseUp (mouse (screen, start, start));
        check (same (s.editWords(), before) && s.history.size() == history, "clicking a peak leaves its exact words and undo history intact");
        const auto end = start.translated (20.0f, -20.0f);
        screen.mouseDown (mouse (screen, start, start));
        screen.mouseDrag (mouse (screen, end, start));
        const auto edited = s.editWords();
        const double oldHz = hs::rowHz (before[0]), newHz = hs::rowHz (edited[0]);
        const double oldDb = hs::responseDb (before, { oldHz })[0], newDb = hs::responseDb (edited, { newHz })[0];
        const double expectedHz = oldHz * std::pow (1000.0, 20.0 / screen.magnitude.getWidth());
        const double expectedDb = oldDb + 20.0 * 60.0 / screen.magnitude.getHeight();
        check (newHz > oldHz && newDb > oldDb && std::abs (12.0 * std::log2 (newHz / expectedHz)) < 1.0 && std::abs (newDb - expectedDb) < 1.5,
            "dragging a formant right and up follows the plot's frequency and cascade dB axes within word resolution");
        screen.mouseDrag (mouse (screen, end.translated (4.0f, -4.0f), start));
        screen.mouseUp (mouse (screen, end, start));
        bool untouched = s.editWords()[0][4] == before[0][4];
        for (int r = 1; r < hs::kRows; ++r) untouched = untouched && s.editWords()[(size_t) r] == before[(size_t) r];
        check (untouched && s.history.size() == history + 1 && same (s.words, s.editWords()), "one peak gesture preserves the other five sections and fifth word, plays the edit, and records one undo");
        s.undo();
        if (s.editing < 0) s.edit (0);
        check (same (s.editWords(), before), "undo restores the exact words from before the peak gesture");
        check (! screen.showHardware, "raw Hz and radius words are hidden by default");
        bool fits = true;
        for (const auto size : { juce::Point<int> { 1120, 700 }, juce::Point<int> { 900, 560 }, juce::Point<int> { 820, 520 } })
        {
            screen.setSize (size.x, size.y);
            for (int mode = 0; mode < 2; ++mode)
            {
                fits = fits && screen.magnitude.getWidth() * 2 == screen.magnitude.getHeight() * 3
                    && screen.stage.contains (screen.magnitude) && ! screen.magnitude.intersects (screen.table)
                    && screen.table.contains (screen.cell (5, 5));
                screen.keyPressed (key ('H', false, 'h'));
                fits = fits && screen.showHardware == (mode == 0);
            }
        }
        check (fits, "the isolated 3:2 plot and six-column editor fit down to the minimum window size with H toggled on and off");
        check (juce::Desktop::getInstance().getNumComponents() == 0, "peak gestures and hardware toggles stay headless");
    }

    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
