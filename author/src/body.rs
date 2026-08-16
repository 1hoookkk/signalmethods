use trench_core::arma_endpoint::{fit_arma, fit_arma_planned, FREE, NO_ZONES};
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{pole_radius, PackedCorners, NUM_CORNERS};
use trench_core::stage_law::{words_from_roots_at, StageRoots, DEFAULT_AUTHORING_SR};

use crate::envelope;

pub const CROWN_MIN_DB: f64 = -3.0;
pub const CROWN_MAX_DB: f64 = 36.0;
pub const CROWN_PARITY_MAX_DB: f64 = 30.0;
pub const GRAY_ORDER: [usize; 8] = [0, 1, 3, 2, 6, 7, 5, 4];
const AUDIT_GRID: usize = 5;
const AUDIT_BINS: usize = 512;

#[derive(Clone)]
pub struct Build {
    pub corners: [[StageRoots; NUM_STAGES]; NUM_CORNERS],
    pub corner_rms_db: [f64; NUM_CORNERS],
    pub sections_used: usize,
    pub packed: PackedCorners,
    pub audit: Audit,
}

#[derive(Clone)]
pub struct Audit {
    pub corner_crown_db: f64,
    pub interior_crown_db: f64,
    pub surge_db: f64,
    pub crown_min_db: f64,
    pub crown_max_db: f64,
    pub parity_db: f64,
    pub stable: bool,
    pub finite: bool,
    pub failures: Vec<String>,
}

impl Audit {
    pub fn pass(&self) -> bool {
        self.failures.is_empty()
    }
}

pub fn target_from(curve: &[f64]) -> Vec<(f64, f64)> {
    envelope::grid().into_iter().zip(curve.iter().copied()).collect()
}

pub fn build(curves: &[Vec<f64>; NUM_CORNERS]) -> Result<Build, String> {
    build_progress(curves, &mut |_, _| {})
}

pub fn build_progress(
    curves: &[Vec<f64>; NUM_CORNERS],
    progress: &mut dyn FnMut(usize, f64),
) -> Result<Build, String> {
    let sr = DEFAULT_AUTHORING_SR;
    let targets: Vec<Vec<(f64, f64)>> = curves.iter().map(|c| target_from(c)).collect();

    let mut corners = [[StageRoots::IDENTITY; NUM_STAGES]; NUM_CORNERS];
    let mut corner_rms_db = [0.0; NUM_CORNERS];
    let mut sections_used = 0;

    let first = GRAY_ORDER[0];
    let fit0 = fit_arma(&targets[first], sr).ok_or("the first corner did not converge")?;
    corners[first] = fit0.roots;
    corner_rms_db[first] = fit0.target_rms_db;
    sections_used = sections_used.max(fit0.sections_used);
    progress(first, fit0.target_rms_db);

    for step in GRAY_ORDER.windows(2) {
        let (prev, ci) = (step[0], step[1]);
        let seed = corners[prev];
        let fit = fit_arma_planned(
            &targets[ci],
            sr,
            &seed,
            &FREE,
            &[true; NUM_STAGES],
            &NO_ZONES,
        )
        .ok_or_else(|| format!("corner {ci} did not converge"))?;
        corners[ci] = fit.roots;
        corner_rms_db[ci] = fit.target_rms_db;
        sections_used = sections_used.max(fit.sections_used);
        progress(ci, fit.target_rms_db);
    }

    let mut words = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
    for (ci, corner) in corners.iter().enumerate() {
        for (si, roots) in corner.iter().enumerate() {
            words[ci][si] = words_from_roots_at(roots, sr);
        }
    }
    let packed = PackedCorners { words };
    let audit = audit(&packed, sr);

    Ok(Build {
        corners,
        corner_rms_db,
        sections_used,
        packed,
        audit,
    })
}

