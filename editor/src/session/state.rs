use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::stage_law::StageRoots;

use crate::domain::document::{lane_is_empty, Document};
use crate::engine::response::{row_db, SR};
use crate::session::history::History;

#[derive(Clone, Copy, Default)]
pub struct Selection {
    pub corner: Option<usize>,
    pub section: Option<usize>,
    pub entry: Option<usize>,
}

pub struct AuditionPosition {
    pub pos: [f32; 3],
    pub markers: Vec<[f32; 3]>,
    pub playing: bool,
}

pub enum FitState {
    Idle,
    Running {
        started: std::time::Instant,
        corners_done: usize,
    },
    Complete {
        rms_db: f64,
        sections: usize,
    },
    Failed(FitError),
}

pub enum FitError {
    NoTarget,
    SectionHeld(usize),
    DidNotConverge,
    SectionFoundNothing(usize),
}

pub struct Session {
    pub document: Document,
    pub selection: Selection,
    pub audition: AuditionPosition,
    pub fit: FitState,
    pub history: History,
    pub notice: Option<(bool, String)>,
}

impl Session {
    pub fn new() -> Self {
        Self {
            document: Document::new(),
            selection: Selection::default(),
            audition: AuditionPosition {
                pos: [0.5, 0.0, 0.0],
                markers: Vec::new(),
                playing: false,
            },
            fit: FitState::Idle,
            history: History::new(),
            notice: None,
        }
    }

    pub fn active_lanes(&self) -> &[StageRoots; NUM_STAGES] {
        if let Some(ci) = self.selection.corner {
            if let Some(frame) = &self.document.field.slots[ci] {
                return &frame.lanes;
            }
        }
        &self.document.workspace.lanes
    }

    pub fn active_lanes_mut(&mut self) -> &mut [StageRoots; NUM_STAGES] {
        if let Some(ci) = self.selection.corner {
            if self.document.field.slots[ci].is_some() {
                return &mut self.document.field.slots[ci].as_mut().unwrap().lanes;
            }
        }
        &mut self.document.workspace.lanes
    }

    pub fn provisional(&self) -> bool {
        self.selection.corner.is_none()
            && (!self.document.pole_candidates.is_empty()
                || !self.document.zero_candidates.is_empty())
    }

    pub fn preview(&self) -> ([StageRoots; NUM_STAGES], [bool; NUM_STAGES]) {
        let mut lanes = *self.active_lanes();
        let mut mask = [false; NUM_STAGES];
        if self.selection.corner.is_none() {
            let mut pi = 0;
            let mut zi = 0;
            for slot in 0..NUM_STAGES {
                if !lane_is_empty(&lanes[slot]) {
                    continue;
                }
                let pole = self.document.pole_candidates.get(pi).copied();
                let zero = self.document.zero_candidates.get(zi).copied();
                if pole.is_none() && zero.is_none() {
                    break;
                }
                if let Some(pp) = pole {
                    lanes[slot].pole_hz = pp.hz;
                    lanes[slot].pole_r = pp.r;
                    pi += 1;
                }
                if let Some(zp) = zero {
                    lanes[slot].zero_hz = zp.hz;
                    lanes[slot].zero_r = zp.r;
                    zi += 1;
                }
                lanes[slot].scale = 1.0;
                mask[slot] = true;
            }
            if mask.iter().any(|&m| m) {
                if let Some(t) = &self.document.target {
                    let grid = author::envelope::grid();
                    let rows: Vec<_> = lanes
                        .iter()
                        .filter(|l| !lane_is_empty(l))
                        .map(|l| l.biquad_at(SR))
                        .collect();
                    let mut diff = 0.0;
                    for (k, &hz) in grid.iter().enumerate() {
                        let sum: f64 = rows.iter().map(|r| row_db(r, hz, SR)).sum();
                        diff += t.curve[k] - sum;
                    }
                    diff /= grid.len() as f64;
                    let n = mask.iter().filter(|&&m| m).count();
                    let per = 10f64.powf(diff / (20.0 * n as f64));
                    for slot in 0..NUM_STAGES {
                        if mask[slot] {
                            lanes[slot].scale = per;
                        }
                    }
                }
            }
        }
        (lanes, mask)
    }

    pub fn current_rows(&self) -> Option<[[f64; NUM_COEFFS]; NUM_STAGES]> {
        let (lanes, _) = self.preview();
        if lanes.iter().all(lane_is_empty) {
            return None;
        }
        Some(std::array::from_fn(|si| lanes[si].biquad_at(SR)))
    }

    pub fn target_pairs(&self) -> Option<Vec<(f64, f64)>> {
        let target = self.document.target.as_ref()?;
        Some(
            author::envelope::grid()
                .into_iter()
                .zip(target.curve.iter().copied())
                .collect(),
        )
    }

    pub fn nearest_corner(&self) -> usize {
        (if self.audition.pos[0] > 0.5 { 1 } else { 0 })
            | (if self.audition.pos[1] > 0.5 { 2 } else { 0 })
            | (if self.audition.pos[2] > 0.5 { 4 } else { 0 })
    }
}
