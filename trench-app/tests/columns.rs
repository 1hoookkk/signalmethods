use author_server::store::find_root;
use trench_app::plot::columns::{
    cascade_db, peak_hold_into, ColumnGrid, HZ_HI, HZ_LO, OVERSAMPLE,
};
use trench_core::minifloat::PackedCorners;
use trench_core::response::biquad_cascade_mag_db;
use trench_core::stage_law::{geometry_from_words_at, P2K_DATUM_SR};

fn klang_kling_stage(stage: usize) -> [f64; 5] {
    let root = find_root().expect("repository root");
    let bytes = std::fs::read(root.join("ref/presets/P2k_032_klang_kling.bin")).expect("preset");
    let packed = PackedCorners::from_body_bytes(&bytes).expect("body");
    geometry_from_words_at(packed.words[0][stage], P2K_DATUM_SR).biquad_at(P2K_DATUM_SR)
}

fn analytic_peak_db(biquads: &[[f64; 5]], points: usize) -> (f64, f64) {
    let ratio = HZ_HI / HZ_LO;
    let mut best = f64::NEG_INFINITY;
    let mut at = HZ_LO;
    for i in 0..points {
        let hz = HZ_LO * ratio.powf(i as f64 / (points - 1) as f64);
        let db = biquad_cascade_mag_db(biquads, hz, P2K_DATUM_SR);
        if db > best {
            best = db;
            at = hz;
        }
    }
    (best, at)
}

fn peak_of(grid: &ColumnGrid, biquads: &[[f64; 5]]) -> f64 {
    let mut cols = Vec::new();
    peak_hold_into(grid, cascade_db(grid, biquads), &mut cols);
    cols.iter().fold(f64::NEG_INFINITY, |a, &b| a.max(b as f64))
}

#[test]
fn columns_resolve_a_narrow_resonance() {
    let stage = klang_kling_stage(0);
    let one = [stage];
    let (truth, at_hz) = analytic_peak_db(&one, 400_000);

    let grid = ColumnGrid::new(1200.0, 1.0, P2K_DATUM_SR);
    let held = peak_of(&grid, &one);

    let coarse = analytic_peak_db(&one, 256).0;

    eprintln!("KlangKling S1: analytic {truth:.2} dB @ {at_hz:.1} Hz, peak-hold {held:.2} dB, 256-point {coarse:.2} dB");

    assert!(
        (held - truth).abs() <= 0.2,
        "peak-hold missed the resonance: {held:.2} vs {truth:.2} dB"
    );
    assert!(
        truth - coarse > 5.0,
        "256-point sampling was expected to undersample badly, but read {coarse:.2} vs {truth:.2} dB"
    );
}

#[test]
fn peak_hold_keeps_sign_and_never_averages() {
    let grid = ColumnGrid::new(4.0, 1.0, P2K_DATUM_SR);
    let values = [12.0, -20.0, 3.0, -1.0];
    let mut out = Vec::new();
    peak_hold_into(&grid, |i| values[i % OVERSAMPLE], &mut out);
    assert_eq!(out[0], -20.0, "expected the extremum by magnitude with its sign");
}

#[test]
fn peak_hold_breaks_on_nan() {
    let grid = ColumnGrid::new(2.0, 1.0, P2K_DATUM_SR);
    let mut out = Vec::new();
    peak_hold_into(
        &grid,
        |i| if i < OVERSAMPLE { f64::NAN } else { 5.0 },
        &mut out,
    );
    assert!(out[0].is_nan(), "a fully-NaN column must break the polyline");
    assert_eq!(out[1], 5.0);
}

#[test]
fn column_grid_is_dpi_stable() {
    for ppp in [1.0f32, 1.25, 1.5, 2.0] {
        let grid = ColumnGrid::new(1000.0, ppp, P2K_DATUM_SR);
        let left = 17.0f32;
        assert!((grid.x_of_hz(left, HZ_LO) - left).abs() < 1e-4, "lo edge at ppp {ppp}");
        assert!(
            (grid.x_of_hz(left, HZ_HI) - (left + grid.width_pt)).abs() < 1e-4,
            "hi edge at ppp {ppp}"
        );
        for hz in [40.0, 137.0, 1000.0, 5512.5, 16000.0] {
            let back = grid.hz_of_x(left, grid.x_of_hz(left, hz));
            assert!(
                (back - hz).abs() / hz < 1e-5,
                "round trip {hz} -> {back} at ppp {ppp}"
            );
        }
    }
}

#[test]
fn display_agrees_with_the_comparator() {
    let biquads: Vec<[f64; 5]> = (0..7).map(klang_kling_stage).collect();
    let grid = ColumnGrid::new(900.0, 1.0, P2K_DATUM_SR);
    let sample = cascade_db(&grid, &biquads);
    let mut worst = 0.0f64;
    for i in 0..grid.samples() {
        let hz = grid.hz_at(i);
        let want = biquad_cascade_mag_db(&biquads, hz, P2K_DATUM_SR);
        worst = worst.max((sample(i) - want).abs());
    }
    eprintln!("power-domain vs per-stage-sum: worst {worst:.3e} dB");
    assert!(
        worst <= 1e-9,
        "the display form changed the numbers: worst {worst:.3e} dB"
    );
}
