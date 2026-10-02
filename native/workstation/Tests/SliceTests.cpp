#define NOMINMAX
#include <windows.h>
#undef near
#undef far

#include "Audition.h"
#include "History.h"
#include "Audio.h"
#include "Performance.h"
#include "Span.h"
#include "Sources.h"
#include "Candidate.h"
#include "trench/core/section_param.hpp"
#include "trench/core/native_body.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Function> void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "Invalid source/edit was accepted");
}
struct Reference {
    HMODULE module{};
    void* (*create)(){};
    void (*destroy)(void*){};
    void (*setParms)(void*, int, int, int, int, int, float){};
    void (*process)(void*, const float*, float*, float*, float*){};
    void (*resetAvg)(void*){};
    void load(const char* path) {
        module = LoadLibraryA(path);
        require(module != nullptr, "tools/peevers_core.dll reference is required for the Span port proof");
        create = symbol<decltype(create)>("peevers_create");
        destroy = symbol<decltype(destroy)>("peevers_destroy");
        setParms = symbol<decltype(setParms)>("peevers_set_parms");
        process = symbol<decltype(process)>("peevers_process_frame");
        resetAvg = symbol<decltype(resetAvg)>("peevers_reset_avg");
    }
private:
    template<class T> T symbol(const char* name) {
        const auto address = GetProcAddress(module, name);
        if (!address) throw std::runtime_error(std::string("Missing reference export ") + name);
        return reinterpret_cast<T>(address);
    }
};
}

#include "RenderChecks.h"

