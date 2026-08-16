use author::{body, envelope, pca};
use trench_core::arma_endpoint::fit_arma;
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::StageRoots;

fn hedz_m0q0() -> [StageRoots; NUM_STAGES] {
    let mut roots = [StageRoots::IDENTITY; NUM_STAGES];
    let table = [
        (9320.86, 0.975292, 346.74, 0.935439),
        (890.72, 0.979289, 1113.22, 0.945821),
        (1569.58, 0.977293, 2014.45, 0.960167),
        (2348.03, 0.996945, 2971.28, 0.972284),
        (4606.88, 0.947884, 7921.93, 0.750122),
        (199.43, 0.991178, 6396.33, 0.999998),
    ];
    for (slot, &(pole_hz, pole_r, zero_hz, zero_r)) in table.iter().enumerate() {
        roots[slot] = StageRoots {
            pole_hz,
            pole_r,
            zero_hz,
            zero_r,
            scale: 1.0,
        };
    }
    roots
}

fn stage_db(r: &StageRoots, hz: f64, sr: f64) -> f64 {
    let c = r.biquad_at(sr);
    let w = std::f64::consts::TAU * hz / sr;
    let (cw, sw) = (w.cos(), w.sin());
    let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
    let nr = c[0] + c[1] * cw + c[2] * c2;
    let ni = -(c[1] * sw + c[2] * s2);
    let dr = 1.0 + c[3] * cw + c[4] * c2;
    let di = -(c[3] * sw + c[4] * s2);
    10.0 * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30)).log10()
}

#[test]
fn a_factory_corner_rebuilds_from_its_curve_alone() {
    let sr = 39_062.5;
    let truth = hedz_m0q0();
    let target: Vec<(f64, f64)> = envelope::grid()
        .into_iter()
        .map(|hz| (hz, truth.iter().map(|s| stage_db(s, hz, sr)).sum()))
        .collect();
    let fit = fit_arma(&target, sr).expect("cold fit converges");
    assert!(
        fit.target_rms_db < 0.5,
        "cold reproduction drifted to {:.3} dB RMS",
        fit.target_rms_db
    );
    let found_ceiling = fit
        .roots
        .iter()
        .any(|s| s.zero_r > 0.995 && (s.zero_hz / 6396.33).log2().abs() < 0.5);
    assert!(found_ceiling, "the unit-circle ceiling was not rediscovered");
    let found_span = fit.roots.iter().any(|s| {
        s.pole_r > 0.0 && s.zero_r > 0.0 && 12.0 * (s.zero_hz / s.pole_hz).log2().abs() > 40.0
    });
    assert!(found_span, "no spanning shelf geometry in the cold fit");
}

