#include "Sources.h"
#include "DvtdSources.h"
#include "BankSources.h"
#include "EqSources.h"
#include "GuitarSources.h"
#include "trench/core/body_from_audio.hpp"
#include "trench/core/formants.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace headspace {

namespace {

using trench::core::p2k::FormantRecipe;
using trench::core::p2k::SectionParam;
using trench::core::p2k::SectionType;

constexpr double kRadiusCeiling = .9995;
constexpr double kDefaultTiltHz = 225.;

bool stable(const Pole& p) {
    return std::isfinite(p.hz) && std::isfinite(p.bandwidth) && p.hz > 0 && p.bandwidth > 0
        && p.hz < .45 * trench::core::kP2kDatumHz
        && std::exp(-std::numbers::pi * p.bandwidth / trench::core::kP2kDatumHz) < kRadiusCeiling;
}

double bandOctaves(const Pole& p) {
    return 2. * std::asinh(p.bandwidth / (2. * p.hz)) / std::numbers::ln2;
}

FormantRecipe recipeFor(const std::array<Pole, 6>& poles, std::span<const double> gains, double tiltHz) {
    FormantRecipe recipe;
    std::size_t present = 0;
    for (const auto& p : poles) if (p.hz > 0) ++present;
    const std::size_t limit = present >= 6 ? 6 : 5;
    std::size_t row = 0;
    for (std::size_t i = 0; i < poles.size() && row < limit; ++i) {
        const auto& p = poles[i];
        if (!(p.hz > 0)) continue;
        const double gain = row < gains.size() ? gains[row] : 20. * std::log10(p.hz / p.bandwidth);
        recipe.rows[row] = {SectionType::kEq, p.hz, bandOctaves(p), gain};
        ++row;
    }
    if (row < 6) {
        const std::size_t shaped = row;
        for (; row < 5; ++row) recipe.rows[row] = {SectionType::kOff, 18000., 1., 0.};
        const bool sixth = shaped == 5 && poles[5].hz > 0;
        const double corner = sixth ? poles[5].hz : tiltHz;
        recipe.rows[5] = {SectionType::kLowPass, corner, sixth ? bandOctaves(poles[5]) : .8, 0., corner * 32.};
    }
    return recipe;
}

Endpoint compileRecipe(const FormantRecipe& recipe) {
    const auto four = trench::core::p2k::words_from_recipe(recipe, trench::core::kP2kDatumHz);
    Endpoint words{};
    for (std::size_t s = 0; s < 6; ++s) std::copy(four[s].begin(), four[s].end(), words[s].begin());
    auto result = normalize(words);
    result = reorder(result, ascendingOrder(result));
    if (!compatible(result)) throw std::invalid_argument("Recipe does not survive packed quantization");
    return result;
}

void add(SourceSet& set, const std::string& name, const std::string& group, std::span<const Pole> poles,
         std::span<const double> gains = {}, double tiltHz = kDefaultTiltHz) {
    std::string reason;
    const auto resolved = resolvePoles(poles, reason);
    if (!resolved) { set.refused.push_back(name + ": " + reason); return; }
    try {
        set.entries.push_back({name, group, *resolved, compileRecipe(recipeFor(*resolved, gains, tiltHz))});
    } catch (const std::exception& e) {
        set.refused.push_back(name + ": " + e.what());
    }
}

template<class Formant> std::vector<Pole> asPoles(std::span<const Formant> formants) {
    std::vector<Pole> poles;
    for (const auto& f : formants) poles.push_back({f.hz, f.bw_hz});
    return poles;
}

}

std::optional<std::array<Pole, 6>> resolvePoles(std::span<const Pole> own, std::string& reason) {
    std::vector<Pole> active;
    for (const auto& p : own) {
        if (!(p.hz > 0)) continue;
        if (!stable(p)) { reason = "Source has no stable conjugate pole below radius 0.9995"; return std::nullopt; }
        active.push_back(p);
    }
    if (active.empty()) { reason = "Source has no poles"; return std::nullopt; }
    std::sort(active.begin(), active.end(), [](const Pole& a, const Pole& b) { return a.hz < b.hz; });
    if (active.size() > 6) active.resize(6);
    std::array<Pole, 6> result{};
    std::copy(active.begin(), active.end(), result.begin());
    return result;
}

SourceSet sourceSet() {
    SourceSet set;
    for (const auto& posture : trench::core::p2k::templates())
        add(set, std::string(posture.name), std::string(posture.type),
            asPoles(std::span<const trench::core::p2k::Formant>(posture.poles.data(), posture.pole_count)));
    for (const auto& vowel : trench::core::p2k::klatt_vowels()) {
        std::array<Pole, 3> three{};
        for (std::size_t i = 0; i < 3; ++i) three[i] = {vowel.f[i].hz, vowel.f[i].bw_hz};
        add(set, std::string(vowel.symbol), "KLATT", three);
    }
    for (const auto& chord : kDvtdChords) {
        std::array<Pole, 5> five{};
        std::array<double, 5> gains{};
        for (std::size_t i = 0; i < 5; ++i) {
            five[i] = {chord.poles[i][0], chord.poles[i][1]};
            gains[i] = 20. * std::log10(chord.zeroWidthHz[i] / chord.poles[i][1]);
        }
        add(set, chord.name, "DVTD", five, gains, chord.shelfHz);
    }
    for (const auto& bank : kBankBodies) {
        std::vector<Pole> poles;
        for (int i = 0; i < bank.count; ++i) poles.push_back({bank.poles[i][0], bank.poles[i][1]});
        add(set, bank.name, "BANKS", poles);
    }
    for (const auto& eq : kEqBodies) {
        std::vector<Pole> poles;
        for (int i = 0; i < eq.count; ++i) poles.push_back({eq.poles[i][0], eq.poles[i][1]});
        add(set, eq.name, "PASSIVE EQ", poles);
    }
    for (const auto& body : kGuitarBodies) {
        std::array<Pole, 6> six{};
        for (std::size_t i = 0; i < 6; ++i) six[i] = {body.poles[i][0], body.poles[i][1]};
        add(set, body.name, "GUITAR", six);
    }
    return set;
}

// speech_poles is strictly for vocal tracts: it decimates to 11 kHz and applies
// pre-emphasis, which kills modes below 300 Hz. Do not use it for struck acoustic
// bodies - fit their modes as {frequency, bandwidth} pairs instead, as kGuitarBodies does.
std::optional<Source> sourceFromAudio(const std::filesystem::path& path, std::string& reason) {
    const auto clip = trench::core::audio::read_wav_mono(path);
    if (!clip) { reason = "Cannot read " + path.filename().string(); return std::nullopt; }
    const auto found = trench::core::audio::speech_poles(clip->samples, clip->sample_rate_hz, 6, 11025., 12);
    if (found.empty()) { reason = "Analysis found no poles"; return std::nullopt; }
    std::vector<Pole> poles;
    for (const auto& resonance : found) poles.push_back({resonance.hz, resonance.bw_hz});
    std::string why;
    const auto resolved = resolvePoles(poles, why);
    if (!resolved) { reason = why; return std::nullopt; }
    try {
        return Source{path.stem().string(), "AUDIO", *resolved,
                      compileRecipe(recipeFor(*resolved, {}, kDefaultTiltHz))};
    } catch (const std::exception& e) {
        reason = e.what();
        return std::nullopt;
    }
}

}
