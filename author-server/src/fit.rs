use serde_json::{json, Value};
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::{
    geometry_from_words_at, words_from_roots_at, RootPair, StageRoots, DEFAULT_AUTHORING_SR,
};

use crate::json::{curve_from_value, lanes_from_value, lanes_to_value};

pub const SR: f64 = DEFAULT_AUTHORING_SR;

const CANDIDATE_INTERVAL_MS: u128 = 120;

pub fn lane_is_empty(l: &StageRoots) -> bool {
    l.pole_r <= 0.0 && l.zero_r <= 0.0
}

pub fn snap_to_words(lanes: &mut [StageRoots; NUM_STAGES]) {
    let c = |p: RootPair| match p {
        RootPair::Conjugate { hz, r } => Some((hz, r)),
        RootPair::Degenerate => Some((0.0, 0.0)),
        RootPair::RealPair { .. } => None,
    };
    for lane in lanes.iter_mut() {
        if lane.pole_r <= 0.0 && lane.zero_r <= 0.0 {
            continue;
        }
        let g = geometry_from_words_at(words_from_roots_at(lane, SR), SR);
        if let (Some((ph, pr)), Some((zh, zr))) = (c(g.pole), c(g.zero)) {
            *lane = StageRoots {
                pole_hz: ph,
                pole_r: pr,
                zero_hz: zh,
                zero_r: zr,
                scale: g.scale,
            };
        }
    }
}

pub fn row_db(c: &[f64; 5], hz: f64, sr: f64) -> f64 {
    let w = std::f64::consts::TAU * hz / sr;
    let (cw, sw) = (w.cos(), w.sin());
    let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
    let nr = c[0] + c[1] * cw + c[2] * c2;
    let ni = -(c[1] * sw + c[2] * s2);
    let dr = 1.0 + c[3] * cw + c[4] * c2;
    let di = -(c[3] * sw + c[4] * s2);
    10.0 * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30)).log10()
}

pub fn skeleton(req: &Value) -> Result<Value, String> {
    let curve = curve_from_value(req.get("curve").ok_or("no curve")?)?;
    let mut lanes = lanes_from_value(req.get("lanes").ok_or("no lanes")?)?;
    let grid = author::envelope::grid();
    let peaks = author::formants::peaks(&grid, &curve);
    if peaks.is_empty() {
        return Err("nothing measurable to place".into());
    }
    let ceiling = trench_core::stage_law::max_contiguous_pole_radius();
    let mut placed = 0;
    let mut slot = 0;
    for peak in &peaks {
        while slot < NUM_STAGES && !lane_is_empty(&lanes[slot]) {
            slot += 1;
        }
        if slot >= NUM_STAGES {
            break;
        }
        let Some((_, r)) = trench_core::praat_endpoint::pole_from_frequency_bandwidth(
            peak.hz,
            peak.bandwidth_hz,
            SR,
        ) else {
            continue;
        };
        lanes[slot].pole_hz = peak.hz;
        lanes[slot].pole_r = r.min(ceiling);
        lanes[slot].scale = 1.0;
        placed += 1;
    }
    snap_to_words(&mut lanes);
    Ok(json!({ "lanes": lanes_to_value(&lanes), "placed": placed }))
}

pub fn stage_words(lanes: &[StageRoots; NUM_STAGES], sr: f64) -> Vec<[u16; 5]> {
    lanes.iter().map(|lane| words_from_roots_at(lane, sr)).collect()
}

pub fn laws_from_value(v: Option<&Value>) -> Result<([[bool; 4]; NUM_STAGES], [bool; NUM_STAGES], [[f64; 2]; NUM_STAGES]), String> {
    let mut freedom = trench_core::arma_endpoint::FREE;
    let mut writable = [true; NUM_STAGES];
    let mut zones = trench_core::arma_endpoint::NO_ZONES;
    let Some(v) = v else {
        return Ok((freedom, writable, zones));
    };
    let arr = v.as_array().ok_or("laws is not an array")?;
    if arr.len() != NUM_STAGES {
        return Err(format!("laws must have {NUM_STAGES} entries"));
    }
    for (i, law) in arr.iter().enumerate() {
        if let Some(w) = law.get("writable").and_then(|x| x.as_bool()) {
            writable[i] = w;
        }
        if let Some(f) = law.get("freedom").and_then(|x| x.as_array()) {
            for (j, b) in f.iter().take(4).enumerate() {
                freedom[i][j] = b.as_bool().unwrap_or(true);
            }
        }
        if let Some(z) = law.get("zone").and_then(|x| x.as_array()) {
            if z.len() == 2 {
                zones[i] = [
                    z[0].as_f64().unwrap_or(0.0),
                    z[1].as_f64().unwrap_or(f64::INFINITY),
                ];
            }
        }
    }
    Ok((freedom, writable, zones))
}

