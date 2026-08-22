//! The ENV macro — a control-rate envelope follower on the input tap.
//!
//! Same shape as the LISTENER, different sense: instead of hearing pitch and
//! moving the frequency datum, this hears LEVEL and moves the MORPH wheel. The
//! body stays verbatim — only the wheel position travels — so a drum hit walks
//! the filter along its authored morph path and the tail walks it back.
//!
//! The follow law: a peak follower (fast attack, slow release) rides the input
//! block peaks; a slow EMA of that follower is the reference (where the
//! material has been living). The offset is the follower's distance ABOVE its
//! own reference, measured in dB and normalised by `ENV_RANGE_DB`, so the macro
//! behaves identically on a quiet mix and a hot one:
//!
//! ```text
//! offset = (env - ref) * amount,   clamped to +-1.0 wheel units
//! ```
//!
//! ONE follower, TWO destinations. `amount` rides the MORPH wheel; `bloom`
//! rides the PREAMP desk drive (BLOOM). They share the same peak follower, the
//! same level and the same reference — each is only a depth on that one signal,
//! so either, both or neither can be armed and they can never drift apart.
//!
//! MORPH only. Modulation never touches Q (standing ruling 2026-07-25).

/// Control hop: one follower step per cascade control block (32 samples,
/// ~0.67 ms at 48 kHz) — the same grid `set_parameters` runs on.
pub const ENV_HOP: usize = crate::cascade::BLOCK_SIZE;
/// Attack time constant: the pop. Fast enough that a kick transient is on the
/// wheel within a millisecond.
const ATTACK_MS: f64 = 1.0;
/// Default release time constant: the relax back into the tail.
const DEFAULT_RELEASE_MS: f64 = 200.0;
/// Reference EMA time constant (seconds) — how long a level must hold before
/// it becomes the material's new average. Listener-style self-centring.
const REF_TAU_S: f64 = 2.0;
/// Level floor: silence reads as this many dB rather than -inf.
const ENV_FLOOR_DB: f64 = -80.0;
/// How far above its own average the material must pop for a full wheel unit
/// of travel at amount 1.0.
const ENV_RANGE_DB: f64 = 12.0;

/// KINETIC MODE — the puck (2026-08-06).
///
/// The follower above is a POSITION follower: the wheel sits wherever the
/// envelope currently is, so it walks up on the hit and walks back down the
/// tail. It can never overshoot, orbit, or still be moving after the sound
/// stopped.
///
/// Kinetic mode makes the wheel a MASS instead. The rise in the envelope - the
/// transient itself, not its level - is an impulse of velocity; a spring pulls
/// the puck back toward where the hands parked it; friction bleeds the motion
/// away. Struck repeatedly it swings, overshoots and settles, so the wheel is
/// still travelling between hits and the groove plays the filter.
///
/// `momentum` 0 = exactly the position follower above, bit for bit.
const SPRING_HZ: f64 = 1.6;
/// Velocity added per unit of envelope RISE at momentum 1.0.
const KICK_GAIN: f64 = 26.0;
/// Friction 0 -> long orbits, 1 -> lands almost dead. Per-second damping.
const DAMP_MIN: f64 = 1.2;
const DAMP_MAX: f64 = 16.0;

pub struct EnvFollower {
    sample_rate: f64,
    attack_alpha: f64,
    release_alpha: f64,
    ref_alpha: f64,
    release_ms: f64,
    /// Peak follower, linear amplitude.
    peak: f64,
    /// Follower level in normalised dB units (dB / ENV_RANGE_DB).
    level: f64,
    /// Slow EMA of `level` — the self-centring reference.
    reference: f64,
    amount: f64,
    offset: f64,
    /// BLOOM depth — the second destination on this same follower.
    bloom_amount: f64,
    bloom_offset: f64,
    primed: bool,
    /// Q depth — the THIRD destination, and the first that is not MORPH.
    q_amount: f64,
    q_offset: f64,
    /// Kinetic state: the puck. `momentum` 0 leaves all of this idle.
    momentum: f64,
    friction: f64,
    velocity: f64,
    position: f64,
    /// Previous `above`, so the RISE can be taken as the impulse.
    above_prev: f64,
}

