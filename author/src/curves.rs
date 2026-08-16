use std::path::Path;

use trench_core::arma_endpoint::{fit_arma, fit_arma_planned, FREE, NO_ZONES};
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::{words_from_roots_at, StageRoots, DEFAULT_AUTHORING_SR};

use crate::envelope;

const SQUARE_ORDER: [usize; 4] = [0, 1, 3, 2];
const CORNER_KEYS: [&str; 8] = [
    "M0_Q0",
    "M100_Q0",
    "M0_Q100",
    "M100_Q100",
    "M0_Q0_T100",
    "M100_Q0_T100",
    "M0_Q100_T100",
    "M100_Q100_T100",
];

pub struct CurveBuild {
    pub name: String,
    pub is_square: bool,
    pub corner_rms_db: Vec<f64>,
    pub corners: [[StageRoots; NUM_STAGES]; NUM_CORNERS],
    pub packed: PackedCorners,
    pub audit: crate::body::Audit,
}

fn breakpoints(v: &serde_json::Value, key: &str) -> Result<Vec<(f64, f64)>, String> {
    let arr = v
        .as_array()
        .ok_or_else(|| format!("{key}: a corner curve is a list of [hz, db] points"))?;
    if arr.len() < 8 {
        return Err(format!("{key}: needs at least 8 breakpoints, has {}", arr.len()));
    }
    let mut out = Vec::with_capacity(arr.len());
    for p in arr {
        let pair = p
            .as_array()
            .filter(|a| a.len() == 2)
            .ok_or_else(|| format!("{key}: each breakpoint is [hz, db]"))?;
        let hz = pair[0].as_f64().ok_or_else(|| format!("{key}: hz not a number"))?;
        let db = pair[1].as_f64().ok_or_else(|| format!("{key}: db not a number"))?;
        if !(20.0..=20_000.0).contains(&hz) || !db.is_finite() || db.abs() > 90.0 {
            return Err(format!("{key}: breakpoint [{hz}, {db}] is out of range"));
        }
        out.push((hz, db));
    }
    if out.windows(2).any(|w| w[1].0 <= w[0].0) {
        return Err(format!("{key}: breakpoints must ascend in frequency"));
    }
    Ok(out)
}

pub fn build(path: &Path, progress: &mut dyn FnMut(&str, f64)) -> Result<CurveBuild, String> {
    let sr = DEFAULT_AUTHORING_SR;
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let doc: serde_json::Value =
        serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
    if doc.get("schema").and_then(|v| v.as_str()) != Some("trench-curve-design-v1") {
        return Err(format!("{}: not a curve design", path.display()));
    }
    let name = doc
        .get("name")
        .and_then(|v| v.as_str())
        .unwrap_or("unnamed")
        .to_string();
    let corners_doc = doc
        .get("corners")
        .ok_or_else(|| format!("{name}: no corners"))?;
    let is_square = CORNER_KEYS[4..]
        .iter()
        .all(|k| corners_doc.get(*k).is_none());
    let corner_count = if is_square { 4 } else { 8 };

    let grid = envelope::grid();
    let mut targets: Vec<Vec<(f64, f64)>> = Vec::with_capacity(corner_count);
    for key in &CORNER_KEYS[..corner_count] {
        let v = corners_doc
            .get(*key)
            .ok_or_else(|| format!("{name}: corner {key} is missing"))?;
        let points = breakpoints(v, key)?;
        let on = envelope::on_grid(&points);
        targets.push(grid.iter().copied().zip(on).collect());
    }

    let order: Vec<usize> = if is_square {
        SQUARE_ORDER.to_vec()
    } else {
        crate::body::GRAY_ORDER.to_vec()
    };

    let mut corners = [[StageRoots::IDENTITY; NUM_STAGES]; NUM_CORNERS];
    let mut corner_rms_db = vec![0.0; corner_count];
    let first = order[0];
    let fit0 = fit_arma(&targets[first], sr)
        .ok_or_else(|| format!("{name}: corner {} did not converge", CORNER_KEYS[first]))?;
    corners[first] = fit0.roots;
    corner_rms_db[first] = fit0.target_rms_db;
    progress(CORNER_KEYS[first], fit0.target_rms_db);
    for step in order.windows(2) {
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
        .ok_or_else(|| format!("{name}: corner {} did not converge", CORNER_KEYS[ci]))?;
        corners[ci] = fit.roots;
        corner_rms_db[ci] = fit.target_rms_db;
        progress(CORNER_KEYS[ci], fit.target_rms_db);
    }
    if is_square {
        for ci in 0..4 {
            corners[ci + 4] = corners[ci];
        }
    }

    let mut words = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
    for (ci, corner) in corners.iter().enumerate() {
        for (si, roots) in corner.iter().enumerate() {
            words[ci][si] = words_from_roots_at(roots, sr);
        }
    }
    let packed = PackedCorners { words };
    let audit = crate::body::audit(&packed, sr);

    Ok(CurveBuild {
        name,
        is_square,
        corner_rms_db,
        corners,
        packed,
        audit,
    })
}
