//! TPT runtime — the second cascade, for the utility bodies.
//!
//! WHY A SECOND RUNTIME
//!
//! The DF2 cascade in `cascade.rs` is the E-mu topology on purpose: the Rossum
//! distortion detector reads `y_prev` to modulate the pole radius and the state
//! registers saturate to a ceiling. That behaviour lives in DF2's registers and
//! it is where the ROM bodies get their character. It is not negotiable there.
//!
//! But DF2's states are not signals. `w1` and `w2` are an internal mix whose
//! meaning depends on the coefficients themselves, so changing coefficients
//! reinterprets stored energy under a rule that did not exist a sample ago —
//! the state shock. DF2 therefore cannot be modulated at audio rate without
//! artefacts, only glided between destinations (which is exactly what E-mu's
//! per-sample coefficient deltas do).
//!
//! A de-esser has no heritage to protect. It needs the opposite trade: perfect
//! surgical accuracy and true audio-rate modulation. So the utility bodies run
//! here instead, on a topology-preserving transform state-variable core, where
//! the states ARE the integrator outputs — real signals, independent of the
//! coefficients. Retune it every sample and nothing is shocked.
//!
//! THE MAPPING
//!
//! A body row is a digital biquad `[b0, b1, b2, a1, a2]`. The TPT core is
//! parameterised by `g = tan(w0*T/2)` and damping `k`, and realises the three
//! analog prototypes under the bilinear transform `s -> (1/g)(1-z^-1)/(1+z^-1)`:
//!
//! ```text
//! D(z) = (1 + k g + g^2) + (2g^2 - 2) z^-1 + (g^2 - k g + 1) z^-2
//! HP   = (1 - 2 z^-1 + z^-2) / D
//! BP   = g (1 - z^-2)        / D
//! LP   = g^2 (1 + 2 z^-1 + z^-2) / D
//! ```
//!
//! Matching `D` to the row's own denominator gives `g` and `k` in closed form,
//! and any numerator is then a linear combination of the three outputs. The
//! response is identical to DF2 — only the state variables change.

/// One TPT state-variable section.
#[derive(Clone, Copy, Debug)]
pub struct TptStage {
    g: f64,
    k: f64,
    /// 1 / (1 + g(g + k)) — the implicit-solve denominator, precomputed.
    a0: f64,
    /// Output mix: highpass, bandpass, lowpass.
    c_hp: f64,
    c_bp: f64,
    c_lp: f64,
    /// Integrator states. These are signals, not a coefficient-dependent mix.
    s1: f64,
    s2: f64,
}

impl Default for TptStage {
    fn default() -> Self {
        Self::passthrough()
    }
}

impl TptStage {
    pub fn passthrough() -> Self {
        let mut s = Self { g: 1.0, k: 2.0, a0: 0.0, c_hp: 0.0, c_bp: 0.0, c_lp: 0.0, s1: 0.0, s2: 0.0 };
        s.set_biquad(&[1.0, 0.0, 0.0, 0.0, 0.0]);
        s
    }

    pub fn reset(&mut self) {
        self.s1 = 0.0;
        self.s2 = 0.0;
    }

    /// Retune from a body row. Cheap enough to call every sample — no trig, no
    /// transcendentals, just the closed-form solve. Leaves the states alone,
    /// which is the whole point.
    ///
    /// Returns false when the row has no conjugate pole pair this core can
    /// represent (real roots, pure gain rows the caller may prefer to skip).
    pub fn set_biquad(&mut self, row: &[f64; 5]) -> bool {
        let (b0, b1, b2, a1, a2) = (row[0], row[1], row[2], row[3], row[4]);
        // g^2 = (1 + a2 + a1) / (1 + a2 - a1), from matching D(z) term by term.
        let num = 1.0 + a2 + a1;
        let den = 1.0 + a2 - a1;
        if !(num.is_finite() && den.is_finite()) || num <= 0.0 || den <= 1.0e-12 {
            return false;
        }
        let u = num / den;
        let g = u.sqrt();
        if !g.is_finite() || g <= 1.0e-9 {
            return false;
        }
        // d = 1 + k g + g^2, recovered from d(1 + a2) = 2(1 + g^2).
        let d = 2.0 * (1.0 + u) / (1.0 + a2);
        let k = (d - 1.0 - u) / g;
        if !(d.is_finite() && k.is_finite()) || k < 0.0 {
            return false;
        }
        // Numerators above sit over the UNnormalised D, so the row's b-terms
        // scale by d before the match.
        let (n0, n1, n2) = (b0 * d, b1 * d, b2 * d);
        let c_bp = (n0 - n2) / (2.0 * g);
        let c_lp = (n0 + n1 + n2) / (4.0 * u);
        let c_hp = (n0 + n2) / 2.0 - c_lp * u;
        if !(c_bp.is_finite() && c_lp.is_finite() && c_hp.is_finite()) {
            return false;
        }
        self.g = g;
        self.k = k;
        self.a0 = 1.0 / (1.0 + g * (g + k));
        self.c_hp = c_hp;
        self.c_bp = c_bp;
        self.c_lp = c_lp;
        true
    }

