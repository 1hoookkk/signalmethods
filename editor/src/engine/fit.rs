use author::body;
use trench_core::arma_endpoint::{fit_arma, fit_arma_planned, ArmaFit, FREE, NO_ZONES};
use trench_core::cascade::NUM_STAGES;
use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::{
    geometry_from_words_at, words_from_roots_at, RootPair, StageRoots,
};

use crate::domain::document::lane_is_empty;
use crate::domain::field::Field;
use crate::engine::response::SR;

pub struct FieldReport {
    pub audit: body::Audit,
    pub frames_crown_db: f64,
    pub relative_ok: bool,
    pub is_square: bool,
    pub legacy: bool,
}

pub fn fit_frame(
    pairs: &[(f64, f64)],
    lanes: [StageRoots; NUM_STAGES],
    declared: bool,
) -> Option<ArmaFit> {
    if !declared || lanes.iter().all(lane_is_empty) {
        return fit_arma(pairs, SR);
    }
    let writable = [true; NUM_STAGES];
    fit_arma_planned(pairs, SR, &lanes, &FREE, &writable, &NO_ZONES)
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

pub fn audit_field(field: &Field) -> Option<(PackedCorners, FieldReport)> {
    let packed = field.words_at(SR)?;
    let is_square = matches!(field.completeness(), Some(true));
    let report = body::audit(&packed, SR);
    let mut frames_crown = f64::NEG_INFINITY;
    for corner in &packed.words {
        let geoms: Vec<_> = corner
            .iter()
            .map(|&w| geometry_from_words_at(w, SR))
            .collect();
        let rows: Vec<[f64; 5]> = geoms.iter().map(|g| g.biquad_at(SR)).collect();
        for i in 0..128 {
            let t = i as f64 / 127.0;
            let hz = 40.0 * (16_000.0f64 / 40.0).powf(t);
            let sum_db: f64 = rows
                .iter()
                .map(|r| crate::engine::response::row_db(r, hz, SR))
                .sum();
            frames_crown = frames_crown.max(sum_db);
        }
    }
    let relative_ok = report.interior_crown_db - frames_crown <= 12.0;
    Some((
        packed,
        FieldReport {
            audit: report,
            frames_crown_db: frames_crown,
            relative_ok,
            is_square,
            legacy: false,
        },
    ))
}
