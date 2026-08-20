use std::sync::atomic::{AtomicU16, AtomicU32, AtomicU64, Ordering};

use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};

pub const WORD_COUNT: usize = NUM_CORNERS * NUM_STAGES * NUM_COEFFS;

pub type WordField = [[[u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];

pub struct Shared {
    seq: AtomicU32,
    words: [AtomicU16; WORD_COUNT],
    ride: [AtomicU32; 3],
    activity: AtomicU64,
}

impl Default for Shared {
    fn default() -> Self {
        Self::new()
    }
}

impl Shared {
    pub fn new() -> Self {
        Self {
            seq: AtomicU32::new(0),
            words: std::array::from_fn(|_| AtomicU16::new(0)),
            ride: std::array::from_fn(|_| AtomicU32::new(0)),
            activity: AtomicU64::new(0),
        }
    }

    pub fn store_words(&self, packed: &PackedCorners) {
        let s = self.seq.load(Ordering::Relaxed);
        self.seq.store(s.wrapping_add(1), Ordering::Release);
        let mut i = 0;
        for corner in &packed.words {
            for stage in corner {
                for w in stage {
                    self.words[i].store(*w, Ordering::Relaxed);
                    i += 1;
                }
            }
        }
        self.seq.store(s.wrapping_add(2), Ordering::Release);
    }

    pub fn try_load_words(&self, out: &mut WordField) -> bool {
        let a = self.seq.load(Ordering::Acquire);
        if a & 1 != 0 {
            return false;
        }
        let mut i = 0;
        for corner in out.iter_mut() {
            for stage in corner.iter_mut() {
                for w in stage.iter_mut() {
                    *w = self.words[i].load(Ordering::Relaxed);
                    i += 1;
                }
            }
        }
        self.seq.load(Ordering::Acquire) == a
    }

    pub fn set_ride(&self, ride: [f32; 3]) {
        for (slot, v) in self.ride.iter().zip(ride) {
            slot.store(v.to_bits(), Ordering::Relaxed);
        }
    }

    pub fn ride(&self) -> [f32; 3] {
        std::array::from_fn(|i| f32::from_bits(self.ride[i].load(Ordering::Relaxed)))
    }

    pub fn bump_activity(&self) {
        self.activity.fetch_add(1, Ordering::Relaxed);
    }

    pub fn activity(&self) -> u64 {
        self.activity.load(Ordering::Relaxed)
    }
}
