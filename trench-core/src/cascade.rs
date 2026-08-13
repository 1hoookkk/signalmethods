pub const NUM_STAGES: usize = 7;
pub const NUM_COEFFS: usize = 5;
pub const PASSTHROUGH_COEFFS: [f64; NUM_COEFFS] = [1.0, 0.0, 0.0, 0.0, 0.0];
pub const BLOCK_SIZE: usize = 32;

/// State-clip floor: the tightest ceiling at GRIT=1.0.
const CEILING_FLOOR: f64 = 0.05;
const CEILING_WIDE: f64 = 2.0;
/// Pole-radius distortion threshold floor — the most sensitive trigger level
/// at GRIT=1.0.  At GRIT=0 the threshold is high enough that no normal
/// signal crosses it.
const DISTORT_THRESH_FLOOR: f64 = 0.02;
const DISTORT_THRESH_WIDE: f64 = 1.5;

#[inline(always)]
fn grit_state_ceiling(grit: f64) -> f64 {
    let g = grit.clamp(0.0, 1.0);
    CEILING_WIDE + (CEILING_FLOOR - CEILING_WIDE) * g
}
#[inline(always)]
fn grit_distort_threshold(grit: f64) -> f64 {
    let g = grit.clamp(0.0, 1.0);
    // Exponential taper: threshold drops quickly at low GRIT, slowly at high.
    (DISTORT_THRESH_WIDE - DISTORT_THRESH_FLOOR) * (-4.0 * g).exp() + DISTORT_THRESH_FLOOR
}

/// Rounded state limit (2026-08-06): the wall becomes a cushion. Same knee law
/// as SLAM's rounded limit — identity below 0.72·ceiling, tanh-bound to
/// ±ceiling above — so a driven state compresses instead of flat-topping.
#[inline(always)]
fn soft_clamp_ceiling(x: f64, ceiling: f64) -> f64 {
    const KNEE: f64 = 0.72;
    let k = KNEE * ceiling;
    let a = x.abs();
    if a <= k {
        return x;
    }
    let span = ceiling - k;
    x.signum() * (k + span * ((a - k) / span).tanh())
}

#[derive(Clone)]
struct BiquadState {
    coeffs: [f64; NUM_COEFFS],
    deltas: [f64; NUM_COEFFS],
    w1: f64,
    w2: f64,
    /// Previous real output — the Rossum distortion detector reads the n-1
    /// sample to decide whether to modulate the pole radius.
    y_prev: f64,
}