int main() {
    using namespace headspace;
    using namespace trench::core;
    std::cout << std::unitbuf;
    try {
        const auto library = loadLibrary(HEADSPACE_ROOT);
        require(library.entries.size() > 100, "Expected the twelve Klatt anchors plus the factory corner corpus");
        require(std::count_if(library.entries.begin(), library.entries.end(), [](const auto& t) { return t.acoustic; }) == 12,
            "Expected only twelve Klatt vowel endpoints");
        for (const auto& t : library.entries) require(compatible(t.words), "Library admitted an incompatible endpoint");
        std::ofstream report("headspace-source-admission.txt");
        for (const auto& t : library.entries) report << "ADMIT " << t.name << '\n';
        for (const auto& reason : library.skipped) report << "SKIP " << reason << '\n';
        std::cout << "PASS source admission: " << library.entries.size() << " endpoints; " << library.skipped.size() << " skips (headspace-source-admission.txt)\n";

        {
            const auto catalog = sourceSet();
            require(catalog.entries.size() > 100, "Expected the posture, Klatt and DVTD sources");
            require(!catalog.refused.empty(), "Expected the filter shapes to be refused");
            std::size_t postures = 0, klatt = 0, dvtd = 0, leanest = 6;
            std::vector<double> prominence;
            for (const auto& source : catalog.entries) {
                if (source.group == "KLATT") ++klatt;
                else if (source.group == "DVTD") ++dvtd;
                else ++postures;
                require(compatible(source.words), "A source compiled to an incompatible endpoint");
                require(ascendingOrder(source.words) == Permutation{0, 1, 2, 3, 4, 5},
                    "A compiled source is not in canonical section order");
                std::size_t shapedHere = 0;
                for (std::size_t s = 0; s < 6; ++s) {
                    const auto p = pole(source.words, s);
                    if (!(p.hz > 0)) continue;
                    require(std::exp(-std::numbers::pi * p.bandwidth / kP2kDatumHz) < .9995,
                        "A source pole is not below radius 0.9995");
                    if (!zeroOf(source.words, s).parked) ++shapedHere;
                }
                require(shapedHere >= 3, "A compiled source carries fewer than three shaped numerators");
                const auto reserved = zeroOf(source.words, 5);
                require(!reserved.parked && reserved.hz > pole(source.words, 5).hz,
                    "The reserved section-6 slot does not carry a high-cut above its pole");
                leanest = std::min(leanest, shapedHere);
                prominence.push_back(peakDb(source.words));
            }
            const auto bahn = std::find_if(catalog.entries.begin(), catalog.entries.end(),
                [](const Source& source) { return source.name == "tense-a bahn s1"; });
            require(bahn != catalog.entries.end() && bahn->poles[0].hz > 400,
                "The DVTD low shelf was compiled as a formant");
            std::array<Pole, 3> three{{{500, 100}, {1500, 120}, {2500, 150}}};
            std::string reason;
            const auto completed = resolvePoles(three, reason);
            require(completed && completed->at(3).hz == 0 && completed->at(5).hz == 0,
                "Short sources are not parked in their unused sections");
            std::array<Pole, 3> tooNarrow{{{500, 100}, {1500, 120}, {2500, .5}}};
            require(!resolvePoles(tooNarrow, reason), "A pole above the radius ceiling was admitted");
            std::sort(prominence.begin(), prominence.end());
            const double spread = prominence.back() - prominence.front();
            require(spread > 6, "Compiled sources do not differ in peak prominence");
            std::cout << "PASS source set: " << postures << " postures, " << klatt << " Klatt targets, " << dvtd
                      << " DVTD chords carry shaped numerators (leanest " << leanest
                      << " of six); " << catalog.refused.size() << " filter shapes refused\n";
            std::cout << "MEASURE source prominence: peak " << prominence.front() << " to " << prominence.back()
                      << " dB, median " << prominence[prominence.size() / 2] << ", p90 "
                      << prominence[prominence.size() * 9 / 10] << ", spread " << spread << " dB\n";
        }

        {
            const auto folder = std::filesystem::path(HEADSPACE_ROOT) / "evidence" / "factory-data" / "p2k" / "bodies";
            std::size_t preserved = 0, distributed = 0, shaped = 0;
            for (const auto& file : std::filesystem::directory_iterator(folder)) {
                if (!file.is_regular_file() || file.path().extension() != ".bin") continue;
                std::ifstream input(file.path(), std::ios::binary);
                const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
                if (bytes.size() != kLegacyBodyBytes) continue;
                const auto body = PackedBody::from_legacy_bytes(bytes);
                for (std::size_t c = 0; c < 4; ++c) {
                    Endpoint endpoint;
                    std::copy_n(body.words[c].begin(), 6, endpoint.begin());
                    if (!compatible(endpoint)) continue;
                    require(std::find_if(library.entries.begin(), library.entries.end(), [&](const Template& t) {
                        return !t.acoustic && t.words == endpoint; }) != library.entries.end(),
                        "A compatible factory corner is missing from the library");
                    ++preserved;
                    if (!std::all_of(endpoint.begin(), endpoint.end(),
                            [&](const auto& section) { return section[4] == endpoint[0][4]; })) ++distributed;
                    for (const auto& section : endpoint) if (decode_word(section[0]) != .25) ++shaped;
                }
            }
            require(preserved > 100, "Expected the whole compatible factory corner corpus in the library");
            require(distributed > 0, "The library lost the factory bodies that distribute gain across sections");
            require(shaped > 0, "The library lost the factory zeros");
            std::cout << "PASS factory corners preserved word for word: " << preserved << " corners, "
                      << distributed << " with a distributed gain word, " << shaped
                      << " sections carrying a shaped numerator\n";
        }

        const auto partner = [&](std::size_t i) -> const Template& {
            return library.entries[(i * 7 + 3) % library.entries.size()];
        };
        for (std::size_t i = 0; i < library.entries.size(); ++i)
        for (const auto& pair : {std::pair<const Template*, const Template*>{&library.entries[i], &partner(i)},
                                 {&library.entries[i], &library.entries[i < 12 ? (i + 1) % 12 : i]}}) {
            const auto& a = *pair.first; const auto& b = *pair.second;
            Model sweep(a); sweep.bootstrap(a, &b);
            for (int step = 0; step <= 100; ++step) {
                const float t = step / 100.f;
                const auto state = sweep.body(t, 0);
                for (std::size_t section = 0; section < 6; ++section) for (std::size_t word = 0; word < 5; ++word) {
                    const int lo = a.words[section][word], hi = b.words[section][word];
                    require(state[section][word] == lo + static_cast<int>((hi - lo) * t), "Equal pad steps must be linear in packed code units");
                }
                const auto sweepState = resolve(state);
                for (const auto hz : {50.0, 200.0, 1000.0, 5000.0, 15000.0})
                    require(std::isfinite(cascade_response_db(sweepState.cascade, hz, kP2kDatumHz)),
                        "Library sweep produced a nonfinite response");
                Model capture(a); capture.setCorner(3, state, "Captured");
                const auto restored = PackedBody::from_legacy_bytes(capture.bytes());
                require(std::equal(state.begin(), state.end(), restored.words[3].begin()), "Sweep capture/export changed the sounding words");
            }
        }
        std::cout << "PASS sampled source pairs at 101 equal intervals: canonical integer interpolation and exact captured/exported state\n";

        renderChecks(library);

        auto first = library.entries[0];
        auto second = library.entries[1];
        first.words[0][4] = encode_word(0.125);
        second.words[5][4] = encode_word(0.0625);
        Model model(first);
        for (const auto& c : model.corners()) require(c == first.words, "Single bootstrap changed words");
        model.bootstrap(first, &second);
        require(model.corners() == Corners{first.words, second.words, first.words, second.words}, "Two-template bootstrap transformed words");
        std::cout << "PASS bootstrap: all 30 words, including supplied gain, copied exactly\n";

        const auto acoustic = *std::find_if(library.entries.begin(), library.entries.end(), [](const auto& t) { return t.acoustic; });
        for (std::size_t c = 0; c < 4; ++c) {
            model.select(c);
            const auto before = model.corners();
            model.applyAcoustic(acoustic);
            for (std::size_t other = 0; other < 4; ++other)
                require(model.corners()[other] == (other == c ? acoustic.words : before[other]), "Acoustic click changed another endpoint");
            model.editPole(c, {700.0 + c * 200, 90.0 + c * 30});
        }
        const auto authored = model.corners();
        for (int mi = 0; mi <= 30; ++mi) for (int qi = 0; qi <= 30; ++qi) {
            const float m = mi / 30.f, q = qi / 30.f;
            model.selectAt(m, q);
            require(model.selected() == static_cast<std::size_t>((m >= 0.5f ? 1 : 0) + (q >= 0.5f ? 2 : 0)), "Pad did not select nearest write corner");
            const auto actual = model.body(m, q);
            const auto expected = packed(authored).interpolate_words(m, q, 0);
            require(std::equal(actual.begin(), actual.end(), expected.begin()), "BODY differs from core bilinear words");
            require(model.edge(0, m) == model.body(m, 0) && model.edge(1, m) == model.body(m, 1), "AUTHOR edge differs from core");
        }
        require(model.corners() == authored, "Audition mutated a corner");
        require(model.body(0, 0) == authored[0] && model.body(1, 0) == authored[1]
            && model.body(0, 1) == authored[2] && model.body(1, 1) == authored[3], "Pad orientation incorrect");
        const auto snapshotFixture = model;
        for (int audition = 0; audition < 2; ++audition) for (std::size_t destination = 0; destination < 4; ++destination) {
            model = snapshotFixture;
            model.selectAt(0.37f, 0.62f);
            const auto selected = model.selected();
            const auto before = model.corners();
            const auto names = model.names();
            const auto sounding = audition == 0 ? model.body(0.37f, 0.62f) : acoustic.words;
            require(sounding != before[selected], "Snapshot fixture must differ from the selected endpoint");
            model.setCorner(destination, sounding, "Snapshot");
            for (std::size_t c = 0; c < 4; ++c) {
                require(model.corners()[c] == (c == destination ? sounding : before[c]), "Snapshot did not preserve exact sounding words and other corners");
                require(model.names()[c] == (c == destination ? "Snapshot" : names[c]), "Snapshot changed another endpoint name");
            }
            require(model.selected() == selected, "Snapshot changed pad-selected target");
        }
        std::cout << "PASS acoustic edits, both edge auditions, 961 pad/nearest-corner positions, and pad/hover snapshots to all four corners\n";

        for (const auto& t : library.entries) for (std::size_t s = 0; s < 6; ++s) {
            if (!t.acoustic) continue;
            model.bootstrap(t); model.select(s % 4);
            const auto before = model.corners();
            const auto old = pole(t.words, s);
            const double bandwidth = old.bandwidth * 1.7;
            auto geometry = geometry_from_words(t.words[s], kP2kDatumHz);
            geometry.pole = ConjugatePair{old.hz, std::exp(-std::acos(-1.0) * bandwidth / kP2kDatumHz)};
            const auto expected = words_from_geometry(geometry, kP2kDatumHz);
            model.editPole(s, {old.hz, bandwidth});
            const auto& edited = model.corners()[model.selected()];
            require(edited[s][2] == expected[2] && edited[s][3] == expected[3], "Pole words were not updated together through core");
            for (std::size_t c = 0; c < 4; ++c) for (std::size_t section = 0; section < 6; ++section)
                for (std::size_t word = 0; word < 5; ++word)
                    if (!(c == model.selected() && (word == 4 || (section == s && (word == 2 || word == 3)))))
                        require(model.corners()[c][section][word] == before[c][section][word], "Pole edit changed unrelated words");
        }
        std::cout << "PASS coupled pole edits across every admitted section; other words and endpoints preserved\n";
        for (const auto& t : library.entries) {
            if (!t.acoustic) continue;
            require(shape(t.words, 0, .5, 1, 0) == t.words, "Neutral macros must return the original 30 words exactly");
            for (const auto settings : {std::array<double, 3>{-3, .8, 1}, {0, .2, 0}, {0, .5, .6}}) {
                const auto shaped = shape(t.words, settings[0], settings[1], settings[2], 0);
                require(compatible(shaped), "Macro produced incompatible endpoint");
                for (std::size_t section = 0; section < 6; ++section) {
                    for (const auto word : {0, 1}) require(shaped[section][word] == t.words[section][word], "Neutral tilt disturbed the zeros");
                    require(shaped[section][4] == shaped[0][4], "Reference gain is not shared across the six sections");
                    if (settings[2] == 1) {
                        const auto original = pole(t.words, section), moved = pole(shaped, section);
                        const auto ratio = std::exp2(settings[0] / 12);
                        require(std::abs(1200 * std::log2(moved.hz / (original.hz * ratio))) < 3, "Tract scaling deflected pole frequency");
                        require(std::abs(moved.bandwidth / (original.bandwidth * ratio * std::pow(8., 1 - 2 * settings[1])) - 1) < .01, "Tension bandwidth law changed");
                    }
                }
                model.bootstrap(t); model.selectAt(.8f, .7f);
                model.setCorner(model.selected(), shaped, "Shaped");
                for (std::size_t c = 0; c < 3; ++c) require(model.corners()[c] == t.words, "Macro changed a nonselected corner");
            }
        }
        rejects([&] { shape(first.words, 13, .5, 1, 0); });
        rejects([&] { shape(first.words, 0, std::numeric_limits<double>::quiet_NaN(), 1, 0); });
        rejects([&] { shape(first.words, 0, .5, 1, .6); });
        std::cout << "PASS perceptual macros: byte-exact neutral, all admitted anchors, selected-only writes, frequency/BW coupling, shared re-pinned gain\n";

        {
            std::size_t accepted = 0, refused = 0;
            for (const auto& t : library.entries) {
                if (t.acoustic) continue;
                require(shape(t.words, 0, .5, 1, 0) == t.words, "Neutral macros disturbed a corpus corner");
                for (const auto settings : {std::array<double, 3>{-3, .8, 1}, {0, .2, 0}, {0, .5, .6}}) {
                    try {
                        const auto shaped = shape(t.words, settings[0], settings[1], settings[2], 0);
                        require(compatible(shaped), "Macro returned an incompatible corpus endpoint");
                        for (std::size_t s = 1; s < 6; ++s)
                            require(shaped[s][4] == shaped[0][4], "Macro left the corpus gain word unshared");
                        ++accepted;
                    } catch (const std::invalid_argument&) { ++refused; }
                }
            }
            require(accepted > refused, "Most corpus corners should still accept the macros");
            std::cout << "PASS corpus macros: " << accepted << " accepted, " << refused
                      << " refused as unrepresentable; every acceptance compatible and gain-shared" << std::endl;
        }

        for (const auto& t : library.entries) {
            if (t.acoustic) {
                require(std::abs(directCurrentDb(t.words)) < .05, "Compiled anchor is not pinned to unity direct-current gain");
                for (std::size_t s = 1; s < 6; ++s)
                    require(t.words[s][4] == t.words[0][4], "Compiled anchor does not share the reference gain");
            }
            if (!t.acoustic) continue;
            double previous = -1000;
            for (const double z : {0., .125, .25, .375, .5}) {
                const auto tilted = shape(t.words, 0, .5, 1, z);
                require(compatible(tilted), "Tilt produced an incompatible endpoint");
                const auto lifted = resolve(tilted);
                require(std::abs(directCurrentDb(tilted)) < .05, "Tilt broke the unity direct-current pin");
                for (std::size_t s = 0; s < 6; ++s)
                    require(std::abs(pole(tilted, s).hz - pole(t.words, s).hz) < 1
                        && std::abs(pole(tilted, s).bandwidth - pole(t.words, s).bandwidth) < 1, "Tilt deflected a pole");
                const double top = cascade_response_db(lifted.cascade, 10000, kP2kDatumHz);
                require(top > previous + 1, "Tilt did not open the top octave monotonically");
                previous = top;
            }
        }
        std::cout << "PASS unity direct-current pin and monotone tilt at every admitted anchor" << std::endl;

        for (const auto& t : library.entries) {
            const auto sorted = reorder(t.words, ascendingOrder(t.words));
            require(ascendingOrder(sorted) == Permutation{0, 1, 2, 3, 4, 5},
                "Auto-sort left a corner out of canonical order");
            const auto swapped = reorder(t.words, Permutation{1, 0, 2, 3, 4, 5});
            require(swapped[0] == t.words[1] && swapped[1] == t.words[0], "Swap did not exchange the S1 and S2 word slots");
            for (std::size_t s = 2; s < 6; ++s) require(swapped[s] == t.words[s], "Swap disturbed an untouched section");
            for (const auto& permuted : {sorted, swapped}) {
                std::array<PackedSection, 6> a = t.words, b = permuted;
                std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
                require(a == b, "Reorder changed the multiset of packed words");
                const auto before = resolve(t.words), after = resolve(permuted);
                for (const auto hz : {50., 200., 1000., 5000., 15000.})
                    require(std::abs(cascade_response_db(before.cascade, hz, kP2kDatumHz)
                        - cascade_response_db(after.cascade, hz, kP2kDatumHz)) < 1e-9, "Reorder changed the sound at rest");
            }
            rejects([&] { reorder(t.words, Permutation{0, 0, 2, 3, 4, 5}); });
        }
        std::cout << "PASS topology reorder: ascending auto-sort, slot swaps, identical rest response, permutation guard" << std::endl;

        {
            Model host(library.entries[0]); host.bootstrap(library.entries[0], &library.entries[3]);
            const auto untouched = host.bytes();
            Audition draft(host);
            draft.follow(host, .5f, 0, true);
            draft.enterStage();
            const auto before = draft.words();
            draft.reorderEdge(1, ascendingOrder(draft.edge(1)));
            require(host.bytes() == untouched, "Topology wrote a corner without a stamp");
            require(draft.words() != before || draft.edge(1) == draft.edge(1), "Topology left no draft");
            require(draft.hasDraftEdge(), "Topology did not become a draft edit");
            History<Model> stamps; const auto priorModel = host;
            host.setCorner(1, draft.words(), "Topology");
            stamps.commit(priorModel, host);
            require(stamps.undo(host) && host.bytes() == untouched, "Stamped topology did not undo exactly");
            std::cout << "PASS topology is a draft edit: no corner write without a stamp, exact undo after one" << std::endl;
        }

        {
            Model push(library.entries[0]); push.bootstrap(library.entries[0], &library.entries[5]);
            for (int step = 0; step <= 100; ++step) {
                const float t = step / 100.f;
                require(push.extended(t, 0) == push.body(t, 0), "Extended morph diverged from canonical interpolation inside 0..1");
            }
            std::size_t clamped = 0;
            double widest = 0;
            for (const auto& t : library.entries) {
                Model body(t); body.bootstrap(t, &library.entries[(&t - library.entries.data() + 7) % library.entries.size()]);
                for (const float push : {-1.f, -.5f, 1.5f, 2.f}) {
                    float reached = 0;
                    const auto out = body.extended(push, 0, &reached);
                    require(compatible(out), "Caricature push produced an incompatible endpoint");
                    require(std::abs(directCurrentDb(out)) < 400, "Caricature push produced a nonfinite body");
                    if (std::abs(reached - push) > 1e-6) ++clamped;
                    for (std::size_t s = 0; s < 6; ++s) {
                        const auto pushed = pole(out, s);
                        if (!(pushed.hz > 0)) continue;
                        const double radius = std::exp(-std::numbers::pi * pushed.bandwidth / kP2kDatumHz);
                        require(radius <= .9995 + 1e-9, "Caricature push exceeded the pole radius guard");
                        widest = std::max(widest, radius);
                    }
                }
            }
            require(push.extended(0, 0) == push.body(0, 0) && push.extended(1, 0) == push.body(1, 0),
                "Push endpoints must equal the corners exactly");
            std::cout << "MEASURE caricature push: " << clamped << " of " << library.entries.size() * 4
                      << " pushes backed off to stay valid; widest guarded radius " << widest << std::endl;
            std::cout << "PASS caricature push: canonical inside 0..1, valid and radius-guarded from -1 to +2" << std::endl;

        {
            std::size_t placed = 0;
            double deepest = 0;
            for (const auto& t : library.entries) {
                if (!t.acoustic) continue;
                for (const auto& section : t.words) require(zeroOf({section, section, section, section, section, section}, 0).parked,
                    "Compiled anchors should start with parked zeros");
                const auto reference = pole(t.words, 2);
                const auto carved = withZero(t.words, 2, {reference.hz, 120, false});
                const auto read = zeroOf(carved, 2);
                require(!read.parked && std::abs(read.hz - reference.hz) < 5 && std::abs(read.bandwidth - 120) < 5,
                    "Zero did not survive the word round trip");
                require(std::abs(directCurrentDb(carved)) < .05, "Placing a zero broke the unity direct-current pin");
                for (std::size_t s = 0; s < 6; ++s)
                    require(pole(carved, s).hz == pole(t.words, s).hz, "Placing a zero moved a pole");
                const double before = cascade_response_db(resolve(t.words).cascade, reference.hz, kP2kDatumHz);
                const double after = cascade_response_db(resolve(carved).cascade, reference.hz, kP2kDatumHz);
                require(after < before, "A placed zero did not cut at its own frequency");
                deepest = std::min(deepest, after - before);
                const auto parked = withZero(carved, 2, {0, 0, true});
                require(parked[2][0] == t.words[2][0] && parked[2][1] == t.words[2][1], "Parking did not restore the identity numerator");
                ++placed;
            }
            rejects([&] { withZero(library.entries[0].words, 2, {5, 100, false}); });

            const auto& source = library.entries[0].words;
            std::size_t section = 2;
            Endpoint carved;
            require(compileZero(source, section, {1000., 100., false}, carved), "Unparking a zero was refused");
            const auto read = zeroOf(carved, section);
            require(!read.parked && std::abs(read.hz - 1000) < 20, "The carved zero did not survive the round trip");
            require(std::abs(directCurrentDb(carved)) < .05, "Carving a zero broke the unity direct-current pin");
            for (std::size_t s = 0; s < 6; ++s)
                require(carved[s][4] == carved[0][4], "Carving a zero unshared the reference gain word");
            std::size_t park = section;
            Endpoint restored;
            require(compileZero(carved, park, {0, 0, true}, restored), "Parking the zero was refused");
            require(restored[section][0] == source[section][0] && restored[section][1] == source[section][1],
                "Parking did not restore the parked numerator");
            std::cout << "PASS zero carve: an unparked notch holds the pinned reference gain and parks back exactly\n";
            std::cout << "MEASURE per-stage zeros: " << placed << " anchors carved, deepest cut at the notch "
                      << deepest << " dB" << std::endl;
            std::cout << "PASS per-stage zeros: round trip, poles untouched, pin held, park restores identity" << std::endl;
        }
        }

        {
            const auto& base = library.entries[0].words;
            const auto& target = library.entries[1].words;
            Candidate natural, cross, invert;
            natural.freeze(base); natural.aim(target);
            cross.freeze(base); cross.aim(target); cross.setPairing(Pairing::kCross);
            invert.freeze(base); invert.aim(target); invert.setPairing(Pairing::kInvert);
            natural.setAmount(1); cross.setAmount(1); invert.setAmount(1);
            require(cross.words() == reorder(natural.words(), {1, 0, 2, 3, 4, 5}), "CROSS did not swap S1 and S2");
            require(invert.words() == reorder(natural.words(), {5, 4, 3, 2, 1, 0}), "INVERT did not reverse the six slots");
            natural.setAmount(.5f); cross.setAmount(.5f);
            require(pole(cross.words(), 0).hz > pole(natural.words(), 0).hz, "CROSS did not lift S1 toward the target F2");
            cross.setPairing(Pairing::kNatural);
            require(cross.words() == natural.words(), "Returning to NATURAL did not restore the plain pairing");
            std::cout << "PASS pairing: NATURAL, CROSS F1xF2 and INVERT ALL pair the target slots before interpolation\n";
        }

        {
            std::size_t swept = 0, refused = 0;
            double worst = 0, worstHz = 0, hottest = -1e9;
            std::vector<double> errors;
            for (const auto& t : library.entries) for (std::size_t s = 0; s < 6; ++s) {
                const auto param = p2k::param_of(t.words[s], kP2kDatumHz);
                if (param.type == p2k::SectionType::kOff) continue;
                auto moved = param;
                moved.fc_hz = std::clamp(param.fc_hz * 1.25, 30.0, 15000.0);
                std::size_t section = s;
                Endpoint settled;
                if (!compileSection(t.words, section, moved, 24.0, settled) || settled == t.words) { ++refused; continue; }
                const auto p = pole(settled, section);
                const double cents = std::abs(1200 * std::log2(p.hz / moved.fc_hz));
                require(ascendingOrder(settled) == Permutation{0, 1, 2, 3, 4, 5},
                    "Compiled sections are not in canonical order");
                const double peak = peakDb(settled);
                require(peak <= 24.0 + 1e-6, "A canvas section edit left a peak above the +24 dB ceiling");
                hottest = std::max(hottest, peak);
                errors.push_back(cents);
                if (cents > worst) {
                    worst = cents; worstHz = moved.fc_hz;
                    std::cout << "WORST " << t.name << " s" << s << " asked " << moved.fc_hz << " got " << p.hz
                              << " section " << section << "\n  was";
                    for (std::size_t i = 0; i < 6; ++i) std::cout << ' ' << pole(t.words, i).hz;
                    std::cout << "\n  now";
                    for (std::size_t i = 0; i < 6; ++i) std::cout << ' ' << pole(settled, i).hz;
                    std::cout << '\n';
                }
                ++swept;
            }
            std::sort(errors.begin(), errors.end());
            const double median = errors[errors.size() / 2];
            const double p99 = errors[errors.size() * 99 / 100];
            require(median < 20, "A typical canvas section edit should land inside a fifth of a semitone");
            require(p99 < 120, "The 99th percentile canvas section edit should land inside a semitone");
            require(worst < 400, "No canvas section edit should miss by more than a major third");
            require(swept > refused, "Most canvas section edits should survive");
            std::cout << "MEASURE canvas section compile: " << swept << " frequency edits survived, " << refused
                      << " refused; pole error median " << median << " cents, p99 " << p99 << ", worst " << worst
                      << " at " << worstHz << " Hz; hottest settled peak " << hottest << " dB\n";

            const auto& body = library.entries.front().words;
            std::size_t section = 0;
            while (section < 6 && p2k::param_of(body[section], kP2kDatumHz).type == p2k::SectionType::kOff) ++section;
            auto collided = p2k::param_of(body[section], kP2kDatumHz);
            collided.fc_hz = pole(body, (section + 1) % 6).hz;
            collided.gain_db = 24.0;
            Endpoint settled;
            require(compileSection(body, section, collided, 24.0, settled), "A collided edit was refused");
            require(peakDb(settled) <= 24.0 + 1e-6, "Collision clamping left a peak above +24 dB");
            std::cout << "MEASURE collision clamp: a +24 dB stage driven onto its neighbour settles at "
                      << peakDb(settled) << " dB\n";
        }

        {
            const auto collide = compile({{{700, 60}, {740, 60}, {2600, 160}, {3300, 250}, {3750, 200}, {4900, 1000}}});
            const auto apart = compile({{{700, 60}, {1500, 60}, {2600, 160}, {3300, 250}, {3750, 200}, {4900, 1000}}});
            const auto near = resolve(collide), clear = resolve(apart);
            const auto rawNear = section_words_to_biquad(collide[0]), rawApart = section_words_to_biquad(apart[0]);
            require(std::abs(near.cascade[0][4] - rawNear[4]) > 1e-6, "Proximity pullback did not broaden a colliding pole");
            require(clear.cascade[0][4] == rawApart[4] && clear.cascade[0][3] == rawApart[3],
                "Proximity pullback fired outside the 100 Hz window");
            const double gap = std::abs(pole(collide, 1).hz - pole(collide, 0).hz);
            require(std::sqrt(near.cascade[0][4]) <= 1. - .03 * std::exp(-gap / 50) + 1e-9, "Pullback radius exceeds the clamp");
            require(near.words == collide, "Pullback mutated the packed words");
            const double peakNear = cascade_response_db(near.cascade, 720, kP2kDatumHz);
            const double peakRaw = cascade_response_db(resolve(apart).cascade, 700, kP2kDatumHz);
            std::cout << "MEASURE proximity pullback: colliding pair peak " << peakNear << " dB, separated reference " << peakRaw << " dB\n";
            std::cout << "PASS proximity pullback broadens only inside the 100 Hz window and never touches the words" << std::endl;
        }


        {
            Model body(first); body.bootstrap(first, &second);
            const auto saved = body.bytes();
            Audition draft(body);
            for (int i = 0; i <= 20; ++i) for (int j = 0; j <= 20; ++j) {
                draft.follow(body, i / 20.f, j / 20.f, false);
                require(draft.words() == body.body(i / 20.f, j / 20.f), "Live pad added a second rounding step");
            }
            draft.pick(acoustic); const auto original = draft;
            History<Audition> history;
            draft.shapeTo(1, .6f, .8f, 0); draft.shapeTo(2, .7f, .7f, .3f);
            const auto shaped = draft; history.commit(original, shaped);
            require(history.undo(draft) && draft == original, "Undo did not restore whole gesture");
            require(history.redo(draft) && draft == shaped, "Redo did not restore exact draft");
            draft.enterStage();
            draft.editStage(0, 0, {700, 100}); draft.editStage(0, 1, {1700, 100});
            draft.editStage(1, 0, {1900, 100}); draft.editStage(1, 1, {500, 100});
            require(pole(draft.edge(0), 0).hz < pole(draft.edge(0), 1).hz && pole(draft.edge(1), 0).hz > pole(draft.edge(1), 1).hz, "Crossing endpoints reordered stages");
            const auto lo = draft.edge(0), hi = draft.edge(1);
            for (int i = 0; i <= 100; ++i) {
                draft.follow(body, i / 100.f, 1, true);
                const auto expected = packed({lo, hi, lo, hi}).interpolate_words(i / 100.f, 0, 0);
                require(std::equal(draft.words().begin(), draft.words().end(), expected.begin()), "Stage crossing sweep lost packed identity");
            }
            require(body.bytes() == saved, "Exploring wrote a corner");
            History<Model> stamps; const auto before = body;
            body.setCorner(2, draft.words(), "Crossing"); stamps.commit(before, body); const auto after = body;
            require(stamps.undo(body) && body == before, "Stamp undo changed bytes or names");
            require(stamps.redo(body) && body == after, "Stamp redo changed bytes or names");
            const auto neutral = shape(hi, 0, .5, 0, 0);
            for (std::size_t stage = 0; stage < 6; ++stage)
                require(std::abs(pole(neutral, stage).hz - (500 + 1000 * stage)) < 3, "Stress silently ranked stages");
            std::cout << "PASS draft-only exploration, exact pad, crossing stage identities, gesture undo/redo, exact stamp history\n";
        }

        for (const auto& source : library.entries) {
            if (!source.acoustic) continue;
            Model saved(source); Audition selected(saved);
            const auto original = selected.words();
            selected.selectPoles((1u << 0) | (1u << 3));
            require(selected.words() == original, "Selecting poles changed sound");
            selected.shapeTo(1, .7f, .8f, .2f);
            for (std::size_t s = 0; s < 6; ++s) if (s != 0 && s != 3)
                for (const auto word : {0, 1, 2, 3})
                    require(selected.words()[s][word] == original[s][word], "Masked macro changed an unselected pole or zero");
            const auto shaped = selected.words(); selected.selectPoles(1u << 2);
            require(selected.words() == shaped && selected.scale == 0 && selected.stress == 1, "Selection failed to rebase without changing sound");
            selected.enterStage(); const auto before = selected.edge(0);
            const auto reference = pole(before, 2);
            selected.editStage(0, 2, {reference.hz * 1.1, reference.bandwidth * .9}, (1u << 2) | (1u << 4));
            for (std::size_t s = 0; s < 6; ++s) if (s != 2 && s != 4)
                for (const auto word : {0, 1, 2, 3})
                    require(selected.edge(0)[s][word] == before[s][word], "Group stage edit changed an unselected pole or zero");
            require(selected.edge(1) == before && saved.corners()[0] == original, "Group stage edit wrote the other end or a corner");
        }
        std::cout << "PASS pole selection, masked macros, relative group edits and untouched sections across all 20 anchors\n";

        auto invalid = first;
        invalid.words[2][0] = 0;
        rejects([&] { model.bootstrap(invalid); });
        invalid = first; invalid.words[4] = kIdentitySection;
        require(compatible(invalid.words), "A parked section must be a legal endpoint");
        model.bootstrap(invalid);
        invalid = first; invalid.words[0][3] = 0;
        rejects([&] { model.bootstrap(invalid); });
        std::array<Pole, 6> poles;
        poles.fill({500, 100}); poles[3].bandwidth = -1;
        rejects([&] { compile(poles); });
        const auto unchanged = model.corners();
        rejects([&] { model.editPole(0, {std::numeric_limits<double>::quiet_NaN(), 100}); });
        rejects([&] { model.editPole(6, {500, 100}); });
        rejects([&] { model.setCorner(4, first.words, "Invalid"); });
        rejects([&] { model.setCorner(0, invalid.words, "Invalid"); });
        rejects([&] { model.selectAt(std::numeric_limits<float>::quiet_NaN(), 0); });
        require(model.corners() == unchanged, "Rejected edit/snapshot mutated corners");
        std::cout << "PASS rejection: absent numerators, missing/unstable poles, invalid pairs, out-of-range tilt, and invalid edits\n";

        model.bootstrap(first, &second);
        model.select(2); model.applyAcoustic(acoustic);
        model.select(3); model.editPole(1, {1800, 150});
        const auto bytes = model.bytes();
        require(bytes.size() == 240, "Export size");
        const auto restored = PackedBody::from_legacy_bytes(bytes);
        for (std::size_t c = 0; c < 4; ++c)
            require(std::equal(model.corners()[c].begin(), model.corners()[c].end(), restored.words[c].begin()), "Export order or words changed");
        const auto filename = std::filesystem::temp_directory_path() /
            ("headspace-slice-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".body240");
        model.exportBody(filename);
        std::ifstream input(filename, std::ios::binary);
        const std::vector<std::uint8_t> disk((std::istreambuf_iterator<char>(input)), {});
        input.close(); std::filesystem::remove(filename);
        require(std::equal(bytes.begin(), bytes.end(), disk.begin(), disk.end()), "Disk export differs");
        std::cout << "PASS canonical A|B|C|D export and disk round-trip: exactly 240 bytes\n";

        {
            std::ofstream levels("headspace-audio-levels.txt");
            double low = 1000, high = -1000, transitionPeak = 0;
            double flatPower = 0;
            for (int n = 1; n * 110 < kP2kDatumHz / 2; ++n) {
                const double amplitude = .2 / (std::acos(-1.) * n);
                flatPower += amplitude * amplitude * .5;
            }
            const double capGain = std::pow(10., -24. / 20.) / std::sqrt(flatPower / 16);
            std::size_t quiet = 0;
            for (const auto& t : library.entries) {
                const auto resolved = resolve(t.words);
                Audio probe; probe.publish({resolved, true, static_cast<float>(listeningGain(resolved, -24))});
                std::vector<float> samples(44100); probe.process(samples);
                double sum = 0, peak = 0;
                for (std::size_t i = 22050; i < samples.size(); ++i) {
                    sum += samples[i] * static_cast<double>(samples[i]); peak = std::max(peak, std::abs(static_cast<double>(samples[i])));
                }
                const double db = 10 * std::log10(std::max(sum / 22050, 1e-30));
                if (listeningGain(resolved, -24) >= capGain * (1 - 1e-9)) ++quiet;
                else { low = std::min(low, db); high = std::max(high, db); }
                levels << t.name << " RMS " << db << " dBFS, peak " << peak << '\n';
                for (const auto& next : library.entries) {
                    probe.publish({resolve(next.words), true, static_cast<float>(listeningGain(resolve(next.words), -24))});
                    std::array<float, 1024> transition; probe.process(transition);
                    require(std::all_of(transition.begin(), transition.end(), [](float x) { return std::isfinite(x) && std::abs(x) <= 1.00001f; }), "Monitor transition clipped");
                    transitionPeak = std::max(transitionPeak, static_cast<double>(probe.peak()));
                }
            }
            require(high - low < 3, "Listening reference leaves jarring source level differences");
            std::cout << "MEASURE audition at -24 dB target: RMS " << low << " to " << high << " dBFS; rapid source-switch peak " << transitionPeak << "\n";
        }

        {
            double peak = -1000, lowRms = 1000, highRms = -1000;
            std::ofstream report("headspace-dc-reference.txt");
            for (const auto& source : library.entries) {
                const auto state = resolve(source.words);
                const double dc = directCurrentDb(source.words);
                double top = -1000;
                for (const auto& point : responseCurve(state)) top = std::max(top, point.db - dc);
                const double rms = -20 * std::log10(listeningGain(state, 0)) - dc - 6;
                peak = std::max(peak, top); lowRms = std::min(lowRms, rms); highRms = std::max(highRms, rms);
                report << source.name << " DC " << dc << " dB; normalized peak " << top << " dB; saw RMS at -6 dB gain " << rms << " dBFS\n";
            }
            std::cout << "MEASURE DC reference: largest peak " << peak << " dB; saw RMS at -6 gain " << lowRms << " to " << highRms << " dBFS\n";
        }

        {
            Performance performance;
            const auto send = [&](unsigned status, unsigned key, unsigned value) {
                return performance.message(status | (key << 8) | (value << 16));
            };
            const auto saved = model.bytes();
            require(send(144, 69, 100) && performance.gate, "MIDI note did not open excitation");
            require(std::abs(performance.hz() - 440) < 1e-9, "MIDI pitch is wrong");
            require(performance.q == 100 / 127.f && performance.velocity == performance.q, "Velocity routing is wrong");
            require(std::abs(performance.strikes - 100 / 127.) < 1e-12, "Note-on lost its strike velocity");
            send(144, 72, 64); send(128, 72, 0);
            require(performance.note == 69 && performance.gate, "Overlapping note release lost held note");
            send(176, 64, 127); send(144, 69, 0);
            require(performance.gate, "Sustain lost note");
            send(176, 64, 0); require(!performance.gate, "Sustain release stuck note");
            send(176, 1, 127); require(performance.morph == 1, "CC1 routing is wrong");
            send(144, 60, 127); send(176, 123, 0); require(!performance.gate, "All notes off stuck note");
            require(!send(145, 60, 127), "Unexpected MIDI channel admitted");
            require(model.bytes() == saved, "Performance changed preset bytes");
            Endpoint bypass; bypass.fill(kIdentitySection);
            Audio loud, soft;
            loud.prepare(48000); soft.prepare(48000);
            loud.publish({resolve(bypass, 48000), true, 1, 440, 1});
            soft.publish({resolve(bypass, 48000), true, 1, 440, .5f});
            std::vector<float> full(48000), half(48000);
            loud.process(full); soft.process(half);
            int cycles = 0;
            for (std::size_t i = 1; i < full.size(); ++i) {
                if (full[i - 1] >= 0 && full[i] < 0) ++cycles;
                require(std::abs(half[i] - .5f * full[i]) < 1e-7, "Audio ignored excitation velocity");
            }
            require(std::abs(cycles - 440) <= 1, "Audio ignored performance pitch");
            std::cout << "PASS MIDI pitch, velocity/Q, mod-wheel/Morph, overlap, sustain and release\n";
        }
        {
            const auto from = library.entries.front().words;
            const auto to = library.entries.back().words;
            Audio glide;
            glide.prepare(kP2kDatumHz);
            glide.publish({resolve(from), true, .000001f});
            std::array<float, 512> warm{}; glide.process(warm);
            glide.publish({resolve(to), false, .000001f});
            const auto edge = packed({from, to, from, to});
            Candidate frozen;
            const auto reversed = reorder(from, {5, 4, 3, 2, 1, 0});
            frozen.freeze(reversed);
            require(frozen.words() == reversed, "Freezing reordered running cascade stages");
            for (int i = 1; i <= Audio::kGlideSamples; ++i) {
                float sample{}; glide.process(std::span<float>(&sample, 1));
                const auto expected = edge.interpolate_words(static_cast<float>(i) / Audio::kGlideSamples, 0, 0);
                require(std::equal(glide.runningWords().begin(), glide.runningWords().end(), expected.begin()), "Audio glide left packed integer lattice");
                const auto captured = glide.captureWords();
                require(captured && *captured == glide.runningWords(), "Live capture differs from rendered words");
                if (i == 128) {
                    Model capturedModel({"From", from});
                    const auto original = capturedModel.bytes();
                    capturedModel.setCorner(2, *captured, "Performance");
                    require(capturedModel.corners()[2] == *captured && *captured != to, "Stamp captured destination instead of running glide");
                    const auto bytes = capturedModel.bytes();
                    require(std::equal(bytes.begin(), bytes.begin() + 120, original.begin())
                        && std::equal(bytes.begin() + 180, bytes.end(), original.begin() + 180), "Live stamp changed another corner");
                    const auto restored = PackedBody::from_legacy_bytes(bytes);
                    require(std::equal(captured->begin(), captured->end(), restored.words[2].begin()), "Export lost live captured words");
                }
            }
            require(glide.runningWords() == to, "Glide missed exact endpoint");
            std::cout << "PASS every sample follows canonical integer word interpolation and exact landing\n";
        }
        {
            const auto from = library.entries.front().words;
            const auto to = library.entries.back().words;
            for (const auto rate : {48000., 96000.}) {
                Audio audio; audio.prepare(rate);
                const auto first = resolve(from, rate), last = resolve(to, rate);
                audio.publish({first, false, .000001f});
                float sample{}; audio.process(std::span<float>(&sample, 1));
                audio.publish({last, false, .000001f});
                for (int i = 1; i <= Audio::kGlideSamples; ++i) {
                    audio.process(std::span<float>(&sample, 1));
                    const auto expected = resolve(audio.runningWords(), rate);
                    for (std::size_t s = 0; s < 6; ++s)
                        require(audio.coefficients()[s] == expected.cascade[s], "Fast glide changed decoded filter section");
                    const auto gain = std::exp(std::lerp(std::log(first.cascade[6][0]), std::log(last.cascade[6][0]),
                        static_cast<double>(i) / Audio::kGlideSamples));
                    require(std::abs(audio.coefficients()[6][0] - gain) < 1e-10 * std::max(1., gain), "Rate correction did not follow its gain glide");
                }
                require(audio.coefficients() == last.cascade, "Rate-corrected glide missed exact final response");
            }
            std::cout << "PASS 48/96 kHz per-sample section decoding and separate rate-gain glide\n";
        }
        {
            Endpoint bypass; bypass.fill(kIdentitySection);
            Audio impulse; impulse.prepare(kP2kDatumHz);
            require(!impulse.captureWords(), "Unrendered audio exposed capture words");
            const auto state = resolve(bypass);
            impulse.publish({state, true, 1, 440, 1, true, 1});
            std::array<float, 32> samples{}; impulse.process(samples);
            require(std::abs(samples[0] - .1f) < 1e-7, "Strike did not inject one impulse");
            require(std::all_of(samples.begin() + 1, samples.end(), [](float v) { return v == 0; }), "Strike mode contains sustained excitation");
            impulse.publish({state, true, 1, 440, 1, true, 1}); impulse.process(samples);
            require(std::all_of(samples.begin(), samples.end(), [](float v) { return v == 0; }), "Held note retriggered impulse");
            impulse.publish({state, false, 1, 440, .5f, true, 1.5}); impulse.process(samples);
            require(std::abs(samples[0] - .05f) < 1e-7, "Repeated strike lost velocity or quick note-off suppressed it");
            auto resonator = compile({Pole{600, 25}});
            Audio ring; ring.prepare(kP2kDatumHz);
            ring.publish({resolve(resonator), false, .0001f, 110, 1, true, 1});
            std::array<float, 512> tail{}; ring.process(tail);
            double energy = 0; for (std::size_t i = 64; i < tail.size(); ++i) energy += tail[i] * tail[i];
            require(energy > 1e-20, "Impulse did not excite the resonator tail");
            std::cout << "PASS impulse-only excitation, velocity, repeated strikes and free resonator decay\n";
        }
        {
            Endpoint words; words.fill(kIdentitySection);
            words[0] = compile({Pole{600, 25}})[0];
            auto target = words; target[0] = compile({Pole{1800, 30}})[0];
            Audio actual; actual.prepare(kP2kDatumHz);
            actual.publish({resolve(words), true, .00001f});
            std::array<float, 2048> warm{}; actual.process(warm);
            actual.publish({resolve(words), false, .00001f});
            std::array<float, 256> release{}; actual.process(release);
            actual.publish({resolve(target), false, .00001f});
            std::array<float, 256> tail{}; actual.process(tail);
            double energy = 0; for (auto v : tail) energy += v * v;
            require(energy > 1e-20, "Coefficient swap erased unexcited ringing tail");
            std::cout << "PASS ringing delay state survives coefficient swap with excitation closed\n";
        }
        Audio audio;
        std::array<float, 256> block;
        for (const auto& state : {model.corners()[0], model.edge(0, 0.31f), model.edge(1, 0.73f), model.body(0.47f, 0.61f), acoustic.words, model.body(0.47f, 0.61f)}) {
            const auto resolved = resolve(state);
            require(audio.publish({resolved, true, 0.000001f}), "Audio queue full");
            audio.process(block);
            require(audio.consumedWords() == resolved.words, "Audio/plot packed state mismatch");
            for (const auto hz : {50.0, 200.0, 1000.0, 5000.0, 15000.0})
                require(std::abs(cascade_response_db(audio.coefficients(), hz, kP2kDatumHz)
                    - cascade_response_db(resolved.cascade, hz, kP2kDatumHz)) < 1e-7, "Settled audio and plot responses differ");
            require(std::all_of(block.begin(), block.end(), [](float x) { return std::isfinite(x); }), "Nonfinite audio");
        }
        std::cout << "PASS shared packed audio/plot states and settled responses after the canonical 256-sample approach\n";

        Audio threaded;
        std::atomic<bool> finished{}, torn{};
        const auto left = resolve(first.words), right = resolve(second.words);
        std::thread consumer([&] {
            std::array<float, 16> samples;
            while (!finished.load()) {
                threaded.process(samples);
                const auto words = threaded.consumedWords();
                if (words != Endpoint{} && words != first.words && words != second.words) torn.store(true);
            }
        });
        for (int i = 0; i < 10000; ++i)
            while (!threaded.publish({i % 2 ? left : right, false, 0})) std::this_thread::yield();
        finished.store(true); consumer.join();
        require(!torn.load(), "Torn audio snapshot");
        std::cout << "PASS audio snapshot publication under concurrent producer/consumer load\n";

        {
            short truncating[]{0, -3};
            Span::demean(truncating, 2);
            require(truncating[0] == 1 && truncating[1] == -2, "Span demean must use the truncating integer mean and floor");
            short symmetric[]{0, 1, 2, 3};
            Span::demean(symmetric, 4);
            require(symmetric[0] == -1 && symmetric[1] == 0 && symmetric[2] == 1 && symmetric[3] == 2, "Span demean changed a positive mean");
            short negatives[]{-1, -2};
            Span::demean(negatives, 2);
            require(negatives[0] == 0 && negatives[1] == -1, "Span demean changed a negative mean");
            short untouched[]{7};
            Span::demean(untouched, 0);
            require(untouched[0] == 7, "Span demean touched a zero-length frame");
            std::cout << "PASS Span demean: truncating integer mean, floor per sample, zero length untouched\n";
        }

        {
            constexpr int order = 1024;
            std::array<short, order> integers{};
            unsigned generator = 12345;
            for (int i = 0; i < order / 2; ++i) {
                generator = generator * 1103515245u + 12345u;
                const short value = static_cast<short>(static_cast<int>((generator >> 16) & 0x7fff) - 16384);
                integers[static_cast<std::size_t>(i) * 2] = value;
                integers[static_cast<std::size_t>(i) * 2 + 1] = static_cast<short>(-value);
            }
            std::array<float, order> input{}, referenceInput{};
            for (int i = 0; i < order; ++i) {
                input[static_cast<std::size_t>(i)] = integers[static_cast<std::size_t>(i)] / 32767.0f;
                referenceInput[static_cast<std::size_t>(i)] = static_cast<float>(integers[static_cast<std::size_t>(i)]);
            }

            Reference reference;
            reference.load(HEADSPACE_ROOT "/tools/peevers_core.dll");
            std::ofstream report("headspace-span-port.txt");
            const double hi = std::log10(268225000.0), lo = std::log10(1e-6);
            const double m = (20.0 * 12.75 + 20.0) / (hi - lo);
            const double b = -20.0 + m * 6.0;
            double worstChain = 0, worstAverage = 0;
            for (const int size : {1024, 512}) {
                const int sizeBins = size / 2 + 1;
                for (int type = 0; type < Span::kWindows; ++type) {
                    std::array<float, order> plain{}, averaged{};
                    std::array<float, 13> reflection{};
                    void* handle = reference.create();
                    reference.setParms(handle, size, size, size / 2, type, 0, 0.0f);
                    reference.process(handle, referenceInput.data(), plain.data(), averaged.data(), reflection.data());
                    reference.destroy(handle);

                    std::array<double, order> power{};
                    int excited = 0, probe = -1;
                    for (int i = 0; i < sizeBins; ++i) {
                        power[static_cast<std::size_t>(i)] = std::pow(10.0, (plain[static_cast<std::size_t>(i)] - b) / m)
                            * static_cast<double>(size) * size;
                        if (power[static_cast<std::size_t>(i)] > 1e3) {
                            ++excited;
                            if (probe < 0) probe = i;
                        }
                    }
                    require(excited > sizeBins / 2, "Span proof input must excite most bins above the reference floor");

                    Span span;
                    span.setFft(size);
                    span.setWindow(type, size);
                    span.setRemoveDc(true);
                    span.setAverage(false);
                    span.frame(input.data());
                    const float scale = static_cast<float>(static_cast<double>(size) / span.windowEnergy());
                    const float unit = static_cast<float>(static_cast<double>(scale) / 1073741824.0);

                    double chain = 0;
                    for (int i = 0; i < sizeBins; ++i) {
                        const double target = power[static_cast<std::size_t>(i)];
                        if (target <= 1e3) continue;
                        chain = std::max(chain, std::abs(std::pow(10.0, span.spectrum()[i] / 10.0) / unit - target) / target);
                    }

                    void* ema = reference.create();
                    reference.setParms(ema, size, size, size / 2, type, 0, 0.99f);
                    Span average;
                    average.setFft(size);
                    average.setWindow(type, size);
                    average.setRemoveDc(true);
                    average.setAverage(true, .99);

                    double averageError = 0;
                    for (int frame = 1; frame <= 4; ++frame) {
                        average.frame(input.data());
                        reference.process(ema, referenceInput.data(), averaged.data(), averaged.data(), reflection.data());
                        if (frame == 1)
                            require(std::abs(static_cast<double>(averaged[static_cast<std::size_t>(probe)])
                                    - static_cast<double>(static_cast<float>(plain[static_cast<std::size_t>(probe)] * 0.01)))
                                <= 1e-6 * std::abs(static_cast<double>(plain[static_cast<std::size_t>(probe)])),
                                "Span averager k is not 0.99 in the reference");
                        for (int i = 0; i < sizeBins; ++i) {
                            const double target = power[static_cast<std::size_t>(i)];
                            if (target <= 1e3) continue;
                            const double expected = target * (1.0 - std::pow(0.99, frame));
                            averageError = std::max(averageError,
                                std::abs(std::pow(10.0, average.spectrum()[i] / 10.0) / unit - expected) / expected);
                        }
                    }
                    reference.destroy(ema);
                    worstChain = std::max(worstChain, chain);
                    worstAverage = std::max(worstAverage, averageError);
                    report << "WINDOW " << Span::windowNames[type] << " nfft " << size
                           << " chain " << chain << " average " << averageError << '\n';
                }
            }
            require(worstChain < 1e-4, "Span spectrum differs from the reference linear power");
            require(worstAverage < 1e-4, "Span averager differs from the reference k = 0.99 trajectory");
            std::cout << "MEASURE Span port against tools/peevers_core.dll: worst chain error " << worstChain
                      << ", worst averager error " << worstAverage << " over nine windows at nfft 512 and 1024\n";

            {
                Span span;
                span.setFft(order);
                span.setWindow(1, order);
                span.setRemoveDc(false);
                span.setAverage(false);
                std::array<float, order> tone{};
                for (int i = 0; i < order; ++i)
                    tone[static_cast<std::size_t>(i)] = std::sin(2.0 * std::numbers::pi * 64.0 * i / order);
                span.frame(tone.data());
                require(std::abs(span.spectrum()[64] - span.fullScaleDb()) < .01,
                    "Span full-scale reference does not match a bin-centred full-scale sine");
                std::cout << "PASS Span full-scale reference: bin-centred full-scale sine reads " << span.spectrum()[64]
                          << " dB against the display reference " << span.fullScaleDb() << " dB\n";
            }
        }

        {
            double worstSteady = 0, worstSwitch = 0;
            std::string steadyName, switchName;
            for (const auto& t : library.entries) {
                const auto resolved = resolve(t.words, 48000.0);
                CascadeRunner runner;
                runner.set_sample_rate(48000.0);
                runner.set_ring_leveller(false);
                runner.set_target(encode_cascade(resolved.cascade));
                SawSource saw{110.0, 48000.0, .1f};
                const double gain = listeningGain(resolved, -18) * std::pow(10., -6. / 20.);
                std::vector<float> block(48000);
                for (auto& sample : block) sample = saw.next();
                runner.process(block);
                double peak = 0;
                for (std::size_t i = 24000; i < block.size(); ++i)
                    peak = std::max(peak, std::abs(static_cast<double>(block[i]) * gain));
                if (peak > worstSteady) { worstSteady = peak; steadyName = t.name; }
                for (std::size_t n = 0; n < library.entries.size(); n += 8) {
                    const auto target = resolve(library.entries[n].words, 48000.0);
                    CascadeRunner switched;
                    switched.set_sample_rate(48000.0);
                    switched.set_ring_leveller(false);
                    switched.set_target(encode_cascade(resolved.cascade));
                    SawSource source{110.0, 48000.0, .1f};
                    std::vector<float> settle(4800);
                    for (auto& sample : settle) sample = source.next();
                    switched.process(settle);
                    switched.set_target(encode_cascade(target.cascade));
                    std::vector<float> transition(1024);
                    for (auto& sample : transition) sample = source.next();
                    switched.process(transition);
                    const double switchedGain = listeningGain(target, -18) * std::pow(10., -6. / 20.);
                    for (const float sample : transition) {
                        const double value = std::abs(static_cast<double>(sample) * switchedGain);
                        if (value > worstSwitch) {
                            worstSwitch = value;
                            switchName = t.name + " -> " + library.entries[n].name;
                        }
                    }
                }
            }
            std::cout << "MEASURE unprotected monitor peak: " << worstSteady << " steady (" << steadyName
                      << "), " << worstSwitch << " switching " << switchName << ", against the 0.5 ceiling\n";

            {
                Audio audio;
                audio.prepare(48000.0);
                const auto start = resolve(library.entries[0].words, 48000.0);
                audio.publish({start, true, static_cast<float>(listeningGain(start, -18) * std::pow(10., -6. / 20.))});
                std::vector<float> warm(4800);
                audio.process(warm);
                double worstSelection = 0;
                std::string selectionName;
                for (const auto& t : library.entries) {
                    const auto target = resolve(t.words, 48000.0);
                    audio.publish({target, true, static_cast<float>(listeningGain(target, -18) * std::pow(10., -6. / 20.))});
                    std::vector<float> transition(1024);
                    audio.process(transition);
                    double peak = 0;
                    for (const float sample : transition) peak = std::max(peak, std::abs(static_cast<double>(sample)));
                    if (peak > worstSelection) { worstSelection = peak; selectionName = t.name; }
                }
                Model drag(library.entries[0]);
                drag.bootstrap(library.entries[0], &library.entries[7]);
                double worstDrag = 0;
                double worstGlideSeconds = 0;
                std::array<float, 256> block{};
                for (int frame = 0; frame < 400; ++frame) {
                    const auto words = drag.body(frame / 400.f, 0.f);
                    const auto resolved = resolve(words, 48000.0);
                    audio.publish({resolved, true, static_cast<float>(listeningGain(resolved, -18) * std::pow(10., -6. / 20.))});
                    const auto began = std::chrono::steady_clock::now();
                    audio.process(block);
                    worstGlideSeconds = std::max(worstGlideSeconds, std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count());
                    for (const float sample : block) worstDrag = std::max(worstDrag, std::abs(static_cast<double>(sample)));
                }
                double worstGain = 0;
                std::string gainName;
                for (const auto& t : library.entries) {
                    const auto resolved = resolve(t.words, 48000.0);
                    const double gain = listeningGain(resolved, -18) * std::pow(10., -6. / 20.);
                    if (gain > worstGain) { worstGain = gain; gainName = t.name; }
                }
                std::cout << "MEASURE monitor gain: worst " << worstGain << " (" << gainName << ")\n";
                std::cout << "MEASURE monitor transients: worst body selection " << worstSelection << " (" << selectionName
                          << "), worst pad drag " << worstDrag << ", against the 0.5 protection ceiling\n";
                require(worstSelection <= 1.00001, "A body selection clipped the monitor output");
                require(worstDrag <= 1.00001, "A pad drag clipped the monitor output");
                std::cout << "MEASURE sample-rate packed glide: worst 256-sample block " << worstGlideSeconds * 1e6 << " us\n";
                require(worstGlideSeconds < 256. / 48000., "Packed glide misses audio deadline");
            }

        }

        {
            Audio audio;
            audio.prepare(48000.0);
            audio.publish({resolve(library.entries[0].words, 48000.0), true, .5f});
            std::vector<float> rendered(96000);
            audio.process(rendered);
            constexpr int n = 32768;
            const std::size_t start = 32768;
            const auto magnitude = [&](double hz) {
                double re = 0, im = 0;
                for (int i = 0; i < n; ++i) {
                    const double window = .5 - .5 * std::cos(2 * std::numbers::pi * i / n);
                    const double phase = 2 * std::numbers::pi * hz * i / 48000.;
                    re += rendered[start + static_cast<std::size_t>(i)] * window * std::cos(phase);
                    im -= rendered[start + static_cast<std::size_t>(i)] * window * std::sin(phase);
                }
                return 20 * std::log10(std::max(std::sqrt(re * re + im * im), 1e-30));
            };
            double harmonic = -1000, between = -1000;
            for (int k = 1; k <= 60; ++k) {
                harmonic = std::max(harmonic, magnitude(110. * k));
                between = std::max(between, magnitude(110. * k + 55.));
            }
            std::cout << "MEASURE saw through a body: strongest harmonic " << harmonic
                      << " dB, loudest inter-harmonic bin " << between << " dB, separation "
                      << harmonic - between << " dB\n";
        }

        {
            Audio audio;
            audio.prepare(48000.0);
            audio.publish({resolve(library.entries[0].words, 48000.0), true, .5f});
            std::array<float, 256> block{};
            double worst = 0, total = 0;
            constexpr int blocks = 4000;
            for (int i = 0; i < blocks; ++i) {
                const auto start = std::chrono::steady_clock::now();
                audio.process(block);
                const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                worst = std::max(worst, elapsed);
                total += elapsed;
            }
            const double deadline = 256. / 48000.;
            std::cout << "MEASURE audio block with the live analyser: mean " << total / blocks * 1e6
                      << " us, worst " << worst * 1e6 << " us for a " << deadline * 1e6
                      << " us deadline (" << worst / deadline * 100 << "% of budget)\n";
            require(worst < deadline, "The audio block misses its deadline");
        }

        {
            std::ofstream report("headspace-monitor.txt");
            double worstPeak = 0, hottestRms = -1000, quietestRms = 1000;
            std::size_t worstCeiling = 0;
            std::string worstName;
            for (const auto& t : library.entries) {
                Audio audio;
                audio.prepare(48000.0);
                const auto resolved = resolve(t.words, 48000.0);
                audio.publish({resolved, true, static_cast<float>(listeningGain(resolved, -18) * std::pow(10., -6. / 20.))});
                std::vector<float> samples(48000);
                audio.process(samples);
                double peak = 0, sum = 0;
                std::size_t ceiling = 0;
                for (std::size_t i = 24000; i < samples.size(); ++i) {
                    const double value = std::abs(static_cast<double>(samples[i]));
                    peak = std::max(peak, value);
                    sum += value * value;
                    if (value >= .4999) ++ceiling;
                }
                const double rms = 10 * std::log10(std::max(sum / 24000., 1e-30));
                worstPeak = std::max(worstPeak, peak);
                hottestRms = std::max(hottestRms, rms);
                quietestRms = std::min(quietestRms, rms);
                if (ceiling > worstCeiling) { worstCeiling = ceiling; worstName = t.name; }
                report << t.name << " peak " << peak << " rms " << rms << " at ceiling " << ceiling << '\n';
            }
            std::cout << "MEASURE monitor at -24 dBFS target over " << library.entries.size() << " bodies: peak "
                      << worstPeak << ", RMS " << quietestRms << " to " << hottestRms
                      << " dBFS, most samples at the protection ceiling " << worstCeiling << " (" << worstName << ")\n";
        }

        {
            Audio audio;
            audio.prepare(48000.0);
            require(audio.sampleRate() == 48000.0, "Audio did not take the running rate");
            std::array<float, Span::kMaxBins> bins{};
            require(audio.spectrumBins(bins.data(), static_cast<int>(bins.size())) > 0, "No spectrum before any audio");
            std::vector<float> samples(48000);
            require(audio.publish({resolve(library.entries[0].words, 48000.0), true, .5f}), "Audio queue full");
            audio.process(samples);
            const int count = audio.spectrumBins(bins.data(), static_cast<int>(bins.size()));
            require(count == Audio::kFftOrder / 2 + 1, "Live spectrum published the wrong bin count");
            require(std::all_of(bins.begin(), bins.begin() + count, [](float x) { return std::isfinite(x); }),
                "Live spectrum is not finite");
            const float peak = *std::max_element(bins.begin(), bins.begin() + count);
            require(peak > audio.spectrumFullScaleDb() - 60.0, "Live spectrum captured no signal from the sounding saw");
            std::cout << "PASS live spectrum: " << count << " finite bins at " << audio.sampleRate()
                      << " Hz, peak " << peak << " dB against the " << audio.spectrumFullScaleDb()
                      << " dB full-scale reference\n";
        }

        {
            std::ofstream report("headspace-rate-agreement.txt");
            double audible48 = 0, audible96 = 0, top48 = 0, top96 = 0, roundTrip = 0;
            for (const auto& t : library.entries) {
                const auto datum = resolve(t.words, kP2kDatumHz);
                for (std::size_t s = 0; s < 6; ++s) {
                    const std::array<Biquad, 1> direct{section_words_to_biquad(t.words[s])};
                    const std::array<Biquad, 1> rebuilt{
                        trench::core::native::rewarp_section(t.words[s], kP2kDatumHz, kP2kDatumHz)};
                    for (int i = 0; i <= 400; ++i) {
                        const double hz = 20.0 * std::pow(1000.0, i / 400.0);
                        const double atDirect = cascade_response_db(direct, hz, kP2kDatumHz);
                        const double atRebuilt = cascade_response_db(rebuilt, hz, kP2kDatumHz);
                        if (std::min(atDirect, atRebuilt) < -60.0) continue;
                        roundTrip = std::max(roundTrip, std::abs(atRebuilt - atDirect));
                    }
                }
                for (const double rate : {48000.0, 96000.0}) {
                    const auto warped = resolve(t.words, rate);
                    std::array<double, 4> band{};
                    double worstHz = 0, peak = -1000;
                    for (int i = 0; i <= 400; ++i)
                        peak = std::max(peak, cascade_response_db(datum.cascade, 20.0 * std::pow(1000.0, i / 400.0), kP2kDatumHz));
                    for (int i = 0; i <= 400; ++i) {
                        const double hz = 20.0 * std::pow(1000.0, i / 400.0);
                        const double atDatum = cascade_response_db(datum.cascade, hz, kP2kDatumHz);
                        const double atRate = cascade_response_db(warped.cascade, hz, rate);
                        if (std::min(atDatum, atRate) < peak - 60.0) continue;
                        const int index = hz <= 2000.0 ? 0 : hz <= 8000.0 ? 1 : hz <= 16000.0 ? 2 : 3;
                        double& slot = band[static_cast<std::size_t>(index)];
                        slot = std::max(slot, std::abs(atRate - atDatum));
                        if (slot == std::abs(atRate - atDatum)) worstHz = hz;
                    }
                    if (rate == 48000.0) {
                        audible48 = std::max(audible48, std::max(band[0], band[1]));
                        top48 = std::max(top48, std::max(band[2], band[3]));
                    } else {
                        audible96 = std::max(audible96, band[0]);
                        top96 = std::max(top96, std::max(band[1], std::max(band[2], band[3])));
                    }
                    report << t.name << " at " << rate << " Hz worst delta " << band[0] << " / " << band[1]
                           << " / " << band[2] << " / " << band[3] << " dB in 20-2k / 2-8k / 8-16k / 16-20k, last at "
                           << worstHz << " Hz\n";
                }
            }
            std::cout << "MEASURE rate agreement against the " << kP2kDatumHz << " Hz datum: worst "
                      << audible48 << " dB at 48 kHz below 8 kHz, " << audible96 << " dB at 96 kHz below 2 kHz, "
                      << top48 << " dB and " << top96 << " dB above 8 kHz where the bilinear warp differs; "
                      << "section round trip " << roundTrip << " dB\n";
            require(roundTrip < .01, "Rewarping a section at the datum changed it");
            require(audible48 < 1.0, "Resolving at 48 kHz moved the response by more than 1 dB below 8 kHz");
            require(audible96 < 3.0, "Resolving at 96 kHz moved the response by more than 3 dB below 2 kHz");
        }
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n'; return 1;
    }
    return 0;
}
