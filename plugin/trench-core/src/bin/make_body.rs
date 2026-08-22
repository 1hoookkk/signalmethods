//! make_body — compose a body from species grammar using measured P2K reference
//! geometry (dossiers/characters/P2k_*.json). Never invented numbers.
//!
//! Usage:
//!   cargo run -p trench-core --bin make-body -- \
//!     --species bass --name "Speaker Knockerz" --out body.body240

use trench_core::cascade::NUM_COEFFS;
use trench_core::minifloat::{LEGACY_CORNERS as NUM_CORNERS, LEGACY_STAGES as NUM_STAGES};
use trench_core::compiler::DEFAULT_AUTHORING_SR;
use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::{words_from_geometry, RootPair, StageGeometry};

const SR: f64 = DEFAULT_AUTHORING_SR;
const PAD: [u16; 5] = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff];

/// Per-section geometry for one corner. Zero Hz=0 means no zero (degenerate).
#[derive(Clone)]
struct SectionGeo {
    pole_hz: f64,
    pole_r: f64,
    zero_hz: f64,
    zero_r: f64,
    scale: f64, // linear b0
}

type CornerGeo = [SectionGeo; NUM_STAGES];

fn sec(pole_hz: f64, pole_r: f64, zero_hz: f64, zero_r: f64, scale_db: f64) -> SectionGeo {
    SectionGeo {
        pole_hz,
        pole_r: pole_r.min(0.999),
        zero_hz,
        zero_r: zero_r.min(0.999),
        scale: 10.0f64.powf(scale_db / 20.0),
    }
}

// ── Measured from P2k_000_Ace of Bass, datum 39062.5 Hz ──

fn bass() -> [CornerGeo; NUM_CORNERS] {
    [
        // M0_Q0: anti-bass — air-dominant, DC real pole, deep scale
        [
            sec(0.0, 0.996, 18217.0, 0.914, -12.6),
            sec(3516.0, 0.942, 15340.0, 0.559, -12.6),
            sec(10259.0, 0.954, 13416.0, 0.718, -12.6),
            sec(13647.0, 0.986, 12014.0, 0.871, -12.6),
            sec(15731.0, 0.996, 15690.0, 0.946, -12.6),
            sec(16269.0, 0.848, 17961.0, 1.0, -12.6),
        ],
        // M100_Q0: boost — full-band spread, moderate scale
        [
            sec(16748.0, 0.866, 2230.0, 0.977, -2.8),
            sec(798.0, 0.848, 2592.0, 0.919, -2.8),
            sec(4429.0, 0.857, 4871.0, 0.750, -2.8),
            sec(7981.0, 0.857, 8874.0, 0.946, -2.8),
            sec(9877.0, 0.848, 14994.0, 0.966, -2.8),
            sec(2247.0, 0.848, 9375.0, 1.0, -2.8),
        ],
        // M0_Q100: anti-bass + Q bloom — DC→87 Hz, all radii hot
        [
            sec(87.0, 0.998, 17446.0, 0.897, -13.9),
            sec(2882.0, 0.999, 3982.0, 0.848, -13.9),
            sec(15689.0, 0.998, 14151.0, 0.966, -13.9),
            sec(14240.0, 0.998, 16958.0, 0.995, -13.9),
            sec(17951.0, 0.984, 8255.0, 0.587, -13.9),
            sec(10556.0, 0.998, 17961.0, 1.0, -13.9),
        ],
        // M100_Q100: boost + Q bloom — radii pushed, scale near unity
        [
            sec(10669.0, 0.954, 1443.0, 0.931, -2.6),
            sec(871.0, 0.994, 2592.0, 0.919, -2.6),
            sec(4427.0, 0.996, 4208.0, 0.897, -2.6),
            sec(8010.0, 0.996, 8477.0, 0.942, -2.6),
            sec(9713.0, 0.976, 10078.0, 0.981, -2.6),
            sec(2222.0, 0.988, 17961.0, 1.0, -2.6),
        ],
    ]
}

// ── Measured from P2k_013_TalkingHedz ──

