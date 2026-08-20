use std::sync::Arc;

use trench_core::cartridge::CornerData;
use trench_core::cascade::Cascade;
use trench_core::minifloat::PackedCorners;
use trench_core::response::biquad_cascade_mag_db;

use super::shared::{Shared, WordField};

pub const ENGINE_SR: f64 = trench_core::stage_law::P2K_DATUM_SR;
pub const BLOCK: usize = 128;

const SLEW_ALPHA: f64 = 0.0109;
const DUCK_BLOCKS: u32 = 16;
const DUCK_POINTS: usize = 48;
const BASE_GAIN: f64 = 0.35;
const SUSTAIN_BLOCKS: u32 = 12;
const OPEN_RATE: f64 = 0.010;
const CLOSE_RATE: f64 = 0.0016;

pub struct Engine {
    shared: Arc<Shared>,
    words: WordField,
    have_words: bool,
    slewed: [f64; 3],
    cascade: Cascade,
    rows: CornerData,
    primed: bool,
    duck_countdown: u32,
    duck_gain: f64,
    gain: f64,
    env: f64,
    voice: f64,
    hold: u32,
    seen_activity: u64,
    pink: [f64; 3],
    seed: u32,
    block: [f32; BLOCK],
    pos: usize,
}

impl Engine {
    pub fn new(shared: Arc<Shared>) -> Self {
        let mut cascade = Cascade::new();
        cascade.set_linear(true);
        Self {
            shared,
            words: [[[0u16; 5]; 7]; 8],
            have_words: false,
            slewed: [0.0; 3],
            cascade,
            rows: [[1.0, 0.0, 0.0, 0.0, 0.0]; 7],
            primed: false,
            duck_countdown: 0,
            duck_gain: 1.0,
            gain: 1.0,
            env: 0.0,
            voice: 0.0,
            hold: 0,
            seen_activity: u64::MAX,
            pink: [0.0; 3],
            seed: 22222,
            block: [0.0; BLOCK],
            pos: BLOCK,
        }
    }

    fn noise(&mut self) -> f64 {
        self.seed = self
            .seed
            .wrapping_mul(196_314_165)
            .wrapping_add(907_633_515);
        let white = (self.seed as f64 / u32::MAX as f64) * 2.0 - 1.0;
        self.pink[0] = 0.99765 * self.pink[0] + white * 0.0990460;
        self.pink[1] = 0.96300 * self.pink[1] + white * 0.2965164;
        self.pink[2] = 0.57000 * self.pink[2] + white * 1.0526913;
        (self.pink[0] + self.pink[1] + self.pink[2] + white * 0.1848) * 0.18
    }

    fn next_block(&mut self) {
        if self.shared.try_load_words(&mut self.words) {
            self.have_words = true;
        }
        if !self.have_words {
            self.block = [0.0; BLOCK];
            return;
        }

        let activity = self.shared.activity();
        if activity != self.seen_activity {
            self.seen_activity = activity;
            self.hold = SUSTAIN_BLOCKS;
        } else if self.hold > 0 {
            self.hold -= 1;
        }

        let ride = self.shared.ride();
        for k in 0..3 {
            self.slewed[k] += SLEW_ALPHA * (ride[k] as f64 - self.slewed[k]);
        }

        let packed = PackedCorners { words: self.words };
        let target = packed.interpolate_biquad(
            self.slewed[0] as f32,
            self.slewed[1] as f32,
            self.slewed[2] as f32,
        );
        if !self.primed {
            self.rows = target;
            self.cascade.snap_targets(&self.rows);
            self.primed = true;
        }

        if self.duck_countdown == 0 {
            self.duck_countdown = DUCK_BLOCKS;
            let mut peak = f64::NEG_INFINITY;
            for i in 0..DUCK_POINTS {
                let t = i as f64 / (DUCK_POINTS - 1) as f64;
                let hz = 40.0 * (16_000.0f64 / 40.0).powf(t);
                peak = peak.max(biquad_cascade_mag_db(&self.rows, hz, ENGINE_SR));
            }
            self.duck_gain = 10f64.powf(-peak.max(0.0) / 20.0);
        }
        self.duck_countdown -= 1;

        let want = if self.hold > 0 { 1.0 } else { 0.0 };
        let rate = if want > self.voice { OPEN_RATE } else { CLOSE_RATE };

        self.cascade.set_targets(&target, BLOCK);
        for i in 0..BLOCK {
            self.voice += (want - self.voice) * rate;
            self.gain += (self.duck_gain - self.gain) * 0.002;
            let n = self.noise();
            let x = n * 1.4 * self.gain * self.voice;
            let y = self.cascade.tick(x as f32) as f64;
            self.env = (self.env * 0.9995).max(y.abs());
            let limit = (0.89 / self.env.max(1e-9)).min(BASE_GAIN);
            self.block[i] = (y * limit).clamp(-1.0, 1.0) as f32;
        }
        self.rows = target;
        let _ = self.cascade.take_instability_flag();
    }

    pub fn fill(&mut self, data: &mut [f32], channels: usize) {
        for frame in data.chunks_mut(channels.max(1)) {
            if self.pos >= BLOCK {
                self.next_block();
                self.pos = 0;
            }
            let s = self.block[self.pos];
            self.pos += 1;
            for slot in frame.iter_mut() {
                *slot = s;
            }
        }
    }
}
