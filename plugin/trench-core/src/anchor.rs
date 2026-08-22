//! THE ANCHOR — fit the body to the singer.
//!
//! A vowel body is authored at one tract size. `VD_vocal_dynamic` is built from
//! the pooled median of 5938 measured utterances, which is nobody: a baritone
//! sings through a throat too small for him, a soprano through one too large.
//! The articulation still happens, it just does not fit.
//!
//! The fix is a single scalar. Because the words encode frequency in log space,
//! one ratio applied to every pole and zero slides the whole geometry in pitch
//! while every formant RELATIONSHIP stays locked — the vowel is preserved, only
//! the tract it lives in changes size. `transpose_conjugate_pair` already does
//! this; it is what the pitch LISTENER drives. The anchor is a second, much
//! slower source for the same ratio.
//!
//! WHY F3, AND WHY SLOWLY
//!
//! Measured across `recipes/tables/academia/` — 220 speakers, 2453 utterances
//! with speaker identity — asking which measure tracks the SPEAKER rather than
//! the VOWEL:
//!
//! ```text
//!   measure           spread across vowels   across speakers   ratio
//!   F1                      476 cents            234 cents      0.49
//!   F2                      596 cents            228 cents      0.38
//!   F3                      238 cents            199 cents      0.84   <- best
//!   mean(F1,F2,F3)          260 cents            184 cents      0.71
//!   F3-F2 spacing           941 cents            400 cents      0.43
//! ```
//!
//! F3 wins, but every ratio is UNDER 1: even F3 moves more with which vowel is
//! being sung than with who is singing it. So a single frame reads the vowel,
//! not the throat, and anchoring to it would be worse than not anchoring at
//! all. Getting the error down to a quarter of the speaker spread takes about
//! 23 distinct vowel events — call it five to ten seconds of singing.
//!
//! That is why this is a REFERENCE, not a measurement: a very slow mean that
//! holds between phrases instead of chasing.
//!
//! No FFT. Same closed-form centroid the aimer uses — `E[dx^2]/E[x^2] =
//! 4 sin^2(w/2)` — over a bandpass covering the measured F3 range.

/// Pooled median F3 across all 5938 measured utterances (Hillenbrand 1995,
/// Peterson-Barney 1952, Mokhtari & Tanaka 2000). The tract the bodies are
/// authored at, and therefore the ratio's denominator.
pub const ANCHOR_REF_HZ: f64 = 2623.0;
/// Measured p1..p99 of per-speaker mean F3, 220 speakers. Outside this is not a
/// singer with an unusual throat, it is the tracker being wrong.
pub const ANCHOR_RATIO_MIN: f64 = 0.860;
pub const ANCHOR_RATIO_MAX: f64 = 1.340;
/// Search band, from the measured spread of F3 with margin. F2 does intrude
/// here on close front vowels; that is exactly the vowel-variance the slow
/// reference averages out.
const BAND_LO_HZ: f64 = 1_800.0;
const BAND_HI_HZ: f64 = 4_200.0;
/// Energy window for the centroid.
const TAU_MS: f64 = 8.0;
/// The reference. ~23 vowel events at roughly 200 ms each.
const REF_TAU_S: f64 = 4.5;
/// Below this the band is too quiet to be a voice; hold the anchor.
const GATE_RMS: f64 = 1.0e-4;
/// Seconds of qualifying audio before the anchor is trusted at full depth.
const CONFIDENCE_S: f64 = 6.0;

#[derive(Clone)]
pub struct Anchor {
    sample_rate: f64,
    hop: usize,
    // Bandpass over the measured F3 range.
    b0: f64,
    a1: f64,
    a2: f64,
    x1: f64,
    x2: f64,
    y1: f64,
    y2: f64,
    // Centroid state.
    e_x: f64,
    e_dx: f64,
    prev: f64,
    alpha: f64,
    // The slow reference, in log2(Hz) so the mean is a geometric one.
    ref_log2: f64,
    ref_alpha: f64,
    primed: bool,
    confidence: f64,
    conf_step: f64,
    amount: f64,
}

impl Anchor {
    pub fn new(sample_rate: f64, hop: usize) -> Self {
        let mut a = Self {
            sample_rate, hop: hop.max(1),
            b0: 0.0, a1: 0.0, a2: 0.0, x1: 0.0, x2: 0.0, y1: 0.0, y2: 0.0,
            e_x: 0.0, e_dx: 0.0, prev: 0.0, alpha: 0.0,
            ref_log2: ANCHOR_REF_HZ.log2(), ref_alpha: 0.0,
            primed: false, confidence: 0.0, conf_step: 0.0, amount: 0.0,
        };
        a.prepare(sample_rate, a.hop);
        a
    }