fn vocal() -> [CornerGeo; NUM_CORNERS] {
    [
        // M0_Q0: 'aw' — descending section order (S6=bass, S1=air)
        [
            sec(9321.0, 0.975, 347.0, 0.935, -5.0),
            sec(891.0, 0.979, 1113.0, 0.946, -5.0),
            sec(1570.0, 0.977, 2014.0, 0.960, -5.0),
            sec(2348.0, 0.997, 2971.0, 0.972, -5.0),
            sec(4607.0, 0.948, 7922.0, 0.750, -5.0),
            sec(199.0, 0.991, 6396.0, 1.0, -5.0),
        ],
        // M100_Q0: 'oui' — S2↘201, S6↗1789 (anchor relay)
        [
            sec(8376.0, 0.962, 1711.0, 0.944, -5.7),
            sec(201.0, 0.992, 806.0, 0.962, -5.7),
            sec(2363.0, 0.982, 2300.0, 0.888, -5.7),
            sec(2732.0, 0.985, 2885.0, 0.946, -5.7),
            sec(4782.0, 0.935, 5989.0, 0.685, -5.7),
            sec(1789.0, 0.997, 17313.0, 1.0, -5.7),
        ],
        // M0_Q100: 'aw' + Q — spare the air: S5 air pole at 4352 r=0.996 (not 0.999)
        [
            sec(10158.0, 0.999, 192.0, 0.923, -5.2),
            sec(953.0, 0.999, 1074.0, 0.944, -5.2),
            sec(1509.0, 0.999, 1933.0, 0.962, -5.2),
            sec(2210.0, 0.999, 2856.0, 0.976, -5.2),
            sec(4352.0, 0.996, 8049.0, 0.718, -5.2),
            sec(157.0, 0.999, 6050.0, 1.0, -5.2),
        ],
        // M100_Q100: 'oui' + Q — mouth pushed, air still spared (S5=4404 r=0.972)
        [
            sec(8989.0, 0.999, 1618.0, 0.954, -5.8),
            sec(194.0, 0.999, 787.0, 0.966, -5.8),
            sec(2139.0, 0.999, 2062.0, 0.888, -5.8),
            sec(2411.0, 0.999, 2584.0, 0.946, -5.8),
            sec(4404.0, 0.972, 5467.0, 0.638, -5.8),
            sec(1509.0, 0.999, 17313.0, 1.0, -5.8),
        ],
    ]
}

// ── Measured from P2k_006_BassBox-303 ──

fn tb303() -> [CornerGeo; NUM_CORNERS] {
    [
        // M0_Q0: sub engine + air frame
        [
            sec(0.0, 0.750, 489.0, 0.971, -6.7),
            sec(79.0, 0.962, 2316.0, 0.914, -6.7),
            sec(479.0, 0.976, 13367.0, 0.884, -6.7),
            sec(13173.0, 0.944, 15232.0, 0.801, -6.7),
            sec(15543.0, 0.935, 17433.0, 0.914, -6.7),
            sec(17356.0, 0.966, 3784.0, 1.0, -6.7),
        ],
        // M100_Q0: harmonic ladder, S1 register-jumper 0→2296 Hz
        [
            sec(2296.0, 0.729, 426.0, 0.954, -7.6),
            sec(257.0, 0.998, 1395.0, 0.940, -7.6),
            sec(1356.0, 0.971, 4170.0, 0.848, -7.6),
            sec(4170.0, 0.848, 6458.0, 0.871, -7.6),
            sec(6444.0, 0.875, 17262.0, 0.760, -7.6),
            sec(8140.0, 0.968, 17313.0, 1.0, -7.6),
        ],
        // M0_Q100: Q re-voices — sub becomes complex, poles shift Hz
        [
            sec(1203.0, 0.992, 0.0, 0.977, -6.9),
            sec(159.0, 0.989, 563.0, 0.971, -6.9),
            sec(1156.0, 0.952, 1236.0, 0.973, -6.9),
            sec(1394.0, 0.880, 2007.0, 0.978, -6.9),
            sec(2053.0, 0.962, 9724.0, 0.942, -6.9),
            sec(9919.0, 0.975, 6396.0, 1.0, -6.9),
        ],
        // M100_Q100: Q re-voices further — completely different Hz set
        [
            sec(8582.0, 0.962, 0.0, 0.979, 0.1),
            sec(364.0, 0.997, 8721.0, 0.750, 0.1),
            sec(10719.0, 0.975, 11099.0, 0.952, 0.1),
            sec(11519.0, 0.977, 12237.0, 0.685, 0.1),
            sec(12697.0, 0.966, 12367.0, 0.931, 0.1),
            sec(14652.0, 0.662, 6396.0, 1.0, 0.1),
        ],
    ]
}

