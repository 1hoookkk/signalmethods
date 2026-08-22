//! THE GATE â€” what kind of energy did the aimer find?
//!
//! `Aimer` answers WHERE and HOW MUCH, exactly, with arithmetic. What it cannot
//! answer is WHAT: it opens on any band that pops above its own average, so a
//! cymbal, a bright synth stab and a singer's "s" all read the same.
//!
//! That is a category judgement, and it is the one job here that genuinely
//! wants a trained model â€” sibilant frames are abundant and free to label
//! (every phoneme-aligned speech corpus marks /s/ /S/ /z/ /tS/), so it is a
//! well-posed supervised problem rather than a taste problem.
//!
//! WHERE IT SITS
//!
//! Not in the audio path, and not in the fast path. The gate runs on ~10 ms
//! frames and only scales the aimer's depth. Whether a sound is sibilant does
//! not change within a syllable, so 10 ms of decision latency costs nothing,
//! while the millisecond timing stays with the centroid, which is exact.
//!
//!   gate (10 ms):  is this sibilance?      -> scales the depth
//!   aimer (1 ms):  where, and how far out? -> drives the wheels
//!
//! THE MODEL SLOT
//!
//! `Gate` is a logistic over three features. `set_weights` replaces the
//! hand-set vector with a trained one of the same shape, and `set_mlp` swaps in
//! a small one-hidden-layer network over the same features for a real model.
//! The features stay the contract either way.
//!
//! NOTE: the shipped weights are HAND SET from the feature definitions below,
//! not trained. They are a working default and a placeholder, and they have not
//! been evaluated against a corpus.

/// Frames the gate decides on â€” 16 control hops, ~10.7 ms at 48 kHz.
pub const GATE_HOPS: usize = 16;
/// Feature count. The contract between the front end and whatever scores it.
pub const NUM_FEATURES: usize = 3;

/// Logistic weights over `[hf_excess, lf_excess, jitter]`, plus bias.
///
/// FITTED, not hand set — `tools/fit_sibilance_gate.py`, 2026-08-07.
/// 22200 frames, class-balanced logistic regression:
///
/// ```text
///   accuracy 0.908   sibilance caught 0.884   voice protected 0.921
///
///   feature      positive mean   negative mean
///   hf_excess        0.157           0.009
///   lf_excess        0.126           0.026
///   jitter           0.090           0.016
/// ```
///
/// JITTER carries the decision (34.4 against hf's 13.8). Sibilance is noise, so
/// its centroid wanders; a synth harmonic or a formant holds still. The first
/// hand-set attempt leaned on `hf_excess` and let every bright transient
/// through, and guessed `lf_excess` NEGATIVE when the fit says mildly positive.
///
/// PROVENANCE, so nobody over-trusts these: the NEGATIVE class is real —
/// 5938 measured vowel utterances (Hillenbrand 1995, Peterson-Barney 1952 and
/// Mokhtari & Tanaka 2000 — the Japanese ETL set, the only one carrying measured
/// BANDWIDTHS and F4, so it resynthesises faithfully rather than through Klatt's
/// generic values; adding it moved voice protection 0.917 -> 0.921)
/// resynthesised through a Klatt cascade on their own F0/F1/F2/F3. Protecting
/// the voice is fitted to measurement. The POSITIVE class is SYNTHETIC —
/// noise through a high resonance, the standard fricative model — because the
/// tree holds no labelled sibilance recordings. Refit against real fricatives
/// (TIMIT or forced-aligned LibriSpeech) before trusting the positive side.
const FITTED_W: [f64; NUM_FEATURES] = [13.7562, 2.2550, 34.3961];
const FITTED_B: f64 = -2.4001;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Scorer {
    /// w . x + b, through a logistic.
    Logistic { w: [f64; NUM_FEATURES], b: f64 },
}

/// A small one-hidden-layer network over the same features. Sized to run at
/// frame rate on any host; weights come from outside.
#[derive(Clone)]
pub struct Mlp {
    pub w1: Vec<[f64; NUM_FEATURES]>,
    pub b1: Vec<f64>,
    pub w2: Vec<f64>,
    pub b2: f64,
}

impl Mlp {
    fn forward(&self, x: &[f64; NUM_FEATURES]) -> f64 {
        let mut acc = self.b2;
        for (i, row) in self.w1.iter().enumerate() {
            let mut h = self.b1.get(i).copied().unwrap_or(0.0);
            for (wj, xj) in row.iter().zip(x) {
                h += wj * xj;
            }
            let h = if h > 0.0 { h } else { 0.0 }; // ReLU
            acc += self.w2.get(i).copied().unwrap_or(0.0) * h;
        }
        acc
    }
}

