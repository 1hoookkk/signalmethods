#pragma once

#include "trench/core/native_body.hpp"

namespace trench::core::rbj {

double q_from_bandwidth_hz(double f0_hz, double bw_hz);
double bandwidth_hz_from_q(double f0_hz, double q);
double q_from_bandwidth_oct(double bw_oct);
double bandwidth_oct_from_q(double q);

native::Section peaking(double f0_hz, double q, double gain_db, double sample_rate_hz);
native::Section lowpass(double f0_hz, double q, double sample_rate_hz);
native::Section highpass(double f0_hz, double q, double sample_rate_hz);
native::Section low_shelf(double f0_hz, double q, double gain_db, double sample_rate_hz);
native::Section high_shelf(double f0_hz, double q, double gain_db, double sample_rate_hz);

}  // namespace trench::core::rbj