#[test]
fn a_body_recompiled_by_root_geometry_is_rate_portable() {
    use trench_core::minifloat::stage_words_to_biquad;
    use trench_core::stage_law::{geometry_from_words_at, words_from_geometry_at, words_from_roots_at};

    fn row_db(c: &[f64; 5], hz: f64, sr: f64) -> f64 {
        let w = std::f64::consts::TAU * hz / sr;
        let (cw, sw) = (w.cos(), w.sin());
        let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
        let nr = c[0] + c[1] * cw + c[2] * c2;
        let ni = -(c[1] * sw + c[2] * s2);
        let dr = 1.0 + c[3] * cw + c[4] * c2;
        let di = -(c[3] * sw + c[4] * s2);
        10.0 * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30)).log10()
    }

    use trench_core::stage_law::roots_from_words_at;

    let datum = 39_062.5;
    let truth = hedz_m0q0();
    let words0: Vec<[u16; 5]> = truth.iter().map(|r| words_from_roots_at(r, datum)).collect();
    let cents = |a: f64, b: f64| ((a / b).ln() / 2.0f64.ln() * 1200.0).abs();
    for rate in [44_100.0, 48_000.0, 96_000.0] {
        let recompiled: Vec<[u16; 5]> = words0
            .iter()
            .map(|&w| words_from_geometry_at(&geometry_from_words_at(w, datum), rate))
            .collect();
        for (si, (&w0, &wr)) in words0.iter().zip(&recompiled).enumerate() {
            let a = roots_from_words_at(w0, datum).expect("datum row is conjugate");
            let b = roots_from_words_at(wr, rate).expect("recompiled row is conjugate");
            if a.pole_r > 0.0 {
                assert!(
                    cents(b.pole_hz, a.pole_hz) < 10.0,
                    "S{si} pole moved {:.1} cents at {rate} Hz",
                    cents(b.pole_hz, a.pole_hz)
                );
                assert!((b.pole_r - a.pole_r).abs() < 5e-4, "S{si} pole radius drifted");
            }
            if a.zero_r > 0.0 {
                assert!(
                    cents(b.zero_hz, a.zero_hz) < 10.0,
                    "S{si} zero moved {:.1} cents at {rate} Hz",
                    cents(b.zero_hz, a.zero_hz)
                );
                assert!((b.zero_r - a.zero_r).abs() < 5e-4, "S{si} zero radius drifted");
            }
            assert!(
                (20.0 * (b.scale / a.scale).log10()).abs() < 0.01,
                "S{si} scale drifted at {rate} Hz"
            );
        }
        let mut sum_sq = 0.0;
        let mut worst = 0.0f64;
        let mut n = 0usize;
        for hz in envelope::grid() {
            let packed: f64 = recompiled
                .iter()
                .map(|&w| row_db(&stage_words_to_biquad(w), hz, rate))
                .sum();
            let analytic: f64 = recompiled
                .iter()
                .map(|&w| {
                    let r = roots_from_words_at(w, rate).unwrap();
                    row_db(&r.biquad_at(rate), hz, rate)
                })
                .sum();
            let d = packed - analytic;
            sum_sq += d * d;
            worst = worst.max(d.abs());
            n += 1;
        }
        let rms = (sum_sq / n as f64).sqrt();
        assert!(
            rms < 0.05 && worst < 0.25,
            "{rate} Hz packing is not transparent: rms {rms:.3} dB, worst {worst:.3} dB"
        );
    }
}

#[test]
fn a_measured_vowel_basis_becomes_a_cube_end_to_end() {
    let dir = std::path::Path::new("../recipes/vocal/dvtd/subject-1");
    let files = envelope::scan(dir);
    assert!(files.len() >= 16, "DVTD subject-1 is missing");
    let rows: Vec<Vec<f64>> = files
        .iter()
        .map(|(_, p)| envelope::on_grid(&envelope::read_vvtf(p).unwrap()))
        .collect();
    let basis = pca::fit(&rows, 3).unwrap();
    assert!(basis.explained > 0.5, "3 components explain {:.2}", basis.explained);
    let curves: [Vec<f64>; 8] = std::array::from_fn(|ci| {
        let scores: Vec<f64> = basis
            .sigma
            .iter()
            .enumerate()
            .map(|(axis, sigma)| if ci >> axis & 1 == 1 { *sigma } else { -sigma })
            .collect();
        basis.reconstruct(&scores)
    });
    let build = body::build(&curves).unwrap();
    for (ci, rms) in build.corner_rms_db.iter().enumerate() {
        assert!(*rms < 3.0, "corner {ci} fit {rms:.3} dB");
    }
    assert!(build.audit.pass(), "audit failed: {:?}", build.audit.failures);
    assert!(
        build.audit.surge_db < 12.0,
        "interior crown surged {:+.2} dB over the corners",
        build.audit.surge_db
    );
    assert_eq!(build.packed.to_native_bytes().len(), 560);
}