// ── Measured from P2k_031_EarBender ──

fn wah() -> [CornerGeo; NUM_CORNERS] {
    [
        // M0_Q0: open air-frame lattice, near-transparent
        [
            sec(302.0, 0.857, 10959.0, 0.857, -6.8),
            sec(3723.0, 0.927, 13401.0, 0.986, -6.8),
            sec(10283.0, 0.888, 14391.0, 0.968, -6.8),
            sec(13571.0, 0.910, 16287.0, 0.919, -6.8),
            sec(14394.0, 0.910, 16811.0, 0.638, -6.8),
            sec(15837.0, 0.848, 3039.0, 1.0, -6.8),
        ],
        // M100_Q0: the bend — beating pairs 738+814 (1.7st), 5521+5910 (1.2st)
        [
            sec(6272.0, 0.484, 839.0, 0.970, -4.3),
            sec(738.0, 0.968, 1619.0, 0.972, -4.3),
            sec(2384.0, 0.952, 2374.0, 0.968, -4.3),
            sec(5910.0, 0.968, 4948.0, 0.988, -4.3),
            sec(5521.0, 0.972, 5605.0, 0.987, -4.3),
            sec(814.0, 0.968, 17961.0, 1.0, -4.3),
        ],
        // M0_Q100: Q sharpens — air poles tighten, deep scale cut
        [
            sec(45.0, 0.997, 10959.0, 0.857, -16.4),
            sec(10190.0, 0.994, 13401.0, 0.986, -16.4),
            sec(12653.0, 0.880, 14391.0, 0.968, -16.4),
            sec(13546.0, 0.994, 16287.0, 0.919, -16.4),
            sec(14560.0, 0.994, 19531.0, 0.883, -16.4),
            sec(16131.0, 0.994, 17961.0, 1.0, -16.4),
        ],
        // M100_Q100: Q re-orders the beating pairs + tightens
        [
            sec(5714.0, 0.992, 5605.0, 0.987, -6.0),
            sec(2035.0, 0.992, 1619.0, 0.972, -6.0),
            sec(1962.0, 0.914, 2374.0, 0.968, -6.0),
            sec(4553.0, 0.973, 4948.0, 0.988, -6.0),
            sec(246.0, 0.990, 839.0, 0.970, -6.0),
            sec(6281.0, 0.500, 17961.0, 1.0, -6.0),
        ],
    ]
}

// ── Measured from P2k_001_MegaSweepz ──

fn sweepz() -> [CornerGeo; NUM_CORNERS] {
    [
        // M0_Q0: air-frame lattice, near-transparent
        [
            sec(12893.0, 0.760, 13401.0, 0.986, -4.7),
            sec(10329.0, 0.944, 10959.0, 0.857, -4.7),
            sec(16277.0, 0.901, 16287.0, 0.919, -4.7),
            sec(13615.0, 0.968, 14391.0, 0.968, -4.7),
            sec(9713.0, 0.976, 16811.0, 0.638, -4.7),
            sec(1295.0, 0.771, 7162.0, 1.0, -4.7),
        ],
        // M100_Q0: the wall — everything pulled under 6 kHz, r→1.0 mids
        [
            sec(6049.0, 0.500, 5605.0, 0.987, -9.5),
            sec(1200.0, 0.998, 2374.0, 0.968, -9.5),
            sec(3475.0, 0.972, 4948.0, 0.988, -9.5),
            sec(5606.0, 0.987, 1619.0, 0.972, -9.5),
            sec(2884.0, 0.997, 839.0, 0.970, -9.5),
            sec(113.0, 0.994, 17961.0, 1.0, -9.5),
        ],
        // M0_Q100: asymmetric Q — S2 defuses (r=0.280), others hot
        [
            sec(14563.0, 0.994, 19531.0, 0.883, -2.0),
            sec(5424.0, 0.280, 10959.0, 0.857, -2.0),
            sec(2242.0, 0.956, 13401.0, 0.986, -2.0),
            sec(16522.0, 0.994, 14391.0, 0.968, -2.0),
            sec(12797.0, 0.866, 2620.0, 0.625, -2.0),
            sec(13548.0, 0.994, 9569.0, 1.0, -2.0),
        ],
        // M100_Q100: Q tightens the wall — asymmetric (S5 r=0.801 vs others 0.996+)
        [
            sec(5937.0, 0.999, 839.0, 0.970, -7.3),
            sec(2478.0, 0.996, 1619.0, 0.972, -7.3),
            sec(1609.0, 0.997, 2374.0, 0.968, -7.3),
            sec(4496.0, 0.999, 4948.0, 0.988, -7.3),
            sec(5003.0, 0.801, 5605.0, 0.987, -7.3),
            sec(176.0, 0.995, 17961.0, 1.0, -7.3),
        ],
    ]
}