impl EnvFollower {
    pub fn new(sample_rate: f64) -> Self {
        let mut e = Self {
            sample_rate,
            attack_alpha: 0.0,
            release_alpha: 0.0,
            ref_alpha: 0.0,
            release_ms: DEFAULT_RELEASE_MS,
            peak: 0.0,
            level: 0.0,
            reference: 0.0,
            amount: 0.0,
            offset: 0.0,
            bloom_amount: 0.0,
            bloom_offset: 0.0,
            primed: false,
            q_amount: 0.0,
            q_offset: 0.0,
            momentum: 0.0,
            friction: 0.5,
            velocity: 0.0,
            position: 0.0,
            above_prev: 0.0,
        };
        e.prepare(sample_rate);
        e
    }

    pub fn prepare(&mut self, sample_rate: f64) {
        self.sample_rate = sample_rate.max(8_000.0);
        let hop_s = ENV_HOP as f64 / self.sample_rate;
        self.ref_alpha = 1.0 - (-hop_s / REF_TAU_S).exp();
        self.update_alphas();
        self.reset();
    }

    pub fn reset(&mut self) {
        self.peak = 0.0;
        self.level = ENV_FLOOR_DB / ENV_RANGE_DB;
        self.reference = self.level;
        self.offset = 0.0;
        self.bloom_offset = 0.0;
        self.q_offset = 0.0;
        self.velocity = 0.0;
        self.position = 0.0;
        self.above_prev = 0.0;
        self.primed = false;
    }

    fn update_alphas(&mut self) {
        let hop_s = ENV_HOP as f64 / self.sample_rate.max(1.0);
        self.attack_alpha = 1.0 - (-hop_s / (ATTACK_MS / 1000.0)).exp();
        self.release_alpha = 1.0 - (-hop_s / (self.release_ms / 1000.0)).exp();
    }

    /// amount: 0 = off (offset pinned to exactly 0, follower idle).
    /// release_ms: the relax time constant (drum vs pad).
    pub fn set(&mut self, amount: f64, release_ms: f64) {
        self.amount = amount.clamp(0.0, 1.0);
        self.release_ms = release_ms.clamp(5.0, 2000.0);
        self.update_alphas();
        if self.amount <= 0.0 {
            self.offset = 0.0;
        }
    }

    /// BLOOM depth: 0 = off (the PREAMP destination contributes exactly 0).
    /// Independent of the MORPH `amount` — same follower, second destination.
    pub fn set_bloom(&mut self, amount: f64) {
        self.bloom_amount = amount.clamp(0.0, 1.0);
        if self.bloom_amount <= 0.0 {
            self.bloom_offset = 0.0;
        }
    }

    /// Q depth — the third destination. 0 = Q is untouched, exactly.
    /// This is the ruling above being broken on purpose (2026-08-06): the
    /// puck needs two axes or it is just the old follower with a wobble.
    pub fn set_q(&mut self, amount: f64) {
        self.q_amount = amount.clamp(-1.0, 1.0);
        if self.q_amount == 0.0 {
            self.q_offset = 0.0;
        }
    }

    /// `momentum` 0 = the position follower, unchanged. Above 0 the wheel is a
    /// mass: transients throw it, a spring pulls it home, friction settles it.
    pub fn set_kinetics(&mut self, momentum: f64, friction: f64) {
        self.momentum = momentum.clamp(0.0, 1.0);
        self.friction = friction.clamp(0.0, 1.0);
        if self.momentum <= 0.0 {
            self.velocity = 0.0;
            self.position = 0.0;
        }
    }

    pub fn momentum(&self) -> f64 {
        self.momentum
    }

    /// The puck's position in wheel units. Observation only - the UI draws it.
    pub fn puck(&self) -> f64 {
        self.position
    }

    pub fn amount(&self) -> f64 {
        self.amount
    }

    pub fn bloom_amount(&self) -> f64 {
        self.bloom_amount
    }

    /// True when EITHER destination is armed — the follower runs for both off
    /// one signal, so arming one never changes what the other would have seen.
    pub fn armed(&self) -> bool {
        self.amount > 0.0 || self.bloom_amount > 0.0 || self.q_amount != 0.0
    }

    /// The follower's level in normalised dB units; the reference sits at the
    /// material's running average. Observation only.
    pub fn level(&self) -> f64 {
        self.level
    }

    pub fn reference(&self) -> f64 {
        self.reference
    }

    /// Current MORPH offset in wheel units — exactly 0 when off.
    pub fn offset(&self) -> f64 {
        if self.amount <= 0.0 { 0.0 } else { self.offset }
    }

    /// Current Q offset in wheel units — exactly 0 when off.
    pub fn q_offset(&self) -> f64 {
        if self.q_amount == 0.0 { 0.0 } else { self.q_offset }
    }

