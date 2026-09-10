#pragma once

#include <array>

// Measured brass mute postures: the same instrument and pitch, muted and open.
//
// These are RAW EXTRACTED STRUCTURES, not presets. Each entry is one measured
// state as six independent stages. No high-Q variants and no four-corner quads
// are synthesised here -- corners A/B/C/D are authored on the canvas by stamping
// the sounding state with keys 1..4.
//
// Stages come from a bounded fit with each pole confined to a physical lane and
// each zero to the anti-resonance corridor above it, so open and closed share
// stage semantics and can be morphed between. Lanes and corridors: brassBands.m.
// Measured anchors behind those lanes, across the five open states: the
// inter-formant trough at 884/895/931/1044/1074 Hz and the bell flare peak at
// 1575/1611/1737/2619/2814 Hz. The closed states have no peaks or troughs at
// all -- a monotonic rolloff, 20 dB down by 468-868 Hz.
//
// The fitting target is the cepstral ENVELOPE, not the raw spectrum. These are
// played notes, so the raw spectrum is source x body and its comb is the harmonic
// series of the pitch; a filter reproducing it would have the played note baked
// in. rmsDb below is measured against that envelope.
//
// The corpus is an exact octave series. Measured: c2 32.8, c3 65.3, c4 130.8,
// c5 261.7 Hz -- C1, C2, C3, C4 -- so c1 is C0 at 16.35 Hz. Its partial comb
// (49, 65, 82, 98, 115, 131 Hz) is multiples 3..8 of 16.35, which is consistent,
// but its fundamental carries almost no energy and the two estimators do not
// agree on it, so fundamentalHz is left at 0 rather than filled in from the
// series. c3 open fails the same test while c3 closed passes at 65.3.
//
// fundamentalHz is 0 whenever two independent estimators (autocorrelation and the
// harmonic product spectrum, both searched from 14 Hz) disagree by more than 3
// percent. c5 shows sidebands at f0 +/- 50 Hz, which is interference, not
// structure. partialsHz holds measured peak frequencies, not multiples of f0.
//
// poleHz/zeroHz of 0 marks a real (non-oscillatory) pair: it has no pitch and one
// is not invented for it. Bandwidths are Hz, with r = exp(-pi*BW/fs).

