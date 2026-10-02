#pragma once

#include "trench/core/packed_body.hpp"
#include "trench/core/section_param.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace headspace {

using Endpoint = std::array<trench::core::PackedSection, 6>;
using Corners = std::array<Endpoint, 4>;
struct Pole { double hz; double bandwidth; };
struct Zero { double hz; double bandwidth; bool parked; };
struct Template { std::string name; Endpoint words; bool acoustic{}; };
struct Library {
    std::vector<Template> entries;
    std::vector<std::string> skipped;
};
struct Resolved {
    Endpoint words;
    trench::core::Cascade cascade;
    double rate{trench::core::kP2kDatumHz};
};

bool compatible(const Endpoint& endpoint);
Endpoint compile(const std::array<Pole, 6>& poles);
Endpoint shape(const Endpoint& anchor, double semitones, double tension, double stress, double tilt, unsigned selection = 0x3f, double span = 1.);
Endpoint normalize(const Endpoint& endpoint);
Pole pole(const Endpoint& endpoint, std::size_t section);
Zero zeroOf(const Endpoint& endpoint, std::size_t section);
Endpoint withZero(const Endpoint& endpoint, std::size_t section, const Zero& value);
Resolved resolve(const Endpoint& endpoint, double rate = trench::core::kP2kDatumHz, bool matchRateGain = true);
double directCurrentDb(const Endpoint& endpoint);
using Permutation = std::array<std::size_t, 6>;
Permutation ascendingOrder(const Endpoint& endpoint);
Endpoint reorder(const Endpoint& endpoint, const Permutation& permutation);
Endpoint canonical(const Endpoint& endpoint);
double peakDb(const Endpoint& endpoint);
bool compileSection(const Endpoint& endpoint, std::size_t& section,
    const trench::core::p2k::SectionParam& param, double ceiling, Endpoint& out);
bool compileZero(const Endpoint& endpoint, std::size_t& section, const Zero& zero, Endpoint& out);
Endpoint guardRadius(const Endpoint& endpoint);
inline constexpr float kPushLow = -1, kPushHigh = 2;
inline constexpr double kProximityHz = 100;
trench::core::PackedBody packed(const Corners& corners);
Library loadLibrary(const std::filesystem::path& root);

class Model {
public:
    bool operator==(const Model&) const = default;
    explicit Model(const Template& first);
    void bootstrap(const Template& first, const Template* second = nullptr);
    void select(std::size_t corner);
    void selectAt(float morph, float q);
    void setCorner(std::size_t corner, const Endpoint& words, const std::string& name);
    void copySelected(std::size_t destination);
    void applyAcoustic(const Template& source);
    void editPole(std::size_t section, Pole value);
    void editZero(std::size_t section, Zero value);
    const Corners& corners() const { return corners_; }
    const std::array<std::string, 4>& names() const { return names_; }
    std::size_t selected() const { return selected_; }
    Endpoint edge(std::size_t row, float morph) const;
    Endpoint body(float morph, float q) const;
    Endpoint extended(float morph, float q, float* reached = nullptr) const;
    std::array<std::uint8_t, 240> bytes() const;
    void exportBody(const std::filesystem::path& destination) const;
private:
    Corners corners_;
    std::array<std::string, 4> names_;
    std::size_t selected_{};
};

}
