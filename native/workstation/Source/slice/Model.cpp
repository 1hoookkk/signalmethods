#include "Model.h"
#include "Klatt.h"
#include "trench/core/formants.hpp"
#include "trench/core/native_body.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace headspace {
using namespace trench::core;

constexpr double kProximityScaleHz = 50;
constexpr double kPullback = .03;
constexpr double kRadiusCeiling = .9995;
constexpr double kRadiusTarget = .9994;

Endpoint normalize(const Endpoint& endpoint) {
    double ratio = 1;
    for (const auto& section : endpoint) {
        const double numerator = decode_word(section[0]);
        if (!(numerator > 0)) throw std::invalid_argument("Section has no direct-current numerator to pin against");
        ratio *= decode_word(section[2]) / numerator;
    }
    const double gain = std::pow(ratio, 1. / 6);
    if (!std::isfinite(gain) || gain <= 0 || gain >= 4)
        throw std::invalid_argument("Reference gain leaves the packed range");
    Endpoint result = endpoint;
    for (auto& section : result) section[4] = encode_word(gain / 4);
    return result;
}

Endpoint shape(const Endpoint& anchor, double semitones, double tension, double stress, double tilt, unsigned selection, double span) {
    if (!compatible(anchor) || !std::isfinite(semitones) || !std::isfinite(tension) || !std::isfinite(stress)
        || !std::isfinite(tilt) || semitones < -12 || semitones > 12 || tension < 0 || tension > 1
        || stress < 0 || stress > 1 || tilt < 0 || tilt > .5
        || !std::isfinite(span) || span < .25 || span > 3.)
        throw std::invalid_argument("Invalid perceptual edit");
    if (semitones == 0 && tension == .5 && stress == 1 && tilt == 0 && span == 1) return anchor;
    Endpoint result = anchor;
    const double ratio = std::exp2(semitones / 12), damping = std::pow(8., 1 - 2 * tension);
    double logSum = 0; std::size_t spanned = 0;
    for (std::size_t s = 0; s < anchor.size(); ++s) {
        const auto p = pole(anchor, s);
        if (p.hz > 0) { logSum += std::log(p.hz); ++spanned; }
    }
    const double centre = spanned ? std::exp(logSum / static_cast<double>(spanned)) : 1000.;
    for (std::size_t section = 0; section < anchor.size(); ++section) {
        if (!(selection & (1u << section))) continue;
        const auto p = pole(anchor, section);
        if (!(p.hz > 0)) continue;
        const double neutral = 500 + 1000 * section;
        const double base = neutral + stress * (p.hz - neutral);
        const double hz = centre * std::pow(base / centre, span) * ratio;
        const double bw = p.bandwidth * ratio * damping;
        if (hz < 20 || hz > 20000 || bw <= 0) throw std::invalid_argument("Tract scale exceeds the usable frequency range");
        auto geometry = geometry_from_words(anchor[section], kP2kDatumHz);
        geometry.pole = ConjugatePair{hz, std::exp(-std::numbers::pi * bw / kP2kDatumHz)};
        geometry.zero = tilt == 0 ? RootPair{DegeneratePair{}} : RootPair{RealPair{tilt, tilt}};
        const auto words = words_from_geometry(geometry, kP2kDatumHz);
        result[section][0] = words[0]; result[section][1] = words[1];
        result[section][2] = words[2]; result[section][3] = words[3];
    }
    result = normalize(result);
    if (!compatible(result)) throw std::invalid_argument("Perceptual edit does not survive packed quantization");
    return result;
}

bool compatible(const Endpoint& endpoint) {
    for (const auto& section : endpoint) {
        if (!(decode_word(section[0]) > 0)) return false;
        const auto geometry = geometry_from_words(section, kP2kDatumHz);
        if (!(geometry.scale > 0)) return false;
        if (std::get_if<DegeneratePair>(&geometry.pole)) continue;
        const auto* p = std::get_if<ConjugatePair>(&geometry.pole);
        if (!p || !std::isfinite(p->hz) || !(p->hz > 0 && p->hz < kP2kDatumHz / 2)
            || !(p->radius > 0 && p->radius < 1)) return false;
    }
    return true;
}

