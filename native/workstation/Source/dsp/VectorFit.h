#pragma once

#include <complex>
#include <vector>

namespace hs
{
std::vector<double> minimumPhaseResponse (const std::vector<double>& magnitudeDb, std::vector<std::complex<double>>& out);

struct Fitted
{
    std::vector<std::complex<double>> poles, zeros;
    double gain = 1.0;
    double errorDb = 0.0;
};

Fitted vectorFit (const std::vector<std::complex<double>>& response, double sampleRateHz, int pairs, int passes);
}