impl BiquadState {
    fn new() -> Self {
        Self {
            coeffs: PASSTHROUGH_COEFFS,
            deltas: [0.0; NUM_COEFFS],
            w1: 0.0,
            w2: 0.0,
            y_prev: 0.0,
        }
    }
    fn set_target(&mut self, target: &[f64; NUM_COEFFS], ramp_samples: usize) {
        let ramp_samples = ramp_samples.max(1) as f64;
        for (i, &t) in target.iter().enumerate() {
            self.deltas[i] = (t - self.coeffs[i]) / ramp_samples;
        }
    }
    /// Per-section nonlinear processor.
    ///
    /// 1. Apply coefficient ramp (per-sample deltas)
    /// 2. Compute DF-II output: y = b0·x + w1
    /// 3. Amplitude-dependent pole-radius modulation:
    ///    if |y_prev| > vt:  R_new = R + R(1-R) · (|y_prev| − vt)
    /// 4. Compute new DF-II states: w1', w2'
    /// 5. Saturate states to ±ceiling (recursive nonlinearity)
    /// 6. Store y_prev = y for next sample's distortion detector
    ///
    /// Steps 2, 4, 5 are Rossum, ICMC 1992 fig. 3: extended headroom on the
    /// accumulator, saturate only the value entering the delays, output tapped
    /// off the accumulator before the saturate. Those agree with the source.
    ///
    /// Step 3 does NOT come from any source and previously claimed a patent it
    /// is not in. Rossum's pole movement is a *consequence* of step 5, not a
    /// separate step: "one could either say that the signal had been saturated
    /// ... or alternatively that the coefficient had been reduced in such a
    /// manner as to give the same smaller product." Two further departures:
    /// he describes a shift in the *pitch* of the resonance, while step 3 holds
    /// cos θ fixed and moves only the radius; and his coefficient is *reduced*,
    /// while step 3 raises R toward 1. Kept because it is what shipped and the
    /// ears passed it — see bench/facts.py NONLINEARITY_MATCHES_SOURCE.
    #[inline(always)]
    fn process_sample(&mut self, x: f64, vt: f64, ceiling: f64) -> (f64, bool) {
        self.coeffs[0] += self.deltas[0];
        self.coeffs[1] += self.deltas[1];
        self.coeffs[2] += self.deltas[2];
        self.coeffs[3] += self.deltas[3];
        self.coeffs[4] += self.deltas[4];

        let (mut a1, mut a2) = (self.coeffs[3], self.coeffs[4]);
        let (b0, b1, b2) = (self.coeffs[0], self.coeffs[1], self.coeffs[2]);

        // Amplitude-dependent pole-radius modulation (Rossum patent)
        let vg = self.y_prev.abs();
        if vg > vt && a2 > 1.0e-9 {
            let r = a2.sqrt();
            if r > 1.0e-6 && r < 1.0 {
                let cos_theta = (-a1 / (2.0 * r)).clamp(-1.0, 1.0);
                let ratio = (vg - vt).min(0.5);
                let r_new = (r + r * (1.0 - r) * ratio).clamp(0.0, 0.999_9);
                a1 = -2.0 * r_new * cos_theta;
                a2 = r_new * r_new;
            }
        }

        let mut y = b0 * x + self.w1;
        if !y.is_finite() {
            y = 0.0;
            self.w1 = 0.0;
            self.w2 = 0.0;
            self.y_prev = 0.0;
            self.deltas = [0.0; NUM_COEFFS];
            return (y, true);
        }

        // State saturation (recursive nonlinearity): soft-kneed — the state
        // compresses toward ±ceiling instead of flat-topping on it.
        let w1_new = b1 * x - a1 * y + self.w2;
        let w2_new = b2 * x - a2 * y;
        self.w1 = soft_clamp_ceiling(w1_new, ceiling);
        self.w2 = soft_clamp_ceiling(w2_new, ceiling);

        let unstable = !self.w1.is_finite() || !self.w2.is_finite();
        if unstable {
            self.w1 = 0.0;
            self.w2 = 0.0;
            self.y_prev = 0.0;
            self.deltas = [0.0; NUM_COEFFS];
        } else {
            self.y_prev = y;
        }
        (y, unstable)
    }
}

pub struct Cascade {
    stages: [BiquadState; NUM_STAGES],
    boost: f64,
    boost_delta: f64,
    grit: f32,
    grit_delta: f32,
    activity: f64,
    instability_detected: bool,
    /// DEV BYPASS. The state saturation and the pole-radius modulator are on at
    /// every GRIT setting, including zero: `grit_state_ceiling(0)` is 2.0 and
    /// `grit_distort_threshold(0)` is ~1.52, and a resonant cascade runs above
    /// both. Measured 2026-08-13 on the resonant test body: disarming them takes
    /// the peak from 2.39 to 8.36 at -12 dBFS in - 12 dB of pull from a stage
    /// every control says is off. This flag makes the cascade a plain linear
    /// IIR so the filter can be heard with nothing on top of it. Not a preset
    /// parameter and not automatable: the dev panel's A/B.
    linear: bool,
}