Endpoint compile(const std::array<Pole, 6>& poles) {
    Endpoint result;
    for (std::size_t s = 0; s < poles.size(); ++s) {
        const auto p = poles[s];
        if (!(p.hz > 0)) { result[s] = kIdentitySection; continue; }
        if (!std::isfinite(p.hz) || !std::isfinite(p.bandwidth)
            || p.hz >= kP2kDatumHz / 2 || !(p.bandwidth > 0))
            throw std::invalid_argument("A finite, positive pole pair is required");
        result[s] = words_from_geometry({ConjugatePair{p.hz,
            std::exp(-std::numbers::pi * p.bandwidth / kP2kDatumHz)}, DegeneratePair{}, 1.0}, kP2kDatumHz);
    }
    result = normalize(result);
    result = reorder(result, ascendingOrder(result));
    if (!compatible(result)) throw std::invalid_argument("Pole pairs do not survive packed quantization");
    return result;
}

Pole pole(const Endpoint& endpoint, std::size_t section) {
    const auto geometry = geometry_from_words(endpoint.at(section), kP2kDatumHz);
    const auto* p = std::get_if<ConjugatePair>(&geometry.pole);
    if (!p) return {0, 0};
    return {p->hz, -std::log(p->radius) * kP2kDatumHz / std::numbers::pi};
}

Zero zeroOf(const Endpoint& endpoint, std::size_t section) {
    const auto geometry = geometry_from_words(endpoint.at(section), kP2kDatumHz);
    if (const auto* z = std::get_if<ConjugatePair>(&geometry.zero))
        if (z->radius > 0 && z->radius < 1 && z->hz > 0)
            return {z->hz, -std::log(z->radius) * kP2kDatumHz / std::numbers::pi, false};
    return {0, 0, true};
}

Endpoint withZero(const Endpoint& endpoint, std::size_t section, const Zero& value) {
    auto geometry = geometry_from_words(endpoint.at(section), kP2kDatumHz);
    if (value.parked) geometry.zero = DegeneratePair{};
    else {
        if (!std::isfinite(value.hz) || !std::isfinite(value.bandwidth) || value.hz < 20
            || value.hz >= kP2kDatumHz / 2 || value.bandwidth <= 0)
            throw std::invalid_argument("Invalid zero frequency or bandwidth");
        geometry.zero = ConjugatePair{value.hz, std::exp(-std::numbers::pi * value.bandwidth / kP2kDatumHz)};
    }
    const auto words = words_from_geometry(geometry, kP2kDatumHz);
    Endpoint result = endpoint;
    result[section][0] = words[0];
    result[section][1] = words[1];
    result = normalize(result);
    if (!compatible(result)) throw std::invalid_argument("Zero does not survive packed quantization");
    return result;
}

Resolved resolve(const Endpoint& endpoint, double rate, bool matchRateGain) {
    Resolved result{endpoint, {}};
    result.rate = rate;
    std::array<double, 6> hz{}, bandwidth{};
    for (std::size_t s = 0; s < endpoint.size(); ++s) {
        const auto p = pole(endpoint, s);
        hz[s] = p.hz;
        bandwidth[s] = p.bandwidth;
    }
    const auto build = [&](double at) {
        Cascade cascade{};
        for (std::size_t s = 0; s < endpoint.size(); ++s) {
            cascade[s] = at == kP2kDatumHz ? section_words_to_biquad(endpoint[s])
                : trench::core::native::rewarp_section(endpoint[s], kP2kDatumHz, at);
            if (!(hz[s] > 0)) continue;
            double nearest = std::numeric_limits<double>::max();
            for (std::size_t t = 0; t < endpoint.size(); ++t)
                if (t != s && hz[t] > 0) nearest = std::min(nearest, std::abs(hz[s] - hz[t]));
            if (nearest >= kProximityHz) continue;
            const double floorBandwidth = -std::log(1. - kPullback * std::exp(-nearest / kProximityScaleHz))
                * kP2kDatumHz / std::numbers::pi;
            const double pulled = std::exp(-std::numbers::pi * std::max(bandwidth[s], floorBandwidth) / at);
            cascade[s][3] = -2 * pulled * std::cos(2 * std::numbers::pi * hz[s] / at);
            cascade[s][4] = pulled * pulled;
        }
        cascade[6] = section_words_to_biquad(kIdentitySection);
        return cascade;
    };
    result.cascade = build(rate);
    if (matchRateGain && rate != kP2kDatumHz) {
        const auto datum = build(kP2kDatumHz);
        double peakHz = 20, peakDb = -1e9;
        for (int i = 0; i <= 40; ++i) {
            const double probe = 20.0 * std::pow(1000.0, i / 40.0);
            const double db = cascade_response_db(datum, probe, kP2kDatumHz);
            if (db > peakDb) { peakDb = db; peakHz = probe; }
        }
        result.cascade[6][0] *= std::pow(10.,
            (peakDb - cascade_response_db(result.cascade, peakHz, rate)) / 20.);
    }
    return result;
}