pub(crate) fn crown_of(roots: &[StageRoots; NUM_STAGES], sr: f64) -> f64 {
    let mut rows = [[0.0; NUM_COEFFS]; NUM_STAGES];
    for (row, stage) in rows.iter_mut().zip(roots) {
        *row = stage.biquad_at(sr);
    }
    (0..AUDIT_BINS)
        .map(|i| {
            let t = i as f64 / (AUDIT_BINS - 1) as f64;
            let hz = 40.0 * (16_000.0f64 / 40.0).powf(t);
            cascade_db(&rows, hz, sr)
        })
        .fold(f64::NEG_INFINITY, f64::max)
}

fn cascade_db(rows: &[[f64; NUM_COEFFS]; NUM_STAGES], hz: f64, sr: f64) -> f64 {
    let w = std::f64::consts::TAU * hz / sr;
    let (cw, sw) = (w.cos(), w.sin());
    let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
    let mut total = 0.0;
    for r in rows {
        let nr = r[0] + r[1] * cw + r[2] * c2;
        let ni = -(r[1] * sw + r[2] * s2);
        let dr = 1.0 + r[3] * cw + r[4] * c2;
        let di = -(r[3] * sw + r[4] * s2);
        total += 10.0
            * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30)).log10();
    }
    total
}

pub fn audit(packed: &PackedCorners, sr: f64) -> Audit {
    let grid: Vec<f64> = (0..AUDIT_BINS)
        .map(|i| {
            let t = i as f64 / (AUDIT_BINS - 1) as f64;
            40.0 * (16_000.0f64 / 40.0).powf(t)
        })
        .collect();
    let mut corner_crown = f64::NEG_INFINITY;
    let mut interior_crown = f64::NEG_INFINITY;
    let mut crown_min = f64::INFINITY;
    let mut crown_max = f64::NEG_INFINITY;
    let mut stable = true;
    let mut finite = true;
    for zi in 0..3 {
        let z = zi as f64 / 2.0;
        for qi in 0..AUDIT_GRID {
            for mi in 0..AUDIT_GRID {
                let m = mi as f64 / (AUDIT_GRID - 1) as f64;
                let q = qi as f64 / (AUDIT_GRID - 1) as f64;
                let rows = packed.interpolate_biquad(m as f32, q as f32, z as f32);
                if rows.iter().flatten().any(|v| !v.is_finite()) {
                    finite = false;
                    continue;
                }
                if rows.iter().any(|r| pole_radius(r[3], r[4]) >= 1.0) {
                    stable = false;
                }
                let crown = grid
                    .iter()
                    .map(|&hz| cascade_db(&rows, hz, sr))
                    .fold(f64::NEG_INFINITY, f64::max);
                crown_min = crown_min.min(crown);
                crown_max = crown_max.max(crown);
                let at_corner = (m == 0.0 || m == 1.0)
                    && (q == 0.0 || q == 1.0)
                    && (z == 0.0 || z == 1.0);
                if at_corner {
                    corner_crown = corner_crown.max(crown);
                } else {
                    interior_crown = interior_crown.max(crown);
                }
            }
        }
    }
    let parity = crown_max - crown_min;
    let surge = interior_crown - corner_crown;
    let mut failures = Vec::new();
    if !finite {
        failures.push("nonfinite interpolated response".into());
    }
    if !stable {
        failures.push("unstable interpolated pole".into());
    }
    if crown_min < CROWN_MIN_DB || crown_max > CROWN_MAX_DB {
        failures.push(format!(
            "crown {crown_min:.2}..{crown_max:.2} dB outside {CROWN_MIN_DB:.0}..{CROWN_MAX_DB:.0}"
        ));
    }
    if parity > CROWN_PARITY_MAX_DB {
        failures.push(format!(
            "crown parity {parity:.2} dB exceeds {CROWN_PARITY_MAX_DB:.0}"
        ));
    }
    Audit {
        corner_crown_db: corner_crown,
        interior_crown_db: interior_crown,
        surge_db: surge,
        crown_min_db: crown_min,
        crown_max_db: crown_max,
        parity_db: parity,
        stable,
        finite,
        failures,
    }
}
