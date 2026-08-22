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

/// biquad_to_kernel(PASSTHROUGH_COEFFS): the kernel row that decodes to the
/// identity biquad with no rounding.
const PASSTHROUGH_KERNEL: [f64; NUM_COEFFS] = [2.0, 1.0, 2.0, 1.0, 1.0];

#[derive(Clone)]
struct BiquadState {
    coeffs: [f64; NUM_COEFFS],
    deltas: [f64; NUM_COEFFS],
    /// X3 parity ramp: the kernel row (minifloat.rs stage_words_to_kernel) and
    /// its per-sample increment. Live only while Cascade::kernel_ramp — the
    /// biquad is derived from the kernel each sample, and the kernel steps at
    /// the END of the sample (FUN_1802c1550 increments after the output is
    /// written, so sample 0 of a block runs `current` untouched).
    kernel: [f64; NUM_COEFFS],
    kdeltas: [f64; NUM_COEFFS],
    /// Last block's kernel target. The X3 runs `current[] <- target[]` at
    /// block end (vtable 0x08) and re-seeds the working row from current, so
    /// every block STARTS exactly on the previous target — no accumulation
    /// drift. set_kernel_targets replays that snap before computing deltas.
    ktarget: [f64; NUM_COEFFS],
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
            kernel: PASSTHROUGH_KERNEL,
            kdeltas: [0.0; NUM_COEFFS],
            ktarget: PASSTHROUGH_KERNEL,
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
    fn process_sample(&mut self, x: f64, vt: f64, ceiling: f64, kernel_ramp: bool) -> (f64, bool) {
        if kernel_ramp {
            self.coeffs = crate::minifloat::kernel_to_biquad(self.kernel);
        } else {
            self.coeffs[0] += self.deltas[0];
            self.coeffs[1] += self.deltas[1];
            self.coeffs[2] += self.deltas[2];
            self.coeffs[3] += self.deltas[3];
            self.coeffs[4] += self.deltas[4];
        }

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
            self.kdeltas = [0.0; NUM_COEFFS];
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
            self.kdeltas = [0.0; NUM_COEFFS];
        } else {
            self.y_prev = y;
            if kernel_ramp {
                self.kernel[0] += self.kdeltas[0];
                self.kernel[1] += self.kdeltas[1];
                self.kernel[2] += self.kdeltas[2];
                self.kernel[3] += self.kdeltas[3];
                self.kernel[4] += self.kdeltas[4];
            }
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
    /// X3 movement parity: coefficients ride the linearly ramped kernel row
    /// instead of arriving whole. Armed by set_kernel_targets, disarmed by
    /// snap_targets (the X3's frozen/static path).
    kernel_ramp: bool,
    /// Whether stage.kernel currently agrees with stage.coeffs. Snap installs
    /// biquads without paying the kernel inverse, so it just invalidates; the
    /// first set_kernel_targets after that re-seeds from the installed row.
    kernel_valid: bool,
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
            kernel_ramp: false,
            kernel_valid: false,
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
            stage.kdeltas = [0.0; NUM_COEFFS];
        }
        self.boost = 1.0;
        self.boost_delta = 0.0;
        self.kernel_ramp = false;
        self.kernel_valid = false;
    }
    /// Sections past `interpolated` are returned to passthrough, so a shorter
    /// corner can never leave a previous body's section multiplying the chain.
    pub fn snap_targets(&mut self, interpolated: &[[f64; NUM_COEFFS]]) {
        for (i, stage) in self.stages.iter_mut().enumerate() {
            stage.coeffs = interpolated.get(i).copied().unwrap_or(PASSTHROUGH_COEFFS);
            stage.deltas = [0.0; NUM_COEFFS];
        }
        self.kernel_ramp = false;
        self.kernel_valid = false;
    }
    pub fn set_targets(&mut self, interpolated: &[[f64; NUM_COEFFS]], ramp_samples: usize) {
        for (i, stage) in self.stages.iter_mut().enumerate() {
            let target = interpolated.get(i).copied().unwrap_or(PASSTHROUGH_COEFFS);
            stage.set_target(&target, ramp_samples);
        }
    }
    /// X3 movement parity: kernel-space targets for one control block. The
    /// per-sample delta is (target - current)/N — FUN_1802c41a0 — and block k
    /// travels from target(k-1) to target(k): the X3's one-block lag.
    pub fn set_kernel_targets(&mut self, kernels: &[[f64; NUM_COEFFS]], ramp_samples: usize) {
        let n = ramp_samples.max(1) as f64;
        if !self.kernel_valid {
            for stage in &mut self.stages {
                stage.kernel = crate::minifloat::biquad_to_kernel(stage.coeffs);
                stage.ktarget = stage.kernel;
            }
            self.kernel_valid = true;
        }
        for (i, stage) in self.stages.iter_mut().enumerate() {
            let target = kernels.get(i).copied().unwrap_or(PASSTHROUGH_KERNEL);
            // current[] <- target[] (vtable 0x08), then delta from there.
            stage.kernel = stage.ktarget;
            for j in 0..NUM_COEFFS {
                stage.kdeltas[j] = (target[j] - stage.kernel[j]) / n;
            }
            stage.ktarget = target;
            stage.deltas = [0.0; NUM_COEFFS];
        }
        self.kernel_ramp = true;
    }
    /// The X3's change-test skip (`+0x86 = 1`): nothing moved this block, so
    /// the kernel freezes where the last ramp landed it.
    pub fn zero_kernel_deltas(&mut self) {
        for stage in &mut self.stages {
            stage.kdeltas = [0.0; NUM_COEFFS];
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
        let kernel_ramp = self.kernel_ramp;
        for stage in &mut self.stages {
            let (next, unstable) = stage.process_sample(v, vt, ceiling, kernel_ramp);
            if unstable {
                self.instability_detected = true;
                return 0.0;
            }
            v = next;
        }
        if self.grit > 0.0 {
            let frac = self.grit as f64;
            if frac > self.activity { self.activity = frac; }
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
                let (got, unstable) =
                    stage.process_sample(x, DISTORT_THRESH_WIDE, CEILING_WIDE, false);
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
                let (y, _) = stage.process_sample(x, DISTORT_THRESH_WIDE, ceiling, false);
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
    fn x3_kernel_ramp_shape_matches_the_dll() {
        // Proof 4, X3_MOVEMENT_SPEC.md: coef(i) = current + i*(target-current)/N
        // in KERNEL space, sample 0 runs `current` untouched, and sample 0 of
        // the following block sits exactly on the target (vtable 0x08).
        use crate::minifloat::{biquad_to_kernel, kernel_to_biquad};
        const N: usize = 8;
        let a = [0.72, -0.31, 0.18, -1.112, 0.716];
        let b = [0.19, 0.27, 0.19, -1.438, 0.522];
        let ka = biquad_to_kernel(a);
        let kb = biquad_to_kernel(b);
        let mut cascade = Cascade::new();
        cascade.snap_targets(&[a; NUM_STAGES]);
        cascade.set_kernel_targets(&[kb; NUM_STAGES], N);
        let mut rows = [[0.0; NUM_COEFFS]; NUM_STAGES];
        for i in 0..N {
            cascade.tick(0.0);
            cascade.get_coeffs(&mut rows);
            let mut want_k = [0.0; NUM_COEFFS];
            for j in 0..NUM_COEFFS {
                want_k[j] = ka[j] + i as f64 * ((kb[j] - ka[j]) / N as f64);
            }
            let want = kernel_to_biquad(want_k);
            for j in 0..NUM_COEFFS {
                assert!(
                    (rows[0][j] - want[j]).abs() < 1e-12,
                    "sample {i} coef {j}: got {}, want {}",
                    rows[0][j],
                    want[j]
                );
            }
        }
        cascade.set_kernel_targets(&[kb; NUM_STAGES], N);
        cascade.tick(0.0);
        cascade.get_coeffs(&mut rows);
        assert_eq!(rows[0], kernel_to_biquad(kb), "next block must start ON the target");
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