double directCurrentDb(const Endpoint& endpoint) {
    double total = 1;
    for (const auto& section : endpoint)
        total *= 4 * decode_word(section[4]) * decode_word(section[0]) / decode_word(section[2]);
    return 20 * std::log10(std::abs(total));
}

bool compileZero(const Endpoint& endpoint, std::size_t& section, const Zero& zero, Endpoint& out) {
    Endpoint words;
    try {
        words = withZero(endpoint, section, zero);
    } catch (const std::exception&) { return false; }
    if (!compatible(words)) return false;
    const auto order = ascendingOrder(words);
    words = reorder(words, order);
    for (std::size_t i = 0; i < order.size(); ++i) if (order[i] == section) { section = i; break; }
    out = words;
    return true;
}

Permutation ascendingOrder(const Endpoint& endpoint) {
    Permutation order{0, 1, 2, 3, 4, 5};
    const auto key = [&](std::size_t section) {
        const auto p = pole(endpoint, section);
        if (!(p.hz > 0)) return 3e30;
        const auto z = zeroOf(endpoint, section);
        if (!z.parked && z.hz >= p.hz * 4) return 2e30;
        return p.hz;
    };
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return key(a) < key(b); });
    return order;
}

Endpoint guardRadius(const Endpoint& endpoint) {
    Endpoint result = endpoint;
    for (auto& section : result) {
        auto geometry = geometry_from_words(section, kP2kDatumHz);
        const auto* p = std::get_if<ConjugatePair>(&geometry.pole);
        if (!p || !(p->radius > kRadiusCeiling)) continue;
        geometry.pole = ConjugatePair{p->hz, kRadiusTarget};
        const auto words = words_from_geometry(geometry, kP2kDatumHz);
        section[2] = words[2];
        section[3] = words[3];
    }
    return result;
}

Endpoint reorder(const Endpoint& endpoint, const Permutation& permutation) {
    Permutation seen{};
    for (const auto index : permutation) {
        if (index >= 6 || seen[index]) throw std::invalid_argument("Topology must be a permutation of the six sections");
        seen[index] = 1;
    }
    Endpoint result;
    for (std::size_t s = 0; s < 6; ++s) result[s] = endpoint[permutation[s]];
    return result;
}

Endpoint canonical(const Endpoint& endpoint) { return reorder(endpoint, ascendingOrder(endpoint)); }

double peakDb(const Endpoint& endpoint) {
    const auto resolved = resolve(endpoint);
    double peak = -1e9;
    for (int i = 0; i <= 240; ++i)
        peak = std::max(peak, cascade_response_db(resolved.cascade,
            20.0 * std::pow(1000.0, i / 240.0), kP2kDatumHz));
    return peak - directCurrentDb(endpoint);
}

bool compileSection(const Endpoint& endpoint, std::size_t& section,
                    const trench::core::p2k::SectionParam& param, double ceiling, Endpoint& out) {
    trench::core::p2k::SectionParam edited = param;
    Endpoint base = endpoint;
    for (int attempt = 0; attempt < 7; ++attempt) {
        auto words = base;
        const std::array<std::uint16_t, 4> current{
            words[section][0], words[section][1], words[section][2], words[section][3]};
        const auto four = trench::core::p2k::words_from_param(edited, current, section, kP2kDatumHz);
        if (four == current) { out = endpoint; return true; }
        std::copy(four.begin(), four.end(), words[section].begin());
        try {
            words = normalize(words);
        } catch (const std::exception&) { return false; }
        if (!compatible(words)) return false;
        const auto order = ascendingOrder(words);
        words = reorder(words, order);
        for (std::size_t i = 0; i < order.size(); ++i) if (order[i] == section) { section = i; break; }
        const double peak = peakDb(words);
        if (peak <= ceiling) { out = words; return true; }
        base = words;
        if (edited.type == trench::core::p2k::SectionType::kEq) edited.gain_db -= peak - ceiling;
        else edited.bw_oct = std::min(edited.bw_oct * 1.4, 6.0);
    }
    return false;
}

