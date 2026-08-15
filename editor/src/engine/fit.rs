use author::body;
use author::frame::LaneLaw;
use trench_core::arma_endpoint::{fit_arma, fit_arma_lane, fit_arma_planned, ArmaFit, FREE, NO_ZONES};
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
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
    laws: [LaneLaw; NUM_STAGES],
    declared: bool,
) -> Option<ArmaFit> {
    if !declared {
        return fit_arma(pairs, SR);
    }
    let mut freedom = FREE;
    let mut writable = [true; NUM_STAGES];
    let mut zones = NO_ZONES;
    for (si, law) in laws.iter().enumerate() {
        freedom[si] = law.freedom;
        writable[si] = law.writable;
        zones[si] = law.zone;
    }
    let mut current = lanes;
    let mut filled: Option<ArmaFit> = None;
    for si in 0..NUM_STAGES {
        if laws[si].writable && lane_is_empty(&current[si]) {
            if let Some(f) = fit_arma_lane(pairs, SR, &current, si, &laws[si].freedom, &laws[si].zone)
            {
                current = f.roots;
                filled = Some(f);
            }
        }
    }
    fit_arma_planned(pairs, SR, &current, &freedom, &writable, &zones).or(filled)
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

pub fn all_pole_laws(laws: &[LaneLaw; NUM_STAGES]) -> [LaneLaw; NUM_STAGES] {
    std::array::from_fn(|si| LaneLaw {
        writable: laws[si].writable,
        freedom: [true, true, false, false],
        zone: [0.0, f64::INFINITY],
    })
}

pub fn fit_poles_frame(
    pairs: &[(f64, f64)],
    lanes: [StageRoots; NUM_STAGES],
    laws: [LaneLaw; NUM_STAGES],
) -> Option<ArmaFit> {
    let plan = all_pole_laws(&laws);
    let mut freedom = FREE;
    let mut writable = [true; NUM_STAGES];
    let mut zones = NO_ZONES;
    for (si, law) in plan.iter().enumerate() {
        freedom[si] = law.freedom;
        writable[si] = law.writable;
        zones[si] = law.zone;
    }
    let mut current = lanes;
    for si in 0..NUM_STAGES {
        if plan[si].writable && current[si].pole_r <= 0.0 {
            let (zh, zr) = (current[si].zero_hz, current[si].zero_r);
            if let Some(f) =
                fit_arma_lane(pairs, SR, &current, si, &plan[si].freedom, &plan[si].zone)
            {
                current = f.roots;
                current[si].zero_hz = zh;
                current[si].zero_r = zr;
            }
        }
    }
    fit_arma_planned(pairs, SR, &current, &freedom, &writable, &zones)
}

pub fn fit_lane(
    pairs: &[(f64, f64)],
    lanes: [StageRoots; NUM_STAGES],
    lane: usize,
    law: LaneLaw,
) -> Option<ArmaFit> {
    fit_arma_lane(pairs, SR, &lanes, lane, &law.freedom, &law.zone)
}

pub fn audit_field(field: &Field) -> Option<(PackedCorners, FieldReport)> {
    let packed = field.words_at(SR)?;
    let is_square = matches!(field.completeness(), Some(true));
    let report = body::audit(&packed, SR);
    let mut frames_crown = f64::NEG_INFINITY;
    for frame in field.slots.iter().flatten() {
        let mut w = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
        for corner in w.iter_mut() {
            for (si, lane) in frame.lanes.iter().enumerate() {
                corner[si] = words_from_roots_at(lane, SR);
            }
        }
        let a = body::audit(&PackedCorners { words: w }, SR);
        frames_crown = frames_crown.max(a.crown_max_db);
    }
    let relative_ok = report.finite
        && report.stable
        && report.crown_max_db <= frames_crown + author::extrude::RELATIVE_MARGIN_DB;
    let legacy = is_square && packed.to_legacy_bytes().is_some();
    Some((
        packed,
        FieldReport {
            audit: report,
            frames_crown_db: frames_crown,
            relative_ok,
            is_square,
            legacy,
        },
    ))
}