namespace headspace {

struct BrassStage {
    double poleHz;
    double poleBwHz;
    double zeroHz;
    double zeroBwHz;
};

struct BrassPosture {
    const char* name;
    const char* note;
    const char* state;          // "closed" or "open"
    double sampleRateHz;
    double fundamentalHz;       // 0 = not resolvable, see above
    int partialCount;
    std::array<double, 8> partialsHz;
    double gainDb;
    double rmsDb;               // fit error against the envelope
    std::array<BrassStage, 6> stages;
};

inline constexpr std::array<BrassPosture, 10> kBrassPostures{{
    {"c1 closed", "c1", "closed", 44100.0, 0.0000, 0,
     {{0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}},
     -102.8909, 2.7334,
     {{{137.4214, 53.6999, 839.7553, 3598.3715},
       {560.1314, 295.5357, 1874.4393, 724.7525},
       {1441.6021, 815.9562, 3449.1164, 1783.6227},
       {2300.4078, 786.2257, 7347.7911, 2000.3409},
       {5000.3625, 899.5949, 7690.1267, 1971.1194},
       {7806.4841, 899.5949, 14967.2069, 3598.3715}}}},
    {"c1 open", "c1", "open", 44100.0, 0.0000, 0,
     {{0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}},
     -53.2645, 2.6915,
     {{{347.1498, 326.7318, 571.2344, 153.7357},
       {636.3026, 234.3587, 1208.4241, 1063.2418},
       {1524.9743, 224.9690, 3449.1164, 3598.3715},
       {2457.3756, 418.4494, 7347.7911, 3598.3715},
       {5172.4119, 899.5949, 6723.1888, 2676.6974},
       {7151.2031, 899.5949, 18061.9689, 2823.8111}}}},
    {"c2 closed", "c2", "closed", 44100.0, 32.8076, 8,
     {{32.9727, 65.2725, 98.2452, 131.2180, 163.5178, 195.8176, 228.7903, 261.7630}},
     -101.2427, 2.9003,
     {{{135.2717, 51.5789, 839.7553, 3598.3715},
       {560.1314, 293.1644, 1874.4393, 751.3390},
       {1400.2719, 899.3446, 3449.1164, 1583.3360},
       {2300.4078, 787.1315, 7332.6512, 1701.2282},
       {5000.3625, 899.5949, 7208.1279, 1531.7913},
       {7598.8937, 899.5949, 15149.6442, 3598.3715}}}},
    {"c2 open", "c2", "open", 44100.0, 32.8561, 8,
     {{32.9727, 65.2725, 98.2452, 131.2180, 164.1907, 196.4905, 229.4632, 262.4359}},
     -52.7547, 2.0453,
     {{{351.4493, 344.0458, 571.2344, 150.4788},
       {635.6146, 240.1619, 1239.8628, 1129.5893},
       {1543.2323, 239.5065, 3449.1164, 3598.3715},
       {2365.1982, 381.0451, 7347.7911, 3598.3715},
       {5000.3625, 899.5949, 7078.6005, 3044.7926},
       {7229.7167, 899.5949, 18631.8945, 2308.9867}}}},
    {"c3 closed", "c3", "closed", 44100.0, 65.2546, 8,
     {{65.2725, 131.2180, 195.8176, 261.7630, 327.7084, 392.9810, 458.2535, 524.1989}},
     -99.9952, 2.7851,
     {{{142.0230, 60.9687, 839.7553, 3598.3715},
       {560.1314, 272.4185, 1874.4393, 709.9495},
       {1441.1622, 874.1064, 3449.1164, 1800.0541},
       {2300.4078, 829.0282, 7347.7911, 2289.9204},
       {5000.3625, 899.5949, 7273.3459, 1694.3299},
       {7610.8300, 899.5949, 14555.7706, 3598.3715}}}},
    {"c3 open", "c3", "open", 44100.0, 0.0000, 0,
     {{0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}},
     -52.0475, 3.0138,
     {{{399.0520, 284.4300, 547.0576, 171.7955},
       {661.4700, 309.4178, 1225.0507, 1003.1704},
       {1565.1529, 246.7753, 3449.1164, 3598.3715},
       {2339.7906, 318.1522, 7347.7911, 3598.3715},
       {5000.3625, 899.5949, 7580.7527, 3598.3715},
       {7340.7402, 899.5949, 18152.1079, 1756.0297}}}},
    {"c4 closed", "c4", "closed", 44100.0, 130.8457, 8,
     {{130.5450, 261.7630, 392.3080, 524.1989, 655.4169, 786.6348, 917.8528, 1048.3978}},
     -102.4558, 2.8447,
     {{{160.5193, 96.9075, 839.7553, 3473.5811},
       {560.1314, 208.7155, 1874.4393, 789.0872},
       {1400.2719, 663.3120, 3449.1164, 2330.3034},
       {2300.4078, 828.3728, 6968.7722, 942.0926},
       {5648.0506, 899.5949, 6342.8660, 964.4158},
       {7075.9885, 899.5949, 13753.3521, 3598.3715}}}},
    {"c4 open", "c4", "open", 44100.0, 130.8457, 8,
     {{131.2180, 262.4359, 392.9810, 524.1989, 655.4169, 786.6348, 917.8528, 1049.0707}},
     -53.5815, 2.5482,
     {{{399.8731, 139.2093, 464.6739, 182.9571},
       {778.0692, 395.5826, 1153.3654, 762.5006},
       {1698.3566, 331.2241, 3449.1164, 3598.3715},
       {2631.5216, 190.7461, 7347.7911, 3401.3417},
       {5178.5539, 899.5949, 6737.1145, 1662.4736},
       {6968.2641, 899.5949, 18578.3450, 2687.2370}}}},
    {"c5 closed", "c5", "closed", 44100.0, 261.6913, 8,
     {{261.7630, 524.1989, 786.6348, 1049.0707, 1310.8337, 1573.2697, 1835.7056, 2097.4686}},
     -104.6822, 2.9026,
     {{{218.3032, 168.2844, 839.7553, 2123.1184},
       {609.3841, 163.3869, 1874.4393, 671.1950},
       {1412.0600, 809.3429, 3449.1164, 1640.1504},
       {2337.2476, 899.5949, 5959.9828, 502.7645},
       {5749.8620, 899.5949, 6552.4458, 925.0392},
       {6553.8622, 899.5949, 11780.1464, 3598.3715}}}},
    {"c5 open", "c5", "open", 44100.0, 261.6913, 8,
     {{262.4359, 524.1989, 786.6348, 1049.0707, 1310.8337, 1573.2697, 1835.0327, 2097.4686}},
     -56.4143, 2.8284,
     {{{399.8731, 69.2980, 400.7376, 91.0294},
       {849.8686, 277.9714, 1008.6593, 378.9805},
       {1757.7768, 491.3875, 3449.1164, 3598.3715},
       {2839.4602, 102.2100, 7084.1085, 3598.3715},
       {5277.6568, 899.5949, 6355.3216, 3598.3715},
       {7116.6542, 899.5949, 18259.2070, 2154.9748}}}},
}};

}