PackedBody packed(const Corners& corners) {
    PackedBody result;
    for (std::size_t c = 0; c < 4; ++c) {
        result.words[c].fill(kIdentitySection);
        std::copy(corners[c].begin(), corners[c].end(), result.words[c].begin());
        result.words[c + 4] = result.words[c];
    }
    return result;
}

Library loadLibrary(const std::filesystem::path& root) {
    Library library;
    for (const auto& t : klattAnchors) {
        std::array<Pole, 6> poles;
        for (std::size_t s = 0; s < 6; ++s) poles[s] = {t.poles[s].hz, t.poles[s].bw_hz};
        try {
            library.entries.push_back({std::string(t.name), compile(poles), true});
        } catch (const std::exception& e) {
            library.skipped.push_back(std::string(t.name) + ": " + e.what());
        }
    }
    const std::array<std::filesystem::path, 3> corpora{
        root / "evidence" / "factory-data" / "p2k" / "bodies",
        root / "evidence" / "factory-data" / "emulator-x" / "templates",
        root / "plugin" / "presets" / "bodies"};
    for (const auto& folder : corpora) {
        std::vector<std::filesystem::path> files;
        if (std::filesystem::is_directory(folder)) {
            for (const auto& entry : std::filesystem::directory_iterator(folder))
                if (entry.is_regular_file() && (entry.path().extension() == ".bin" || entry.path().extension() == ".body240"))
                    files.push_back(entry.path());
        } else {
            library.skipped.push_back(folder.string() + ": body folder is not present");
            continue;
        }
        std::sort(files.begin(), files.end());
        for (const auto& file : files) {
            std::ifstream input(file, std::ios::binary);
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
            auto name = file.stem().string();
            if (name.size() > 8 && name.compare(0, 4, "P2k_") == 0) name = name.substr(8);
            if (bytes.size() != kLegacyBodyBytes) {
                library.skipped.push_back(name + ": not a 240-byte legacy body");
                continue;
            }
            PackedBody body;
            try {
                body = PackedBody::from_legacy_bytes(bytes);
            } catch (const std::exception& e) {
                library.skipped.push_back(name + ": " + e.what());
                continue;
            }
            for (std::size_t c = 0; c < 4; ++c) {
                const auto label = name + " c" + std::to_string(c);
                Endpoint endpoint;
                std::copy_n(body.words[c].begin(), 6, endpoint.begin());
                if (!compatible(endpoint)) {
                    library.skipped.push_back(label + ": not a compatible six-section endpoint");
                    continue;
                }
                library.entries.push_back({label, endpoint, false});
            }
        }
    }
    library.skipped.push_back("DVTD and all other vowel collections: excluded by the current source restriction");
    library.skipped.push_back("83-group Morpheus skeleton image: different section count and datum; not a six-section P2K template bank");
    return library;
}

Model::Model(const Template& first) { bootstrap(first); }

void Model::bootstrap(const Template& first, const Template* second) {
    if (!compatible(first.words) || (second && !compatible(second->words)))
        throw std::invalid_argument("Incompatible endpoint");
    const auto& right = second ? *second : first;
    corners_ = {first.words, right.words, first.words, right.words};
    names_ = {first.name, right.name, first.name, right.name};
    selected_ = 0;
}

void Model::select(std::size_t corner) {
    if (corner >= 4) throw std::out_of_range("Corner");
    selected_ = corner;
}

void Model::copySelected(std::size_t destination) {
    corners_.at(destination) = corners_[selected_];
    names_.at(destination) = names_[selected_];
}

void Model::selectAt(float morph, float q) {
    if (!std::isfinite(morph) || !std::isfinite(q)) throw std::invalid_argument("Invalid pad position");
    selected_ = (morph >= 0.5f ? 1 : 0) + (q >= 0.5f ? 2 : 0);
}

