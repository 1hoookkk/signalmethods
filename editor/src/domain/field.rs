use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::{words_from_roots_at, StageRoots};

#[derive(Clone)]
pub struct CornerFrame {
    pub name: String,
    pub lanes: [StageRoots; NUM_STAGES],
    pub pca_score: Option<[f64; 3]>,
}

impl CornerFrame {
    pub fn new(name: String, lanes: [StageRoots; NUM_STAGES]) -> Self {
        Self {
            name,
            lanes,
            pca_score: None,
        }
    }
}

#[derive(Clone)]
pub struct Field {
    pub slots: [Option<CornerFrame>; NUM_CORNERS],
}

impl Field {
    pub fn empty() -> Self {
        Self {
            slots: Default::default(),
        }
    }

    pub fn default_factory() -> Self {
        let mut f = Self::empty();
        // Factory TalkingHedz corners
        let c0_roots = [
            StageRoots { pole_hz: 10522.9, pole_r: 0.97529, zero_hz: 391.5, zero_r: 0.93544, scale: 0.5619 },
            StageRoots { pole_hz: 1005.6, pole_r: 0.97929, zero_hz: 1256.8, zero_r: 0.94582, scale: 0.5619 },
            StageRoots { pole_hz: 1772.0, pole_r: 0.97729, zero_hz: 2274.2, zero_r: 0.96017, scale: 0.5619 },
            StageRoots { pole_hz: 2650.8, pole_r: 0.99695, zero_hz: 3354.5, zero_r: 0.97228, scale: 0.5619 },
            StageRoots { pole_hz: 5201.0, pole_r: 0.94788, zero_hz: 8943.5, zero_r: 0.75012, scale: 0.5619 },
            StageRoots { pole_hz: 225.1, pole_r: 0.99118, zero_hz: 7221.2, zero_r: 1.00000, scale: 0.5619 },
            StageRoots::IDENTITY,
        ];
        let c1_roots = [
            StageRoots { pole_hz: 9456.2, pole_r: 0.96220, zero_hz: 1931.3, zero_r: 0.94375, scale: 0.5216 },
            StageRoots { pole_hz: 226.8, pole_r: 0.99216, zero_hz: 909.6, zero_r: 0.96220, scale: 0.5216 },
            StageRoots { pole_hz: 2667.9, pole_r: 0.98228, zero_hz: 2597.2, zero_r: 0.88834, scale: 0.5216 },
            StageRoots { pole_hz: 3084.1, pole_r: 0.98525, zero_hz: 3257.5, zero_r: 0.94582, scale: 0.5216 },
            StageRoots { pole_hz: 5398.8, pole_r: 0.93544, zero_hz: 6760.9, zero_r: 0.68483, scale: 0.5216 },
            StageRoots { pole_hz: 2019.9, pole_r: 0.99658, zero_hz: 19545.6, zero_r: 1.00000, scale: 0.5216 },
            StageRoots::IDENTITY,
        ];
        let c2_roots = [
            StageRoots { pole_hz: 11467.8, pole_r: 0.99902, zero_hz: 216.9, zero_r: 0.92285, scale: 0.5487 },
            StageRoots { pole_hz: 1075.8, pole_r: 0.99911, zero_hz: 1212.1, zero_r: 0.94375, scale: 0.5487 },
            StageRoots { pole_hz: 1703.5, pole_r: 0.99915, zero_hz: 2182.3, zero_r: 0.96220, scale: 0.5487 },
            StageRoots { pole_hz: 2495.2, pole_r: 0.99918, zero_hz: 3224.6, zero_r: 0.97629, scale: 0.5487 },
            StageRoots { pole_hz: 4912.9, pole_r: 0.99633, zero_hz: 9087.3, zero_r: 0.71820, scale: 0.5487 },
            StageRoots { pole_hz: 177.6, pole_r: 0.99915, zero_hz: 6830.4, zero_r: 1.00000, scale: 0.5487 },
            StageRoots::IDENTITY,
        ];
        let c3_roots = [
            StageRoots { pole_hz: 10148.2, pole_r: 0.99911, zero_hz: 1826.6, zero_r: 0.95405, scale: 0.5115 },
            StageRoots { pole_hz: 219.3, pole_r: 0.99936, zero_hz: 888.1, zero_r: 0.96625, scale: 0.5115 },
            StageRoots { pole_hz: 2415.0, pole_r: 0.99924, zero_hz: 2328.3, zero_r: 0.88834, scale: 0.5115 },
            StageRoots { pole_hz: 2721.4, pole_r: 0.99918, zero_hz: 2917.1, zero_r: 0.94582, scale: 0.5115 },
            StageRoots { pole_hz: 4971.8, pole_r: 0.97228, zero_hz: 6171.8, zero_r: 0.63757, scale: 0.5115 },
            StageRoots { pole_hz: 1703.6, pole_r: 0.99908, zero_hz: 19545.6, zero_r: 1.00000, scale: 0.5115 },
            StageRoots::IDENTITY,
        ];

        f.slots[0] = Some(CornerFrame::new("C0 M0_Q0".into(), c0_roots));
        f.slots[1] = Some(CornerFrame::new("C1 M100_Q0".into(), c1_roots));
        f.slots[2] = Some(CornerFrame::new("C2 M0_Q100".into(), c2_roots));
        f.slots[3] = Some(CornerFrame::new("C3 M100_Q100".into(), c3_roots));
        f.slots[4] = Some(CornerFrame::new("C4 M0_Q0_T1".into(), c0_roots));
        f.slots[5] = Some(CornerFrame::new("C5 M100_Q0_T1".into(), c1_roots));
        f.slots[6] = Some(CornerFrame::new("C6 M0_Q100_T1".into(), c2_roots));
        f.slots[7] = Some(CornerFrame::new("C7 M100_Q100_T1".into(), c3_roots));
        f
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
        }
    }
}