    /// Current PREAMP drive offset in drive units — exactly 0 when off.
    pub fn bloom_offset(&self) -> f64 {
        if self.bloom_amount <= 0.0 {
            0.0
        } else {
            self.bloom_offset
        }
    }

    /// One control-rate step. `peak` is the block's peak magnitude.
    pub fn advance(&mut self, peak: f32) {
        if !self.armed() {
            return;
        }
        let p = if peak.is_finite() {
            f64::from(peak.abs())
        } else {
            0.0
        };
        let alpha = if p > self.peak {
            self.attack_alpha
        } else {
            self.release_alpha
        };
        self.peak += (p - self.peak) * alpha;
        let db = (20.0 * self.peak.max(1.0e-9).log10()).max(ENV_FLOOR_DB);
        self.level = db / ENV_RANGE_DB;
        if !self.primed {
            // First hop with the macro live: centre on what is playing rather
            // than sweeping up from the silence floor.
            self.primed = true;
            self.reference = self.level;
        }
        self.reference += (self.level - self.reference) * self.ref_alpha;
        let above = self.level - self.reference;
        // BLOOM always rides the level itself: drive follows loudness, and a
        // puck swinging past its rest point must not open the desk drive.
        self.bloom_offset = (above * self.bloom_amount).clamp(-1.0, 1.0);

        let travel = if self.momentum > 0.0 {
            // The puck. The impulse is the RISE in `above` - a transient - so a
            // steady loud passage adds no velocity, only the hits do.
            let hop_s = ENV_HOP as f64 / self.sample_rate.max(1.0);
            let kick = (above - self.above_prev).max(0.0);
            let w = 2.0 * std::f64::consts::PI * SPRING_HZ;
            let damp = DAMP_MIN + (DAMP_MAX - DAMP_MIN) * self.friction;
            self.velocity += kick * self.momentum * KICK_GAIN;
            self.velocity += -w * w * self.position * hop_s;
            self.velocity -= self.velocity * damp * hop_s;
            self.position = (self.position + self.velocity * hop_s).clamp(-1.0, 1.0);
            if self.position.abs() >= 1.0 {
                self.velocity = 0.0;   // it hit the wall of the space, not a bounce
            }
            self.position
        } else {
            above
        };
        self.above_prev = above;

        self.offset = (travel * self.amount).clamp(-1.0, 1.0);
        self.q_offset = (travel * self.q_amount).clamp(-1.0, 1.0);
    }
}

/// BAND DETECTOR — what the follower is allowed to hear.
///
/// `block_peak` below hands the follower the whole mix, so the wheel opens on
/// any loud note regardless of where the body is working. For a carve that is
/// wrong: the notch should open when there is energy IN THE BAND IT IS PARKED
/// ON, and stay shut otherwise.
///
/// So the tap runs through one bandpass tuned to the cascade's own working
/// frequency, read back from the live coefficients each control block. The
/// detector therefore follows the Q wheel for free — move the notch and the
/// listening window moves with it. No FFT, no window latency: one biquad at
/// audio rate, against the six the cascade is already running.
const DETECTOR_Q: f64 = 3.0;
/// Don't recompute the trig unless the target actually moved (in cents).
const RETUNE_CENTS: f64 = 15.0;

#[derive(Clone)]
pub struct BandDetector {
    b0: f64,
    a1: f64,
    a2: f64,
    x1: f64,
    x2: f64,
    y1: f64,
    y2: f64,
    hz: f64,
}

impl Default for BandDetector {
    fn default() -> Self {
        Self::new()
    }
}

impl BandDetector {
    pub fn new() -> Self {
        Self { b0: 0.0, a1: 0.0, a2: 0.0, x1: 0.0, x2: 0.0, y1: 0.0, y2: 0.0, hz: 0.0 }
    }

    pub fn reset(&mut self) {
        self.x1 = 0.0;
        self.x2 = 0.0;
        self.y1 = 0.0;
        self.y2 = 0.0;
    }

    /// Centre frequency the detector is currently listening at. Observation.
    pub fn hz(&self) -> f64 {
        self.hz
    }

    /// RBJ constant-peak bandpass. No-ops when the target has not moved.
    pub fn tune(&mut self, hz: f64, sample_rate: f64) {
        let hz = hz.clamp(20.0, sample_rate * 0.45);
        if self.hz > 0.0 && (hz / self.hz).ln().abs() * 1731.234 < RETUNE_CENTS {
            return;
        }
        self.hz = hz;
        let w0 = 2.0 * std::f64::consts::PI * hz / sample_rate;
        let alpha = w0.sin() / (2.0 * DETECTOR_Q);
        let a0 = 1.0 + alpha;
        self.b0 = alpha / a0;
        self.a1 = -2.0 * w0.cos() / a0;
        self.a2 = (1.0 - alpha) / a0;
    }