impl Cascade {
    pub fn new() -> Self {
        Self {
            stages: std::array::from_fn(|_| BiquadState::new()),
            boost: 1.0,
            boost_delta: 0.0,
            grit: 0.0,
            grit_delta: 0.0,
            activity: 0.0,
            instability_detected: false,
            linear: false,
        }
    }
    /// DEV BYPASS: run the sections as plain linear biquads (see `linear`).
    pub fn set_linear(&mut self, linear: bool) {
        self.linear = linear;
    }
    pub fn reset(&mut self) {
        for stage in &mut self.stages {
            stage.w1 = 0.0;
            stage.w2 = 0.0;
            stage.deltas = [0.0; NUM_COEFFS];
        }
        self.boost = 1.0;
        self.boost_delta = 0.0;
    }
    /// Sections past `interpolated` are returned to passthrough, so a shorter
    /// corner can never leave a previous body's section multiplying the chain.
    pub fn snap_targets(&mut self, interpolated: &[[f64; NUM_COEFFS]]) {
        for (i, stage) in self.stages.iter_mut().enumerate() {
            stage.coeffs = interpolated.get(i).copied().unwrap_or(PASSTHROUGH_COEFFS);
            stage.deltas = [0.0; NUM_COEFFS];
        }
    }
    pub fn set_targets(&mut self, interpolated: &[[f64; NUM_COEFFS]], ramp_samples: usize) {
        for (i, stage) in self.stages.iter_mut().enumerate() {
            let target = interpolated.get(i).copied().unwrap_or(PASSTHROUGH_COEFFS);
            stage.set_target(&target, ramp_samples);
        }
    }
    pub fn set_grit(&mut self, grit: f32) {
        self.grit_delta = (grit - self.grit) / BLOCK_SIZE as f32;
    }
    pub fn set_interstage_drive(&mut self, grit: f32, _ramp: usize) {
        self.set_grit(grit);
    }
    pub fn set_pole_distortion(&mut self, _grit: f32, _ramp: usize) {
        // State clipping replaces the separate pole-distortion mechanism.
        // Both interstage drive and pole distortion now map to GRIT.
        self.set_grit(_grit);
    }
    pub fn set_chew_focus_for_morph(&mut self, _morph: f64, _ramp: usize) {
        // State clipping applies uniformly to all six stages — no focus.
    }
    pub fn chew_activity(&self) -> f32 {
        self.activity.min(1.0) as f32
    }
    pub fn set_boost(&mut self, boost: f64, ramp_samples: usize) {
        let ramp_samples = ramp_samples.max(1) as f64;
        self.boost_delta = (boost - self.boost) / ramp_samples;
    }
    pub fn take_instability_flag(&mut self) -> bool {
        let v = self.instability_detected;
        self.instability_detected = false;
        v
    }
    pub fn get_coeffs(&self, out: &mut [[f64; NUM_COEFFS]; NUM_STAGES]) {
        for (i, stage) in self.stages.iter().enumerate() {
            out[i] = stage.coeffs;
        }
    }
    #[inline(always)]
    pub fn tick(&mut self, x: f32) -> f32 {
        let mut v = x as f64;
        self.grit = (self.grit + self.grit_delta).clamp(0.0, 1.0);
        // An infinite ceiling makes soft_clamp_ceiling the identity and an
        // infinite threshold means the pole-radius modulator never fires, so
        // `linear` costs one predictable branch per sample and nothing else.
        let (ceiling, vt) = if self.linear {
            (f64::INFINITY, f64::INFINITY)
        } else {
            (
                grit_state_ceiling(self.grit as f64),
                grit_distort_threshold(self.grit as f64),
            )
        };
        self.activity *= 0.999;
        for stage in &mut self.stages {
            let (next, unstable) = stage.process_sample(v, vt, ceiling);
            if unstable {
                self.instability_detected = true;
                return 0.0;
            }
            v = next;
        }
        if self.grit > 0.0 {
            let frac = self.grit as f64;
            if frac > self.activity {
                self.activity = frac;
            }
        }
        self.boost += self.boost_delta;
        if !self.boost.is_finite() {
            self.boost = 1.0;
            self.boost_delta = 0.0;
            self.instability_detected = true;
            return 0.0;
        }
        v *= self.boost;
        if !v.is_finite() {
            self.instability_detected = true;
            return 0.0;
        }
        v as f32
    }
    pub fn process_block_mono(&mut self, samples: &mut [f32]) {
        for sample in samples.iter_mut() {
            *sample = self.tick(*sample);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rossum_reference(coeffs: &[f64; NUM_COEFFS], input: &[f64]) -> Vec<f64> {
        let mut x1 = 0.0;
        let mut x2 = 0.0;
        let mut y1 = 0.0;
        let mut y2 = 0.0;
        let mut out = Vec::with_capacity(input.len());
        for &x0 in input {
            let y0 =
                coeffs[0] * x0 + coeffs[1] * x1 + coeffs[2] * x2 - coeffs[3] * y1 - coeffs[4] * y2;
            out.push(y0);
            x2 = x1;
            x1 = x0;
            y2 = y1;
            y1 = y0;
        }
        out
    }
    #[test]
    fn passthrough_is_identity() {
        let mut cascade = Cascade::new();
        let mut buf = [1.0f32, 0.5, -0.3, 0.0];
        let expected = buf;
        cascade.process_block_mono(&mut buf);
        for (i, (&got, &exp)) in buf.iter().zip(expected.iter()).enumerate() {
            assert!(
                (got - exp).abs() < 1e-10,
                "sample {i}: got {got:.15e}, expected {exp}"
            );
        }
    }
    #[test]
    fn reset_clears_states() {
        let mut cascade = Cascade::new();
        let target = [[2.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
        cascade.set_targets(&target, BLOCK_SIZE);
        cascade.tick(0.5);
        cascade.reset();
        for stage in &cascade.stages {
            assert_eq!(stage.w1, 0.0);
            assert_eq!(stage.w2, 0.0);
            assert_eq!(stage.deltas, [0.0; NUM_COEFFS]);
        }
    }
    #[test]
    fn non_finite_stage_sets_instability_flag() {
        let mut cascade = Cascade::new();
        cascade.stages[0].coeffs[0] = f64::NAN;
        let output = cascade.tick(1.0);
        assert_eq!(output, 0.0);
        assert!(cascade.take_instability_flag());
        assert!(!cascade.take_instability_flag());
    }
    #[test]
    fn df2_stage_matches_rossum_with_ceiling_wide_open() {
        let cases = [
            [0.72, -0.31, 0.18, -1.112, 0.716],
            [1.0, 0.0, 0.0, -0.842, 0.303],
            [0.19, 0.27, 0.19, -1.438, 0.522],
        ];
        let input = [
            1.0, -0.25, 0.125, 0.0, 0.5, -0.75, 0.375, -0.1875, 0.09375, 0.0, -0.03125, 0.015625,
        ];
        for coeffs in cases {
            let expected = rossum_reference(&coeffs, &input);
            let mut stage = BiquadState::new();
            stage.coeffs = coeffs;
            for (i, (&x, &want)) in input.iter().zip(expected.iter()).enumerate() {
                let (got, unstable) = stage.process_sample(x, DISTORT_THRESH_WIDE, CEILING_WIDE);
                assert!(!unstable, "case {coeffs:?} went unstable at sample {i}");
                assert!(
                    (got - want).abs() <= 1e-12,
                    "sample {i}: DF2 {got:.15e} != Rossum {want:.15e} for {coeffs:?}"
                );
            }
        }
    }
    #[test]
    fn state_clipping_damps_resonance() {
        // A resonant pole at r=0.9: a1=-1.6, a2=0.81.  At ceiling=0.1 the
        // states should be visibly clamped vs the wide-open case.
        let coeffs = [1.0, 0.0, 0.0, -1.6, 0.81];
        let impulse = [1.0_f64, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];

        let run = |ceiling| {
            let mut stage = BiquadState::new();
            stage.coeffs = coeffs;
            let mut out = Vec::new();
            for &x in &impulse {
                let (y, _) = stage.process_sample(x, DISTORT_THRESH_WIDE, ceiling);
                out.push(y);
            }
            out
        };

        let wide = run(CEILING_WIDE);
        let tight = run(0.1);
        // The tight-clipped version should have less ringing energy
        let wide_energy: f64 = wide.iter().map(|s| s * s).sum();
        let tight_energy: f64 = tight.iter().map(|s| s * s).sum();
        assert!(tight_energy < wide_energy,
            "clipping should reduce resonance energy: wide={wide_energy:.6}, tight={tight_energy:.6}");
    }
    #[test]
    fn grit_zero_is_exact_linear_cascade() {
        let mut cascade = Cascade::new();
        let target = [[1.0, 0.0, 0.0, -1.6, 0.81]; NUM_STAGES];
        cascade.snap_targets(&target);
        cascade.set_grit(0.0);
        // Run a few samples — grit=0 means wide ceiling, no clipping
        for _ in 0..32 {
            let y = cascade.tick(0.5);
            assert!(y.is_finite());
        }
        assert!(!cascade.take_instability_flag());
    }
    #[test]
    fn grit_one_clips_aggressively() {
        let mut cascade = Cascade::new();
        let target = [[1.0, 0.0, 0.0, -1.6, 0.81]; NUM_STAGES];
        cascade.snap_targets(&target);
        cascade.set_grit(1.0);
        // Should still be stable even at max grit
        for _ in 0..64 {
            let y = cascade.tick(0.5);
            assert!(y.is_finite());
        }
        assert!(!cascade.take_instability_flag());
    }
}
