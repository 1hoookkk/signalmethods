use std::path::Path;

use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::{
    authoring_limits_at, snap_radius, words_from_geometry_at, RootPair, StageGeometry,
};

use crate::body::{audit, Audit};
use crate::recipes::{corner_geometry, CORNERS};

pub const AUTHORING_SR: f64 = 39_062.5;
pub const T2_BANDWIDTH_FACTOR: f64 = 0.32;
pub const T2_GAIN_LIFT_DB: f64 = 6.0;
pub const RELATIVE_MARGIN_DB: f64 = 7.0;

pub struct Extruded {
    pub name: String,
    pub packed: PackedCorners,
    pub audit: Audit,
    pub square_audit: Audit,
    pub bandwidth_exponent: f64,
    pub gain_trim_db: f64,
    pub pass: bool,
}

fn relative_pass(cube: &Audit, square: &Audit) -> bool {
    cube.finite
        && cube.stable
        && cube.crown_max_db <= square.crown_max_db + RELATIVE_MARGIN_DB
        && cube.parity_db <= square.parity_db + RELATIVE_MARGIN_DB
}

fn has_roots(g: &StageGeometry) -> bool {
    g.pole != RootPair::Degenerate || g.zero != RootPair::Degenerate
}

fn intensify(stage: &StageGeometry, exponent: f64) -> StageGeometry {
    let mut out = *stage;
    if let RootPair::Conjugate { hz, r } = out.pole {
        if r > 0.0 && r < 1.0 {
            let lifted = r.powf(exponent);
            let ceiling = authoring_limits_at(AUTHORING_SR).pole_radius_max;
            out.pole = RootPair::Conjugate {
                hz,
                r: if lifted <= ceiling { lifted } else { snap_radius(lifted) },
            };
        }
    }
    out
}

fn crown_of(stages: &[StageGeometry; NUM_STAGES]) -> f64 {
    let rows: Vec<[f64; NUM_COEFFS]> =
        stages.iter().map(|g| g.biquad_at(AUTHORING_SR)).collect();
    (0..512)
        .map(|i| {
            let t = i as f64 / 511.0;
            let hz = 40.0 * (16_000.0f64 / 40.0).powf(t);
            let w = std::f64::consts::TAU * hz / AUTHORING_SR;
            let (cw, sw) = (w.cos(), w.sin());
            let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
            rows.iter()
                .map(|r| {
                    let nr = r[0] + r[1] * cw + r[2] * c2;
                    let ni = -(r[1] * sw + r[2] * s2);
                    let dr = 1.0 + r[3] * cw + r[4] * c2;
                    let di = -(r[3] * sw + r[4] * s2);
                    10.0 * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30))
                        .log10()
                })
                .sum()
        })
        .fold(f64::NEG_INFINITY, f64::max)
}

pub fn cube_from_square(path: &Path) -> Result<Extruded, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let doc: serde_json::Value =
        serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
    let name = doc
        .get("name")
        .and_then(|v| v.as_str())
        .unwrap_or("unnamed")
        .to_string();
    let sections = doc
        .get("sections")
        .and_then(|v| v.as_array())
        .ok_or_else(|| format!("{name}: no sections"))?;

    let mut square = [[StageGeometry::IDENTITY; NUM_STAGES]; 4];
    for (ci, corner) in CORNERS.iter().enumerate() {
        for (si, section) in sections.iter().enumerate().take(NUM_STAGES) {
            square[ci][si] = corner_geometry(section, corner)
                .ok_or_else(|| format!("{name}: section {si} corner {corner} incomplete"))?;
        }
    }

    let mut square_words = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
    for ci in 0..4 {
        for si in 0..NUM_STAGES {
            square_words[ci][si] = words_from_geometry_at(&square[ci][si], AUTHORING_SR);
            square_words[ci + 4][si] = square_words[ci][si];
        }
    }
    let square_audit = audit(&PackedCorners { words: square_words }, AUTHORING_SR);

    let mut last: Option<Extruded> = None;
    for &exponent in &[T2_BANDWIDTH_FACTOR, 0.45, 0.6, 0.75, 0.9] {
        let mut words = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
        let mut worst_trim = 0.0f64;
        for ci in 0..4 {
            let mut top = square[ci];
            for stage in top.iter_mut() {
                if has_roots(stage) {
                    *stage = intensify(stage, exponent);
                }
            }
            let base_crown = crown_of(&square[ci]);
            let top_crown = crown_of(&top);
            let target = (base_crown + T2_GAIN_LIFT_DB).min(35.0);
            let trim_db = (top_crown - target).max(0.0);
            worst_trim = worst_trim.max(trim_db);
            if trim_db > 0.0 {
                let active = top.iter().filter(|s| has_roots(s)).count().max(1);
                let factor = 10f64.powf(-trim_db / 20.0 / active as f64);
                for stage in top.iter_mut() {
                    if has_roots(stage) {
                        stage.scale *= factor;
                    }
                }
            }
            for si in 0..NUM_STAGES {
                words[ci][si] = words_from_geometry_at(&square[ci][si], AUTHORING_SR);
                words[ci + 4][si] = words_from_geometry_at(&top[si], AUTHORING_SR);
            }
        }
        let packed = PackedCorners { words };
        let report = audit(&packed, AUTHORING_SR);
        let pass = relative_pass(&report, &square_audit);
        last = Some(Extruded {
            name: name.clone(),
            packed,
            audit: report,
            square_audit: square_audit.clone(),
            bandwidth_exponent: exponent,
            gain_trim_db: -worst_trim,
            pass,
        });
        if pass {
            break;
        }
    }
    last.ok_or_else(|| format!("{name}: no extrusion attempted"))
}
