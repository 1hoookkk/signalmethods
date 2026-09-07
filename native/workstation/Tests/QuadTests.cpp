#include "ui/Look.h"
#include "ui/Screen.h"
#include "ui/Spectrogram.h"
#include <algorithm>
#include <complex>
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

juce::MouseEvent mouse (hs::Screen& screen, juce::Point<float> position, juce::Point<float> origin, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
{
    return { juce::Desktop::getInstance().getMainMouseSource(), position, mods,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &screen, &screen, juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, position != origin };
}

std::vector<std::uint8_t> bytesOf (const juce::File& f)
{
    juce::MemoryBlock mb;
    f.loadFileAsData (mb);
    return { (const std::uint8_t*) mb.getData(), (const std::uint8_t*) mb.getData() + mb.getSize() };
}

bool same (const hs::Words& a, const hs::Words& b) { return a == b; }

bool shape (const trench::core::PackedSection& a, const trench::core::PackedSection& b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]; }

double dcDb (const hs::Words& w) { return hs::responseDb (w, { 1.0 })[0]; }

bool levelled (const hs::Words& w)
{
    bool equal = true;
    for (size_t r = 1; r < hs::kRows; ++r) equal = equal && w[r][4] == w[0][4];
    return equal && std::abs (dcDb (w)) < 0.1;
}

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

std::complex<double> whole (const hs::Words& w, double hz, size_t rows)
{
    std::complex<double> out (1.0, 0.0);
    const auto z1 = std::polar (1.0, -6.283185307179586 * hz / trench::core::kP2kDatumHz), z2 = z1 * z1;
    for (size_t s = 0; s < rows; ++s)
    {
        const auto b = trench::core::section_words_to_biquad (w[s]);
        out *= (b[0] + b[1] * z1 + b[2] * z2) / (1.0 + b[3] * z1 + b[4] * z2);
    }
    return out;
}