// ── Cross-family recombinations (E-mu method: poses from different characters re-paired) ──

fn crossbass() -> [CornerGeo; NUM_CORNERS] {
    // AceOfBass M0 (anti-bass) + TalkingHedz M100 (oui vowel mouth)
    // Q: single bloomer on S6 from Bass, spare-the-air from Vocal
    let bass_m0   = bass();   let bass_m100   = bass();
    let vocal_m0  = vocal();  let vocal_m100  = vocal();
    [
        bass_m0[0].clone(),    // M0_Q0: anti-bass (dark, scooped)
        vocal_m100[1].clone(), // M100_Q0: 'oui' vowel mouth
        bass_m0[2].clone(),    // M0_Q100: anti-bass + bloom
        vocal_m100[3].clone(), // M100_Q100: 'oui' + Q pushed
    ]
}

fn subwall() -> [CornerGeo; NUM_CORNERS] {
    // BassBox-303 M0 (sub engine) + MegaSweepz M100 (wall collapse)
    let tb_m0     = tb303();   let tb_m100   = tb303();
    let sw_m0     = sweepz();  let sw_m100   = sweepz();
    [
        tb_m0[0].clone(),     // M0_Q0: sub engine + air frame
        sw_m100[1].clone(),   // M100_Q0: wall — everything under 6k
        tb_m0[2].clone(),     // M0_Q100: sub + Q voicing
        sw_m100[3].clone(),   // M100_Q100: wall + asymmetric Q
    ]
}

fn beatsub() -> [CornerGeo; NUM_CORNERS] {
    // EarBender M100 (beating pairs) + BassBox-303 M0 (sub engine, reversed)
    let wah_m0    = wah();     let wah_m100   = wah();
    let tb_m0     = tb303();   let tb_m100    = tb303();
    [
        wah_m100[1].clone(),  // M0_Q0: beating pairs (738+814, 5521+5910)
        tb_m0[0].clone(),     // M100_Q0: sub engine (reversed morph)
        wah_m100[3].clone(),  // M0_Q100: beating pairs + Q tightened
        tb_m0[2].clone(),     // M100_Q100: sub + Q voicing
    ]
}

fn formwall() -> [CornerGeo; NUM_CORNERS] {
    // TalkingHedz M0 (formant lattice) + MegaSweepz M100 (wall)
    let vocal_m0  = vocal();  let vocal_m100 = vocal();
    let sw_m0     = sweepz(); let sw_m100    = sweepz();
    [
        vocal_m0[0].clone(),   // M0_Q0: 'aw' formant lattice
        sw_m100[1].clone(),    // M100_Q0: wall collapse
        vocal_m0[2].clone(),   // M0_Q100: 'aw' + spare-the-air Q
        sw_m100[3].clone(),    // M100_Q100: wall + asymmetric Q
    ]
}

// ── 8 more species from P2K dossiers ──
// (generated from dossiers/characters/ — verbatim reference geometry)

