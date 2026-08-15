use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::stage_law::StageRoots;

use crate::domain::document::Document;
use crate::engine::response::SR;
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

    pub fn current_rows(&self) -> Option<[[f64; NUM_COEFFS]; NUM_STAGES]> {
        let lanes = self.active_lanes();
        if lanes.iter().all(crate::domain::document::lane_is_empty) {
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