    /// Peak magnitude of one control block, heard through the band only.
    #[inline]
    pub fn block_peak(&mut self, block: &[f32]) -> f32 {
        let mut p = 0.0f64;
        for &s in block {
            let x = f64::from(s);
            // Direct form I: b = [b0, 0, -b0].
            let y = self.b0 * (x - self.x2) - self.a1 * self.y1 - self.a2 * self.y2;
            let y = if y.is_finite() { y } else { 0.0 };
            self.x2 = self.x1;
            self.x1 = x;
            self.y2 = self.y1;
            self.y1 = y;
            let a = y.abs();
            if a > p {
                p = a;
            }
        }
        p as f32
    }
}

/// The cascade's working frequency: the pole of whichever stage is most
/// selective. For the carve bodies all six stages share one frequency, so this
/// is the notch; for a general body it is the sharpest thing in the chain.
pub fn working_hz(
    rows: &[[f64; crate::cascade::NUM_COEFFS]; crate::cascade::NUM_STAGES],
    sample_rate: f64,
) -> Option<f64> {
    let mut best: Option<(f64, f64)> = None;
    for row in rows {
        let (a1, a2) = (row[3], row[4]);
        if a2 <= 1.0e-9 || a2 >= 1.0 {
            continue;
        }
        let r = a2.sqrt();
        let cos_theta = -a1 / (2.0 * r);
        if !(-1.0..=1.0).contains(&cos_theta) {
            continue;
        }
        let hz = cos_theta.acos() * sample_rate / (2.0 * std::f64::consts::PI);
        if hz > 20.0 && best.map_or(true, |(br, _)| r > br) {
            best = Some((r, hz));
        }
    }
    best.map(|(_, hz)| hz)
}

/// THE AIMER — the wheel points itself.
///
/// A parked notch is a guess. Sibilance sits at a different frequency for every
/// voice and shifts word to word, so the band has to be found, not set.
///
/// It is found by the energy's centre of gravity, not by a pitch tracker:
/// sibilance is noise and has no pitch. For any signal
///
/// ```text
/// E[dx^2] / E[x^2] = 4 sin^2(w/2)
/// ```
///
/// exactly — so two running means and a first difference give the centroid in
/// closed form. No FFT, no bins, no window latency, ~10 operations per sample.
/// The estimate is continuous, so the wheel lands between bands rather than
/// snapping to one.
///
/// A highpass in front restricts the search, otherwise the fundamental drags
/// the centroid down out of the region worth carving.
///
/// TWO outputs, from one pass: `hz` is WHERE (drives Q) and `excess` is HOW
/// MUCH (drives MORPH) — how far the band's energy has risen above its own
/// running average, so a steady bright mix reads as nothing and a sudden ess
/// reads as a full open.
/// Where the voice stops and the ess begins — MEASURED, not chosen.
///
/// Derived from `recipes/tables/academia/`: 3188 vowel utterances across
/// Hillenbrand 1995 and Peterson-Barney 1952, men, women and children.
///
/// ```text
///   F3   p50 2770   p90 3400   p99 3900   max 4430 Hz
///   vowels with any formant above 2000 Hz:  93.3%
///                                 3000 Hz:  30.2%
///                                 4000 Hz:   0.6%
///                                 4500 Hz:   0.0%
/// ```
///
/// A 2 kHz floor — the obvious guess, and what this was first written with —
/// puts the aimer on F3 for 93% of vowels: it would hunt the singer's own voice
/// and carve it. Above 4430 Hz not one of the 3188 has a formant at all, so
/// that is the real boundary and everything above it is fricative, breath or
/// cymbal. Nothing a vowel does can reach up here.
const VOICE_CEILING_HZ: f64 = 4_500.0;
const AIM_TAU_MS: f64 = 5.0;
/// How long the search band's level must hold before it becomes the norm.
const AIM_REF_TAU_S: f64 = 1.0;
/// dB above its own average for a full open.
const AIM_RANGE_DB: f64 = 10.0;

#[derive(Clone)]
pub struct Aimer {
    sample_rate: f64,
    /// 2-pole highpass: the bottom of the search band.
    hp_a1: f64,
    hp_a2: f64,
    hp_b0: f64,
    hp_x1: f64,
    hp_x2: f64,
    hp_y1: f64,
    hp_y2: f64,
    e_x: f64,
    e_dx: f64,
    x_prev: f64,
    alpha: f64,
    ref_alpha: f64,
    level: f64,
    reference: f64,
    primed: bool,
    hz: f64,
    excess: f64,
    lo_hz: f64,
    hi_hz: f64,
}