#[test]
fn a_mouth_fits_into_a_frame_under_lane_locks() {
    use trench_core::arma_endpoint::{fit_arma_planned, FREE, NO_ZONES};
    use trench_core::stage_law::DEFAULT_AUTHORING_SR;

    let sr = DEFAULT_AUTHORING_SR;
    let dir = std::path::Path::new("../recipes/vocal/dvtd/subject-1");
    let files = envelope::scan(dir);
    let (mouth_name, mouth_path) = files.first().expect("DVTD subject-1 is missing");
    let curve = envelope::on_grid(&envelope::read_vvtf(mouth_path).unwrap());
    let target: Vec<(f64, f64)> = envelope::grid().into_iter().zip(curve).collect();

    let cold = fit_arma(&target, sr).expect("cold fit converges");
    assert!(cold.target_rms_db < 6.0, "cold fit {:.2} dB", cold.target_rms_db);

    let active: Vec<usize> = (0..NUM_STAGES)
        .filter(|&si| cold.roots[si].pole_r > 0.0 || cold.roots[si].zero_r > 0.0)
        .collect();
    assert!(active.len() >= 3, "too few sections to impose laws on");
    let held = active[0];
    let locked = active[1];
    let zoned = active[2];

    let mut laws = [author::frame::LaneLaw::OPEN; NUM_STAGES];
    laws[held].writable = false;
    laws[locked].freedom = [false, true, true, true];
    laws[zoned].zone = [
        cold.roots[zoned].pole_hz / 1.4,
        cold.roots[zoned].pole_hz * 1.4,
    ];

    let mut freedom = FREE;
    let mut writable = [true; NUM_STAGES];
    let mut zones = NO_ZONES;
    for (si, law) in laws.iter().enumerate() {
        freedom[si] = law.freedom;
        writable[si] = law.writable;
        zones[si] = law.zone;
    }
    let fit = fit_arma_planned(&target, sr, &cold.roots, &freedom, &writable, &zones)
        .expect("planned fit converges");

    let h = (fit.roots[held], cold.roots[held]);
    assert!(
        h.0.pole_hz == h.1.pole_hz
            && h.0.pole_r == h.1.pole_r
            && h.0.zero_hz == h.1.zero_hz
            && h.0.zero_r == h.1.zero_r
            && h.0.scale == h.1.scale,
        "the held lane was touched"
    );
    assert!(
        fit.roots[locked].pole_hz == cold.roots[locked].pole_hz,
        "the locked pole frequency moved"
    );
    assert!(
        fit.roots[zoned].pole_hz >= zones[zoned][0] - 1e-9
            && fit.roots[zoned].pole_hz <= zones[zoned][1] + 1e-9,
        "the zoned pole left its corridor"
    );
    assert!(
        fit.target_rms_db < cold.target_rms_db + 3.0,
        "laws wrecked the fit: {:.2} vs {:.2}",
        fit.target_rms_db,
        cold.target_rms_db
    );

    let kept = author::frame::Frame {
        name: "m1-artifact".into(),
        sr_hz: sr,
        provenance: format!("fit of {mouth_name} under lane locks"),
        words: String::new(),
        lanes: fit.roots,
        laws,
    };
    let path = std::env::temp_dir().join("trench-m1-frame-artifact.json");
    author::frame::write(&path, &kept).unwrap();
    let reloaded = author::frame::read(&path).unwrap();
    assert_eq!(reloaded.sr_hz, sr);
    assert!(!reloaded.laws[held].writable);
    assert!(!reloaded.laws[locked].freedom[0]);
    assert!(reloaded.laws[zoned].has_zone());
    for si in 0..NUM_STAGES {
        let (a, b) = (reloaded.lanes[si], kept.lanes[si]);
        assert!(
            (a.pole_hz - b.pole_hz).abs() < 1e-9
                && (a.pole_r - b.pole_r).abs() < 1e-12
                && (a.zero_hz - b.zero_hz).abs() < 1e-9
                && (a.zero_r - b.zero_r).abs() < 1e-12
                && (a.scale - b.scale).abs() < 1e-9,
            "lane {si} did not round-trip through the frame file"
        );
    }
}