fn basstracer() -> [CornerGeo; NUM_CORNERS] { [ [ sec(17683.3,0.990,489.1,0.971,-2.5),sec(529.7,0.848,2315.6,0.914,-2.5),sec(478.6,0.976,13366.6,0.884,-2.5),sec(15675.4,0.964,15232.0,0.801,-2.5),sec(13057.8,0.975,17432.7,0.914,-2.5),sec(1569.7,0.638,3335.9,1.0,-2.5)],[sec(9432.7,0.956,426.0,0.954,-5.3),sec(1344.1,0.948,1395.0,0.940,-5.3),sec(2335.7,0.718,4169.6,0.848,-5.3),sec(6366.6,0.857,6457.9,0.871,-5.3),sec(4182.0,0.829,9469.0,0.950,-5.3),sec(357.4,0.969,17960.8,1.0,-5.3)],[sec(9769.4,0.999,10045.8,0.857,-5.6),sec(0.0,0.934,0.0,0.977,-5.6),sec(1395.5,0.919,1235.7,0.973,-5.6),sec(2028.5,0.999,2006.7,0.978,-5.6),sec(1137.7,0.991,563.0,0.971,-5.6),sec(434.6,0.999,17960.8,1.0,-5.6)],[sec(12700.9,0.988,12237.2,0.685,-0.8),sec(8485.9,0.981,8720.9,0.750,-0.8),sec(14445.6,0.673,12366.8,0.931,-0.8),sec(10571.1,0.996,11099.0,0.952,-0.8),sec(11553.2,0.998,0.0,0.979,-0.8),sec(382.6,0.999,10151.4,1.0,-0.8)] ] }
fn deadringer() -> [CornerGeo; NUM_CORNERS] { [ [ sec(14366.0,0.810,2523.3,0.935,-3.9),sec(2415.4,0.995,1067.1,0.996,-3.9),sec(0.0,0.998,10729.0,0.946,-3.9),sec(15607.0,0.985,11845.7,0.914,-3.9),sec(10406.8,0.991,15201.8,0.954,-3.9),sec(11243.2,0.960,11133.6,1.0,-3.9)],[sec(3622.0,0.994,783.3,0.950,-6.8),sec(849.0,0.994,1893.3,0.729,-6.8),sec(541.1,0.980,2375.8,0.966,-6.8),sec(4371.9,0.987,3101.0,0.952,-6.8),sec(2154.0,0.983,3774.0,0.956,-6.8),sec(2841.1,0.989,10151.4,1.0,-6.8)],[sec(12516.1,0.984,2523.3,0.935,-3.3),sec(2779.8,0.993,1067.1,0.996,-3.3),sec(10335.9,0.848,10729.0,0.946,-3.3),sec(14461.1,0.982,11845.7,0.914,-3.3),sec(10978.4,0.884,15201.8,0.954,-3.3),sec(166.3,0.966,8392.2,1.0,-3.3)],[sec(4430.2,0.995,783.3,0.950,-7.4),sec(2781.1,0.992,1893.3,0.729,-7.4),sec(740.4,0.999,2375.8,0.966,-7.4),sec(3618.9,0.996,3101.0,0.952,-7.4),sec(2491.0,0.984,3774.0,0.956,-7.4),sec(587.5,0.966,17960.8,1.0,-7.4)] ] }
fn klubklassik() -> [CornerGeo; NUM_CORNERS] { [ [ sec(4615.7,0.901,1252.1,0.897,-5.1),sec(200.7,0.944,2828.9,0.944,-5.1),sec(881.8,0.931,2014.5,0.960,-5.1),sec(9395.9,0.927,2971.3,0.972,-5.1),sec(1603.9,0.931,7921.9,0.750,-5.1),sec(2315.1,0.950,2065.1,1.0,-5.1)],[sec(10686.4,0.923,0.0,0.797,-2.7),sec(3611.4,0.942,3431.0,0.999,-2.7),sec(3977.9,0.938,4145.0,0.940,-2.7),sec(13219.7,0.940,7359.8,0.829,-2.7),sec(1040.8,0.927,10780.6,0.718,-2.7),sec(5868.6,0.893,10738.0,1.0,-2.7)],[sec(4555.3,0.972,2971.3,0.972,-9.9),sec(1504.9,0.600,1252.1,0.897,-9.9),sec(1560.6,0.997,2014.5,0.960,-9.9),sec(2348.7,0.996,2828.9,0.944,-9.9),sec(870.5,0.997,7921.9,0.750,-9.9),sec(925.3,0.866,11333.4,1.0,-9.9)],[sec(13046.9,0.976,7359.8,0.829,-3.6),sec(5753.0,0.935,10780.6,0.718,-3.6),sec(3456.2,0.984,4145.0,0.940,-3.6),sec(10204.4,0.962,3431.0,0.999,-3.6),sec(3659.0,0.972,0.0,0.797,-3.6),sec(1024.8,0.966,13476.0,1.0,-3.6)] ] }
fn radiocraze() -> [CornerGeo; NUM_CORNERS] { [ [ sec(4947.6,0.988,400.6,0.997,-3.0),sec(199.1,0.946,1654.3,0.998,-3.0),sec(1043.7,0.966,2884.3,0.997,-3.0),sec(9964.6,0.718,6292.8,0.962,-3.0),sec(5273.9,0.910,8848.1,0.989,-9.0),sec(4836.1,0.976,12795.3,1.0,-3.0)],[sec(2657.9,0.998,256.3,0.996,-5.0),sec(1397.0,0.871,777.6,0.960,-11.0),sec(351.2,0.983,3156.0,0.950,-5.0),sec(4422.9,0.998,4290.8,0.950,-5.0),sec(1484.2,0.998,6121.5,0.848,-11.0),sec(3251.9,0.992,16003.3,1.0,-5.0)],[sec(7687.6,0.740,566.8,0.998,-1.0),sec(1586.9,0.848,2175.5,0.999,-1.0),sec(2591.9,0.914,8830.5,0.954,-1.0),sec(10573.0,0.781,5344.1,0.989,-1.0),sec(3938.2,0.650,11993.6,0.923,-7.0),sec(5064.2,0.940,17960.8,1.0,-1.0)],[sec(4358.8,0.993,87.2,0.999,2.4),sec(1542.3,0.985,3870.3,0.999,-15.6),sec(6508.7,0.250,5581.4,0.995,-15.6),sec(6175.2,0.998,7170.2,0.998,2.4),sec(3349.8,0.992,17757.9,0.956,-3.6),sec(3729.0,0.729,7990.8,1.0,2.4)] ] }
fn deepbouche() -> [CornerGeo; NUM_CORNERS] { [ [ sec(3483.8,0.966,3318.8,0.942,-6.2),sec(2957.7,0.983,2646.7,0.946,-6.2),sec(2543.2,0.993,2372.2,0.971,-6.2),sec(1916.5,0.993,1225.1,0.954,-6.2),sec(1561.9,0.995,373.3,0.946,-6.2),sec(227.5,0.998,17960.8,1.0,-18.3)],[sec(7746.5,0.962,6525.6,0.848,-3.1),sec(3060.4,0.986,2880.4,0.952,-3.1),sec(2781.7,0.991,2171.1,0.948,-3.1),sec(2216.5,0.993,1383.1,0.964,-3.1),sec(1655.5,0.997,406.9,0.958,-3.1),sec(329.5,0.999,10151.4,1.0,-15.1)],[sec(5110.9,0.977,3855.6,0.960,-7.0),sec(3050.8,0.992,2589.5,0.931,-7.0),sec(2211.4,0.998,2304.2,0.971,-7.0),sec(1349.8,0.998,2455.5,0.906,-7.0),sec(992.5,0.998,0.0,0.948,-7.0),sec(434.6,0.997,17960.8,1.0,-25.1)],[sec(2827.9,0.998,2723.3,0.992,-8.1),sec(3243.0,0.998,3256.9,0.989,-8.1),sec(3092.2,0.999,2000.6,0.987,-8.1),sec(2536.2,0.999,778.1,0.998,-8.1),sec(1911.5,0.999,4891.8,0.177,-8.1),sec(242.8,0.999,7581.6,1.0,-20.1)] ] }
fn acidravage() -> [CornerGeo; NUM_CORNERS] { [ [ sec(4615.7,0.901,1252.1,0.897,-6.8),sec(200.7,0.944,2014.5,0.960,-6.8),sec(881.8,0.931,2828.9,0.944,-6.8),sec(9395.9,0.927,2971.3,0.972,-6.8),sec(1603.9,0.931,7921.9,0.750,-6.8),sec(2315.1,0.950,3784.2,1.0,-6.8)],[sec(10686.4,0.923,0.0,0.801,-3.4),sec(1040.8,0.927,3431.0,0.999,-3.4),sec(3611.4,0.942,4145.0,0.940,-3.4),sec(13219.7,0.940,7359.8,0.829,-3.4),sec(3977.9,0.938,10780.6,0.718,-3.4),sec(5868.6,0.893,17960.8,1.0,-3.4)],[sec(4645.7,0.996,2971.3,0.972,-8.8),sec(9769.4,0.999,7921.9,0.750,-8.8),sec(2345.6,0.999,2014.5,0.960,-8.8),sec(1558.8,0.999,1252.1,0.897,-8.8),sec(869.7,0.999,2828.9,0.944,-8.8),sec(175.0,0.999,3335.9,1.0,-8.8)],[sec(5665.8,0.966,7359.8,0.829,-4.3),sec(10585.0,0.994,10780.6,0.718,-4.3),sec(12856.4,0.994,4145.0,0.940,-4.3),sec(4643.9,0.997,3431.0,0.999,-4.3),sec(3703.7,0.997,0.0,0.797,-4.3),sec(630.3,0.997,17960.8,1.0,-4.3)] ] }
fn razorblades() -> [CornerGeo; NUM_CORNERS] { [ [ sec(12648.5,0.993,0.0,0.939,-2.3),sec(443.2,0.980,1619.8,0.966,-2.3),sec(1383.0,0.987,6046.2,0.923,-2.3),sec(13518.5,0.976,12311.5,0.960,-2.3),sec(6182.6,0.995,13063.3,0.910,-2.3),sec(10096.3,0.948,10151.4,1.0,-2.3)],[sec(3870.8,0.999,0.0,0.594,-10.4),sec(82.6,0.982,778.8,0.995,-10.4),sec(1135.2,0.998,1845.3,0.938,-10.4),sec(3143.9,0.998,3029.9,0.968,-10.4),sec(5444.8,0.999,4581.4,0.960,-10.4),sec(2539.9,0.996,17960.8,1.0,-10.4)],[sec(14370.0,0.931,3882.9,0.771,-2.6),sec(761.4,0.906,14827.0,0.995,-2.6),sec(1603.9,0.931,934.6,0.984,-2.6),sec(16088.6,0.927,17407.7,0.997,-2.6),sec(11660.6,0.935,12690.9,0.989,-2.6),sec(15994.7,0.931,12363.2,1.0,-2.6)],[sec(6542.5,0.991,0.0,0.594,-11.1),sec(118.0,0.993,379.2,0.914,-11.1),sec(18159.8,0.980,2875.0,0.707,-11.1),sec(1382.1,0.989,12367.8,0.729,-11.1),sec(8641.2,0.990,13211.2,0.897,-11.1),sec(1879.1,0.989,17960.8,1.0,-11.1)] ] }
fn lucifersq() -> [CornerGeo; NUM_CORNERS] { [ [ sec(79.4,0.962,2315.6,0.914,-13.1),sec(17683.3,0.990,2895.7,0.857,-13.1),sec(478.6,0.976,13366.6,0.884,-13.1),sec(13057.8,0.975,15232.0,0.801,-13.1),sec(15675.4,0.964,17432.7,0.914,-13.1),sec(0.0,0.753,6729.9,1.0,-13.1)],[sec(9432.7,0.956,1395.0,0.940,-3.2),sec(357.4,0.969,426.0,0.954,-3.2),sec(3162.2,0.760,4169.6,0.848,-3.2),sec(6628.7,0.857,6457.9,0.871,-3.2),sec(4275.1,0.829,9469.0,0.950,-3.2),sec(1036.2,0.888,6947.9,1.0,-3.2)],[sec(5092.7,0.927,414.7,0.960,-4.0),sec(1377.2,0.999,1231.2,0.962,-4.0),sec(2282.3,0.996,2206.7,0.954,-4.0),sec(4139.4,0.987,3189.4,0.966,-4.0),sec(3249.5,0.994,4244.0,0.974,-4.0),sec(257.0,0.999,17960.8,1.0,-4.0)],[sec(12781.4,0.958,12366.8,0.931,2.6),sec(1171.5,0.989,0.0,0.979,2.6),sec(10124.1,0.974,11099.0,0.952,2.6),sec(11369.5,0.970,12237.2,0.685,2.6),sec(8592.7,0.960,8720.9,0.750,2.6),sec(14876.5,0.650,8787.9,1.0,2.6)] ] }

