//! The LISTENER — a control-rate pitch follower.
//!
//! Watches the input stream, tracks the fundamental of a voice or monophonic
//! instrument, and emits a transposition ratio for the engine. The body stays
//! verbatim — the follow moves the frequency datum (the same Hz-anchored
//! recompile the capture rig proved) — so the filter chases the melody without
//! leaving its authored character.
//!
//! The follow law: the tracker measures the voice's pitch relative to its own
//! slow reference (where the voice has been living), glides it, and emits
//! `ratio = 2^(offset · amount)` clamped to the engine's transposition window
//! (0.5..2.0, ±12 semitones). The reference is a slow EMA (~2 s), so the
//! filter centres on the singer's home pitch and follows the melody around it;
//! unvoiced frames hold the last ratio, like a wah pedal holding its position.

/// Vocal tracking range. Outside it the frame is unvoiced (hold).
pub const LISTEN_MIN_HZ: f64 = 60.0;
pub const LISTEN_MAX_HZ: f64 = 500.0;
/// Analysis hop: 512 samples ≈ 11.6 ms at 44.1 kHz. The glide smooths; the hop
/// only bounds how often the tracker looks.
pub const LISTEN_HOP: usize = 512;
/// Autocorrelation window: 1024 samples ≈ 23 ms — at least one full period of
/// the lowest tracked note.
const WINDOW: usize = 1024;
/// Voicing gate: the peak autocorrelation must reach this fraction of the
/// zero-lag energy before the frame counts as voiced.
const VOICING_THRESHOLD: f64 = 0.35;
/// The true period is the FIRST lag reaching this fraction of the global ACF
/// peak — the classic octave-down guard on harmonic-rich signals.
const OCTAVE_GUARD: f64 = 0.9;
/// Default home pitch (A3): the reference starts here and re-centres on the
/// voice within a couple of seconds.
const DEFAULT_REF_HZ: f64 = 220.0;
/// Reference EMA time constant (seconds) — how long a pitch must be sung
/// before it becomes the new home.
const REF_TAU_S: f64 = 2.0;

pub struct PitchListener {
    buf: Vec<f32>,
    mask: usize,
    write: usize,
    written: usize,
    hop_count: usize,
    sample_rate: f64,
    min_lag: usize,
    max_lag: usize,
    acf: Vec<f64>,
    amount: f64,
    speed_ms: f64,
    glide_alpha: f64,
    ref_alpha: f64,
    log2_ref: f64,
    offset: f64,
    ratio: f64,
    f0_hz: f64,
}

impl PitchListener {
    pub fn new(sample_rate: f64) -> Self {
        let mut l = Self {
            buf: Vec::new(),
            mask: 0,
            write: 0,
            written: 0,
            hop_count: 0,
            sample_rate,
            min_lag: 0,
            max_lag: 0,
            acf: Vec::new(),
            amount: 0.0,
            speed_ms: 80.0,
            glide_alpha: 0.0,
            ref_alpha: 0.0,
            log2_ref: DEFAULT_REF_HZ.log2(),
            offset: 0.0,
            ratio: 1.0,
            f0_hz: 0.0,
        };
        l.prepare(sample_rate);
        l
    }

    pub fn prepare(&mut self, sample_rate: f64) {
        self.sample_rate = sample_rate.max(8_000.0);
        self.min_lag = (self.sample_rate / LISTEN_MAX_HZ).ceil() as usize;
        self.max_lag = (self.sample_rate / LISTEN_MIN_HZ).floor() as usize;
        let cap = (WINDOW + self.max_lag + 1).next_power_of_two();
        self.buf = vec![0.0; cap];
        self.mask = cap - 1;
        self.ref_alpha = 1.0 - (-(LISTEN_HOP as f64 / self.sample_rate) / REF_TAU_S).exp();
        self.update_glide_alpha();
        self.reset();
    }

    pub fn reset(&mut self) {
        self.write = 0;
        self.written = 0;
        self.hop_count = 0;
        self.log2_ref = DEFAULT_REF_HZ.log2();
        self.offset = 0.0;
        self.ratio = 1.0;
        self.f0_hz = 0.0;
    }