    pub fn prepare(&mut self, sample_rate: f64, hop: usize) {
        self.sample_rate = sample_rate.max(8_000.0);
        self.hop = hop.max(1);
        let f0 = (BAND_LO_HZ * BAND_HI_HZ).sqrt();
        let q = f0 / (BAND_HI_HZ - BAND_LO_HZ);
        let w = 2.0 * std::f64::consts::PI * f0 / self.sample_rate;
        let al = w.sin() / (2.0 * q);
        let a0 = 1.0 + al;
        self.b0 = al / a0;
        self.a1 = -2.0 * w.cos() / a0;
        self.a2 = (1.0 - al) / a0;
        self.alpha = 1.0 - (-(1.0 / self.sample_rate) / (TAU_MS / 1000.0)).exp();
        let hop_s = self.hop as f64 / self.sample_rate;
        self.ref_alpha = 1.0 - (-hop_s / REF_TAU_S).exp();
        self.conf_step = hop_s / CONFIDENCE_S;
        self.reset();
    }

    pub fn reset(&mut self) {
        self.x1 = 0.0; self.x2 = 0.0; self.y1 = 0.0; self.y2 = 0.0;
        self.e_x = 0.0; self.e_dx = 0.0; self.prev = 0.0;
        self.ref_log2 = ANCHOR_REF_HZ.log2();
        self.primed = false;
        self.confidence = 0.0;
    }

    /// 0 = off, exact bypass: `ratio()` returns exactly 1.0.
    pub fn set_amount(&mut self, amount: f64) {
        let a = amount.clamp(0.0, 1.0);
        if self.amount <= 0.0 && a > 0.0 {
            self.reset();
        }
        self.amount = a;
    }

    pub fn amount(&self) -> f64 {
        self.amount
    }

    /// The tracked tract size in Hz. Observation.
    pub fn tracked_hz(&self) -> f64 {
        self.ref_log2.exp2()
    }

    /// How settled the anchor is, 0..1. Observation — it is a slow measure and
    /// the UI should say so rather than look broken for the first few seconds.
    pub fn confidence(&self) -> f64 {
        self.confidence
    }

    /// The scalar every pole and zero gets multiplied by. Exactly 1.0 when off
    /// or unsettled, so a cold start never moves the body.
    pub fn ratio(&self) -> f64 {
        if self.amount <= 0.0 {
            return 1.0;
        }
        let raw = (self.tracked_hz() / ANCHOR_REF_HZ)
            .clamp(ANCHOR_RATIO_MIN, ANCHOR_RATIO_MAX);
        // Blend in over confidence AND depth: 1.0 -> raw.
        1.0 + (raw - 1.0) * self.confidence * self.amount
    }

