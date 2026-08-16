use trench_core::minifloat::PackedCorners;
use trench_core::minifloat::LEGACY_STAGES as NUM_STAGES;
use trench_core::response::biquad_cascade_complex;
use trench_core::stage_law::{words_from_geometry, RootPair, StageGeometry, DEFAULT_AUTHORING_SR};

fn conj(hz: f64, r: f64, scale: f64) -> StageGeometry {
    StageGeometry {
        pole: RootPair::Conjugate { hz, r },
        zero: RootPair::Degenerate,
        scale,
    }
}

fn distinct_body() -> PackedCorners {
    let freqs_c0 = [300.0, 700.0, 1400.0, 2600.0, 4800.0, 9000.0];
    let freqs_c1 = [420.0, 950.0, 1900.0, 3300.0, 6000.0, 11000.0];
    let mut words = [[[0u16; 5]; NUM_STAGES]; 4];
    for ci in 0..4 {
        for si in 0..NUM_STAGES {
            let base = if ci % 2 == 0 {
                freqs_c0[si]
            } else {
                freqs_c1[si]
            };
            let r = if ci >= 2 { 0.94 } else { 0.90 };
            words[ci][si] = words_from_geometry(&conj(base, r, 0.4));
        }
    }
    PackedCorners::from_legacy_words(&words)
}

fn mag_db(pc: &PackedCorners, m: f64, q: f64, f: f64) -> f64 {
    let rows = pc.interpolate_biquad(m as f32, q as f32, 0.0);
    let (re, im) = biquad_cascade_complex(&rows, f, DEFAULT_AUTHORING_SR);
    20.0 * (re * re + im * im).sqrt().max(1e-12).log10()
}

#[test]
fn endpoint_preserving_permutation_changes_interior_not_corners() {
    let base = distinct_body();

    let perm = [3usize, 1, 5, 0, 4, 2];
    let mut permuted = base.clone();
    for (slot, &old) in perm.iter().enumerate() {
        permuted.words[1][slot] = base.words[1][old];
    }

    let grid: Vec<f64> = (0..256)
        .map(|k| 30.0 * (16_000.0f64 / 30.0).powf(k as f64 / 255.0))
        .collect();

    let mut corner_max = 0.0f64;
    for &f in &grid {
        corner_max =
            corner_max.max((mag_db(&base, 1.0, 0.0, f) - mag_db(&permuted, 1.0, 0.0, f)).abs());
    }
    assert!(
        corner_max < 1e-9,
        "corner M100_Q0 transfer function changed under stage permutation: {corner_max} dB"
    );

    for &(m, q) in &[(0.0, 0.0), (0.0, 1.0), (1.0, 1.0)] {
        let mut d = 0.0f64;
        for &f in &grid {
            d = d.max((mag_db(&base, m, q, f) - mag_db(&permuted, m, q, f)).abs());
        }
        assert!(d < 1e-9, "corner ({m},{q}) changed: {d} dB");
    }

    let mut interior_max = 0.0f64;
    for &f in &grid {
        interior_max =
            interior_max.max((mag_db(&base, 0.5, 0.0, f) - mag_db(&permuted, 0.5, 0.0, f)).abs());
    }
    assert!(
        interior_max > 1.0,
        "interior transfer function did not change under stage permutation: {interior_max} dB \
         — endpoint-only scoring would be possible, which must not happen"
    );
}