    /// amount: 0 = off (ratio pinned to 1.0). speed_ms: glide time constant
    /// (how fast the filter chases the melody).
    pub fn set(&mut self, amount: f64, speed_ms: f64) {
        self.amount = amount.clamp(0.0, 1.0);
        self.speed_ms = speed_ms.clamp(10.0, 2000.0);
        self.update_glide_alpha();
        if self.amount <= 0.0 {
            self.offset = 0.0;
            self.ratio = 1.0;
        }
    }

    fn update_glide_alpha(&mut self) {
        let tau = self.speed_ms / 1000.0;
        self.glide_alpha =
            1.0 - (-(LISTEN_HOP as f64 / self.sample_rate.max(1.0)) / tau).exp();
    }

    pub fn amount(&self) -> f64 {
        self.amount
    }

    /// Current follow ratio — 1.0 when off.
    pub fn ratio(&self) -> f64 {
        if self.amount <= 0.0 { 1.0 } else { self.ratio }
    }

    /// Last analysed voiced f0 in Hz; 0 when the last frame was unvoiced.
    pub fn f0(&self) -> f64 {
        self.f0_hz
    }

    pub fn process_block(&mut self, mono: &[f32]) {
        if self.amount <= 0.0 {
            return;
        }
        self.process_block_detect(mono);
    }

    /// Detector-only listening: tracks f0 with the ratio pinned to 1.0
    /// (amount 0). GROWL uses this — it needs the note, not a transposition.
    pub fn process_block_detect(&mut self, mono: &[f32]) {
        for &s in mono {
            self.buf[self.write & self.mask] = s;
            self.write += 1;
            self.written += 1;
        }
        self.hop_count += mono.len();
        if self.hop_count >= LISTEN_HOP {
            self.hop_count = 0;
            self.analyse();
        }
    }

    #[inline]
    fn at(&self, offset: usize) -> f32 {
        // Index 0 = newest sample; larger offsets reach further back.
        self.buf[(self.write - 1 - offset) & self.mask]
    }