// ── Build ──

fn corner_to_stages(corner: &CornerGeo) -> [[u16; NUM_COEFFS]; NUM_STAGES] {
    let mut stages = [PAD; NUM_STAGES];
    for si in 0..NUM_STAGES {
        let s = &corner[si];
        if s.pole_hz <= 1.0 && s.pole_r <= 0.01 {
            continue; // identity stage (no pole)
        }
        let pole = if s.pole_hz > 1.0 {
            RootPair::Conjugate { hz: s.pole_hz, r: s.pole_r }
        } else if s.pole_r > 0.01 {
            // Real pole at DC
            RootPair::RealPair { root_a: s.pole_r, root_b: 0.0 }
        } else {
            RootPair::Degenerate
        };
        let zero = if s.zero_hz > 1.0 {
            RootPair::Conjugate { hz: s.zero_hz, r: s.zero_r }
        } else if s.zero_r > 0.01 && s.zero_hz <= 1.0 {
            RootPair::RealPair { root_a: s.zero_r, root_b: 0.0 }
        } else {
            RootPair::Degenerate
        };
        let geom = StageGeometry { pole, zero, scale: s.scale };
        stages[si] = words_from_geometry(&geom);
    }
    stages
}

fn certify(packed: &PackedCorners) -> (usize, usize) {
    let mut unstable = 0usize;
    let mut nonfinite = 0usize;
    let grid = trench_core::response::log_frequency_grid(20.0, SR * 0.499, 220);
    for qi in 0..33 {
        let q = qi as f64 / 32.0;
        for mi in 0..33 {
            let m = mi as f64 / 32.0;
            let mut rows =
                [trench_core::cascade::PASSTHROUGH_COEFFS; trench_core::cascade::NUM_STAGES];
            for si in 0..NUM_STAGES {
                let mut w = [0u16; 5];
                for wi in 0..5 {
                    let w00 = packed.words[0][si][wi] as f64;
                    let w10 = packed.words[1][si][wi] as f64;
                    let w01 = packed.words[2][si][wi] as f64;
                    let w11 = packed.words[3][si][wi] as f64;
                    let w0 = w00 + (w10 - w00) * m;
                    let w1 = w01 + (w11 - w01) * m;
                    let v = w0 + (w1 - w0) * q;
                    w[wi] = v.round().clamp(0.0, 65535.0) as u16;
                }
                let k = trench_core::minifloat::stage_words_to_kernel(w);
                rows[si] = trench_core::minifloat::kernel_to_biquad(k);
            }
            for &f in &grid {
                let (r, i) = trench_core::response::biquad_cascade_complex(&rows, f, SR);
                if !r.is_finite() || !i.is_finite() { nonfinite += 1; break; }
                let db = 10.0 * (r * r + i * i + 1e-30).log10();
                if db > 60.0 { unstable += 1; break; }
            }
        }
    }
    (unstable, nonfinite)
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let flag = |name: &str| -> Option<String> {
        args.iter().position(|a| a == name).and_then(|i| args.get(i + 1).cloned())
    };
    let species = flag("--species").expect("--species bass|vocal|tb303|wah|sweepz");
    let name = flag("--name").unwrap_or_else(|| "body".to_string());
    let out = flag("--out").unwrap_or_else(|| format!("{}.body240", name.replace(' ', "_")));

    let corners: [CornerGeo; NUM_CORNERS] = match species.as_str() {
        "bass" => bass(),           "vocal" => vocal(),
        "tb303" => tb303(),         "wah" => wah(),
        "sweepz" => sweepz(),
        "crossbass" => crossbass(), "subwall" => subwall(),
        "beatsub" => beatsub(),     "formwall" => formwall(),
        "basstracer" => basstracer(), "deadringer" => deadringer(),
        "klubklassik" => klubklassik(), "radiocraze" => radiocraze(),
        "deepbouche" => deepbouche(), "acidravage" => acidravage(),
        "razorblades" => razorblades(), "lucifersq" => lucifersq(),
        _ => { eprintln!("unknown species: {}", species); std::process::exit(1); }
    };

    let mut packed = [[PAD; NUM_STAGES]; NUM_CORNERS];
    for ci in 0..NUM_CORNERS {
        packed[ci] = corner_to_stages(&corners[ci]);
    }
    let pc = PackedCorners::from_legacy_words(&packed);

    let cert = certify(&pc);
    println!("Certify 33x33: {} unstable, {} nonfinite", cert.0, cert.1);
    if cert.0 > 0 || cert.1 > 0 {
        eprintln!("CERTIFICATION FAILED");
        std::process::exit(1);
    }

    let mut body = [0u8; 240];
    for ci in 0..NUM_CORNERS {
        for si in 0..NUM_STAGES {
            let base = (ci * NUM_STAGES + si) * NUM_COEFFS * 2;
            for wi in 0..NUM_COEFFS {
                let w = pc.words[ci][si][wi];
                body[base + wi * 2] = (w & 0xff) as u8;
                body[base + wi * 2 + 1] = ((w >> 8) & 0xff) as u8;
            }
        }
    }
    std::fs::write(&out, &body).expect("write body");
    println!("-> {} (240 bytes)  species={}", out, species);
    println!("DONE: {}", name);
}
