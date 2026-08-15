use author::frame::Frame;
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::words_from_roots_at;

#[derive(Clone)]
pub struct Field {
    pub slots: [Option<Frame>; NUM_CORNERS],
}

impl Field {
    pub fn empty() -> Self {
        Self {
            slots: Default::default(),
        }
    }

    pub fn completeness(&self) -> Option<bool> {
        let square = self.slots[..4].iter().all(|s| s.is_some())
            && self.slots[4..].iter().all(|s| s.is_none());
        let cube = self.slots.iter().all(|s| s.is_some());
        if cube {
            Some(false)
        } else if square {
            Some(true)
        } else {
            None
        }
    }

    pub fn words_at(&self, rate: f64) -> Option<PackedCorners> {
        let is_square = self.completeness()?;
        let mut words = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
        for (ci, corner) in words.iter_mut().enumerate() {
            let frame = if is_square && ci >= 4 {
                self.slots[ci - 4].as_ref()
            } else {
                self.slots[ci].as_ref()
            }?;
            for (si, lane) in frame.lanes.iter().enumerate() {
                corner[si] = words_from_roots_at(lane, rate);
            }
        }
        Some(PackedCorners { words })
    }

    pub fn swap_sections(&mut self, corner: usize, a: usize, b: usize) {
        if a == b || a >= NUM_STAGES || b >= NUM_STAGES {
            return;
        }
        if let Some(f) = &mut self.slots[corner] {
            f.lanes.swap(a, b);
            f.laws.swap(a, b);
        }
    }
}