    #[inline(always)]
    pub fn process(&mut self, x: f64) -> f64 {
        let hp = (x - (self.k + self.g) * self.s1 - self.s2) * self.a0;
        let bp = self.g * hp + self.s1;
        self.s1 = self.g * hp + bp;
        let lp = self.g * bp + self.s2;
        self.s2 = self.g * bp + lp;
        let y = self.c_hp * hp + self.c_bp * bp + self.c_lp * lp;
        if y.is_finite() {
            y
        } else {
            self.s1 = 0.0;
            self.s2 = 0.0;
            0.0
        }
    }
}

pub use crate::cascade::NUM_STAGES;

/// Six TPT sections in series. Same law as the DF2 cascade: responses multiply,
/// dB add, judge the probed whole.
#[derive(Clone)]
pub struct TptCascade {
    stages: [TptStage; NUM_STAGES],
    /// Rows the core could not represent fall back to a direct-form-I section,
    /// whose states are real past inputs and outputs — still coefficient
    /// independent, so still safe to retune live.
    fallback: [Option<Df1Stage>; NUM_STAGES],
}

/// Direct form I: states are literal past x and y. Used only for rows the TPT
/// core cannot express (real roots, degenerate denominators).
#[derive(Clone, Copy, Debug, Default)]
pub struct Df1Stage {
    row: [f64; 5],
    x1: f64,
    x2: f64,
    y1: f64,
    y2: f64,
}

impl Df1Stage {
    #[inline(always)]
    fn process(&mut self, x: f64) -> f64 {
        let [b0, b1, b2, a1, a2] = self.row;
        let y = b0 * x + b1 * self.x1 + b2 * self.x2 - a1 * self.y1 - a2 * self.y2;
        let y = if y.is_finite() { y } else { 0.0 };
        self.x2 = self.x1;
        self.x1 = x;
        self.y2 = self.y1;
        self.y1 = y;
        y
    }
}

impl Default for TptCascade {
    fn default() -> Self {
        Self::new()
    }
}

impl TptCascade {
    pub fn new() -> Self {
        Self {
            stages: [TptStage::passthrough(); NUM_STAGES],
            fallback: [None; NUM_STAGES],
        }
    }

    pub fn reset(&mut self) {
        for s in &mut self.stages {
            s.reset();
        }
        for f in self.fallback.iter_mut().flatten() {
            *f = Df1Stage { row: f.row, ..Default::default() };
        }
    }

    /// Retune the whole cascade. Safe to call per sample — that is the reason
    /// this runtime exists.
    pub fn set_rows(&mut self, rows: &[[f64; 5]]) {
        for i in 0..NUM_STAGES.min(rows.len()) {
            if self.stages[i].set_biquad(&rows[i]) {
                self.fallback[i] = None;
            } else {
                let keep = self.fallback[i].unwrap_or_default();
                self.fallback[i] = Some(Df1Stage { row: rows[i], ..keep });
            }
        }
    }

