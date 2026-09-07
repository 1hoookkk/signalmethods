#include "Locator.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace lens
{
namespace
{
constexpr double kPi = 3.141592653589793;

template <typename T> T readLe (const std::uint8_t*& p)
{
    T v {};
    std::memcpy (&v, p, sizeof (T));
    p += sizeof (T);
    return v;
}
}

double Locator::gridHz (int bin) { return kLowHz * std::pow (kHighHz / kLowHz, (double) bin / (kBins - 1)); }

bool Locator::load (const juce::File& index)
{
    juce::MemoryBlock mb;
    if (! index.loadFileAsData (mb) || mb.getSize() < 8) return false;
    const auto* p = static_cast<const std::uint8_t*> (mb.getData());
    const auto* end = p + mb.getSize();
    if (std::memcmp (p, "TRCI", 4) != 0) return false;
    p += 4;
    const auto count = readLe<std::uint32_t> (p);
    items.clear();
    items.reserve (count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (p + 2 > end) return false;
        const auto nameLength = readLe<std::uint16_t> (p);
        if (p + nameLength + 1 + 8 + 70 + kBins * 4 > end) return false;
        Node n;
        n.name.assign (reinterpret_cast<const char*> (p), nameLength);
        p += nameLength;
        n.source = readLe<std::uint8_t> (p);
        n.datum = readLe<double> (p);
        for (auto& row : n.words) for (auto& w : row) w = readLe<std::uint16_t> (p);
        for (auto& v : n.descriptor) v = readLe<float> (p);
        items.push_back (std::move (n));
    }
    return items.size() == count;
}

float Locator::levelDb (const float* frame, int n)
{
    double acc = 0.0;
    for (int i = 0; i < n; ++i) acc += (double) frame[i] * (double) frame[i];
    return (float) (10.0 * std::log10 (std::max (1.0e-12, acc / std::max (1, n))));
}

Descriptor Locator::describe (const float* frame, double sampleRateHz)
{
    if (! prepared)
    {
        envelope.setParms (kFrame, kFrame, kFrame / 4, 7);
        envelope.lpcenv = 1;
        prepared = true;
    }
    envelope.frame (frame);
    const int bins = envelope.nfft2;
    Descriptor out {};
    double mean = 0.0;
    for (int b = 0; b < kBins; ++b)
    {
        const double position = gridHz (b) / sampleRateHz * envelope.nfft;
        const int lo = std::clamp ((int) std::floor (position), 0, bins - 1), hi = std::min (lo + 1, bins - 1);
        const double f = std::clamp (position - lo, 0.0, 1.0);
        const double index = (1.0 - f) * envelope.fx[(size_t) lo] + f * envelope.fx[(size_t) hi];
        out[(size_t) b] = (float) (index * 10.0 / envelope.m);
        mean += out[(size_t) b];
    }
    float top = -1.0e9f;
    for (auto v : out) top = std::max (top, v);
    mean = 0.0;
    for (auto& v : out) { v = std::max (v, top - kFloorDb); mean += v; }
    mean /= kBins;
    for (auto& v : out) v = (float) (v - mean);
    return out;
}

Descriptor Locator::describeAveraged (const float* frame, double sampleRateHz, float memory)
{
    const auto d = describe (frame, sampleRateHz);
    if (! averaged) { average = d; averaged = true; return average; }
    for (int b = 0; b < kBins; ++b) average[(size_t) b] = memory * average[(size_t) b] + (1.0f - memory) * d[(size_t) b];
    return average;
}

std::vector<float> Locator::decimate (const std::vector<float>& mono, int factor)
{
    const int taps = 8 * factor + 1, half = taps / 2;
    std::vector<double> fir ((size_t) taps);
    const double cutoff = 0.45 / factor;
    double sum = 0.0;
    for (int i = 0; i < taps; ++i)
    {
        const double t = i - half;
        const double sinc = t == 0.0 ? 2.0 * cutoff : std::sin (2.0 * kPi * cutoff * t) / (kPi * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * kPi * i / (taps - 1)) + 0.08 * std::cos (4.0 * kPi * i / (taps - 1));
        fir[(size_t) i] = sinc * w;
        sum += fir[(size_t) i];
    }
    for (auto& v : fir) v /= sum;
    std::vector<float> out;
    for (size_t i = (size_t) half; i + (size_t) half < mono.size(); i += (size_t) factor)
    {
        double acc = 0.0;
        for (int j = 0; j < taps; ++j) acc += fir[(size_t) j] * mono[i + (size_t) j - (size_t) half];
        out.push_back ((float) acc);
    }
    return out;
}

float Locator::distance (const Descriptor& a, const Descriptor& b)
{
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int k = 0; k < kBins; ++k) { dot += (double) a[(size_t) k] * b[(size_t) k]; na += (double) a[(size_t) k] * a[(size_t) k]; nb += (double) b[(size_t) k] * b[(size_t) k]; }
    if (na < 1.0e-9 || nb < 1.0e-9) return 1.0f;
    return (float) (1.0 - dot / std::sqrt (na * nb));
}

std::vector<Match> Locator::rank (const Descriptor& d, int most) const
{
    std::vector<Match> all;
    all.reserve (items.size());
    for (int i = 0; i < (int) items.size(); ++i) all.push_back ({ i, distance (d, items[(size_t) i].descriptor) });
    const int keep = std::min (most, (int) all.size());
    std::partial_sort (all.begin(), all.begin() + keep, all.end(), [] (const Match& a, const Match& b) { return a.distance < b.distance; });
    all.resize ((size_t) keep);
    return all;
}

double Locator::responseDb (const trench::core::CornerWords& words, double datum, double hz)
{
    const std::complex<double> z = std::polar (1.0, -2.0 * kPi * hz / datum);
    double total = 0.0;
    for (const auto& row : words)
    {
        const auto q = trench::core::section_words_to_biquad (row);
        const std::complex<double> num = q[0] + q[1] * z + q[2] * z * z;
        const std::complex<double> den = 1.0 + q[3] * z + q[4] * z * z;
        total += 20.0 * std::log10 (std::max (1e-9, std::abs (num / den)));
    }
    return total;
}
}