impl Default for Aimer {
    fn default() -> Self {
        Self::new(48_000.0)
    }
}

impl Aimer {
    pub fn new(sample_rate: f64) -> Self {
        let mut a = Self {
            sample_rate,
            hp_a1: 0.0, hp_a2: 0.0, hp_b0: 0.0,
            hp_x1: 0.0, hp_x2: 0.0, hp_y1: 0.0, hp_y2: 0.0,
            e_x: 0.0, e_dx: 0.0, x_prev: 0.0,
            alpha: 0.0, ref_alpha: 0.0,
            level: 0.0, reference: 0.0, primed: false,
            hz: 0.0, excess: 0.0,
            lo_hz: VOICE_CEILING_HZ, hi_hz: 12_000.0,
        };
        a.prepare(sample_rate);
        a
    }

    /// The band to hunt within. Defaults to 2 k - 12 k (sibilance); widen it and
    /// the same machine aims at whatever else is sticking out.
    pub fn set_range(&mut self, lo_hz: f64, hi_hz: f64) {
        self.lo_hz = lo_hz.clamp(20.0, self.sample_rate * 0.45);
        self.hi_hz = hi_hz.clamp(self.lo_hz * 1.05, self.sample_rate * 0.45);
        self.design_hp();
    }

    pub fn prepare(&mut self, sample_rate: f64) {
        self.sample_rate = sample_rate.max(8_000.0);
        let dt = 1.0 / self.sample_rate;
        self.alpha = 1.0 - (-dt / (AIM_TAU_MS / 1000.0)).exp();
        self.ref_alpha = 1.0 - (-(ENV_HOP as f64 / self.sample_rate) / AIM_REF_TAU_S).exp();
        self.design_hp();
        self.reset();
    }

    fn design_hp(&mut self) {
        // Butterworth 2-pole highpass at the bottom of the search band.
        let w0 = 2.0 * std::f64::consts::PI * self.lo_hz / self.sample_rate;
        let alpha = w0.sin() / std::f64::consts::SQRT_2;
        let a0 = 1.0 + alpha;
        self.hp_b0 = (1.0 + w0.cos()) / 2.0 / a0;
        self.hp_a1 = -2.0 * w0.cos() / a0;
        self.hp_a2 = (1.0 - alpha) / a0;
    }

    pub fn reset(&mut self) {
        self.hp_x1 = 0.0; self.hp_x2 = 0.0; self.hp_y1 = 0.0; self.hp_y2 = 0.0;
        self.e_x = 0.0; self.e_dx = 0.0; self.x_prev = 0.0;
        self.level = 0.0; self.reference = 0.0; self.primed = false;
        self.hz = 0.0; self.excess = 0.0;
    }

    /// Where the energy is, in Hz. Observation.
    pub fn hz(&self) -> f64 {
        self.hz
    }

    /// Where the energy is as a wheel position 0..1, log-spaced across the
    /// search band — the mapping the bodies are authored on.
    pub fn q_position(&self) -> f64 {
        if self.hz <= 0.0 {
            return 0.0;
        }
        ((self.hz / self.lo_hz).ln() / (self.hi_hz / self.lo_hz).ln()).clamp(0.0, 1.0)
    }

    /// How far the band has popped above its own average, 0..1. Drives MORPH.
    pub fn excess(&self) -> f64 {
        self.excess
    }