    #[inline(always)]
    pub fn tick(&mut self, x: f32) -> f32 {
        let mut v = f64::from(x);
        for i in 0..NUM_STAGES {
            v = match &mut self.fallback[i] {
                Some(f) => f.process(v),
                None => self.stages[i].process(v),
            };
        }
        v as f32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Reference: the same row run as a plain difference equation.
    fn df_reference(row: &[f64; 5], input: &[f64]) -> Vec<f64> {
        let [b0, b1, b2, a1, a2] = *row;
        let (mut x1, mut x2, mut y1, mut y2) = (0.0, 0.0, 0.0, 0.0);
        input
            .iter()
            .map(|&x| {
                let y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1;
                x1 = x;
                y2 = y1;
                y1 = y;
                y
            })
            .collect()
    }

    fn impulse(n: usize) -> Vec<f64> {
        let mut v = vec![0.0; n];
        v[0] = 1.0;
        v
    }

    fn rows() -> Vec<[f64; 5]> {
        vec![
            // Resonant lowpass.
            [0.0201, 0.0402, 0.0201, -1.5610, 0.6414],
            // High-Q peak, the hard case: radius 0.995.
            [1.0, -1.6, 0.9, -1.97, 0.990025],
            // A razor notch of the kind the carve bodies use: zero on the
            // circle, pole just behind it.
            [1.0, -1.9021, 1.0, -1.8850, 0.9801],
            // Near-DC pole.
            [1.0, 0.0, 0.0, -1.9950, 0.995006],
        ]
    }

    #[test]
    fn frozen_tpt_matches_the_difference_equation() {
        let x = impulse(2048);
        for row in rows() {
            let mut s = TptStage::passthrough();
            assert!(s.set_biquad(&row), "row should map: {row:?}");
            let got: Vec<f64> = x.iter().map(|&v| s.process(v)).collect();
            let want = df_reference(&row, &x);
            let err = got
                .iter()
                .zip(&want)
                .map(|(a, b)| (a - b).abs())
                .fold(0.0f64, f64::max);
            // Loosest row is a pole at radius 0.9975; 1e-7 absolute is ~-140 dB.
            assert!(err < 1.0e-7, "row {row:?} deviated by {err:.3e}");
        }
    }

    #[test]
    fn passthrough_row_is_unity() {
        let mut s = TptStage::passthrough();
        let x = impulse(64);
        let got: Vec<f64> = x.iter().map(|&v| s.process(v)).collect();
        assert!((got[0] - 1.0).abs() < 1.0e-12, "got {}", got[0]);
        for &v in &got[1..] {
            assert!(v.abs() < 1.0e-12, "tail should be silent, got {v}");
        }
    }

    /// The claim this runtime was built for, put to the test.
    #[test]
    fn smooth_per_sample_sweep_is_parity_between_the_forms() {
        const N: usize = 4096;
        let sr = 48_000.0;
        // A steady tone, so any output jump is the modulation and not the input.
        let input: Vec<f64> = (0..N)
            .map(|i| (2.0 * std::f64::consts::PI * 300.0 * i as f64 / sr).sin())
            .collect();

        // Sweep a high-Q pole across two octaves over the buffer, per sample.
        let row_at = |t: f64| -> [f64; 5] {
            let hz = 400.0 * 2f64.powf(2.0 * t);
            let r = 0.995;
            let w = 2.0 * std::f64::consts::PI * hz / sr;
            [1.0, 0.0, 0.0, -2.0 * r * w.cos(), r * r]
        };

        // DF2, retuned every sample (canonical form, states w1/w2).
        let mut df2 = (0.0f64, 0.0f64);
        let mut df2_out = Vec::with_capacity(N);
        for (i, &x) in input.iter().enumerate() {
            let [b0, b1, b2, a1, a2] = row_at(i as f64 / N as f64);
            let y = b0 * x + df2.0;
            df2.0 = b1 * x - a1 * y + df2.1;
            df2.1 = b2 * x - a2 * y;
            df2_out.push(y);
        }

        // TPT, retuned every sample.
        let mut tpt = TptStage::passthrough();
        let mut tpt_out = Vec::with_capacity(N);
        for (i, &x) in input.iter().enumerate() {
            tpt.set_biquad(&row_at(i as f64 / N as f64));
            tpt_out.push(tpt.process(x));
        }

        // A 300 Hz input through a resonance sweeping 400..1600 Hz can produce
        // energy at neither. Anything up at 8 kHz+ was manufactured by the
        // modulation itself — that IS the click. Measure it, normalised by each
        // output's own level so the comparison is not gain-dominated.
        let spurious = |v: &[f64]| {
            let rms = (v[64..].iter().map(|y| y * y).sum::<f64>() / (N - 64) as f64).sqrt();
            let mut hf = 0.0;
            let mut bin = 8_000.0;
            while bin < 20_000.0 {
                let (mut re, mut im) = (0.0, 0.0);
                for (n, &y) in v[64..].iter().enumerate() {
                    let w = 2.0 * std::f64::consts::PI * bin * n as f64 / sr;
                    re += y * w.cos();
                    im -= y * w.sin();
                }
                hf += (re * re + im * im) / ((N - 64) as f64).powi(2);
                bin += 250.0;
            }
            hf.sqrt() / rms.max(1.0e-12)
        };
        // MEASURED 2026-08-07, and it did NOT go the way the theory predicted:
        // DF2 3.528e-3, TPT 3.531e-3. Identical to three figures. Under a SMOOTH
        // per-sample sweep the per-sample coefficient delta is tiny and DF2's
        // state shock never materialises — both forms realise the same
        // time-varying response and manufacture the same sidebands.
        //
        // The state-shock argument is real but it applies to ABRUPT changes, not
        // glides (see `probe::abrupt_change_while_ringing`). This test therefore
        // pins the parity so a future change to either form is noticed.
        let (s_df2, s_tpt) = (spurious(&df2_out), spurious(&tpt_out));
        let ratio = s_tpt / s_df2;
        assert!(
            (0.5..2.0).contains(&ratio),
            "expected parity under a smooth sweep; DF2 {s_df2:.3e}, TPT {s_tpt:.3e}"
        );
    }

    #[test]
    fn cascade_falls_back_rather_than_dropping_a_row() {
        let mut c = TptCascade::new();
        // A real-root row the TPT core cannot express.
        let mut rows = [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
        rows[2] = [1.0, 0.0, 0.0, -1.6, 0.63];
        c.set_rows(&rows);
        let mut out = Vec::new();
        for i in 0..512 {
            out.push(c.tick(if i == 0 { 1.0 } else { 0.0 }));
        }
        let want = df_reference(&rows[2], &impulse(512));
        let err = out
            .iter()
            .zip(&want)
            .map(|(a, b)| (f64::from(*a) - b).abs())
            .fold(0.0f64, f64::max);
        assert!(err < 1.0e-6, "fallback row deviated by {err:.3e}");
    }
}

#[cfg(test)]
mod probe {
    use super::*;
    /// Not an assertion — a measurement. Prints how each form reacts to an
    /// ABRUPT coefficient change while ringing, which is where the state-shock
    /// argument actually applies.
    #[test]
    fn abrupt_change_while_ringing() {
        let sr = 48_000.0;
        let row = |hz: f64, r: f64| {
            let w = 2.0 * std::f64::consts::PI * hz / sr;
            [1.0f64, 0.0, 0.0, -2.0 * r * w.cos(), r * r]
        };
        for (from, to) in [(400.0, 3_000.0), (400.0, 500.0), (1_000.0, 1_100.0)] {
            let (a, b) = (row(from, 0.995), row(to, 0.995));
            // Ring on the first setting, then switch in one sample, input silent.
            let n_ring = 2_000usize;
            let n_tail = 2_000usize;

            let mut w = (0.0f64, 0.0f64);
            let mut df2 = Vec::new();
            for i in 0..(n_ring + n_tail) {
                let x = if i == 0 { 1.0 } else { 0.0 };
                let r = if i < n_ring { a } else { b };
                let y = r[0] * x + w.0;
                w.0 = r[1] * x - r[3] * y + w.1;
                w.1 = r[2] * x - r[4] * y;
                df2.push(y);
            }

            let mut s = TptStage::passthrough();
            let mut tpt = Vec::new();
            for i in 0..(n_ring + n_tail) {
                let x = if i == 0 { 1.0 } else { 0.0 };
                s.set_biquad(if i < n_ring { &a } else { &b });
                tpt.push(s.process(x));
            }

            // Envelope either side of the switch: an energy-preserving switch
            // keeps the ring's amplitude; a shock steps it.
            let env = |v: &[f64], lo: usize, hi: usize| {
                v[lo..hi].iter().fold(0.0f64, |m, y| m.max(y.abs()))
            };
            let jump = |v: &[f64]| {
                let before = env(v, n_ring - 400, n_ring);
                let after = env(v, n_ring, n_ring + 400);
                20.0 * (after / before.max(1e-30)).log10()
            };
            println!(
                "{from:>6.0} -> {to:<6.0} Hz   DF2 {:+6.2} dB   TPT {:+6.2} dB",
                jump(&df2),
                jump(&tpt)
            );
        }
    }
}
