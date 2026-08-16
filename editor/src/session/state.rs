use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::stage_law::StageRoots;

use crate::domain::document::{lane_is_empty, Document};
use crate::engine::response::SR;
use crate::session::history::History;

#[derive(Clone, Copy, Default, PartialEq, Eq)]
pub struct Selection {
    pub corner: Option<usize>,
    pub section: Option<usize>,
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
    Failed(String),
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
            selection: Selection {
                corner: Some(0),
                section: None,
            },
            audition: AuditionPosition {
                pos: [0.5, 0.5, 0.0],
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

    pub fn current_rows(&self) -> Option<[[f64; NUM_COEFFS]; NUM_STAGES]> {
        let lanes = *self.active_lanes();
        if lanes.iter().all(lane_is_empty) {
            return None;
        }
        Some(std::array::from_fn(|si| lanes[si].biquad_at(SR)))
    }

    pub fn current_interpolated_rows(&self) -> [[f64; NUM_COEFFS]; NUM_STAGES] {
        if let Some(words) = self.document.field.words_at(SR) {
            let corner_idx = self.nearest_corner();
            let geoms: Vec<_> = words.words[corner_idx]
                .iter()
                .map(|&w| trench_core::stage_law::geometry_from_words_at(w, SR))
                .collect();
            let mut rows = [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
            for (si, g) in geoms.iter().enumerate().take(NUM_STAGES) {
                rows[si] = g.biquad_at(SR);
            }
            return rows;
        }
        if let Some(rows) = self.current_rows() {
            return rows;
        }
        [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES]
    }

    pub fn nearest_corner(&self) -> usize {
        (if self.audition.pos[0] > 0.5 { 1 } else { 0 })
            | (if self.audition.pos[1] > 0.5 { 2 } else { 0 })
            | (if self.audition.pos[2] > 0.5 { 4 } else { 0 })
    }
}