    /// One control block. Returns nothing — read `q_position` and `excess`.
    pub fn advance(&mut self, block: &[f32]) {
        let (mut sum_x, mut sum_dx) = (0.0, 0.0);
        for &s in block {
            let x0 = f64::from(s);
            let y = self.hp_b0 * (x0 - 2.0 * self.hp_x1 + self.hp_x2)
                - self.hp_a1 * self.hp_y1
                - self.hp_a2 * self.hp_y2;
            let y = if y.is_finite() { y } else { 0.0 };
            self.hp_x2 = self.hp_x1;
            self.hp_x1 = x0;
            self.hp_y2 = self.hp_y1;
            self.hp_y1 = y;

            let dx = y - self.x_prev;
            self.x_prev = y;
            self.e_x += (y * y - self.e_x) * self.alpha;
            self.e_dx += (dx * dx - self.e_dx) * self.alpha;
            sum_x += y * y;
            sum_dx += dx * dx;
        }
        if !self.primed && !block.is_empty() {
            // Snap the energy means to what is actually playing. Charging them
            // up from zero would prime the reference far below the material and
            // leave the notch standing open for the first seconds.
            let n = block.len() as f64;
            self.e_x = sum_x / n;
            self.e_dx = sum_dx / n;
        }

        // w = 2 asin(sqrt(E[dx^2]/E[x^2]) / 2), exact for a sinusoid and the
        // energy centroid for noise. Guarded: silence has no centre of gravity.
        if self.e_x > 1.0e-12 {
            let ratio = (self.e_dx / self.e_x).max(0.0);
            let s = (ratio.sqrt() / 2.0).clamp(0.0, 1.0);
            let w = 2.0 * s.asin();
            let hz = w * self.sample_rate / (2.0 * std::f64::consts::PI);
            self.hz = hz.clamp(self.lo_hz, self.hi_hz);
        }

        let db = (10.0 * self.e_x.max(1.0e-18).log10()).max(ENV_FLOOR_DB);
        self.level = db / AIM_RANGE_DB;
        if !self.primed {
            self.primed = true;
            self.reference = self.level;
        }
        self.reference += (self.level - self.reference) * self.ref_alpha;
        self.excess = (self.level - self.reference).clamp(0.0, 1.0);
    }
}