#[inline]
fn logistic(z: f64) -> f64 {
    1.0 / (1.0 + (-z).exp())
}

pub struct Gate {
    scorer: Scorer,
    mlp: Option<Mlp>,
    /// Frame accumulators.
    hops: usize,
    hf_sum: f64,
    lf_sum: f64,
    hz_sum: f64,
    hz_sq_sum: f64,
    /// Last decision, held between frames.
    probability: f64,
    /// How hard the decision is applied. 0 = gate off, pass everything.
    strength: f64,
    features: [f64; NUM_FEATURES],
}

impl Default for Gate {
    fn default() -> Self {
        Self::new()
    }
}

impl Gate {
    pub fn new() -> Self {
        Self {
            scorer: Scorer::Logistic { w: FITTED_W, b: FITTED_B },
            mlp: None,
            hops: 0,
            hf_sum: 0.0,
            lf_sum: 0.0,
            hz_sum: 0.0,
            hz_sq_sum: 0.0,
            probability: 1.0,
            strength: 0.0,
            features: [0.0; NUM_FEATURES],
        }
    }

    pub fn reset(&mut self) {
        self.hops = 0;
        self.hf_sum = 0.0;
        self.lf_sum = 0.0;
        self.hz_sum = 0.0;
        self.hz_sq_sum = 0.0;
        self.probability = 1.0;
        self.features = [0.0; NUM_FEATURES];
    }

    /// 0 = off (the aimer's depth passes through untouched, exact bypass).
    /// 1 = the gate's opinion is applied in full.
    pub fn set_strength(&mut self, strength: f64) {
        self.strength = strength.clamp(0.0, 1.0);
    }

    pub fn strength(&self) -> f64 {
        self.strength
    }

    /// Replace the hand-set logistic with trained weights of the same shape.
    pub fn set_weights(&mut self, w: [f64; NUM_FEATURES], b: f64) {
        self.scorer = Scorer::Logistic { w, b };
        self.mlp = None;
    }

    /// Swap in a trained network over the same features. Takes precedence.
    pub fn set_mlp(&mut self, mlp: Mlp) {
        self.mlp = Some(mlp);
    }

    pub fn clear_mlp(&mut self) {
        self.mlp = None;
    }

    /// Probability the last decided frame was sibilant. Observation.
    pub fn probability(&self) -> f64 {
        self.probability
    }

    /// The features behind that decision. Observation â€” and the thing to log if
    /// you ever want to build a training set from real sessions.
    pub fn features(&self) -> [f64; NUM_FEATURES] {
        self.features
    }

    /// One control hop of evidence. `hf_excess` and `lf_excess` are the two
    /// bands' rise above their own running averages (0..1); `hz` is the aimer's
    /// current centroid. Decides once per `GATE_HOPS`.
    pub fn observe(&mut self, hf_excess: f64, lf_excess: f64, hz: f64) {
        self.hf_sum += hf_excess;
        self.lf_sum += lf_excess;
        self.hz_sum += hz;
        self.hz_sq_sum += hz * hz;
        self.hops += 1;
        if self.hops < GATE_HOPS {
            return;
        }
        let n = self.hops as f64;
        let hf = self.hf_sum / n;
        let lf = self.lf_sum / n;
        let mean = self.hz_sum / n;
        let var = (self.hz_sq_sum / n - mean * mean).max(0.0);
        // Jitter: centroid wander as a fraction of the centroid itself, so it
        // means the same thing at 3 kHz and 9 kHz. Noise wanders, tone does not.
        let jitter = if mean > 1.0 { (var.sqrt() / mean * 8.0).min(1.0) } else { 0.0 };

        self.features = [hf, lf, jitter];
        let z = match (&self.mlp, &self.scorer) {
            (Some(m), _) => m.forward(&self.features),
            (None, Scorer::Logistic { w, b }) => {
                b + w.iter().zip(&self.features).map(|(wi, xi)| wi * xi).sum::<f64>()
            }
        };
        self.probability = logistic(z);

        self.hops = 0;
        self.hf_sum = 0.0;
        self.lf_sum = 0.0;
        self.hz_sum = 0.0;
        self.hz_sq_sum = 0.0;
    }

