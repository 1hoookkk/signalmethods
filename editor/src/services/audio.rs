use std::sync::{Arc, Mutex};

use trench_core::cartridge::CornerData;
use trench_core::cascade::{Cascade, NUM_STAGES};

pub const TAP_LEN: usize = 4096;
const BASE_GAIN: f32 = 0.05;

pub struct AudioShared {
    pub rows: CornerData,
    pub playing: bool,
    pub gain: f32,
    pub tap: [f32; TAP_LEN],
    pub tap_pos: usize,
}

pub struct Audio {
    pub shared: Arc<Mutex<AudioShared>>,
    stream: Option<cpal::Stream>,
    pub rate: f64,
    pub error: Option<String>,
}

pub fn gain_for(rows: &CornerData, sample_rate_hz: f64) -> f32 {
    let peak_db = (0..128)
        .map(|i| {
            let t = i as f64 / 127.0;
            let hz = 40.0 * (16_000.0f64 / 40.0).powf(t);
            (0..NUM_STAGES)
                .map(|si| crate::engine::response::row_db(&rows[si], hz, sample_rate_hz))
                .sum::<f64>()
        })
        .fold(f64::MIN, f64::max);
    BASE_GAIN * 10f32.powf(-(peak_db.clamp(-40.0, 200.0) as f32) / 20.0)
}

impl Audio {
    pub fn new() -> Self {
        Self {
            shared: Arc::new(Mutex::new(AudioShared {
                rows: [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES],
                playing: false,
                gain: BASE_GAIN,
                tap: [0.0; TAP_LEN],
                tap_pos: 0,
            })),
            stream: None,
            rate: trench_core::stage_law::DEFAULT_AUTHORING_SR,
            error: None,
        }
    }

    pub fn push_rows(&mut self, rows: CornerData) {
        let gain = gain_for(&rows, self.rate);
        if let Ok(mut shared) = self.shared.lock() {
            shared.rows = rows;
            shared.gain = gain;
        }
    }

    pub fn ensure_stream(&mut self) {
        if self.stream.is_some() || self.error.is_some() {
            return;
        }
        use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
        let host = cpal::default_host();
        let Some(device) = host.default_output_device() else {
            self.error = Some("no output device".into());
            return;
        };
        let Ok(config) = device.default_output_config() else {
            self.error = Some("no output config".into());
            return;
        };
        if config.sample_format() != cpal::SampleFormat::F32 {
            self.error = Some(format!("format {:?}", config.sample_format()));
            return;
        }
        let channels = config.channels() as usize;
        self.rate = config.sample_rate().0 as f64;
        let shared = self.shared.clone();
        let mut cascade = Cascade::new();
        cascade.set_linear(true);
        let mut rng = 0x2545_F491_4F6C_DD1Du64;
        let mut pink = [0.0f64; 7];
        let mut held_playing = false;
        let mut held_rows: CornerData = [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
        let mut held_gain = BASE_GAIN;
        let mut written: Vec<f32> = Vec::new();
        let stream = device.build_output_stream(
            &config.into(),
            move |data: &mut [f32], _: &cpal::OutputCallbackInfo| {
                if let Ok(s) = shared.try_lock() {
                    held_playing = s.playing;
                    held_rows = s.rows;
                    held_gain = s.gain;
                }
                if !held_playing {
                    data.fill(0.0);
                    return;
                }
                let frames = (data.len() / channels).max(1);
                cascade.set_targets(&held_rows, frames);
                written.clear();
                for frame in data.chunks_mut(channels) {
                    rng = rng
                        .wrapping_mul(6364136223846793005)
                        .wrapping_add(1442695040888963407);
                    let white = ((rng >> 40) as f64 / (1u64 << 23) as f64) - 1.0;
                    pink[0] = 0.99886 * pink[0] + white * 0.0555179;
                    pink[1] = 0.99332 * pink[1] + white * 0.0750759;
                    pink[2] = 0.96900 * pink[2] + white * 0.1538520;
                    pink[3] = 0.86650 * pink[3] + white * 0.3104856;
                    pink[4] = 0.55000 * pink[4] + white * 0.5329522;
                    pink[5] = -0.7616 * pink[5] - white * 0.0168980;
                    let x = (pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6]
                        + white * 0.5362)
                        * 0.11;
                    pink[6] = white * 0.115926;
                    let y = (cascade.tick(x as f32) * held_gain).clamp(-1.0, 1.0);
                    written.push(y);
                    for slot in frame.iter_mut() {
                        *slot = y;
                    }
                }
                if let Ok(mut s) = shared.try_lock() {
                    for &y in &written {
                        let p = s.tap_pos;
                        s.tap[p] = y;
                        s.tap_pos = (p + 1) % TAP_LEN;
                    }
                }
            },
            |e| eprintln!("audio: {e}"),
            None,
        );
        match stream {
            Ok(s) => {
                let _ = s.play();
                self.stream = Some(s);
            }
            Err(e) => self.error = Some(format!("{e}")),
        }
    }

    pub fn tap_snapshot(&self) -> Vec<f32> {
        match self.shared.lock() {
            Ok(s) => {
                let mut out = Vec::with_capacity(TAP_LEN);
                out.extend_from_slice(&s.tap[s.tap_pos..]);
                out.extend_from_slice(&s.tap[..s.tap_pos]);
                out
            }
            Err(_) => Vec::new(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use trench_core::stage_law::StageRoots;

    #[test]
    fn coefficient_ramp_lands_on_target_within_one_callback() {
        let sr = 48_000.0;
        let hot = StageRoots {
            pole_hz: 900.0,
            pole_r: 0.998,
            zero_hz: 4_000.0,
            zero_r: 0.6,
            scale: 1.0,
        };
        let mut rows: CornerData = [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
        rows[0] = hot.biquad_at(sr);
        let mut cascade = Cascade::new();
        cascade.set_linear(true);
        for _ in 0..3 {
            let frames = 480;
            cascade.set_targets(&rows, frames);
            for _ in 0..frames {
                let y = cascade.tick(0.1);
                assert!(y.is_finite());
            }
        }
        let mut landed = [[0.0; 5]; NUM_STAGES];
        cascade.get_coeffs(&mut landed);
        for k in 0..5 {
            assert!(
                (landed[0][k] - rows[0][k]).abs() < 1e-9,
                "coefficient {k} overshot: {} vs {}",
                landed[0][k],
                rows[0][k]
            );
        }
    }

    #[test]
    fn gain_follows_the_object_peak() {
        let sr = 48_000.0;
        let flat: CornerData = [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
        assert!((gain_for(&flat, sr) - BASE_GAIN).abs() < 1e-3);
        let hot = StageRoots {
            pole_hz: 900.0,
            pole_r: 0.998,
            zero_hz: 4_000.0,
            zero_r: 0.6,
            scale: 1.0,
        };
        let mut rows = flat;
        rows[0] = hot.biquad_at(sr);
        let ducked = gain_for(&rows, sr);
        assert!(
            ducked < BASE_GAIN * 0.1,
            "a hot resonance must duck the level, got {ducked}"
        );
    }
}