pub fn fit(req: &Value) -> Result<Value, String> {
    fit_watched(req, &mut |_| {})
}

pub fn fit_watched(req: &Value, emit: &mut dyn FnMut(Value)) -> Result<Value, String> {
    let curve = curve_from_value(req.get("curve").ok_or("no curve")?)?;
    let lanes = lanes_from_value(req.get("lanes").ok_or("no lanes")?)?;
    let cold = req.get("cold").and_then(|x| x.as_bool()).unwrap_or(false);
    let sr = req.get("sr").and_then(|x| x.as_f64()).unwrap_or(SR);
    let grid = author::envelope::grid();
    if curve.len() != grid.len() {
        return Err(format!("curve must have {} points", grid.len()));
    }
    let pairs: Vec<(f64, f64)> = grid.iter().copied().zip(curve.iter().copied()).collect();
    let mut last = std::time::Instant::now();
    let mut watch = |roots: &[StageRoots; NUM_STAGES], rms: f64| {
        if last.elapsed().as_millis() < CANDIDATE_INTERVAL_MS {
            return;
        }
        last = std::time::Instant::now();
        let words: Vec<[u16; 5]> = roots.iter().map(|l| words_from_roots_at(l, sr)).collect();
        emit(json!({ "candidate": { "words": words, "rms": rms } }));
    };
    let result = if cold || lanes.iter().all(lane_is_empty) {
        trench_core::arma_endpoint::fit_arma_pinned_pairs_watched(&pairs, sr, &[], &[], &mut watch)
    } else {
        let (freedom, writable, zones) = laws_from_value(req.get("laws"))?;
        let prefix = freedom.iter().take_while(|f| !f[0]).count();
        let pins_only = writable.iter().all(|&w| w)
            && zones.iter().all(|z| z[0] <= 0.0 && z[1].is_infinite())
            && freedom.iter().all(|f| f[1] && f[3])
            && freedom.iter().skip(prefix).all(|f| f[0])
            && prefix > 0;
        if pins_only {
            let pinned_hz: Vec<f64> = lanes[..prefix].iter().map(|l| l.pole_hz).collect();
            let mut pinned_zero_hz = [0.0; NUM_STAGES];
            for i in 0..NUM_STAGES {
                if !freedom[i][2] {
                    pinned_zero_hz[i] = lanes[i].zero_hz;
                }
            }
            trench_core::arma_endpoint::fit_arma_pinned_pairs_watched(
                &pairs,
                sr,
                &pinned_hz,
                &pinned_zero_hz,
                &mut watch,
            )
        } else {
            trench_core::arma_endpoint::fit_arma_planned_watched(
                &pairs, sr, &lanes, &freedom, &writable, &zones, &mut watch,
            )
        }
    };
    let fit = result.ok_or("optimizer did not converge")?;
    let words = stage_words(&fit.roots, sr);
    Ok(json!({
        "lanes": lanes_to_value(&fit.roots),
        "words": words,
        "target_rms_db": fit.target_rms_db,
        "intended_packed_rms_db": fit.intended_packed_rms_db,
        "sections_used": fit.sections_used,
    }))
}

pub fn response(req: &Value) -> Result<Value, String> {
    let mut lanes = lanes_from_value(req.get("lanes").ok_or("no lanes")?)?;
    let sr = req.get("sr").and_then(|x| x.as_f64()).unwrap_or(SR);
    snap_to_words(&mut lanes);
    let words = stage_words(&lanes, sr);
    Ok(json!({ "lanes": lanes_to_value(&lanes), "words": words }))
}
