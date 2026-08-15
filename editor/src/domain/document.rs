use author::frame::LaneLaw;
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::StageRoots;

use crate::domain::field::Field;

#[derive(Clone)]
pub struct Target {
    pub name: String,
    pub curve: Vec<f64>,
}

#[derive(Clone)]
pub struct Workspace {
    pub lanes: [StageRoots; NUM_STAGES],
    pub laws: [LaneLaw; NUM_STAGES],
    pub lane_jobs: [Option<(usize, f64)>; NUM_STAGES],
    pub seed_name: Option<String>,
}

impl Workspace {
    pub fn empty() -> Self {
        Self {
            lanes: [StageRoots::IDENTITY; NUM_STAGES],
            laws: [LaneLaw::OPEN; NUM_STAGES],
            lane_jobs: [None; NUM_STAGES],
            seed_name: None,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.lanes.iter().all(lane_is_empty)
    }

    pub fn declared(&self) -> bool {
        self.lanes.iter().any(|l| !lane_is_empty(l))
            || self
                .laws
                .iter()
                .any(|l| !l.writable || l.freedom != [true; 4] || l.has_zone())
    }
}

#[derive(Clone, Copy)]
pub struct PolePair {
    pub hz: f64,
    pub r: f64,
}

#[derive(Clone, Copy)]
pub struct ZeroPair {
    pub hz: f64,
    pub r: f64,
}

#[derive(Clone)]
pub struct Document {
    pub target: Option<Target>,
    pub workspace: Workspace,
    pub pole_candidates: Vec<PolePair>,
    pub zero_candidates: Vec<ZeroPair>,
    pub field: Field,
}

impl Document {
    pub fn new() -> Self {
        Self {
            target: None,
            workspace: Workspace::empty(),
            pole_candidates: Vec::new(),
            zero_candidates: Vec::new(),
            field: Field::empty(),
        }
    }
}

pub fn lane_is_empty(l: &StageRoots) -> bool {
    l.pole_r <= 0.0 && l.zero_r <= 0.0
}