    /// Apply the decision to the aimer's depth. Strength 0 returns `depth`
    /// bit for bit.
    #[inline]
    pub fn apply(&self, depth: f64) -> f64 {
        if self.strength <= 0.0 {
            return depth;
        }
        depth * (1.0 - self.strength + self.strength * self.probability)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The measured class means from the fit — see FITTED_W above.
    const SIBILANT: [f64; NUM_FEATURES] = [0.157, 0.126, 0.090];
    const VOICED: [f64; NUM_FEATURES] = [0.009, 0.026, 0.016];
    /// A synth: reaches past the voice ceiling like an ess, but holds still.
    const BRIGHT_TONAL: [f64; NUM_FEATURES] = [0.157, 0.026, 0.000];

    /// Feed a frame whose measured features come out as `f`.
    fn run(g: &mut Gate, f: [f64; NUM_FEATURES]) {
        let hz = 7_000.0;
        let d = f[2] * hz / 8.0; // jitter = sd/mean*8, sd of an alternation is d
        for i in 0..GATE_HOPS {
            g.observe(f[0], f[1], hz + if i % 2 == 0 { d } else { -d });
        }
    }

    #[test]
    fn off_is_an_exact_bypass() {
        let mut g = Gate::new();
        run(&mut g, SIBILANT);
        for d in [0.0, 0.25, 0.5, 1.0] {
            assert_eq!(g.apply(d), d, "strength 0 must not touch the depth");
        }
    }

    #[test]
    fn the_helper_reproduces_the_measured_features() {
        let mut g = Gate::new();
        run(&mut g, SIBILANT);
        for (got, want) in g.features().iter().zip(&SIBILANT) {
            assert!((got - want).abs() < 1e-6, "{:?} != {SIBILANT:?}", g.features());
        }
    }

    #[test]
    fn sibilance_passes() {
        let mut g = Gate::new();
        g.set_strength(1.0);
        run(&mut g, SIBILANT);
        assert!(g.probability() > 0.75, "got {:.2}", g.probability());
    }

    #[test]
    fn voiced_material_is_rejected() {
        let mut g = Gate::new();
        g.set_strength(1.0);
        run(&mut g, VOICED);
        assert!(g.probability() < 0.5, "got {:.2}", g.probability());
    }

    #[test]
    fn steady_bright_tone_is_rejected() {
        // Same high-band reach as an ess, but the centroid does not wander.
        // This is the case the hand-set weights got wrong.
        let mut g = Gate::new();
        g.set_strength(1.0);
        run(&mut g, BRIGHT_TONAL);
        assert!(g.probability() < 0.5, "got {:.2} from {:?}", g.probability(), g.features());
    }

    #[test]
    fn jitter_is_what_separates_them() {
        // The two differ ONLY in wander, and that alone flips the verdict.
        let mut a = Gate::new();
        let mut b = Gate::new();
        run(&mut a, SIBILANT);
        run(&mut b, [SIBILANT[0], SIBILANT[1], 0.0]);
        // Removing wander alone drops it from confident to a coin toss: jitter
        // is worth more than both level features put together. It lands ON the
        // boundary rather than under it, which is the honest reading — the
        // other two features nearly clear the bias by themselves.
        assert!(a.probability() > 0.9, "sibilant: {:.3}", a.probability());
        assert!(b.probability() < 0.55, "no wander: {:.3}", b.probability());
        assert!(a.probability() - b.probability() > 0.4,
                "{:.3} vs {:.3}", a.probability(), b.probability());
    }

    #[test]
    fn trained_weights_replace_the_fitted_ones() {
        let mut g = Gate::new();
        g.set_strength(1.0);
        run(&mut g, SIBILANT);
        let before = g.probability();
        g.set_weights([-13.76, -2.26, -34.40], 2.4);
        run(&mut g, SIBILANT);
        assert!(before > 0.75 && g.probability() < 0.25,
                "{before:.2} -> {:.2}", g.probability());
    }

    #[test]
    fn an_mlp_takes_precedence_and_runs() {
        let mut g = Gate::new();
        g.set_strength(1.0);
        g.set_mlp(Mlp {
            w1: vec![[0.0, 0.0, 100.0], [0.0, 0.0, 0.0]],
            b1: vec![0.0, 0.0],
            w2: vec![1.0, 0.0],
            b2: -5.0,
        });
        run(&mut g, SIBILANT);
        assert!(g.probability() > 0.9, "got {:.3}", g.probability());
        run(&mut g, VOICED);
        assert!(g.probability() < 0.1, "got {:.3}", g.probability());
    }

    #[test]
    fn strength_scales_between_bypass_and_full_opinion() {
        let mut g = Gate::new();
        run(&mut g, VOICED);
        let p = g.probability();
        g.set_strength(0.5);
        let half = g.apply(1.0);
        g.set_strength(1.0);
        let full = g.apply(1.0);
        assert!((full - p).abs() < 1e-12);
        assert!(half > full && half < 1.0, "half {half:.3}, full {full:.3}");
    }
}