    /// One control hop.
    pub fn advance(&mut self, block: &[f32]) {
        if self.amount <= 0.0 || block.is_empty() {
            return;
        }
        let mut in_sq = 0.0;
        for &s in block {
            let x = f64::from(s);
            let y = self.b0 * (x - self.x2) - self.a1 * self.y1 - self.a2 * self.y2;
            let y = if y.is_finite() { y } else { 0.0 };
            self.x2 = self.x1; self.x1 = x;
            self.y2 = self.y1; self.y1 = y;
            let d = y - self.prev;
            self.prev = y;
            self.e_x += (y * y - self.e_x) * self.alpha;
            self.e_dx += (d * d - self.e_dx) * self.alpha;
            in_sq += x * x;
        }
        // Gate on the INPUT, not the filtered signal. Gating on the output lets
        // the bandpass's own ringdown keep feeding the reference after the sound
        // has stopped, and that decay's centroid is the filter's centre
        // frequency rather than the singer's F3 — a slow drift toward 2750 Hz
        // during every gap. The filter keeps running; only the reference stops.
        if (in_sq / block.len() as f64).sqrt() < GATE_RMS || self.e_x <= 1.0e-14 {
            return;
        }
        let ratio = (self.e_dx / self.e_x).max(0.0);
        let s = (ratio.sqrt() / 2.0).clamp(0.0, 1.0);
        let hz = (2.0 * s.asin()) * self.sample_rate / (2.0 * std::f64::consts::PI);
        if !(BAND_LO_HZ..=BAND_HI_HZ).contains(&hz) {
            return;
        }
        let l = hz.log2();
        if !self.primed {
            self.primed = true;
            self.ref_log2 = l;
        }
        // Geometric mean: averaging in log2 keeps this a ratio, not an offset.
        self.ref_log2 += (l - self.ref_log2) * self.ref_alpha;
        self.confidence = (self.confidence + self.conf_step).min(1.0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SR: f64 = 48_000.0;
    const HOP: usize = 32;

    fn feed(a: &mut Anchor, hz: f64, seconds: f64) {
        let n = (SR * seconds) as usize;
        let sig: Vec<f32> = (0..n)
            .map(|i| (2.0 * std::f64::consts::PI * hz * i as f64 / SR).sin() as f32)
            .collect();
        for b in sig.chunks(HOP) {
            a.advance(b);
        }
    }

    #[test]
    fn off_is_an_exact_bypass() {
        let mut a = Anchor::new(SR, HOP);
        feed(&mut a, 3_200.0, 10.0);
        assert_eq!(a.ratio(), 1.0, "amount 0 must leave the geometry alone");
    }

    #[test]
    fn a_cold_start_does_not_move_the_body() {
        let mut a = Anchor::new(SR, HOP);
        a.set_amount(1.0);
        feed(&mut a, 3_200.0, 0.05);
        assert!((a.ratio() - 1.0).abs() < 0.02,
                "an unsettled anchor must be near unity, got {:.3}", a.ratio());
    }

    #[test]
    fn it_finds_a_big_tract_and_a_small_one() {
        // A baritone sits low, a soprano high; both inside the measured range.
        for (hz, want) in [(2_300.0, 2_300.0 / ANCHOR_REF_HZ),
                           (3_300.0, 3_300.0 / ANCHOR_REF_HZ)] {
            let mut a = Anchor::new(SR, HOP);
            a.set_amount(1.0);
            feed(&mut a, hz, 25.0);
            let got = a.ratio();
            assert!(a.confidence() > 0.99, "should be settled, got {:.2}", a.confidence());
            assert!((got - want).abs() < 0.03,
                    "{hz} Hz -> ratio {got:.3}, expected {want:.3}");
        }
    }

    #[test]
    fn the_ratio_never_leaves_the_measured_speaker_range() {
        for hz in [1_850.0, 4_150.0] {
            let mut a = Anchor::new(SR, HOP);
            a.set_amount(1.0);
            feed(&mut a, hz, 25.0);
            assert!((ANCHOR_RATIO_MIN..=ANCHOR_RATIO_MAX).contains(&a.ratio()),
                    "{hz} Hz gave {:.3}, outside the 220-speaker range", a.ratio());
        }
    }

    #[test]
    fn silence_holds_the_anchor_rather_than_drifting() {
        let mut a = Anchor::new(SR, HOP);
        a.set_amount(1.0);
        feed(&mut a, 3_300.0, 25.0);
        let held = a.ratio();
        let quiet = vec![0.0f32; (SR * 10.0) as usize];
        for b in quiet.chunks(HOP) {
            a.advance(b);
        }
        assert!((a.ratio() - held).abs() < 1.0e-9,
                "silence moved the anchor {held:.4} -> {:.4}", a.ratio());
    }

    #[test]
    fn it_settles_slowly_enough_to_ignore_a_single_vowel() {
        // The corpus says one frame reads the vowel, not the throat. Settled on
        // one tract, a brief excursion must barely move the anchor.
        let mut a = Anchor::new(SR, HOP);
        a.set_amount(1.0);
        feed(&mut a, 2_600.0, 25.0);
        let before = a.ratio();
        feed(&mut a, 3_900.0, 0.2);          // one 200 ms vowel, far away
        let after = a.ratio();
        let cents = 1200.0 * (after / before).log2().abs();
        // The excursion is 700 cents. A 4.5 s reference over 200 ms admits
        // 1 - exp(-0.2/4.5) = 4.35% of it, so ~30 cents is the design value,
        // not a tolerance. Anything much larger means the reference got faster.
        let admitted = 700.0 * (1.0 - (-0.2f64 / REF_TAU_S).exp());
        assert!(cents < admitted * 1.2,
                "one vowel moved the anchor {cents:.0} cents, design admits {admitted:.0}");
    }
}