void Model::setCorner(std::size_t corner, const Endpoint& words, const std::string& name) {
    if (corner >= 4) throw std::out_of_range("Corner index out of range");
    if (!compatible(words)) throw std::invalid_argument("Incompatible endpoint");
    corners_[corner] = words;
    names_[corner] = name;
}

void Model::applyAcoustic(const Template& source) {
    if (!source.acoustic || !compatible(source.words)) throw std::invalid_argument("Incompatible acoustic endpoint");
    corners_[selected_] = source.words;
    names_[selected_] = source.name;
}

void Model::editZero(std::size_t section, Zero value) {
    corners_[selected_] = withZero(corners_[selected_], section, value);
    names_[selected_] = "Edited endpoint";
}

void Model::editPole(std::size_t section, Pole value) {
    if (!std::isfinite(value.hz) || !std::isfinite(value.bandwidth) || value.hz <= 0
        || value.hz >= kP2kDatumHz / 2 || value.bandwidth <= 0)
        throw std::invalid_argument("Invalid frequency or bandwidth");
    auto endpoint = corners_[selected_];
    auto geometry = geometry_from_words(endpoint.at(section), kP2kDatumHz);
    geometry.pole = ConjugatePair{value.hz, std::exp(-std::numbers::pi * value.bandwidth / kP2kDatumHz)};
    const auto edited = words_from_geometry(geometry, kP2kDatumHz);
    endpoint[section][2] = edited[2];
    endpoint[section][3] = edited[3];
    endpoint = normalize(endpoint);
    if (!compatible(endpoint)) throw std::invalid_argument("Pole edit does not survive packed quantization");
    corners_[selected_] = endpoint;
    names_[selected_] = "Edited endpoint";
}

Endpoint Model::edge(std::size_t row, float morph) const {
    if (row > 1) throw std::out_of_range("Morph row");
    return body(morph, static_cast<float>(row));
}

namespace {
bool admissible(const Endpoint& endpoint) {
    if (!compatible(endpoint)) return false;
    const double dc = directCurrentDb(endpoint);
    return std::isfinite(dc) && std::abs(dc) < 400;
}
}

Endpoint Model::extended(float morph, float q, float* reached) const {
    if (!std::isfinite(morph) || !std::isfinite(q)) throw std::invalid_argument("Invalid audition position");
    const float wanted = std::clamp(morph, kPushLow, kPushHigh);
    const float row = std::clamp(q, 0.f, 1.f);
    const auto at = [&](float t) {
        const auto lo = body(0, row), hi = body(1, row);
        Endpoint result;
        for (std::size_t s = 0; s < 6; ++s)
            for (std::size_t w = 0; w < 5; ++w) {
                const auto difference = static_cast<float>(static_cast<std::int32_t>(hi[s][w]) - static_cast<std::int32_t>(lo[s][w]));
                const auto delta = static_cast<std::int32_t>(difference * t);
                result[s][w] = static_cast<std::uint16_t>(
                    std::clamp<std::int32_t>(static_cast<std::int32_t>(lo[s][w]) + delta, 0, 65535));
            }
        return guardRadius(result);
    };
    float low = std::clamp(wanted, 0.f, 1.f), high = wanted;
    auto result = at(high);
    if (!admissible(result)) {
        for (int step = 0; step < 24; ++step) {
            const float mid = (low + high) / 2;
            const auto candidate = at(mid);
            if (admissible(candidate)) { low = mid; result = candidate; } else high = mid;
        }
        if (reached) *reached = low;
    } else if (reached) *reached = wanted;
    return result;
}

Endpoint Model::body(float morph, float q) const {
    if (!std::isfinite(morph) || !std::isfinite(q)) throw std::invalid_argument("Invalid audition position");
    const auto words = packed(corners_).interpolate_words(std::clamp(morph, 0.0f, 1.0f), std::clamp(q, 0.0f, 1.0f), 0);
    Endpoint result;
    std::copy_n(words.begin(), 6, result.begin());
    return result;
}

std::array<std::uint8_t, 240> Model::bytes() const { return packed(corners_).legacy_bytes(); }

void Model::exportBody(const std::filesystem::path& destination) const {
    const auto data = bytes();
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(data.data()), data.size());
    output.close();
    if (!output) throw std::runtime_error("Could not write body");
}

}