double wrapped (double radians)
{
    while (radians > 3.141592653589793) radians -= 6.283185307179586;
    while (radians < -3.141592653589793) radians += 6.283185307179586;
    return radians;
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
        int klatt = 0, h95 = 0, pb52 = 0, reads = 0, other = 0;
        bool named = true, polesOnly = true, sixLive = true, resonant = true;
        for (const auto& e : s.stars)
        {
            if (e.kind == "vowel" && (e.body == "Klatt 1980" || e.body == "neutral")) { ++klatt; named = named && e.name.isNotEmpty() && e.name.length() <= 2 && ! e.name.containsAnyOf ("0123456789"); }
            else if (e.kind == "vowel" && e.body == "Hillenbrand 1995") { ++h95; named = named && juce::StringArray::fromTokens (e.name, " ", "").size() == 2 && e.name.length() <= 8 && ! e.name.startsWith ("vowel"); }
            else if (e.kind == "vowel" && e.body == "Peterson Barney 1952") ++pb52;
            else if (e.kind == "read")
            {
                ++reads;
                resonant = resonant && (e.body.startsWith ("Aud ") || e.body.startsWith ("303 ")) && e.corner.isNotEmpty();
                sixLive = sixLive && hs::rowOf (e.words[5]).type == hs::RowType::notch;
                for (size_t r = 0; r + 1 < hs::kRows; ++r) { sixLive = sixLive && hs::sectionOf (e.words[r]).pole; polesOnly = polesOnly && ! hs::sectionOf (e.words[r]).zero; }
            }
            else ++other;
        }
        std::printf ("      palette: %d Klatt, %d Hillenbrand, %d Peterson Barney, %d XL reads, %d other\n", klatt, h95, pb52, reads, other);
        check (s.libraryCount == s.stars.size() && klatt == 13 && h95 == 48 && pb52 == 30 && reads >= 40 && s.starNamed ("303 open C2") >= 0 && s.starNamed ("303 closed C2") >= 0 && other == 0, "the palette holds the 12 Klatt vowels and schwa, the 48 Hillenbrand medians, the 30 Peterson and Barney means, the XL bank's resonant notes and the 303 at five octaves open and closed, no E-mu preset and no impulse response");
        check (named, "Klatt vowels are named by symbol alone, Hillenbrand vowels by symbol and speaker group");
        check (resonant && sixLive && polesOnly, "every XL read is a note of a resonant family with six live stages, poles only, and the ceiling notch");
        const int men = s.starNamed ("i men");
        const auto fm = men >= 0 ? hs::formantsOf (s.stars[(size_t) men].words) : std::array<double, 4> {};
        std::printf ("      i men reads %.0f %.0f %.0f\n", fm[0], fm[1], fm[2]);
        check (men >= 0 && std::abs (fm[0] - 338.0) < 12.0 && std::abs (fm[1] - 2319.0) < 60.0, "the Hillenbrand men's i sits at its published F1 and F2");
        check (s.starNamed (ipa ("\xc9\x91") + " women") >= 0 && s.starNamed (ipa ("\xca\x8a") + " boys") >= 0 && s.starNamed (ipa ("\xca\x8c") + " girls") >= 0 && s.starNamed ("ah women") < 0, "Hillenbrand's hod, hood and hud read as their own symbols, not the bank's codes");
        const int bell = s.starNamed ("Aud Bell 1 C4");
        bool ring = false;
        std::printf ("      Aud Bell 1 C4 rows:");
        if (bell >= 0)
            for (size_t r = 0; r + 1 < hs::kRows; ++r)
            {
                const auto g = trench::core::geometry_from_words (s.stars[(size_t) bell].words[r], trench::core::kP2kDatumHz);
                const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
                if (pole == nullptr || pole->radius < 0.05) continue;
                const double bw = -std::log (pole->radius) * trench::core::kP2kDatumHz / 3.141592653589793;
                std::printf ("  %.0f Hz bw %.0f", pole->hz, bw);
                ring = ring || (pole->hz > 200.0 && pole->hz < 6000.0);
            }
        std::printf ("\n");
        check (bell >= 0 && s.stars[(size_t) bell].body == "Aud Bell 1" && s.stars[(size_t) bell].corner == "C4" && ring, "an XL bell note reads as a read named by family and note with a resonance in the audible band");
        check (s.quad.complete() && s.cornerName (0) == "i" && s.cornerName (1) == "u" && s.cornerName (2) == ipa ("\xc9\x91") && s.cornerName (3) == ipa ("\xc9\x99") && s.sounding, "a fresh session boots with i, u, a and schwa in the four corners");
        const auto boot = s.words;
        s.hover (5);
        check (same (s.words, boot) && s.hovered == 5, "hover only names a point, the sound stays");
        s.select (5);
        check (s.sounding && same (s.words, s.stars[5].words) && s.auditioning == 5, "clicking a vowel plays it exactly");
        s.setPuck (s.quad.morph, s.quad.q);
        check (same (s.words, hs::wordsAt (s.quad, s.stars)) && s.auditioning == -1, "touching the stage returns to the four corners");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setMade (700.0, 1100.0);
        const auto f = hs::formantsOf (s.words);
        std::printf ("      made 700/1100 reads %.0f %.0f %.0f %.0f\n", f[0], f[1], f[2], f[3]);
        check (s.inMade() && s.madeLive && std::abs (f[0] - 700.0) < 5.0 && std::abs (f[1] - 1100.0) < 8.0 && s.status == "700/1100", "clicking empty chart makes a vowel at that F1 and F2 and plays it");
        check (hs::rowOf (s.words[5]).type == hs::RowType::notch && hs::rowHz (s.words[5]) > 11000.0, "a made vowel carries the high safety notch in row 6");
        s.key (key ('K', true, 'k'));
        check (s.stars.size() == s.libraryCount + 1 && s.stars.back().kind == "capture" && s.stars.back().name == "700/1100" && same (s.stars.back().words, hs::madeVowel (700.0, 1100.0).words), "Ctrl+S keeps the made vowel as a point named by its formants");
        s.setMade (700.0, 1100.0);
        s.key (key ('2', false, '2'));
        check (s.stars.size() == s.libraryCount + 2 && s.stars.back().name == "700/1100 2" && s.cornerName (1) == "700/1100 2", "the 2 key keeps the made vowel and puts it in the top-right corner, never a counter name");
        const int i = s.starNamed ("i");
        s.select (i);
        s.key (key ('3', false, '3'));
        check (s.cornerName (2) == "i" && s.pinName (0) == "i", "the 3 key puts the playing vowel in the bottom-left corner, which is M0 Q0");
        s.pinCorner (0, s.starNamed ("u"));
        check (s.cornerName (0) == "u" && s.pinName (2) == "u", "placing by drag lands in M0 Q1 for the top-left corner");
        s.select (s.starNamed ("e"));
        s.key (key ('4', false, '4'));
        check (s.cornerName (3) == "e", "the D key puts the playing vowel in the bottom-right corner");
        s.select (s.starNamed ("o"));
        s.toCorner (1);
        check (s.cornerName (1) == "o" && s.placeable(), "the to-corner button does what the key does");
        s.select (s.starNamed ("i"));
        s.toColumn (0);
        s.select (s.starNamed ("u"));
        s.toColumn (1);
        check (s.cornerName (0) == "i" && s.cornerName (2) == "i" && s.cornerName (1) == "u" && s.cornerName (3) == "u" && s.working == 1, "a column places one endpoint in A and C and the other in B and D");
        s.setPuck (35.0, 0.0);
        const auto low = s.words;
        s.setPuck (35.0, 100.0);
        check (same (low, s.words), "with the pair repeated down the columns Q does nothing and MORPH is the whole sweep");
        s.audio.onWheel (0.3);
        check (std::abs (s.quad.morph - 30.0) < 1e-9 && s.quad.q == 100.0, "the mod wheel drives MORPH on the pad");
        s.setPuck (35.0, 65.0);
        const auto pad = s.words;
        s.toCorner (2);
        check (s.placeable() && s.stars.back().kind == "capture" && same (s.stars.back().words, pad) && s.cornerName (2) == s.stars.back().name, "the to-corner button with the pad playing keeps the puck's sound and puts it in that corner");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int i = s.starNamed ("i");
        const auto source = s.stars[(size_t) i].words;
        const auto before = hs::formantsOf (source);
        s.setTransposed (i, before[0] * 1.5);
        const auto after = hs::formantsOf (s.words);
        std::printf ("      i %.0f %.0f %.0f -> %.0f %.0f %.0f\n", before[0], before[1], before[2], after[0], after[1], after[2]);
        check (s.inMade() && std::abs (after[0] / before[0] - 1.5) < 0.06 && std::abs (after[1] / before[1] - 1.5) < 0.06 && std::abs (after[2] / before[2] - 1.5) < 0.06, "transposing i to a first formant half again as high moves every formant by the same ratio");
        bool fifth = s.words[5] == source[5];
        for (size_t r = 0; r < hs::kRows; ++r) fifth = fifth && s.words[r][4] == source[r][4];
        double widthBefore = 0.0, widthAfter = 0.0;
        {
            const auto a = trench::core::geometry_from_words (source[0], trench::core::kP2kDatumHz), b = trench::core::geometry_from_words (s.words[0], trench::core::kP2kDatumHz);
            const auto* pa = std::get_if<trench::core::ConjugatePair> (&a.pole);
            const auto* pb = std::get_if<trench::core::ConjugatePair> (&b.pole);
            if (pa != nullptr && pb != nullptr) { widthBefore = -std::log (pa->radius) / pa->hz; widthAfter = -std::log (pb->radius) / pb->hz; }
        }
        check (fifth && widthBefore > 0.0 && std::abs (widthAfter / widthBefore - 1.0) < 0.05, "the ceiling row and every fifth word are untouched and each row keeps its width in semitones");
        const auto heard = s.words;
        s.key (key ('K', true, 'k'));
        check (s.stars.size() == s.libraryCount + 1 && s.stars.back().kind == "capture" && s.stars.back().name.startsWith ("i ") && s.stars.back().parentA == "i" && same (s.stars.back().words, heard), "Ctrl+S keeps the transposed sound as a card named by its source and formants");
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
        int ceilings = 0, corners = 0;
        for (const auto& corner : library)
        {
            ++corners;
            const auto s = hs::sectionOf (corner.words[5]);
            if (s.zero && s.zeroRadius > 0.999) ++ceilings;
        }
        std::printf ("      factory row 6: %d of %d corners carry a zero on the circle\n", ceilings, corners);
    }

    {
        hs::Session s (root, tempQuad(), false);
        hs::Screen screen (s);
        const int i = s.starNamed ("i");
        const auto before = s.stars[(size_t) i].words;
        s.edit (0);
        auto sec = hs::sectionOf (before[1]);
        check (sec.pole && sec.zero && std::abs (sec.poleHz - 2020.0) < 40.0, "a Klatt row reads as its own pole and zero");
        sec.poleHz *= std::pow (2.0, 1.0 / 12.0);
        s.beginRowEdit(); s.setSection (0, 1, sec);
        const auto after = s.editWords();
        const auto got = hs::sectionOf (after[1]);
        check (after[1][0] == before[1][0] && after[1][1] == before[1][1] && levelled (after) && std::abs (1200.0 * std::log2 (got.poleHz / sec.poleHz)) < 10.0 && std::abs (got.poleRadius - sec.poleRadius) < 1e-3, "moving a pole a semitone leaves the zero words exactly, lands within 10 cents, and the corner stays at 0 dB DC");
        auto z = hs::sectionOf (after[2]);
        z.zeroHz *= 1.5;
        s.beginRowEdit(); s.setSection (0, 2, z);
        const auto moved = s.editWords();
        check (moved[2][2] == after[2][2] && moved[2][3] == after[2][3] && levelled (moved) && std::abs (hs::sectionOf (moved[2]).zeroHz / z.zeroHz - 1.0) < 0.01, "moving a zero leaves the pole words exactly and the corner at 0 dB DC");
        check (std::abs (screen.stage.cascadeDb (1) - hs::responseDb (moved, { hs::sectionOf (moved[1]).poleHz })[0]) < 0.01, "the Cascade column is the whole cascade at the pole, the number under the handle");
        screen.stage.zerosMode = true;
        const auto zp = screen.stage.zeroPoint (2).roundToInt().toFloat();
        check (screen.stage.zeroAt (zp.toInt()) == 2 && screen.stage.peakAt (zp.toInt()) != 2, "a zero handle is hit on the plot apart from the pole handle");
        screen.mouseDown (mouse (screen, zp, zp));
        screen.mouseDrag (mouse (screen, { zp.x, (float) screen.stage.magnitude.getBottom() }, zp));
        screen.mouseUp (mouse (screen, { zp.x, (float) screen.stage.magnitude.getBottom() }, zp));
        const auto notched = s.editWords();
        const auto zn = hs::sectionOf (notched[2]);
        check (zn.zero && zn.zeroRadius > 0.999 && hs::responseDb (notched, { zn.zeroHz })[0] < -30.0 && notched[2][2] == moved[2][2] && notched[2][3] == moved[2][3] && levelled (notched), "a zero dragged to the floor sits on the circle, a notch, the pole untouched, the corner at 0 dB DC");
        screen.stage.zerosMode = false;
        const auto rest = hs::sectionOf (notched[3]);
        hs::Section only;
        only.zero = true; only.zeroHz = 3000.0; only.zeroRadius = 0.9;
        s.beginRowEdit(); s.setSection (0, 3, only);
        const auto dipped = s.editWords();
        bool others = true;
        for (size_t r = 0; r < hs::kRows; ++r) if (r != 3) others = others && shape (dipped[r], notched[r]);
        const double dip = hs::responseDb (dipped, { 3000.0 })[0] - hs::responseDb (notched, { 3000.0 })[0];
        const double far = hs::responseDb (dipped, { 300.0 })[0] - hs::responseDb (notched, { 300.0 })[0];
        std::printf ("      zero alone on row 4: %.1f dB at 3 kHz, %.1f dB at 300 Hz\n", dip, far);
        check (! rest.pole && ! rest.zero && others && dip < -6.0 && dip < far - 6.0, "a zero placed on a rest row dips most at its own frequency and no other row changes");
        const int pin = s.quad.pins[(size_t) hs::Session::kCornerPin[0]];
        s.undo();
        check (s.stars[(size_t) pin].words[3] == notched[3] && s.stars[(size_t) pin].words[2] == notched[2], "one undo takes the placed zero away and leaves the notch");
        s.undo();
        check (s.stars[(size_t) pin].words[2] == moved[2], "the next undo restores the zero's exact words from before the drag");
    }

    {
        hs::Session s (root, tempQuad(), false);
        hs::Screen screen (s);
        const int i = s.starNamed ("i");
        const auto before = s.stars[(size_t) i].words;
        s.edit (0);
        const auto p1 = hs::sectionOf (before[0]), p2 = hs::sectionOf (before[1]);
        const double valley = std::sqrt (p1.poleHz * p2.poleHz);
        const double floorBefore = hs::responseDb (before, { valley })[0];
        const auto start = screen.stage.carveKey.getCentre().toFloat();
        screen.mouseDown (mouse (screen, start, start));
        screen.mouseDrag (mouse (screen, start.translated (240.0f, 0.0f), start));
        screen.mouseUp (mouse (screen, start.translated (240.0f, 0.0f), start));
        const auto after = s.editWords();
        const auto z1 = hs::sectionOf (after[0]), z2 = hs::sectionOf (after[1]);
        const double floorAfter = hs::responseDb (after, { valley })[0];
        std::printf ("      carve: pole 1 %.0f zero %.0f r %.3f, pole 2 %.0f zero %.0f r %.3f, valley %.0f Hz %.1f -> %.1f dB\n", z1.poleHz, z1.zeroHz, z1.zeroRadius, z2.poleHz, z2.zeroHz, z2.zeroRadius, valley, floorBefore, floorAfter);
        bool polesKept = true;
        for (size_t r = 0; r < hs::kRows; ++r) polesKept = polesKept && after[r][2] == before[r][2] && after[r][3] == before[r][3];
        check (polesKept && shape (after[5], before[5]) && levelled (after), "carving moves only zeros; every pole and the ceiling row keep their shape and the corner stays at 0 dB DC");
        check (z1.zero && z1.zeroHz > p1.poleHz * 1.2 && z1.zeroHz < p2.poleHz && std::abs (z1.zeroRadius - 0.97) < 0.01 && z2.zeroHz > p2.poleHz, "at full carve each zero sits in the valley above its pole at radius 0.97");
        check (floorAfter < floorBefore - 6.0, "the floor between the first two peaks drops by more than 6 dB");
        check (s.history.size() == 1, "one carve gesture is one undo");
        s.undo();
        check (same (s.stars[(size_t) s.quad.pins[(size_t) hs::Session::kCornerPin[0]]].words, before), "undo returns the exact words");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPuck (23.0, 40.0);
        const auto heard = s.words;
        s.key (key ('K', true, 'k'));
        const auto name = hs::formantName (heard);
        check (s.stars.size() == s.libraryCount + 1 && s.stars.back().name == name && s.stars.back().kind == "capture" && same (s.stars.back().words, heard) && name.containsChar ('/'), "Ctrl+S keeps the puck's sound as a point named by its formants");
        check (s.stars.back().parentA == ipa ("\xc9\x91") && s.stars.back().morph == 23.0 && s.stars.back().q == 40.0 && s.auditioning == (int) s.libraryCount, "the capture records its corners and position and is playing");
        s.key (key ('1', false, '1'));
        check (s.cornerName (0) == name && same (hs::cornersOf (s.quad, s.stars)[2], heard), "the 1 key puts the capture in the top-left corner");
        const auto voice = voiceWav();
        const int k = s.addRead (voice);
        check (k == (int) s.libraryCount + 1 && s.stars.back().kind == "read" && s.stars.back().name == voice.getFileNameWithoutExtension(), "a dropped wav becomes a point named by the file");
        const auto f = hs::formantsOf (s.stars.back().words);
        std::printf ("      read formants %.0f %.0f %.0f %.0f\n", f[0], f[1], f[2], f[3]);
        check (f[0] > 300.0 && f[0] < 900.0, "the read finds the first formant of a synthetic voice near 500 Hz");
        {
            const auto& read = s.stars.back().words;
            bool parked = true;
            for (size_t r = 0; r + 1 < hs::kRows && parked; ++r)
            {
                const auto geometry = trench::core::geometry_from_words (read[r], trench::core::kP2kDatumHz);
                const auto* pole = std::get_if<trench::core::ConjugatePair> (&geometry.pole);
                const auto* zero = std::get_if<trench::core::ConjugatePair> (&geometry.zero);
                parked = pole != nullptr && pole->radius >= 0.05 && (zero == nullptr || zero->radius < 0.05);
            }
            check (parked && hs::rowOf (read[5]).type == hs::RowType::notch && hs::rowHz (read[5]) > 11000.0, "a read writes six live poles with every zero parked and pins row 6 to the ceiling notch");
        }
        s.setPuck (s.quad.morph, s.quad.q);
        hs::Session again (root, s.file, false);
        again.setPuck (again.quad.morph, again.quad.q);
        check (again.stars.size() == again.libraryCount + 2 && again.stars[again.libraryCount].kind == "capture" && again.stars[again.libraryCount + 1].kind == "read" && again.cornerName (0) == name && same (again.words, s.words) && same (again.stars[again.libraryCount + 1].words, s.stars[s.libraryCount + 1].words), "save then reopen restores captures, reads and corners by name");
    }

    {
        const auto klatt = hs::vowelWords ({ 310.0, 2020.0, 2960.0, 3300.0 });
        const int points = 1025;
        const double nyquist = trench::core::kP2kDatumHz / 2.0;
        std::vector<std::complex<double>> truth ((size_t) points);
        std::vector<double> magnitude ((size_t) points);
        for (int k = 0; k < points; ++k)
        {
            truth[(size_t) k] = whole (klatt, (double) k * nyquist / (double) (points - 1), hs::kRows - 1);
            magnitude[(size_t) k] = 20.0 * std::log10 (std::abs (truth[(size_t) k]));
        }
        std::vector<std::complex<double>> rebuilt;
        const auto phase = hs::minimumPhaseResponse (magnitude, rebuilt);
        double worst = 0.0;
        for (double hz : { 310.0, 2020.0, 2960.0 })
        {
            const size_t k = (size_t) std::lround (hz / nyquist * (double) (points - 1));
            worst = std::max (worst, std::abs (wrapped (phase[k] - std::arg (truth[k]))));
        }
        std::printf ("      minimum phase off by %.4f rad at the formants\n", worst);
        check (phase.size() == (size_t) points && rebuilt.size() == (size_t) points && worst < 0.15, "the minimum phase of a known cascade matches its true phase");

        const auto fit = hs::vectorFit (truth, trench::core::kP2kDatumHz, 5, 8);
        std::printf ("      fitted poles:");
        for (const auto& p : fit.poles)
            if (p.imag() > 0.0) std::printf ("  %.1f Hz r %.5f", std::arg (p) * trench::core::kP2kDatumHz / 6.283185307179586, std::abs (p));
        std::printf ("  error %.3f dB\n", fit.errorDb);
        bool found = fit.poles.size() == 10;
        for (size_t r = 0; r + 1 < hs::kRows; ++r)
        {
            const auto g = trench::core::geometry_from_words (klatt[r], trench::core::kP2kDatumHz);
            const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
            if (pole == nullptr) { found = false; continue; }
            double cents = 1.0e9, radius = 1.0e9;
            for (const auto& p : fit.poles)
            {
                if (! (p.imag() > 0.0)) continue;
                const double hz = std::arg (p) * trench::core::kP2kDatumHz / 6.283185307179586;
                if (! (hz > 0.0)) continue;
                const double d = std::abs (1200.0 * std::log2 (hz / pole->hz));
                if (d < cents) { cents = d; radius = std::abs (std::abs (p) - pole->radius); }
            }
            std::printf ("      row %d pole %.1f Hz r %.5f off by %.1f cents, %.5f\n", (int) r + 1, pole->hz, pole->radius, cents, radius);
            found = found && cents < 25.0 && radius < 0.01;
        }
        check (found && fit.errorDb < 0.5, "vector fitting recovers a known cascade's poles and zeros");

        const auto words = hs::fittedWords (fit, trench::core::kP2kDatumHz);
        const auto grid = trench::core::logarithmic_frequency_grid (60.0, 8000.0, 240);
        const auto want = hs::responseDb (klatt, grid), got = hs::responseDb (words, grid);
        double gap = 0.0;
        for (size_t i = 0; i < grid.size(); ++i) gap = std::max (gap, std::abs (got[i] - want[i]));
        std::printf ("      fitted words stray %.2f dB from the cascade between 60 Hz and 8 kHz\n", gap);
        check (gap < 1.5, "fitted words rebuild the cascade within a dB");
    }

    {
        const auto ah = root.getChildFile ("evidence/research-results/emu-sgi-1993/runtime/sounds/vowel_ah.aiff");
        const auto star = hs::fitWav (ah);
        const auto formants = star ? hs::formantsOf (star->words) : std::array<double, 4> {};
        std::printf ("      vowel_ah fit reads %.0f %.0f %.0f %.0f\n", formants[0], formants[1], formants[2], formants[3]);
        check (star && formants[0] > 550.0 && formants[0] < 950.0 && hs::rowOf (star->words[5]).type == hs::RowType::notch && hs::rowHz (star->words[5]) > 11000.0,
               "Peevers's vowel_ah fits with its first formant where an ah sits");

        const auto bellFile = root.getChildFile ("evidence/factory-data/xl1-dsf-aud/Aud Bell 1 C4.wav");
        const auto rung = hs::fitWav (bellFile);
        bool ring = false;
        std::printf ("      Aud Bell 1 C4 fit rows:");
        if (rung)
            for (size_t r = 0; r + 1 < hs::kRows; ++r)
            {
                const auto g = trench::core::geometry_from_words (rung->words[r], trench::core::kP2kDatumHz);
                const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
                if (pole == nullptr) continue;
                std::printf ("  %.0f Hz r %.4f", pole->hz, pole->radius);
                ring = ring || (pole->hz > 740.0 && pole->hz < 830.0 && pole->radius > 0.99);
            }
        std::printf ("\n");
        check (rung && ring, "the bell's ring survives the fit");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int bell = s.starNamed ("Aud Bell 1 C4");
        s.select (bell);
        const size_t before = s.stars.size();
        s.key (key ('F', true, 'f'));
        check (bell >= 0 && s.stars.size() == before + 1 && s.stars.back().name.endsWith (" fit") && s.stars.back().kind == "read" && s.selected == (int) s.stars.size() - 1,
               "Ctrl+F refits the selected read as a new card");
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
        check (s.stars.size() == s.libraryCount + 1 && same (s.stars.back().words, heard) && s.cornerName (3) == hs::formantName (heard), "a corner key during a pair keeps the sound and puts it in that corner");
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
        s.key (key ('S', true, 's')); s.key (key ('L', true, 'l'));
        check (s.source == 2, "L returns to the loop");
        check (! s.setLoop (juce::File ("C:/nowhere/none.wav")) && s.status.startsWith ("cannot read"), "a missing wav says so");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPuck (50.0, 50.0); s.keep();
        s.pin (2, s.starNamed ("e"));
        s.select ((int) s.libraryCount); s.key (key (juce::KeyPress::deleteKey));
        const size_t edits = s.history.size();
        check (s.stars.size() == s.libraryCount && edits == 3, "a capture, a pin and a delete record three edits");
        for (size_t i = 0; i < edits; ++i) s.key (key ('Z', true, 'z'));
        check (s.pinName (2) == "i" && s.stars.size() == s.libraryCount && s.history.empty() && s.future.size() == edits, "undo returns to the boot corners");
        for (size_t i = 0; i < edits; ++i) s.key (key ('Y', true, 'y'));
        check (s.pinName (2) == "e" && s.stars.size() == s.libraryCount, "redo restores the edits");
        s.key (key ('Z', true, 'z'));
        check (s.stars.size() == s.libraryCount + 1 && s.stars.back().kind == "capture", "one undo brings the deleted capture back");
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
        check (s.editing == 0 && s.auditioning == -1 && s.onCorner() && s.editable() && same (s.words, before), "opening a corner puts the pad on it, makes it the target and plays it exactly");
        s.beginRowEdit();
        s.setRow (0, 1, { hs::RowType::peak, 70, 10 });
        const int edited = s.quad.pins[(size_t) hs::Session::kCornerPin[0]];
        bool untouched = true;
        for (size_t r = 0; r < hs::kRows; ++r) if (r != 1) untouched = untouched && shape (s.stars[(size_t) edited].words[r], before[r]);
        check (edited != vowel && s.stars[(size_t) edited].kind == "capture" && s.stars[(size_t) edited].name == hs::formantName (s.stars[(size_t) edited].words) && s.cornerName (0) == s.stars[(size_t) edited].name, "editing a vowel makes a capture named by its formants and puts it in that corner");
        std::printf ("      after a row edit the corner sits at %.3f dB DC\n", dcDb (s.stars[(size_t) edited].words));
        check (untouched && shape (s.stars[(size_t) edited].words[1], hs::rowWords ({ hs::RowType::peak, 70, 10 }, before[1][4])) && levelled (s.stars[(size_t) edited].words) && same (s.stars[(size_t) vowel].words, before), "only the edited row's shape changes, the corner is re-levelled to 0 dB DC, and the vowel is untouched");
        s.setRow (0, 1, { hs::RowType::peak, 71, 10 });
        check (s.quad.pins[(size_t) hs::Session::kCornerPin[0]] == edited && s.stars.size() == s.libraryCount + 1 && same (s.words, s.stars[(size_t) edited].words), "a second edit stays in the same capture and is what plays");
        const auto path = juce::File::createTempFile ("edited.body240");
        s.write (path);
        const auto bytes = bytesOf (path);
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
        check (shape (body.words[2][1], hs::rowWords ({ hs::RowType::peak, 71, 10 }, before[1][4])), "W writes the edited row into the M0 Q1 corner of the 240 bytes");
        s.undo();
        check (s.editing == -1 && s.cornerName (0) == "i" && s.stars.size() == s.libraryCount, "one undo removes the edit and its capture");
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
        bool curve = false;
        for (int y = screen.stage.magnitude.getY(); y < screen.stage.magnitude.getBottom() && ! curve; ++y)
            for (int x = screen.stage.magnitude.getX(); x < screen.stage.magnitude.getRight() && ! curve; ++x)
                curve = image.getPixelAt (x, y).getARGB() == hs::Look::blue.getARGB();
        check (image.getPixelAt (screen.stage.area.getX() - 6, screen.stage.area.getCentreY()) == hs::Look::grid, "the four areas are ruled off from each other");
        check (written && file.getSize() > 4000 && image.getWidth() == 1120 && image.getPixelAt (4, 4) == hs::Look::ground && image.getPixelAt (screen.palette.chart.getX() + 2, screen.palette.chart.getBottom() - 3) == hs::Look::panel && curve, "the screen renders to artifacts/shots/headspace.png without a window: ground, white axes and the blue curve of what plays");
        check (juce::Desktop::getInstance().getNumComponents() == 0, "no window was opened");
        check (screen.body.area.toFloat().contains (screen.body.puckPoint()), "the puck sits on the body");
        const auto f = screen.palette.formantsAt (screen.palette.chartPoint (700.0, 1100.0).toInt());
        check (std::abs (f.first - 700.0) < 6.0 && std::abs (f.second - 1100.0) < 10.0 && screen.palette.chart.toFloat().contains (screen.palette.chartPoint (hs::kSchwaF1, hs::kSchwaF2)), "the vowel space maps F1 and F2 both ways and schwa sits inside it");
        check (screen.palette.pointAt (screen.palette.chartPoint (hs::kSchwaF1, hs::kSchwaF2).toInt()) == s.starNamed (ipa ("\xc9\x99")), "schwa is found at the chart's origin");
        {
            const auto at = [&] (const juce::String& name) { const auto ff = hs::formantsOf (s.stars[(size_t) s.starNamed (name)].words); return screen.palette.chartPoint (ff[0], ff[1]); };
            const auto pi = at ("i"), pu = at ("u"), pa = at (ipa ("\xc9\x91"));
            check (pi.x < pu.x && pi.x < pa.x && pi.y < pa.y && pu.y < pa.y && pu.x > screen.palette.chart.getCentreX(), "the vowel chart is the standard one: i top-left, u top-right, a at the bottom");
            const int bell = s.starNamed ("Aud Bell 1 C4");
            const auto fv = hs::formantsOf (s.stars[(size_t) bell].words);
            check (screen.palette.pointAt (screen.palette.chartPoint (fv[0], fv[1]).toInt()) != bell, "reads are never on the vowel chart");
        }
        const auto u = hs::formantsOf (s.stars[(size_t) s.starNamed ("u")].words), iy = hs::formantsOf (s.stars[(size_t) s.starNamed ("i")].words);
        std::printf ("      u reads %.0f %.0f, i reads %.0f %.0f\n", u[0], u[1], iy[0], iy[1]);
        check (u[0] > 250.0 && u[0] < 400.0 && u[1] > 1150.0 && u[1] < 1350.0 && iy[1] > 1900.0, "the Klatt vowels sit at their own Table II F1 and F2, the wide tilt row is not a formant");
        {
            const auto from = screen.palette.chartPoint (iy[0], iy[1]);
            const auto to = screen.palette.chartPoint (iy[0] * 1.3, iy[1]);
            const auto shift = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
            screen.mouseDown (mouse (screen, from, from, shift));
            screen.mouseDrag (mouse (screen, to, from, shift));
            const auto moved = hs::formantsOf (s.words);
            std::printf ("      i shifted to %.0f %.0f\n", moved[0], moved[1]);
            check (s.inMade() && std::abs (moved[0] / iy[0] - 1.3) < 0.06 && std::abs (moved[1] / iy[1] - 1.3) < 0.06 && s.made.name.startsWith ("i "), "Shift-dragging a point in the vowel space transposes it live, every formant by the same ratio");
            screen.mouseUp (mouse (screen, to, from, shift));
            check (s.inMade() && s.stars.size() == s.libraryCount, "letting go leaves the transposed sound playing, unkept");
        }
        auto fits = [&] {
            const auto& m = screen.mother.area; const auto& st = screen.stage.area; const auto& pl = screen.palette.area; const auto& bt = screen.bottom;
            bool ok = m.getX() < st.getX() && m.getY() == st.getY() && m.getWidth() > st.getWidth() && m.getHeight() > pl.getHeight()
                && ! m.intersects (st) && ! m.intersects (pl) && ! st.intersects (bt) && ! pl.intersects (bt) && pl.getY() > m.getBottom() && bt.getX() > pl.getRight();
            ok = ok && m.contains (screen.mother.cell[0]) && m.contains (screen.mother.cell[1]) && ! screen.mother.cell[0].intersects (screen.mother.cell[1])
                && screen.mother.cell[0].getWidth() >= 200 && m.contains (screen.mother.octavesKey) && m.contains (screen.mother.bakeKey)
                && ! screen.mother.octavesKey.intersects (screen.mother.bakeKey);
            for (int i = 0; i < 2; ++i) ok = ok && screen.mother.cell[(size_t) i].contains (screen.mother.plot[(size_t) i]);
            for (int i = 0; i < 3; ++i)
                ok = ok && m.contains (screen.mother.rail[(size_t) i]) && screen.mother.rail[(size_t) i].getY() > screen.mother.cell[0].getBottom()
                    && ! screen.mother.rail[(size_t) i].intersects (screen.mother.octavesKey) && ! screen.mother.rail[(size_t) i].intersects (screen.mother.bakeKey)
                    && (i == 0 || ! screen.mother.rail[(size_t) i].intersects (screen.mother.rail[(size_t) (i - 1)]));
            ok = ok && st.contains (screen.stage.magnitude) && screen.stage.magnitude.getWidth() * 2 == screen.stage.magnitude.getHeight() * 3 && st.contains (screen.stage.carveKey) && screen.stage.magnitude.getWidth() >= 300;
            ok = ok && pl.contains (screen.palette.chart) && pl.contains (screen.palette.picker) && ! screen.palette.chart.intersects (screen.palette.picker) && pl.contains (screen.palette.dropZone);
            ok = ok && bt.contains (screen.body.area) && bt.contains (screen.engine.area) && bt.contains (screen.keyboard.area) && ! screen.body.area.intersects (screen.engine.area)
                && screen.keyboard.area.getY() >= screen.body.area.getBottom() && screen.keyboard.area.getY() >= screen.engine.area.getBottom()
                && screen.keyboard.area.getHeight() == 44 && screen.keyboard.area.getWidth() == bt.getWidth() && screen.keyboard.area.getBottom() == bt.getBottom()
                && screen.engine.area.contains (screen.engine.label) && screen.engine.area.contains (screen.engine.keys[5])
                && screen.engine.keys[4].getRight() < screen.engine.keys[5].getX() && ! screen.engine.label.intersects (screen.engine.keys[0]);
            ok = ok && screen.body.box[0].getRight() == screen.body.box[1].getX() && screen.body.box[0].getBottom() == screen.body.box[2].getY();
            return ok;
        };
        check (fits(), "one screen: the mother is the largest quarter top-left, the stage top-right, the palette below the mother, the body, engine and keyboard bottom-right, nothing overlapping");
        check (screen.body.cornerAt (screen.body.plot[2].getCentre()) == 2 && screen.body.cornerAt (screen.palette.chart.getCentre()) < 0, "a drop on the third cell lands in corner C");
        screen.setSize (1000, 640);
        check (fits(), "the one screen still fits at the smallest window");
        screen.setSize (1120, 700);
        {
            s.setPuck (40.0, 30.0);
            s.setPair (0, s.starNamed ("i")); s.setPair (1, s.starNamed ("u"));
            screen.mouseDown (mouse (screen, screen.mother.plot[0].getCentre().toFloat(), screen.mother.plot[0].getCentre().toFloat()));
            screen.mouseUp (mouse (screen, screen.mother.plot[0].getCentre().toFloat(), screen.mother.plot[0].getCentre().toFloat()));
            std::printf ("      endpoint: auditioning %d pairA %d (%s) pairB %d anchorTarget %d i=%d\n", s.auditioning, s.pairA, s.pairA >= 0 ? s.stars[(size_t) s.pairA].name.toRawUTF8() : "-", s.pairB, s.anchorTarget, s.starNamed ("i"));
            check (s.auditioning == s.pairA && s.pairA == s.starNamed ("i"), "pressing an endpoint plays it exactly");
            auto along = [&] (int which) { const auto r = screen.mother.rail[(size_t) which]; return juce::Point<float> ((float) (r.getX() + r.getWidth() * 0.7), (float) r.getCentreY()); };
            auto press = [&] (juce::Point<float> at) { screen.mouseDown (mouse (screen, at, at)); screen.mouseUp (mouse (screen, at, at)); };
            press (along (0));
            check (s.inPair() && std::abs (s.pairT - 0.7) < 0.02, "pressing the rail sweeps the pair there");
            screen.mouseDown (mouse (screen, screen.palette.card (0).getCentre().toFloat(), screen.palette.card (0).getCentre().toFloat()));
            screen.mouseDrag (mouse (screen, screen.mother.plot[1].getCentre().toFloat(), screen.palette.card (0).getCentre().toFloat()));
            screen.mouseUp (mouse (screen, screen.mother.plot[1].getCentre().toFloat(), screen.palette.card (0).getCentre().toFloat()));
            check (s.pairB == screen.palette.cards()[0] && s.pairA == s.starNamed ("i") && s.inPair(), "dropping a card on the right endpoint replaces it, keeps the left one, and the sweep goes on");
            screen.mouseDown (mouse (screen, screen.mother.plot[0].getCentre().toFloat(), screen.mother.plot[0].getCentre().toFloat()));
            screen.mouseUp (mouse (screen, screen.mother.plot[0].getCentre().toFloat(), screen.mother.plot[0].getCentre().toFloat()));
            check (s.anchorTarget == 0 && s.editable() && same (screen.stage.words(), s.stars[(size_t) s.pairA].words), "a click on an anchor makes it the target and the stage shows its words with handles");
            {
                const int libraryI = s.starNamed ("i");
                const auto libraryWords = s.stars[(size_t) libraryI].words;
                const auto start = screen.stage.peakPoint (0).roundToInt().toFloat();
                screen.mouseDown (mouse (screen, start, start));
                screen.mouseDrag (mouse (screen, start.translated (16.0f, -12.0f), start));
                screen.mouseUp (mouse (screen, start.translated (16.0f, -12.0f), start));
                check (s.pairA >= (int) s.libraryCount && s.stars[(size_t) s.pairA].kind == "capture" && same (s.stars[(size_t) libraryI].words, libraryWords) && s.anchorTarget == 0, "a drag on the stage while an anchor is the target edits a capture in the anchor's place and leaves the library card untouched");
            }
            s.setPuck (0.0, 100.0);
            check (s.editable() && s.working == 0 && s.anchorTarget < 0 && screen.stage.peakAt (screen.stage.peakPoint (0).roundToInt()) >= 0, "the pad on corner A makes A the target with handles");
            {
                const auto cornerBefore = hs::cornersOf (s.quad, s.stars)[(size_t) hs::Session::kCornerPin[0]];
                s.setProbe (0.7, 0.0, 1.0);
                const auto cornerAfter = hs::cornersOf (s.quad, s.stars)[(size_t) hs::Session::kCornerPin[0]];
                check (s.auditioning == -1 && ! same (cornerAfter, cornerBefore) && same (cornerAfter, hs::motherWordsAt (s.explore(), s.stars)) && same (s.words, cornerAfter), "with the pad on a corner the rails write into that corner and it is what plays");
            }
            {
                const int uStar = s.starNamed ("u");
                s.setPuck (100.0, 100.0);
                s.placeInTarget (uStar);
                check (s.cornerName (1) == "u" && s.auditioning == -1, "a card placed while the pad is on B becomes B");
            }
            s.setPuck (40.0, 30.0);
            check (! s.editable() && screen.stage.peakAt (screen.stage.peakPoint (0).roundToInt()) == -1 && same (screen.stage.words(), s.words), "between corners the stage shows the lerp and the handles are off");
            check (screen.mother.rail[0].getX() == screen.mother.cell[0].getX(), "the rails sit flush under the anchors");
            {
                s.setPuck (0.0, 100.0);
                screen.mouseDown (mouse (screen, screen.stage.modeKey.getCentre().toFloat(), screen.stage.modeKey.getCentre().toFloat()));
                screen.mouseUp (mouse (screen, screen.stage.modeKey.getCentre().toFloat(), screen.stage.modeKey.getCentre().toFloat()));
                const auto pp = screen.stage.peakPoint (0).roundToInt();
                check (screen.stage.zerosMode && screen.stage.peakAt (pp) == -1, "ZEROS mode: the pole handles no longer take the click");
                const auto wordsBefore = screen.stage.words();
                const juce::Point<float> at ((float) pp.x, (float) (screen.stage.magnitude.getBottom() - 30));
                screen.mouseDown (mouse (screen, at, at));
                screen.mouseUp (mouse (screen, at, at));
                bool woke = false;
                for (size_t r = 0; r + 1 < hs::kRows; ++r) woke = woke || (! hs::sectionOf (wordsBefore[r]).zero && hs::sectionOf (screen.stage.words()[r]).zero);
                check (woke, "ZEROS mode: a click on the plot wakes a zero on a row that had none");
                screen.mouseDown (mouse (screen, screen.stage.modeKey.getCentre().toFloat(), screen.stage.modeKey.getCentre().toFloat()));
                screen.mouseUp (mouse (screen, screen.stage.modeKey.getCentre().toFloat(), screen.stage.modeKey.getCentre().toFloat()));
                check (! screen.stage.zerosMode && screen.stage.peakAt (screen.stage.peakPoint (0).roundToInt()) >= 0, "POLES mode again: the pole handles take the click");
                s.setPuck (40.0, 30.0);
            }
            screen.keyPressed (key ('/', false, '/'));
            for (const char ch : { 'b', 'e', 'l' }) screen.keyPressed (key (ch, false, ch));
            check (screen.palette.finding && screen.palette.find == "bel" && ! screen.palette.cards().empty() && s.stars[(size_t) screen.palette.cards()[0]].name.containsIgnoreCase ("Bell"), "slash then letters find cards by name without dragging the list");
            screen.keyPressed (key (juce::KeyPress::returnKey, false, 0));
            check (! screen.palette.finding && s.auditioning >= 0 && s.stars[(size_t) s.auditioning].name.containsIgnoreCase ("Bell"), "Enter places the first card found in the target, here nothing targeted so it plays, and closes the find");
            {
                const auto caret = juce::Point<float> ((float) (screen.mother.tag[0].getRight() - 6), (float) screen.mother.tag[0].getCentreY());
                screen.mouseDown (mouse (screen, caret, caret)); screen.mouseUp (mouse (screen, caret, caret));
                check (screen.palette.finding && s.anchorTarget == 0, "the anchor's caret makes it the target and opens the find");
                for (const char ch : { '3', '0', '3', ' ', 'o', 'p', 'e', 'n', ' ', 'c', '2' }) screen.keyPressed (key (ch, false, ch));
                screen.keyPressed (key (juce::KeyPress::returnKey, false, 0));
                check (s.pairA == s.starNamed ("303 open C2") && ! screen.palette.finding, "typing a name and Enter puts that card in the targeted anchor");
                const auto caretB = juce::Point<float> ((float) (screen.body.tag[1].getRight() - 6), (float) screen.body.tag[1].getCentreY());
                screen.mouseDown (mouse (screen, caretB, caretB)); screen.mouseUp (mouse (screen, caretB, caretB));
                for (const char ch : { 'b', 'e', 'l', 'l', ' ', '1', ' ', 'c', '4' }) screen.keyPressed (key (ch, false, ch));
                screen.keyPressed (key (juce::KeyPress::returnKey, false, 0));
                check (s.cornerName (1) == "Aud Bell 1 C4" && s.onCorner() && s.working == 1, "a corner's caret targets it, and the found card becomes that corner");
                s.setPair (0, s.starNamed ("i")); s.setPair (1, s.starNamed ("u"));
            }
            press (along (1));
            check (s.inPair() && std::abs (s.frequency - 0.7) < 0.02 && std::abs (s.pairT - 0.7) < 0.02, "pressing the FREQUENCY rail moves the probe there");
            press (along (2));
            check (s.inPair() && std::abs (s.stress - 0.7) < 0.02 && std::abs (s.frequency - 0.7) < 0.02, "pressing the STRESS rail moves the probe there");
            juce::MouseWheelDetails step;
            step.deltaX = 0.0f; step.deltaY = 1.0f; step.isReversed = false; step.isSmooth = false; step.isInertial = false;
            screen.mouseWheelMove (mouse (screen, along (1), along (1)), step);
            check (std::abs (s.octaves - 1.25) < 1e-9, "the wheel over the FREQUENCY rail steps the octaves by a quarter");
            s.setProbe (0.5, 0.5, 0.5);
            const auto mothered = screen.shot();
            const auto motherFile = folder.getChildFile ("headspace_mother.png");
            motherFile.deleteFile();
            juce::FileOutputStream motherOut (motherFile);
            check (motherOut.openedOk() && png.writeImageToStream (mothered, motherOut) && motherFile.getSize() > 4000, "the mother at a live probe renders to artifacts/shots/headspace_mother.png");
            motherOut.flush();
        }
        s.edit (0);
        screen.stage.showHardware = true;
        const auto rawFile = folder.getChildFile ("headspace_raw.png");
        rawFile.deleteFile();
        juce::FileOutputStream rawOut (rawFile);
        check (rawOut.openedOk() && png.writeImageToStream (screen.shot(), rawOut), "the H view renders the raw words over the plot headlessly");
        rawOut.flush();
        screen.stage.showHardware = false;
    }

    {
        hs::Session s (root, tempQuad(), false);
        hs::Screen screen (s);
        s.edit (0);
        s.beginRowEdit();
        s.setRow (0, 0, { hs::RowType::peak, 50, 10 });
        const auto before = s.editWords();
        const auto history = s.history.size();
        const auto start = screen.stage.peakPoint (0).roundToInt().toFloat();
        check (screen.stage.peakAt (start.toInt()) == 0, "a formant handle is hit on the magnitude response");
        screen.mouseDown (mouse (screen, start, start));
        screen.mouseUp (mouse (screen, start, start));
        check (same (s.editWords(), before) && s.history.size() == history, "clicking a peak leaves its exact words and undo history intact");
        const auto end = start.translated (20.0f, -20.0f);
        screen.mouseDown (mouse (screen, start, start));
        screen.mouseDrag (mouse (screen, end, start));
        const auto edited = s.editWords();
        const double oldHz = hs::rowHz (before[0]), newHz = hs::rowHz (edited[0]);
        const double oldR = hs::sectionOf (before[0]).poleRadius, newR = hs::sectionOf (edited[0]).poleRadius;
        const double expectedHz = oldHz * std::pow (1000.0, 20.0 / screen.stage.magnitude.getWidth());
        const double expectedR = 1.0 - (1.0 - oldR) * std::pow (10.0, -(hs::plot::dbAt ((int) end.y, screen.stage.magnitude) - hs::plot::dbAt ((int) start.y, screen.stage.magnitude)) / 20.0);
        std::printf ("      formant drag: hz %.0f -> %.0f (expected %.0f), r %.4f -> %.4f (expected %.4f), zero r %.4f -> %.4f\n", oldHz, newHz, expectedHz, oldR, newR, expectedR, hs::sectionOf (before[0]).zeroRadius, hs::sectionOf (edited[0]).zeroRadius);
        check (newHz > oldHz && newR > oldR && std::abs (12.0 * std::log2 (newHz / expectedHz)) < 1.0 && std::abs (newR - expectedR) < 0.01 && std::abs (hs::sectionOf (edited[0]).zeroRadius - hs::sectionOf (before[0]).zeroRadius) < 1e-3,
            "dragging a formant right and up sets its angle from the x axis and its radius from the y axis, nothing solved, the zero untouched");
        screen.mouseDrag (mouse (screen, end.translated (4.0f, -4.0f), start));
        screen.mouseUp (mouse (screen, end, start));
        bool untouched = s.editWords()[0][0] == before[0][0] && s.editWords()[0][1] == before[0][1];
        for (int r = 1; r < hs::kRows; ++r) untouched = untouched && shape (s.editWords()[(size_t) r], before[(size_t) r]);
        check (untouched && levelled (s.editWords()) && s.history.size() == history + 1 && same (s.words, s.editWords()), "one peak gesture preserves the other five sections and the row's zero, re-levels the corner, plays the edit, and records one undo");
        s.undo();
        if (s.editing < 0) s.edit (0);
        check (same (s.editWords(), before), "undo restores the exact words from before the peak gesture");
        check (! screen.stage.showHardware, "raw words are hidden by default");
        const auto p0 = screen.stage.peakPoint (0).roundToInt().toFloat();
        const auto s0 = hs::sectionOf (s.editWords()[0]);
        juce::MouseWheelDetails wheel;
        wheel.deltaX = 0.0f; wheel.deltaY = 1.0f; wheel.isReversed = false; wheel.isSmooth = false; wheel.isInertial = false;
        screen.mouseWheelMove (mouse (screen, p0, p0), wheel);
        const auto s1 = hs::sectionOf (s.editWords()[0]);
        check (hs::widthSt (s1.poleHz, s1.poleRadius) < hs::widthSt (s0.poleHz, s0.poleRadius) && std::abs (s1.poleHz / s0.poleHz - 1.0) < 0.01, "the wheel over a pole narrows it and leaves its pitch");
        check (juce::Desktop::getInstance().getNumComponents() == 0, "peak gestures stay headless");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int i = s.starNamed ("i"), u = s.starNamed ("u"), bell = s.starNamed ("Aud Bell 1 C4");
        auto between = [&] (int a, int b, double t) { hs::Corners c { s.stars[(size_t) a].words, s.stars[(size_t) b].words, s.stars[(size_t) a].words, s.stars[(size_t) b].words }; return hs::lerp (c, t, 0.0); };
        check (s.inPair() && s.pairA >= 0 && s.pairB >= 0 && s.stars[(size_t) s.pairA].kind == "read" && s.stars[(size_t) s.pairB].kind == "read" && s.stars[(size_t) s.pairA].body != s.stars[(size_t) s.pairB].body && same (s.words, between (s.pairA, s.pairB, 0.5)), "a fresh session plays the sweep between two reads of different families");
        s.setPair (0, i); s.setPair (1, u);
        s.setPair (1, bell);
        check (s.inPair() && s.pairA == i && s.pairB == bell && std::abs (s.pairT - 0.5) < 1e-9 && same (s.words, between (i, bell, 0.5)), "replacing the right endpoint keeps the left one and keeps sounding at the same place");
        s.audio.onWheel (0.3);
        check (s.inPair() && std::abs (s.pairT - 0.3) < 1e-9 && same (s.words, between (i, bell, 0.3)), "the mod wheel sweeps the pair");
        const auto found = s.words;
        s.toCorner (0);
        const auto cornerA = hs::cornersOf (s.quad, s.stars)[(size_t) hs::Session::kCornerPin[0]];
        check (same (cornerA, found) && s.inPair() && std::abs (s.pairT - 0.3) < 1e-9 && same (s.words, found), "copying the sound to A stores those exact words and the sweep goes on");
        s.audio.onWheel (0.8);
        s.setPair (0, u);
        check (same (hs::cornersOf (s.quad, s.stars)[(size_t) hs::Session::kCornerPin[0]], found) && s.pairA == u && s.pairB == bell && s.inPair(), "sweeping on and replacing the left endpoint leave A untouched");
        hs::Session again (root, s.file, false);
        check (again.pairA == again.starNamed ("u") && again.pairB == again.starNamed ("Aud Bell 1 C4") && std::abs (again.pairT - 0.8) < 1e-9 && again.inPair(), "the exploration is saved with the session and plays again on reopening");
        s.setPuck (50.0, 50.0);
        const auto path = juce::File::createTempFile ("loop.body240");
        s.write (path);
        const auto bytes = bytesOf (path);
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
        bool reproduced = bytes.size() == 240;
        for (int m = 0; m <= 100 && reproduced; m += 50)
            for (int q = 0; q <= 100 && reproduced; q += 50)
            {
                s.setPuck ((double) m, (double) q);
                const auto cw = body.interpolate_words (m / 100.0f, q / 100.0f, 0.0f);
                for (size_t row = 0; row < hs::kRows; ++row) reproduced = reproduced && cw[row] == s.words[row];
            }
        check (reproduced, "the canonical grid writes 240 bytes that reload through the plugin's lerp to the pad's own sound");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const int i = s.starNamed ("i"), u = s.starNamed ("u");
        auto anchor = [&] (int k) { return s.stars[(size_t) k].words; };
        s.setPair (0, s.starNamed ("i")); s.setPair (1, s.starNamed ("u"));
        auto probe = [&] (double m, double f, double t) { s.setProbe (m, f, t); return s.words; };
        check (same (probe (0.0, 0.0, 1.0), anchor (i)), "at MORPH 0 the words are anchor A verbatim");
        check (same (probe (1.0, 0.0, 1.0), anchor (u)), "at MORPH 1 the words are anchor B verbatim");
        check (same (probe (0.0, 1.0, 1.0), hs::transposed (anchor (i), 2.0)), "FREQUENCY 1 plays the anchor transposed by the octaves");
        const auto rest = probe (0.0, 0.0, 0.0);
        const auto tube = hs::formantsOf (rest);
        std::printf ("      the relaxed i reads %.0f %.0f %.0f %.0f\n", tube[0], tube[1], tube[2], tube[3]);
        check (same (rest, hs::relaxed (anchor (i))) && std::abs (tube[0] / 500.0 - 1.0) < 0.01 && std::abs (tube[1] / 1500.0 - 1.0) < 0.01, "STRESS 0 plays the relaxed anchor with its poles on the neutral tube");
        s.setProbe (0.3, 0.6, 0.25);
        const auto chip = hs::motherBodyOf (s.explore(), s.stars).interpolate_words (0.3f, 0.6f, 0.25f);
        bool three = s.inPair() && s.sounding;
        for (size_t row = 0; row < hs::kRows; ++row) three = three && chip[row] == s.words[row];
        check (three, "the probe plays the chip's three-axis lerp of the eight made corners");
        s.setPair (0, s.starNamed ("i")); s.setPair (1, s.starNamed ("u"));
        s.setProbe (0.2, 0.4, 0.6);
        s.audio.onWheel (0.8);
        check (s.inPair() && std::abs (s.pairT - 0.8) < 1e-9 && std::abs (s.frequency - 0.4) < 1e-9 && std::abs (s.stress - 0.6) < 1e-9, "the mod wheel rides MORPH and leaves FREQUENCY and STRESS where they were");
        check (s.playingLabel == "i > u  80  freq 40  stress 60", "the label says frequency and stress when they are off their rest");
        s.key (key ('Z', false, 'z'));
        const int played = s.heldNote;
        s.key (key ('M', false, 'm'));
        check (played == 48 && s.heldNote == 59 && s.note == 59, "the Z row plays notes from C3, Z is C and M is B");
        s.keyNoteOff (59);
        s.key (key (juce::KeyPress::pageUpKey, false, 0));
        s.key (key ('C', false, 'c'));
        check (s.keyOctave == 60 && s.heldNote == 64, "Page Up lifts the key row an octave and C is now E4");
        s.keyNoteOff (64);
        s.key (key ('S', true, 's'));
        check (s.source == 0 && s.heldNote == -1, "Ctrl+S is the saw, plain S is a note");
        const auto heard = s.words;
        s.key (key ('1', false, '1'));
        check (s.stars.size() == s.libraryCount + 1 && s.stars.back().parentA == "i" && same (hs::cornersOf (s.quad, s.stars)[(size_t) hs::Session::kCornerPin[0]], heard)
               && s.inPair() && std::abs (s.pairT - 0.8) < 1e-9 && std::abs (s.frequency - 0.4) < 1e-9 && std::abs (s.stress - 0.6) < 1e-9, "a corner key at the probe keeps the heard words and puts them in that corner");
        s.setProbe (0.5, 0.5, 0.75);
        const auto plane = hs::motherBodyOf (s.explore(), s.stars);
        bool baked = s.bake() && s.quad.morph == 50.0 && s.quad.q == 50.0;
        for (int pin = 0; pin < 4 && baked; ++pin)
        {
            const auto want = plane.interpolate_words ((float) (pin & 1), (float) ((pin >> 1) & 1), 0.75f);
            const auto got = hs::cornersOf (s.quad, s.stars)[(size_t) pin];
            for (size_t row = 0; row < hs::kRows; ++row) baked = baked && want[row] == got[row];
        }
        check (baked, "bake puts the plane at this stress into the body at the probe's MORPH and FREQUENCY");
        s.setOctaves (-0.5);
        hs::Session again (root, s.file, false);
        check (again.pairA == again.starNamed ("i") && again.pairB == again.starNamed ("u") && std::abs (again.pairT - 0.5) < 1e-9 && std::abs (again.frequency - 0.5) < 1e-9
               && std::abs (again.stress - 0.75) < 1e-9 && std::abs (again.octaves + 0.5) < 1e-9 && again.inPair(), "the mother is saved with the session and plays again on reopening");
    }

    {
        hs::Audio audio;
        audio.prepare (44100.0);
        hs::Session s (root, tempQuad(), false);
        std::array<std::uint16_t, 30> flat {};
        for (size_t r = 0; r < hs::kRows; ++r) for (size_t k = 0; k < hs::kWords; ++k) flat[r * 5 + k] = s.stars[(size_t) s.starNamed ("i")].words[r][k];
        audio.publish (flat);
        audio.setSource (0);
        std::vector<float> left (512), right (512);
        float* outs[2] = { left.data(), right.data() };
        auto rms = [&] { audio.audioDeviceIOCallbackWithContext (nullptr, 0, outs, 2, 512, {}); double e = 0.0; for (float v : left) e += v * v; return std::sqrt (e / 512.0); };
        auto run = [&] (int blocks) { double last = 0.0; for (int i = 0; i < blocks; ++i) last = rms(); return last; };
        auto peak = [&] (int blocks) { double top = 0.0; for (int i = 0; i < blocks; ++i) top = std::max (top, rms()); return top; };
        const double silent = peak (8);
        audio.noteOn (60, 1.0f);
        const double one = peak (20);
        audio.noteOn (67, 1.0f);
        const double two = peak (20);
        double hzLow = 0.0, hzHigh = 0.0;
        for (int v = 0; v < hs::Audio::kVoices; ++v) { const double hz = audio.voiceHz (v); if (std::abs (hz - 261.63) < 1.0) hzLow = hz; if (std::abs (hz - 392.0) < 1.0) hzHigh = hz; }
        check (silent == 0.0 && one > 0.005 && two > 0.005 && audio.activeVoices() == 2 && hzLow > 0.0 && hzHigh > 0.0, "two held keys are two voices at their own pitches through the one cascade");
        audio.noteOff (60);
        const double stillHeld = run (10);
        check (audio.activeVoices() >= 1 && stillHeld > 0.005, "releasing one key leaves the other sounding");
        audio.noteOff (67);
        const double tail = run (120);
        check (tail < two * 0.02 && audio.activeVoices() == 0, "releasing the last key rings the cascade down to silence");
        audio.noteOn (60, 0.25f);
        const double soft = peak (20);
        audio.noteOff (60);
        run (60);
        audio.noteOn (60, 1.0f);
        const double loud = peak (20);
        audio.noteOff (60);
        run (60);
        std::printf ("      velocity: soft %.4f, loud %.4f\n", soft, loud);
        check (soft > 0.001 && soft < loud * 0.5, "velocity scales the strike and the held level");
        audio.sustain (true);
        audio.noteOn (64, 1.0f);
        run (10);
        audio.noteOff (64);
        const double pedalled = run (30);
        audio.sustain (false);
        const double lifted = run (120);
        check (pedalled > 0.005 && lifted < pedalled * 0.02, "the sustain pedal holds a released key and lifting it lets the note go");
        audio.noteOn (69, 1.0f);
        audio.bend (2.0);
        run (2);
        bool bent = false;
        for (int v = 0; v < hs::Audio::kVoices; ++v) bent = bent || std::abs (audio.voiceHz (v) - 493.88) < 1.0;
        check (bent, "pitch bend of two semitones takes A4 to B4");
        audio.bend (0.0);
        audio.noteOff (69);
        run (60);
        audio.setPlaying (true);
        check (run (20) > 0.005, "Space still drones without a key");
        audio.setPlaying (false);
        run (120);
        audio.setSource (3);
        audio.noteOn (60, 1.0f);
        const double plucked = peak (2);
        const double rung = run (20);
        audio.noteOff (60);
        std::printf ("      pluck: strike %.4f, 230 ms later %.5f\n", plucked, rung);
        check (plucked > 0.01 && rung < plucked * 0.05, "pluck strikes a burst into the cascade and lets it ring without a tone");
        run (60);
        auto take = [&] (int blocks) { std::vector<float> out; for (int i = 0; i < blocks; ++i) { rms(); out.insert (out.end(), left.begin(), left.end()); } return out; };
        auto differ = [] (const std::vector<float>& a, const std::vector<float>& b) { double d = 0.0; for (size_t i = 0; i < a.size(); ++i) d = std::max (d, (double) std::abs (a[i] - b[i])); return d; };
        auto strike = [&] (int midi) { audio.prepare (44100.0); audio.publish (flat); run (4); audio.noteOn (midi, 1.0f); const auto out = take (2); audio.noteOff (midi); return out; };
        const double pluckDiff = differ (strike (60), strike (60));
        audio.setSource (0);
        const auto sawOne = strike (62);
        const double sawDiff = differ (sawOne, strike (62));
        double sawPeak = 0.0;
        for (float v : sawOne) sawPeak = std::max (sawPeak, (double) std::abs (v));
        std::printf ("      strike twice: pluck differs %.4f, saw differs %.6f, saw peak %.3f\n", pluckDiff, sawDiff, sawPeak);
        check (pluckDiff > 0.005 && sawDiff < 1e-3 && sawPeak > 0.005, "saw notes carry no burst of their own: two strikes of a note are one signal, two plucks are not");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPair (0, s.starNamed ("i")); s.setPair (1, s.starNamed ("u")); s.setProbe (0.5, 0.0, 1.0);
        check (s.playingLabel == "i > u  50", "a fresh session says the sweep between its two endpoints plays");
        s.select (s.starNamed ("i"));
        check (s.playingLabel == "i", "a clicked card says its name");
        s.setMade (700.0, 1100.0);
        check (s.playingLabel == "made 700/1100", "a made vowel says it was made");
        s.setTransposed (s.starNamed ("i"), 465.0);
        check (s.playingLabel.startsWith ("i moved to 4"), "a transposed vowel says where it moved from and to");
        s.morphPair (s.starNamed ("i"), s.starNamed ("u"), 0.23);
        check (s.playingLabel == "i > u  23", "a pair says both names and the position");
        s.edit (1);
        check (s.playingLabel == "B  u", "an open corner says its letter and name");
        s.setPuck (40.0, 30.0);
        check (s.playingLabel == "pad 40 30", "the pad says MORPH and Q");
    }

    {
        hs::Audio audio;
        audio.prepare (44100.0);
        hs::Session s (root, tempQuad(), false);
        std::array<std::uint16_t, 30> flat {};
        for (size_t r = 0; r < hs::kRows; ++r) for (size_t k = 0; k < hs::kWords; ++k) flat[r * 5 + k] = s.stars[(size_t) s.starNamed ("i")].words[r][k];
        audio.publish (flat);
        audio.setSource (0);
        audio.noteOn (60, 1.0f);
        std::vector<float> left (512), right (512);
        float* outs[2] = { left.data(), right.data() };
        for (int i = 0; i < 20; ++i) audio.audioDeviceIOCallbackWithContext (nullptr, 0, outs, 2, 512, {});
        std::vector<float> after (16384, 0.0f), before (16384, 0.0f);
        const int tapped = audio.pull (after.data(), (int) after.size());
        const int source = audio.pullInput (before.data(), (int) before.size());
        double sourceEnergy = 0.0, filteredEnergy = 0.0, apart = 0.0;
        for (int i = 0; i < std::min (tapped, source); ++i)
        {
            sourceEnergy += (double) before[(size_t) i] * (double) before[(size_t) i];
            filteredEnergy += (double) after[(size_t) i] * (double) after[(size_t) i];
            apart = std::max (apart, (double) std::abs (before[(size_t) i] - after[(size_t) i]));
        }
        std::printf ("      taps: %d input, %d output, source energy %.2f, filtered energy %.2f, largest difference %.4f\n", source, tapped, sourceEnergy, filteredEnergy, apart);
        check (tapped == source && source >= 10240 && sourceEnergy > 1.0 && filteredEnergy > 0.0 && apart > 0.01, "the input tap carries the source before the filter and the output tap after it");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPuck (40.0, 30.0);
        hs::Screen screen (s);
        screen.setSize (1120, 700);
        const auto folder = root.getChildFile ("native/workstation/artifacts/shots");
        folder.createDirectory();
        juce::PNGImageFormat png;
        auto has = [] (const juce::Image& image, juce::Rectangle<int> r, juce::Colour colour) {
            for (int y = r.getY(); y < r.getBottom(); ++y)
                for (int x = r.getX(); x < r.getRight(); ++x)
                    if (image.getPixelAt (x, y) == colour) return true;
            return false;
        };
        auto brightest = [] (const juce::Image& image, juce::Rectangle<int> r) {
            int top = 0;
            for (int y = r.getY(); y < r.getBottom(); ++y)
                for (int x = r.getX(); x < r.getRight(); ++x)
                    top = std::max (top, (int) image.getPixelAt (x, y).getRed());
            return top;
        };
        const auto quiet = screen.shot();
        std::vector<float> saw (8192, 0.0f), quieter (8192, 0.0f);
        double phase = 0.0;
        for (int i = 0; i < 8192; ++i)
        {
            phase += 220.0 / 44100.0;
            phase -= std::floor (phase);
            saw[(size_t) i] = (float) ((2.0 * phase - 1.0) * 0.4);
            quieter[(size_t) i] = saw[(size_t) i] * 0.25f;
        }
        s.noteOn (60);
        screen.engine.feed (saw.data(), quieter.data(), 8192);
        const auto playing = screen.shot();
        check (! has (quiet, screen.stage.magnitude, hs::Look::ink) && has (playing, screen.stage.magnitude, hs::Look::ink),
            "the stage draws the output's spectrum over the hero curve while a key is held");
        const auto playingFile = folder.getChildFile ("headspace_playing.png");
        playingFile.deleteFile();
        juce::FileOutputStream playingOut (playingFile);
        const bool wrote = playingOut.openedOk() && png.writeImageToStream (playing, playingOut);
        playingOut.flush();
        check (wrote && playingFile.existsAsFile() && playingFile.getSize() > 4000 && has (playing, screen.stage.magnitude, hs::Look::ink),
            "the played screen renders to artifacts/shots/headspace_playing.png with the live spectrum on the stage");
        screen.engine.silenceFor (400);
        check (! screen.engine.live() && ! has (screen.shot(), screen.stage.magnitude, hs::Look::ink), "the live spectrum fades when nothing sounds");
        s.setSource (0);
        const auto sawShot = screen.shot();
        const int sawLit = brightest (sawShot, screen.engine.keys[2]), noiseDim = brightest (sawShot, screen.engine.keys[3]);
        s.setSource (1);
        const auto noiseShot = screen.shot();
        const int sawDim = brightest (noiseShot, screen.engine.keys[2]), noiseLit = brightest (noiseShot, screen.engine.keys[3]);
        std::printf ("      sources: SAW %d lit %d dim, NOISE %d lit %d dim, ink %d, dim %d\n", sawLit, sawDim, noiseLit, noiseDim, (int) hs::Look::ink.getRed(), (int) hs::Look::dim.getRed());
        check (has (sawShot, screen.engine.keys[2], hs::Look::ink) && ! has (sawShot, screen.engine.keys[3], hs::Look::ink)
                && has (noiseShot, screen.engine.keys[3], hs::Look::ink) && ! has (noiseShot, screen.engine.keys[2], hs::Look::ink)
                && sawLit == (int) hs::Look::ink.getRed() && noiseDim == (int) hs::Look::dim.getRed(),
            "the sources are words and the active one is ink");
    }

    {
        hs::Session s (root, tempQuad(), false);
        hs::Screen screen (s);
        s.setProbe (0.5, 0.0, 1.0);
        const auto heard = s.heard;
        const auto held = s.stars.size();
        const auto from = screen.engine.plot.getCentre().toFloat();
        const auto to = screen.body.plot[1].getCentre().toFloat();
        screen.mouseDown (mouse (screen, from, from));
        screen.mouseDrag (mouse (screen, to, from));
        screen.mouseUp (mouse (screen, to, from));
        const auto corners = hs::cornersOf (s.quad, s.stars);
        check (same (corners[(size_t) hs::Session::kCornerPin[1]], heard) && s.stars.size() == held + 1 && s.stars.back().kind == "capture" && s.cornerName (1) == s.stars.back().name,
            "dragging the sound's name onto corner B copies the heard words there");
    }

    {
        hs::Session s (root, tempQuad(), false);
        hs::Screen screen (s);
        const auto rule = screen.shot();
        const auto strip = screen.keyboard.area;
        bool filled = false;
        for (int y = strip.getY(); y < strip.getBottom(); ++y)
            for (int x = strip.getX(); x < strip.getRight(); ++x)
                filled = filled || rule.getPixelAt (x, y) == hs::Look::text;
        s.noteOn (60);
        const auto sounding = screen.shot();
        int bar = 0;
        for (int y = strip.getY(); y < strip.getBottom(); ++y)
            for (int x = strip.getX(); x < strip.getRight(); ++x)
                if (sounding.getPixelAt (x, y) == hs::Look::ink) ++bar;
        std::printf ("      keyboard %d px tall, %d ink pixels while C4 sounds\n", strip.getHeight(), bar);
        check (! filled && bar > 0 && strip.getHeight() == 44 && screen.keyboard.noteAt (screen.keyboard.pianoKey (61).getCentre()) == 61 && screen.keyboard.noteAt (screen.keyboard.pianoKey (60).getCentre()) == 60,
            "the keyboard is a rule of keys");
    }

    {
        const auto file = tempQuad();
        {
            const auto first = std::make_unique<hs::Session> (root, file, false);
            first->setRoute (0, true, 0.5);
        }
        const auto owned = std::make_unique<hs::Session> (root, file, false);
        auto& s = *owned;
        check (s.keyToFrequency.on && std::abs (s.keyToFrequency.depth - 0.5) < 1.0e-9 && ! s.velocityToStress.on && s.wheelToMorph.on,
            "the patch is saved with the session");

        s.setPuck (40.0, 30.0);
        s.setPair (0, s.starNamed ("i"));
        s.setPair (1, s.starNamed ("u"));
        s.setRoute (0, true, 1.0);
        s.playedNote (60, 0.8f);
        std::printf ("      C4 played: frequency %.4f stress %.4f pair %d\n", s.frequency, s.stress, (int) s.inPair());
        check (std::abs (s.frequency - 0.5) < 1.0e-9 && s.inPair() && same (s.words, hs::motherWordsAt (s.explore(), s.stars)),
            "a played key moves FREQUENCY between corners");

        s.setRoute (1, true, 1.0);
        s.playedNote (60, 0.25f);
        const bool soft = std::abs (s.stress - 0.25) < 1.0e-9;
        s.playedNote (60, 1.0f);
        check (soft && std::abs (s.stress - 1.0) < 1.0e-9, "velocity moves STRESS");

        s.setPuck (0.0, 100.0);
        const double heldFrequency = s.frequency, heldStress = s.stress;
        const auto heldCorners = hs::cornersOf (s.quad, s.stars);
        s.playedNote (72, 1.0f);
        check (! s.live() && s.frequency == heldFrequency && s.stress == heldStress && hs::cornersOf (s.quad, s.stars) == heldCorners,
            "the routes sleep when the pad sits on a corner");

        s.setPuck (40.0, 30.0);
        s.setRoute (2, false, 1.0);
        const double heldMorph = s.pairT;
        s.audio.onWheel (0.9);
        check (! s.wheelToMorph.on && s.pairT == heldMorph, "the wheel route can be turned off");

        const auto view = std::make_unique<hs::Screen> (s);
        auto& screen = *view;
        s.setRoute (0, true, 1.0);
        const auto image = screen.shot();
        int top = 0;
        for (int y = screen.engine.routes[0].getY(); y < screen.engine.routes[0].getBottom(); ++y)
            for (int x = screen.engine.routes[0].getX(); x < screen.engine.routes[0].getRight(); ++x)
                top = std::max (top, (int) image.getPixelAt (x, y).getRed());
        bool placed = true;
        for (const auto& r : screen.engine.routes)
            placed = placed && screen.engine.area.contains (r) && r.getY() >= screen.engine.keys[0].getBottom() && r.getBottom() <= screen.keyboard.area.getY();
        std::printf ("      routes at %d,%d %dx%d, brightest %d, depth box %d wide\n",
            screen.engine.routes[0].getX(), screen.engine.routes[0].getY(), screen.engine.routes[0].getWidth(), screen.engine.routes[0].getHeight(), top, screen.engine.depths[0].getWidth());
        check (placed && top > 200, "the routes render under the words");
    }

    {
        hs::Peevers p;
        std::vector<float> w ((size_t) 256, 0.0f);
        p.win_calc (w.data(), 7, 256);
        const bool hanning = std::abs (w[64] - 0.5f) < 1.0e-6f && std::abs (w[0]) < 1.0e-6f;
        p.win_calc (w.data(), 6, 256);
        const bool hamming = std::abs (w[0] - 0.08f) < 1.0e-6f;
        p.win_calc (w.data(), 1, 256);
        const bool blackman = std::abs (w[128] - 1.0f) < 1.0e-6f;
        check (hanning && hamming && blackman, "Peevers's windows match his coefficients");

        std::vector<float> x ((size_t) 256, 0.0f), fx ((size_t) 512, 0.0f);
        x[0] = 1.0f;
        p.spectrum (x.data(), 256, fx.data(), 256);
        bool level = true;
        for (int i = 0; i < 256; ++i) level = level && std::abs ((double) fx[(size_t) i] - 1.0 / 65536.0) < 1.0e-9;
        check (level, "the FFT of an impulse is flat");

        for (int i = 0; i < 256; ++i) x[(size_t) i] = (float) (1000.0 * std::sin (2.0 * 3.141592653589793 * 16.0 * i / 256.0));
        p.spectrum (x.data(), 256, fx.data(), 256);
        int top = 1;
        for (int i = 1; i < 128; ++i) if (fx[(size_t) i] > fx[(size_t) top]) top = i;
        check (top == 16, "a sine at bin 16 peaks at bin 16");
    }

    {
        hs::Peevers p;
        const double pi = 3.141592653589793, rate = 44100.0, w = 2.0 * pi * 2000.0 / rate, r = 0.98;
        double y1 = 0.0, y2 = 0.0;
        std::uint32_t seed = 2463534242u;
        for (int i = 0; i < 8192; ++i)
        {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            const double e = ((double) seed / 4294967295.0 * 2.0 - 1.0) * 3000.0;
            const double y = e + 2.0 * r * std::cos (w) * y1 - r * r * y2;
            y2 = y1;
            y1 = y;
            p.gal ((float) y);
        }
        std::vector<float> impulse ((size_t) 256, 0.0f), fx ((size_t) 512, 0.0f);
        for (int i = 0; i < 256; ++i) impulse[(size_t) i] = p.lattice (i == 0 ? 32000.0f : 0.0f);
        p.spectrum (impulse.data(), 256, fx.data(), 256);
        p.log_of (fx.data(), 128);
        int top = 1;
        for (int i = 1; i < 128; ++i) if (fx[(size_t) i] > fx[(size_t) top]) top = i;
        const double want = 2000.0 * 256.0 / 44100.0;
        std::printf ("      the LPC-12 envelope peaks at bin %d, the pole sits at %.2f\n", top, want);
        check (std::abs ((double) top - want) <= 1.0, "the Env envelope of a two-pole resonance peaks at the pole within one bin");
    }

    {
        hs::Audio audio;
        audio.prepare (44100.0);
        hs::Session s (root, tempQuad(), false);
        std::array<std::uint16_t, 30> flat {};
        const int i0 = s.starNamed ("i");
        for (size_t r = 0; r < hs::kRows; ++r) for (size_t k = 0; k < hs::kWords; ++k) flat[r * 5 + k] = s.stars[(size_t) i0].words[r][k];
        audio.publish (flat);
        audio.setSource (0);
        audio.noteOn (57, 1.0f);
        std::vector<float> left ((size_t) 512), right ((size_t) 512);
        float* outs[2] = { left.data(), right.data() };
        for (int i = 0; i < 40; ++i) audio.audioDeviceIOCallbackWithContext (nullptr, 0, outs, 2, 512, {});
        audio.noteOff (57);
        hs::Spectrogram spec;
        spec.peevers.setParms (1024, 1024, 512, 7);
        std::vector<float> tap ((size_t) 40 * 512, 0.0f);
        const int got = audio.pull (tap.data(), (int) tap.size());
        for (int i = 0; i < got; ++i) tap[(size_t) i] *= 32767.0f;
        spec.feed (tap.data(), got);
        const double f0 = 440.0 * std::pow (2.0, (57 - 69) / 12.0), spacing = f0 * 1024.0 / 44100.0;
        bool harmonic = spec.frameCount() > 0;
        if (harmonic)
        {
            const auto& newest = spec.latest();
            std::vector<std::pair<float, int>> peaks;
            for (int b = 1; b < 128; ++b) if (newest[(size_t) b] > newest[(size_t) b - 1] && newest[(size_t) b] >= newest[(size_t) b + 1]) peaks.push_back ({ newest[(size_t) b], b });
            std::sort (peaks.begin(), peaks.end(), [] (const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
            harmonic = peaks.size() >= 3;
            for (int k = 0; k < 3 && k < (int) peaks.size(); ++k)
            {
                const double n = std::floor ((double) peaks[(size_t) k].second / spacing + 0.5);
                std::printf ("      peak %d at bin %d, %.0f Hz, harmonic %.0f of %.1f Hz\n", k + 1, peaks[(size_t) k].second, peaks[(size_t) k].second * 44100.0 / 1024.0, n, f0);
                harmonic = harmonic && n >= 1.0 && std::abs ((double) peaks[(size_t) k].second - n * spacing) <= 1.0;
            }
        }
        check (harmonic, "a held saw through the i corner shows its harmonics in the latest frame");
    }

    {
        hs::Spectrogram spec;
        spec.peevers.lpcenv = 1;
        spec.peevers.setParms (1024, 1024, 512, 7);
        const auto ah = root.getChildFile ("evidence/research-results/emu-sgi-1993/runtime/sounds/vowel_ah.aiff");
        const bool read = spec.load (ah);
        double hz = 0.0;
        int first = -1;
        const int half = spec.length / 2 / spec.peevers.stride;
        if (read && spec.frameCount() > 2)
        {
            const auto& middle = spec.frameAt (juce::jlimit (0, spec.frameCount() - 1, half));
            for (int b = 3; b < 400 && first < 0; ++b) if (middle[(size_t) b] > middle[(size_t) b - 1] && middle[(size_t) b] >= middle[(size_t) b + 1]) first = b;
            hz = (double) first * spec.rate / (double) spec.peevers.nfft;
        }
        std::printf ("      vowel_ah.aiff at %.0f Hz, %d frames, first formant at bin %d, %.0f Hz\n", spec.rate, spec.frameCount(), first, hz);
        check (read && first > 0 && hz > 550.0 && hz < 950.0, "Peevers's own vowel_ah.aiff reads with its first formant where an ah sits");
    }

    {
        hs::Spectrogram spec;
        spec.setSize (900, 560);
        const int n = spec.peevers.winsize + 300 * spec.peevers.stride;
        std::vector<float> saw ((size_t) n, 0.0f);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            phase += 110.0 / 44100.0;
            phase -= std::floor (phase);
            saw[(size_t) i] = (float) ((2.0 * phase - 1.0) * 8000.0);
        }
        spec.feed (saw.data(), n);
        const auto image = spec.shot();
        const auto folder = root.getChildFile ("native/workstation/artifacts/shots");
        folder.createDirectory();
        const auto file = folder.getChildFile ("spectrogram.png");
        file.deleteFile();
        juce::PNGImageFormat png;
        juce::FileOutputStream out (file);
        const bool written = out.openedOk() && png.writeImageToStream (image, out);
        out.flush();
        int lit = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                if (image.getPixelAt (x, y) != hs::Look::ground) ++lit;
        const double share = (double) lit / (double) (image.getWidth() * image.getHeight());
        std::printf ("      spectrogram.png %d frames, %.1f%% of pixels drawn\n", spec.frameCount(), share * 100.0);
        check (written && file.getSize() > 4000 && image.getPixelAt (4, 4) == hs::Look::ground && share > 0.05 && juce::Desktop::getInstance().getNumComponents() == 0, "the spectrogram renders to artifacts/shots/spectrogram.png without a window");
    }

    {
        hs::Session s (root, tempQuad(), false);
        const auto names = s.families();
        bool aud = false, three = false;
        for (const auto& n : names) { aud = aud || n == "Aud Bell 1"; three = three || n == "303 open"; }
        std::printf ("      %d families across the two banks, first %s, last %s\n", (int) names.size(),
            names.empty() ? "" : names.front().toRawUTF8(), names.empty() ? "" : names.back().toRawUTF8());
        check (names.size() >= 10 && aud && three, "the banks list their families");

        const bool loaded = s.loadFamily ("Aud Bell 1");
        const auto bank = s.audio.bankNow();
        juce::String carried;
        if (bank != nullptr) for (const auto& sample : *bank) carried += juce::String (sample.midi) + " ";
        std::printf ("      Aud Bell 1 loads %d notes at midi %s\n", bank == nullptr ? 0 : (int) bank->size(), carried.toRawUTF8());
        const int want[8] = { 48, 60, 72, 84, 41, 53, 65, 77 };
        bool notes = bank != nullptr && bank->size() == 8;
        if (notes) for (size_t i = 0; i < 8; ++i) notes = notes && (*bank)[i].midi == want[i] && (*bank)[i].samples.size() > 100;
        check (loaded && notes && s.familyName == "Aud Bell 1", "loading a family fills the sampler with its notes");
    }

    {
        hs::Audio audio;
        audio.prepare (44100.0);
        hs::Session s (root, tempQuad(), false);
        std::array<std::uint16_t, 30> corner {};
        const int i0 = s.starNamed ("i");
        for (size_t r = 0; r < hs::kRows; ++r) for (size_t k = 0; k < hs::kWords; ++k) corner[r * 5 + k] = s.stars[(size_t) i0].words[r][k];
        audio.publish (corner);
        auto bank = std::make_shared<hs::Audio::Bank>();
        hs::Audio::Sample sine;
        sine.name = "sine A4";
        sine.midi = 69;
        sine.rate = 44100.0;
        sine.samples.assign (44100, 0.0f);
        for (int i = 0; i < 44100; ++i) sine.samples[(size_t) i] = (float) (0.5 * std::sin (2.0 * 3.141592653589793 * 440.0 * i / 44100.0));
        bank->push_back (sine);
        audio.setBank (bank);
        audio.samplerNoteOn (69, 1.0f);
        std::vector<float> left ((size_t) 512, 0.0f), right ((size_t) 512, 0.0f);
        float* outs[2] = { left.data(), right.data() };
        double device = 0.0;
        for (int i = 0; i < 20; ++i)
        {
            audio.audioDeviceIOCallbackWithContext (nullptr, 0, outs, 2, 512, {});
            for (int k = 0; k < 512; ++k) device += (double) left[(size_t) k] * left[(size_t) k];
        }
        std::vector<float> sampled ((size_t) 20 * 512, 0.0f), cascade ((size_t) 20 * 512, 0.0f);
        const int gotSampler = audio.pullSampler (sampled.data(), (int) sampled.size());
        const int gotCascade = audio.pull (cascade.data(), (int) cascade.size());
        double sampledEnergy = 0.0, cascadeEnergy = 0.0;
        for (int i = 0; i < gotSampler; ++i) sampledEnergy += (double) sampled[(size_t) i] * sampled[(size_t) i];
        for (int i = 0; i < gotCascade; ++i) cascadeEnergy += (double) cascade[(size_t) i] * cascade[(size_t) i];
        std::printf ("      sampler tap %d samples %.1f energy, cascade tap %d samples %.3e energy, device %.1f energy\n",
            gotSampler, sampledEnergy, gotCascade, cascadeEnergy, device);
        check (gotSampler == 20 * 512 && sampledEnergy > 100.0 && cascadeEnergy < 1.0e-12 && device > 20.0,
            "the sampler plays the nearest note on its own path");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setPuck (0.0, 100.0);
        hs::Spectrogram spec (nullptr, &s);
        spec.setSize (1100, 620);
        const int n = spec.peevers.winsize + 300 * spec.peevers.stride;
        std::vector<float> saw ((size_t) n, 0.0f);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            phase += 220.0 / 44100.0;
            phase -= std::floor (phase);
            saw[(size_t) i] = (float) ((2.0 * phase - 1.0) * 8000.0);
        }
        spec.feed (saw.data(), n);
        spec.pickedFrame = 150;
        const auto stamp = juce::String (150.0 * (double) spec.peevers.stride / 44100.0, 2);
        const int before = (int) s.stars.size();
        const bool took = spec.takeFrame();
        const int pinned = s.quad.pins[(size_t) hs::Session::kCornerPin[0]];
        const auto& star = s.stars[(size_t) juce::jlimit (0, (int) s.stars.size() - 1, pinned)];
        const auto formants = hs::formantsOf (star.words);
        const double bin = 44100.0 / (double) spec.peevers.nfft;
        double closest = 1.0e9, at = 0.0;
        for (double f : formants)
        {
            if (f <= 0.0) continue;
            const double harmonic = std::floor (f / 220.0 + 0.5);
            if (harmonic < 1.0) continue;
            const double d = std::abs (f - harmonic * 220.0);
            if (d < closest) { closest = d; at = f; }
        }
        std::printf ("      %d frames, corner A holds %s, formants %.0f %.0f %.0f %.0f, nearest %.0f Hz sits %.1f Hz off a harmonic of 220, one bin is %.1f Hz\n",
            spec.frameCount(), star.name.toRawUTF8(), formants[0], formants[1], formants[2], formants[3], at, closest, bin);
        check (took && (int) s.stars.size() == before + 1 && pinned == before && star.kind == "capture"
            && star.name.endsWith ("@ " + stamp) && closest <= bin,
            "a frame picked off the surface becomes a capture in the target");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.setReadingRoom (true);
        const bool used = s.key (key ('Z', false, 'z'));
        std::printf ("      Z in the reading room: sampler note %d, engine held note %d\n", s.samplerNote, s.heldNote);
        check (used && s.readingRoom && s.samplerNote == s.keyOctave && s.heldNote == -1, "the reading room routes the Z row to the sampler");
    }

    {
        hs::Session s (root, tempQuad(), false);
        s.loadFamily ("Aud Bell 1");
        hs::Spectrogram spec (nullptr, &s);
        spec.setSize (1100, 620);
        const int n = spec.peevers.winsize + 200 * spec.peevers.stride;
        std::vector<float> saw ((size_t) n, 0.0f);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            phase += 110.0 / 44100.0;
            phase -= std::floor (phase);
            saw[(size_t) i] = (float) ((2.0 * phase - 1.0) * 8000.0);
        }
        spec.feed (saw.data(), n);
        const auto image = spec.shot();
        const auto folder = root.getChildFile ("native/workstation/artifacts/shots");
        folder.createDirectory();
        const auto file = folder.getChildFile ("readingroom.png");
        file.deleteFile();
        juce::PNGImageFormat png;
        juce::FileOutputStream out (file);
        const bool written = out.openedOk() && png.writeImageToStream (image, out);
        out.flush();
        int inked = 0, lit = 0, blues = 0;
        for (int y = spec.panel.getY(); y < spec.panel.getBottom(); ++y)
            for (int x = spec.panel.getX(); x < spec.panel.getRight(); ++x)
            {
                const auto pixel = image.getPixelAt (x, y);
                if ((int) pixel.getRed() > 200) ++inked;
                if (pixel != hs::Look::ground) ++lit;
            }
        for (int y = spec.surface.getY(); y < spec.surface.getBottom(); ++y)
            for (int x = spec.surface.getX(); x < spec.surface.getRight(); ++x)
            {
                const auto pixel = image.getPixelAt (x, y);
                if ((int) pixel.getBlue() - (int) pixel.getRed() > 20) ++blues;
            }
        std::printf ("      readingroom.png %d frames, %d ink and %d drawn pixels in the panel, %d blue pixels on the surface, ink %d dim %d\n",
            spec.frameCount(), inked, lit, blues, (int) hs::Look::ink.getRed(), (int) hs::Look::dim.getRed());
        check (written && file.getSize() > 4000 && inked > 20 && lit > 300 && blues > 200 && juce::Desktop::getInstance().getNumComponents() == 0,
            "the window renders with the panel to artifacts/shots/readingroom.png");
    }

    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