/// Peak magnitude of one control block — the follower's input.
#[inline]
pub fn block_peak(block: &[f32]) -> f32 {
    let mut p = 0.0f32;
    for &s in block {
        let a = s.abs();
        if a > p {
            p = a;
        }
    }
    if p.is_finite() { p } else { 0.0 }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SR: f64 = 48_000.0;

    fn run(e: &mut EnvFollower, peak: f32, seconds: f64) {
        let hops = (seconds * SR / ENV_HOP as f64).round() as usize;
        for _ in 0..hops {
            e.advance(peak);
        }
    }

    #[test]
    fn detector_hears_its_own_band_and_rejects_the_rest() {
        let sr = 48_000.0;
        let mut d = BandDetector::new();
        d.tune(1_000.0, sr);
        // Settle, then measure: same amplitude, different frequency.
        let mut peak_at = 0.0f32;
        for block in sine(1_000.0, sr, 4_800).chunks(ENV_HOP) {
            peak_at = d.block_peak(block);
        }
        d.reset();
        let mut peak_off = 0.0f32;
        for block in sine(150.0, sr, 4_800).chunks(ENV_HOP) {
            peak_off = d.block_peak(block);
        }
        assert!(peak_at > 0.7, "in-band signal should pass, got {peak_at}");
        assert!(
            peak_off < peak_at * 0.15,
            "out-of-band {peak_off} should be far below in-band {peak_at}"
        );
    }

    #[test]
    fn working_hz_reads_the_pole_back_from_coefficients() {
        let sr = 48_000.0;
        let (hz, r) = (2_500.0, 0.99);
        let w = 2.0 * std::f64::consts::PI * hz / sr;
        let mut rows = [crate::cascade::PASSTHROUGH_COEFFS; crate::cascade::NUM_STAGES];
        rows[3] = [1.0, 0.0, 0.0, -2.0 * r * w.cos(), r * r];
        let got = working_hz(&rows, sr).expect("conjugate pole");
        assert!((got - hz).abs() < 1.0, "expected {hz} Hz, got {got}");
    }

    #[test]
    fn detector_retunes_only_when_the_target_moves() {
        let mut d = BandDetector::new();
        d.tune(1_000.0, 48_000.0);
        d.tune(1_002.0, 48_000.0); // ~3 cents: below the retune threshold
        assert_eq!(d.hz(), 1_000.0);
        d.tune(1_100.0, 48_000.0); // ~165 cents: retunes
        assert_eq!(d.hz(), 1_100.0);
    }

    #[test]
    fn aimer_finds_the_tone_it_is_pointed_at() {
        let sr = 48_000.0;
        for target in [5_000.0, 7_000.0, 10_000.0] {
            let mut a = Aimer::new(sr);
            a.set_range(VOICE_CEILING_HZ, 12_000.0);
            for block in sine(target, sr, 24_000).chunks(ENV_HOP) {
                a.advance(block);
            }
            let cents = ((a.hz() / target).ln() / 2.0f64.ln() * 1200.0).abs();
            assert!(
                cents < 100.0,
                "target {target} Hz, aimed {} Hz ({cents:.0} cents off)",
                a.hz()
            );
        }
    }

    #[test]
    fn aimer_moves_when_the_energy_moves() {
        let sr = 48_000.0;
        let mut a = Aimer::new(sr);
        a.set_range(VOICE_CEILING_HZ, 12_000.0);
        for block in sine(5_000.0, sr, 24_000).chunks(ENV_HOP) {
            a.advance(block);
        }
        let low = a.q_position();
        for block in sine(9_000.0, sr, 24_000).chunks(ENV_HOP) {
            a.advance(block);
        }
        let high = a.q_position();
        assert!(low < 0.4, "5 kHz should sit low on the wheel, got {low:.2}");
        assert!(high > 0.7, "9 kHz should sit high on the wheel, got {high:.2}");
    }

    #[test]
    fn aimer_opens_on_a_pop_and_closes_on_steady_material() {
        let sr = 48_000.0;
        let mut a = Aimer::new(sr);
        a.set_range(VOICE_CEILING_HZ, 12_000.0);
        // Steady bright material should read as nothing once it is the norm.
        for block in sine(6_000.0, sr, 96_000).chunks(ENV_HOP) {
            a.advance(block);
        }
        let steady = a.excess();
        // A sudden 20 dB pop in the same band should open it.
        let hot: Vec<f32> = sine(6_000.0, sr, 4_800).iter().map(|s| s * 10.0).collect();
        for block in hot.chunks(ENV_HOP) {
            a.advance(block);
        }
        let popped = a.excess();
        assert!(steady < 0.1, "steady material should not open, got {steady:.2}");
        assert!(popped > 0.5, "a 20 dB pop should open it, got {popped:.2}");
    }

    fn sine(hz: f64, sr: f64, n: usize) -> Vec<f32> {
        (0..n)
            .map(|i| (2.0 * std::f64::consts::PI * hz * i as f64 / sr).sin() as f32)
            .collect()
    }

    #[test]
    fn attack_is_fast_and_release_is_slow() {
        let mut e = EnvFollower::new(SR);
        e.set(1.0, 200.0);
        // 5 ms of a hit: a 1 ms attack is fully there.
        run(&mut e, 1.0, 0.005);
        let after_attack = e.level();
        // 5 ms of silence: a 200 ms release has barely moved.
        run(&mut e, 0.0, 0.005);
        let after_release = e.level();
        assert!(
            after_attack > -0.05,
            "1 ms attack should reach full scale (0 dB) in 5 ms, got {after_attack:.3}"
        );
        assert!(
            after_release > -0.05,
            "200 ms release must barely move in 5 ms, got {after_release:.3}"
        );
        // A full release time constant later it has clearly decayed.
        run(&mut e, 0.0, 0.2);
        assert!(
            e.level() < after_release - 0.5,
            "level {:.3} should have decayed well below {after_release:.3}",
            e.level()
        );
    }

    #[test]
    fn reference_centres_on_steady_material() {
        let mut e = EnvFollower::new(SR);
        e.set(1.0, 200.0);
        // A steady tone for 8 s: the reference catches the level, offset -> 0.
        run(&mut e, 0.25, 8.0);
        assert!(
            e.offset().abs() < 0.02,
            "steady material must self-centre, offset {:.4}",
            e.offset()
        );
        // A level change of 12 dB with the reference still parked: the offset
        // is the distance above the average, not the absolute level.
        run(&mut e, 1.0, 0.02);
        assert!(
            e.offset() > 0.7,
            "a +12 dB pop should push most of a wheel unit, got {:.3}",
            e.offset()
        );
    }

    #[test]
    fn offset_clamps_to_one_wheel_unit() {
        let mut e = EnvFollower::new(SR);
        e.set(1.0, 200.0);
        run(&mut e, 0.0005, 8.0); // reference parked near -66 dB
        run(&mut e, 1.0, 0.02); // +66 dB pop = 5.5 raw units
        assert!(
            (e.offset() - 1.0).abs() < 1.0e-9,
            "offset must clamp at +1.0, got {:.4}",
            e.offset()
        );
    }

    #[test]
    fn amount_scales_the_offset() {
        let mut settings = Vec::new();
        for amount in [1.0, 0.5] {
            let mut e = EnvFollower::new(SR);
            e.set(amount, 200.0);
            run(&mut e, 0.25, 8.0);
            run(&mut e, 0.5, 0.02); // +6 dB — half a wheel unit, clear of the clamp
            settings.push(e.offset());
        }
        assert!(
            (settings[1] - settings[0] * 0.5).abs() < 1.0e-9,
            "amount must scale linearly: {:?}",
            settings
        );
    }

    #[test]
    fn amount_zero_is_exact_bypass() {
        let mut e = EnvFollower::new(SR);
        e.set(0.0, 200.0);
        run(&mut e, 1.0, 1.0);
        assert_eq!(e.offset(), 0.0, "off macro must contribute exactly zero");
        assert_eq!(e.level(), ENV_FLOOR_DB / ENV_RANGE_DB, "off macro must not follow");
    }

    #[test]
    fn bloom_zero_is_exact_bypass() {
        let mut e = EnvFollower::new(SR);
        e.set(1.0, 200.0);
        run(&mut e, 0.25, 8.0);
        run(&mut e, 1.0, 0.02);
        assert!(e.offset() > 0.0, "the MORPH destination must be live");
        assert_eq!(e.bloom_offset(), 0.0, "bloom off must contribute exactly zero");
        assert_eq!(e.bloom_amount(), 0.0);
    }

    #[test]
    fn bloom_law_is_louder_means_more_drive() {
        let mut e = EnvFollower::new(SR);
        e.set_bloom(1.0);
        run(&mut e, 0.25, 8.0); // reference parked on the material
        let steady = e.bloom_offset();
        run(&mut e, 0.5, 0.02); // +6 dB pop
        let popped = e.bloom_offset();
        assert!(
            popped > steady + 0.4,
            "a +6 dB pop must push the drive up: {steady:.3} -> {popped:.3}"
        );
        assert!(
            (popped - (e.level() - e.reference())).abs() < 1.0e-12,
            "bloom offset must be (level - ref) * bloom_amount"
        );
    }

    #[test]
    fn bloom_self_centres_on_steady_loud_material() {
        let mut e = EnvFollower::new(SR);
        e.set_bloom(1.0);
        run(&mut e, 0.9, 8.0); // steady and LOUD
        assert!(
            e.bloom_offset().abs() < 0.02,
            "steady loud material must not sit permanently driven, offset {:.4}",
            e.bloom_offset()
        );
    }

    #[test]
    fn bloom_offset_clamps() {
        let mut e = EnvFollower::new(SR);
        e.set_bloom(1.0);
        run(&mut e, 0.0005, 8.0); // reference parked near -66 dB
        run(&mut e, 1.0, 0.02); // +66 dB pop = 5.5 raw units
        assert!(
            (e.bloom_offset() - 1.0).abs() < 1.0e-9,
            "bloom offset must clamp at +1.0, got {:.4}",
            e.bloom_offset()
        );
        // ...and downward, from a loud reference into silence.
        let mut e = EnvFollower::new(SR);
        e.set_bloom(1.0);
        run(&mut e, 1.0, 8.0);
        run(&mut e, 0.0, 4.0);
        assert!(
            (e.bloom_offset() + 1.0).abs() < 1.0e-9,
            "bloom offset must clamp at -1.0, got {:.4}",
            e.bloom_offset()
        );
    }

    #[test]
    fn one_follower_feeds_both_destinations() {
        // The follower's level/reference must be bit-identical whichever
        // destination is armed — one signal, two depths.
        let trace = |morph: f64, bloom: f64| -> (f64, f64) {
            let mut e = EnvFollower::new(SR);
            e.set(morph, 200.0);
            e.set_bloom(bloom);
            run(&mut e, 0.25, 4.0);
            run(&mut e, 0.5, 0.02);
            run(&mut e, 0.02, 0.3);
            (e.level(), e.reference())
        };
        let morph_only = trace(1.0, 0.0);
        let bloom_only = trace(0.0, 1.0);
        let both = trace(1.0, 1.0);
        assert_eq!(bloom_only, morph_only, "bloom alone must run the same follower");
        assert_eq!(both, morph_only, "arming both must not change the follower");
        // Same signal, two independent depths.
        let mut e = EnvFollower::new(SR);
        e.set(1.0, 200.0);
        e.set_bloom(0.5);
        run(&mut e, 0.25, 4.0);
        run(&mut e, 0.5, 0.02);
        assert!(e.offset() > 0.0);
        assert!(
            (e.bloom_offset() - e.offset() * 0.5).abs() < 1.0e-12,
            "the two depths must scale the same signal: {:.4} vs {:.4}",
            e.bloom_offset(),
            e.offset()
        );
    }

    #[test]
    fn block_peak_finds_the_transient() {
        assert_eq!(block_peak(&[0.1, -0.9, 0.2]), 0.9);
        assert_eq!(block_peak(&[]), 0.0);
        assert_eq!(block_peak(&[f32::NAN]), 0.0);
    }
}