    fn analyse(&mut self) {
        if self.written < WINDOW + self.max_lag {
            return; // ring not warm yet
        }
        let n_lags = self.max_lag - self.min_lag + 1;
        self.acf.resize(n_lags, 0.0);
        // Zero-lag energy is the voicing reference, NOT the first in-range lag
        // (which can be negative for harmonic-rich signals and would gate out
        // every voiced frame).
        let mut energy = 0.0f64;
        for n in 0..WINDOW {
            energy += f64::from(self.at(n)) * f64::from(self.at(n));
        }
        let mut peak = 0.0f64;
        for (k, lag) in (self.min_lag..=self.max_lag).enumerate() {
            let mut s = 0.0f64;
            for n in 0..WINDOW {
                s += f64::from(self.at(n)) * f64::from(self.at(n + lag));
            }
            self.acf[k] = s;
            if s > peak {
                peak = s;
            }
        }
        if peak / energy.max(1.0e-12) < VOICING_THRESHOLD {
            self.f0_hz = 0.0; // unvoiced — hold the glide, hold the reference
            return;
        }
        // Octave guard: the true period is the FIRST lag at 90% of the peak.
        let mut k = 0;
        while k < n_lags && self.acf[k] < OCTAVE_GUARD * peak {
            k += 1;
        }
        if k >= n_lags {
            self.f0_hz = 0.0;
            return;
        }
        // Scan to the local peak just past the crossing — the parabolic
        // interpolation is only valid ON a peak, not on the rising edge.
        while k + 1 < n_lags && self.acf[k + 1] > self.acf[k] {
            k += 1;
        }
        let mut lag_frac = (self.min_lag + k) as f64;
        if k > 0 && k + 1 < n_lags {
            let (a, b, c) = (self.acf[k - 1], self.acf[k], self.acf[k + 1]);
            let denom = a - 2.0 * b + c;
            if denom.abs() > 1.0e-12 {
                lag_frac += 0.5 * (a - c) / denom;
            }
        }
        let f0 = (self.sample_rate / lag_frac).clamp(LISTEN_MIN_HZ, LISTEN_MAX_HZ);
        self.f0_hz = f0;
        let l2 = f0.log2();
        self.log2_ref += (l2 - self.log2_ref) * self.ref_alpha;
        let target = l2 - self.log2_ref;
        self.offset += (target - self.offset) * self.glide_alpha;
        self.ratio = (2.0_f64.powf(self.offset * self.amount)).clamp(0.5, 2.0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tone(fs: f64, f0: f64, n: usize, harmonics: bool) -> Vec<f32> {
        let mut out = Vec::with_capacity(n);
        let mut phase = 0.0f64;
        for _ in 0..n {
            let mut s = (2.0 * std::f64::consts::PI * f0 * phase).sin();
            if harmonics {
                let kmax = (0.45 * fs / f0) as usize;
                for k in 2..=kmax {
                    s += (2.0 * std::f64::consts::PI * f0 * k as f64 * phase).sin() / k as f64;
                }
            }
            phase += 1.0 / fs;
            out.push(s as f32);
        }
        out
    }

    fn track_f0(sr: f64, signal: &[f32], amount: f64) -> f64 {
        let mut l = PitchListener::new(sr);
        l.set(amount, 80.0);
        l.process_block(signal);
        l.f0()
    }

    #[test]
    fn finds_sine_f0() {
        let f0 = track_f0(44_100.0, &tone(44_100.0, 220.0, 8192, false), 1.0);
        assert!(
            (f0 - 220.0).abs() < 4.0,
            "sine f0 {f0} not within 2% of 220"
        );
    }

    #[test]
    fn octave_guard_picks_fundamental_not_subharmonic() {
        // Harmonic-rich saw: the ACF peak can sit at half the period; the
        // guard must still land on the true fundamental.
        let f0 = track_f0(44_100.0, &tone(44_100.0, 220.0, 8192, true), 1.0);
        assert!(
            (f0 - 220.0).abs() < 6.0,
            "saw f0 {f0} not within 3% of 220 (octave error?)"
        );
    }

    #[test]
    fn noise_is_unvoiced_and_ratio_holds() {
        let mut l = PitchListener::new(44_100.0);
        l.set(1.0, 80.0);
        // MINSTD multiplicative LCG — genuinely decorrelated, unlike an
        // arithmetic-progression "noise" which has hidden periodicity.
        let mut state = 1u64;
        let noise: Vec<f32> = (0..8192)
            .map(|_| {
                state = state.wrapping_mul(48_271) % 2_147_483_647;
                state as f32 / 1_073_741_823.5 - 1.0
            })
            .collect();
        l.process_block(&noise);
        assert_eq!(l.f0(), 0.0, "noise must be unvoiced");
        assert_eq!(l.ratio(), 1.0, "unvoiced must hold the ratio");
    }

    #[test]
    fn follows_pitch_above_reference_then_recentres() {
        let mut l = PitchListener::new(44_100.0);
        l.set(1.0, 80.0);
        // Settle on A3 (220) for ~3 s — the reference locks onto home.
        let a3 = tone(44_100.0, 220.0, 3 * 44_100, true);
        for chunk in a3.chunks(LISTEN_HOP) {
            l.process_block(chunk);
        }
        assert!(
            (l.ratio() - 1.0).abs() < 0.05,
            "at home pitch the ratio must sit at 1.0, got {:.3}",
            l.ratio()
        );
        // An octave up (440): measure EARLY (~0.15 s), before the reference
        // drifts — the ratio should be chasing the octave (target 2.0).
        let a4 = tone(44_100.0, 440.0, (0.15 * 44_100.0) as usize, true);
        for chunk in a4.chunks(LISTEN_HOP) {
            l.process_block(chunk);
        }
        assert!(
            l.ratio() > 1.5,
            "ratio {:.3} should chase the octave (amount 1.0)",
            l.ratio()
        );
        // A long sustain eventually re-centres the reference toward 440.
        let sustained = tone(44_100.0, 440.0, 6 * 44_100, true);
        for chunk in sustained.chunks(LISTEN_HOP) {
            l.process_block(chunk);
        }
        assert!(
            l.ratio() < 1.2,
            "ratio {:.3} should re-centre toward 1.0 after ~6 s at the new pitch",
            l.ratio()
        );
    }

    #[test]
    fn amount_zero_pins_ratio_to_one() {
        let mut l = PitchListener::new(44_100.0);
        l.set(0.0, 80.0);
        l.process_block(&tone(44_100.0, 300.0, 8192, true));
        assert_eq!(l.ratio(), 1.0);
        assert_eq!(l.f0(), 0.0, "off listener must not track");
    }
}
