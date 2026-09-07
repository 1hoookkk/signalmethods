#pragma once

#include "dsp/Peevers.h"
#include <juce_core/juce_core.h>
#include <trench/core/packed_body.hpp>
#include <array>
#include <string>
#include <vector>

namespace lens
{
constexpr int kBins = 128;
constexpr double kLowHz = 40.0, kHighHz = 5000.0;
constexpr int kFrame = 512;
constexpr float kFloorDb = 30.0f;
using Descriptor = std::array<float, kBins>;

struct Node
{
    std::string name;
    int source = 0;
    double datum = 44100.0;
    trench::core::CornerWords words {};
    Descriptor descriptor {};
};

struct Match { int node = -1; float distance = 0.0f; };

class Locator
{
public:
    bool load (const juce::File& index);
    const std::vector<Node>& nodes() const { return items; }
    static double gridHz (int bin);
    Descriptor describe (const float* frame, double sampleRateHz);
    Descriptor describeAveraged (const float* frame, double sampleRateHz, float memory = 0.8f);
    void resetAverage() { averaged = false; }
    static std::vector<float> decimate (const std::vector<float>& mono, int factor);
    static float levelDb (const float* frame, int n);
    std::vector<Match> rank (const Descriptor& d, int most) const;
    static float distance (const Descriptor& a, const Descriptor& b);
    const std::array<float, 13>& reflection() const { return envelope.lpc.k; }
    static Descriptor shape (const std::vector<double>& magnitudeDb, const std::vector<double>& hz);
    static double responseDb (const trench::core::CornerWords& words, double datum, double hz);

private:
    std::vector<Node> items;
    hs::Peevers envelope;
    bool prepared = false, averaged = false;
    Descriptor average {};
};
}
