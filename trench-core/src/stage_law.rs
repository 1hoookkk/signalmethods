use crate::minifloat::{decode, encode, COMBINE_K};
pub const DEFAULT_AUTHORING_SR: f64 = crate::compiler::DEFAULT_AUTHORING_SR;
pub const P2K_DATUM_SR: f64 = 44_100.0;
const TAU: f64 = core::f64::consts::PI * 2.0;
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum RootPair {
    Conjugate { hz: f64, r: f64 },
    RealPair { root_a: f64, root_b: f64 },
    Degenerate,
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct StageGeometry {
    pub pole: RootPair,
    pub zero: RootPair,
    pub scale: f64,
}
impl StageGeometry {
    pub const IDENTITY: StageGeometry = StageGeometry {
        pole: RootPair::Degenerate,
        zero: RootPair::Degenerate,
        scale: 1.0,
    };
    pub fn biquad_at(&self, sample_rate_hz: f64) -> [f64; 5] {
        let (zero_p, zero_q) = pair_coefficients_at(self.zero, sample_rate_hz);
        let (pole_p, pole_q) = pair_coefficients_at(self.pole, sample_rate_hz);
        [
            self.scale,
            self.scale * zero_p,
            self.scale * zero_q,
            pole_p,
            pole_q,
        ]
    }
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct StageRoots {
    pub pole_hz: f64,
    pub pole_r: f64,
    pub zero_hz: f64,
    pub zero_r: f64,
    pub scale: f64,
}
impl StageRoots {
    pub const IDENTITY: StageRoots = StageRoots {
        pole_hz: 0.0,
        pole_r: 0.0,
        zero_hz: 0.0,
        zero_r: 0.0,
        scale: 1.0,
    };
    pub fn biquad(&self) -> [f64; 5] {
        self.biquad_at(DEFAULT_AUTHORING_SR)
    }
    pub fn biquad_at(&self, sample_rate_hz: f64) -> [f64; 5] {
        let (wz, wp) = (
            TAU * self.zero_hz / sample_rate_hz,
            TAU * self.pole_hz / sample_rate_hz,
        );
        let k = self.scale;
        [
            k,
            k * (-2.0 * self.zero_r * wz.cos()),
            k * (self.zero_r * self.zero_r),
            -2.0 * self.pole_r * wp.cos(),
            self.pole_r * self.pole_r,
        ]
    }
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AuthoringLimits {
    pub sample_rate_hz: f64,
    pub display_freq_min_hz: f64,
    pub display_freq_max_hz: f64,
    pub authoring_freq_max_hz: f64,
    pub pole_radius_max: f64,
    pub zero_radius_max: f64,
    pub scale_min: f64,
    pub scale_max: f64,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum RootValidity {
    Ok = 0,
    NotFinite = 1,
    DisplayDomainLow = 2,
    DisplayDomainHigh = 3,
    AuthoringFreq = 4,
    PoleRadius = 5,
    ZeroRadius = 6,
    Scale = 7,
}
pub fn encoded_radius(r: f64) -> f64 {
    (1.0 - decode(encode(1.0 - r * r))).max(0.0).sqrt()
}
pub fn radius_survives_encoding(r: f64) -> bool {
    encoded_radius(r) < 1.0
}
pub fn max_contiguous_pole_radius() -> f64 {
    let (mut lo, mut hi) = (0.0f64, 1.0f64);
    loop {
        let mid = 0.5 * (lo + hi);
        if mid <= lo || mid >= hi {
            return lo;
        }
        if radius_survives_encoding(mid) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
}
pub fn max_encodable_zero_radius() -> f64 {
    1.0
}
pub fn authoring_limits() -> AuthoringLimits {
    authoring_limits_at(DEFAULT_AUTHORING_SR)
}
pub fn authoring_limits_at(sample_rate_hz: f64) -> AuthoringLimits {
    AuthoringLimits {
        sample_rate_hz,
        display_freq_min_hz: crate::armadillo::display_lo_hz(sample_rate_hz),
        display_freq_max_hz: crate::armadillo::display_hi_hz(sample_rate_hz),
        authoring_freq_max_hz: sample_rate_hz * 0.49,
        pole_radius_max: max_contiguous_pole_radius(),
        zero_radius_max: max_encodable_zero_radius(),
        scale_min: 0.0,
        scale_max: COMBINE_K,
    }
}
pub fn validate_stage_roots(r: &StageRoots) -> RootValidity {
    validate_stage_roots_at(r, DEFAULT_AUTHORING_SR)
}
pub fn validate_stage_roots_at(r: &StageRoots, sample_rate_hz: f64) -> RootValidity {
    let lim = authoring_limits_at(sample_rate_hz);
    if ![r.pole_hz, r.pole_r, r.zero_hz, r.zero_r, r.scale]
        .iter()
        .all(|v| v.is_finite())
    {
        return RootValidity::NotFinite;
    }
    if r.scale < lim.scale_min || r.scale > lim.scale_max {
        return RootValidity::Scale;
    }
    if r.pole_r < 0.0 || !radius_survives_encoding(r.pole_r) {
        return RootValidity::PoleRadius;
    }
    if r.zero_r < 0.0
        || r.zero_r > lim.zero_radius_max
        || (r.zero_r != lim.zero_radius_max && !radius_survives_encoding(r.zero_r))
    {
        return RootValidity::ZeroRadius;
    }
    for (hz, radius) in [(r.pole_hz, r.pole_r), (r.zero_hz, r.zero_r)] {
        if radius == 0.0 {
            continue;
        }
        if hz < lim.display_freq_min_hz {
            return RootValidity::DisplayDomainLow;
        }
        if hz > lim.display_freq_max_hz {
            return RootValidity::DisplayDomainHigh;
        }
        if hz > lim.authoring_freq_max_hz {
            return RootValidity::AuthoringFreq;
        }
    }
    RootValidity::Ok
}
pub fn words_from_roots(r: &StageRoots) -> [u16; 5] {
    words_from_roots_at(r, DEFAULT_AUTHORING_SR)
}
pub fn words_from_roots_at(r: &StageRoots, sample_rate_hz: f64) -> [u16; 5] {
    let wz = TAU * r.zero_hz / sample_rate_hz;
    let wp = TAU * r.pole_hz / sample_rate_hz;
    let (rz, rp) = (r.zero_r, r.pole_r);
    let c0 = 2.0 - 2.0 * rz * wz.cos();
    let c1 = 1.0 - rz * rz;
    let c2 = 2.0 - 2.0 * rp * wp.cos();
    let c3 = 1.0 - rp * rp;
    let c4 = r.scale;
    [
        encode((c0 - c1) / 4.0),
        encode(c1),
        encode((c2 - c3) / 4.0),
        encode(c3),
        encode(c4 / 4.0),
    ]
}
pub fn recompile_stage_words(words: [u16; 5], from_rate: f64, to_rate: f64) -> [u16; 5] {
    if from_rate == to_rate {
        return words;
    }
    match roots_from_words_at(words, from_rate) {
        Some(mut roots) => {
            let ceiling = to_rate * 0.49;
            roots.pole_hz = roots.pole_hz.min(ceiling);
            roots.zero_hz = roots.zero_hz.min(ceiling);
            words_from_roots_at(&roots, to_rate)
        }
        None => words,
    }
}
pub fn words_from_geometry(g: &StageGeometry) -> [u16; 5] {
    words_from_geometry_at(g, DEFAULT_AUTHORING_SR)
}
pub fn words_from_geometry_at(g: &StageGeometry, sample_rate_hz: f64) -> [u16; 5] {
    let (zero_p, zero_q) = pair_coefficients_at(g.zero, sample_rate_hz);
    let (pole_p, pole_q) = pair_coefficients_at(g.pole, sample_rate_hz);
    [
        encode((zero_p + 1.0 + zero_q) / 4.0),
        encode(1.0 - zero_q),
        encode((pole_p + 1.0 + pole_q) / 4.0),
        encode(1.0 - pole_q),
        encode(g.scale / 4.0),
    ]
}
fn pair_coefficients_at(pair: RootPair, sr: f64) -> (f64, f64) {
    match pair {
        RootPair::Conjugate { hz, r } => {
            let angle = TAU * hz / sr;
            (-2.0 * r * angle.cos(), r * r)
        }
        RootPair::RealPair { root_a, root_b } => (-(root_a + root_b), root_a * root_b),
        RootPair::Degenerate => (0.0, 0.0),
    }
}
pub fn geometry_from_words(words: [u16; 5]) -> StageGeometry {
    geometry_from_words_at(words, DEFAULT_AUTHORING_SR)
}
pub fn geometry_from_words_at(words: [u16; 5], sample_rate_hz: f64) -> StageGeometry {
    StageGeometry {
        zero: pair_geometry_at(decode(words[0]), decode(words[1]), sample_rate_hz),
        pole: pair_geometry_at(decode(words[2]), decode(words[3]), sample_rate_hz),
        scale: 4.0 * decode(words[4]),
    }
}
pub fn roots_from_words(words: [u16; 5]) -> Option<StageRoots> {
    roots_from_words_at(words, DEFAULT_AUTHORING_SR)
}
pub fn roots_from_words_at(words: [u16; 5], sample_rate_hz: f64) -> Option<StageRoots> {
    let g = geometry_from_words_at(words, sample_rate_hz);
    let (zero_hz, zero_r) = conjugate_or_origin(&g.zero)?;
    let (pole_hz, pole_r) = conjugate_or_origin(&g.pole)?;
    Some(StageRoots {
        pole_hz,
        pole_r,
        zero_hz,
        zero_r,
        scale: g.scale,
    })
}
fn conjugate_or_origin(p: &RootPair) -> Option<(f64, f64)> {
    match p {
        RootPair::Conjugate { hz, r } => Some((*hz, *r)),
        RootPair::Degenerate => Some((0.0, 0.0)),
        RootPair::RealPair { .. } => None,
    }
}
fn pair_geometry_at(d_mag: f64, d_rsq: f64, sample_rate_hz: f64) -> RootPair {
    let q = 1.0 - d_rsq;
    let c = 4.0 * d_mag + d_rsq;
    let p = c - 2.0;
    if p == 0.0 && q == 0.0 {
        return RootPair::Degenerate;
    }
    let disc = p * p - 4.0 * q;
    if disc < 0.0 {
        let r = q.sqrt();
        let cos_w = -p / (2.0 * r);
        RootPair::Conjugate {
            hz: cos_w.acos() / TAU * sample_rate_hz,
            r,
        }
    } else {
        let s = disc.sqrt();
        RootPair::RealPair {
            root_a: (-p + s) / 2.0,
            root_b: (-p - s) / 2.0,
        }
    }
}
#[cfg(test)]
mod authoring_domain {
    use super::*;
    #[test]
    fn the_contiguous_ceiling_survives_encoding_and_the_next_step_does_not() {
        let r = max_contiguous_pole_radius();
        assert!(r > 0.0 && r < 1.0, "ceiling {r} is not a usable radius");
        assert!(
            radius_survives_encoding(r),
            "the reported ceiling {r} does not survive the encoder"
        );
        let next = f64::from_bits(r.to_bits() + 1);
        assert!(
            !radius_survives_encoding(next),
            "the ceiling is not tight: {next} also survives"
        );
        let analytic = (1.0f64 - (2.0f64).powi(-15)).sqrt();
        assert!(
            (r - analytic).abs() <= 4.0 * f64::EPSILON,
            "contiguous ceiling {r} drifted from the encoder's denormal overflow {analytic}"
        );
    }
    #[test]
    fn the_contiguous_ceiling_decodes_to_a_stable_pole() {
        let r = max_contiguous_pole_radius();
        let decoded = encoded_radius(r);
        assert!(
            decoded < 1.0,
            "the ceiling decodes to r = {decoded}, which is on or outside the unit circle"
        );
    }
    #[test]
    fn the_encoder_hole_above_the_contiguous_ceiling_is_refused_not_silently_moved() {
        let ceiling = max_contiguous_pole_radius();
        let in_hole = f64::from_bits(ceiling.to_bits() + 1);
        assert_eq!(
            encoded_radius(in_hole),
            1.0,
            "the hole should reach the unit circle"
        );
        assert_eq!(
            validate_stage_roots(&StageRoots {
                pole_r: in_hole,
                ..valid()
            }),
            RootValidity::PoleRadius
        );
        let above_hole = 0.99999f64;
        assert!(
            above_hole > ceiling,
            "the probe radius must be past the ceiling"
        );
        assert!(
            radius_survives_encoding(above_hole),
            "past the hole should be representable"
        );
        assert_eq!(
            validate_stage_roots(&StageRoots {
                pole_r: above_hole,
                ..valid()
            }),
            RootValidity::Ok
        );
    }
    #[test]
    fn a_zero_inside_the_hole_is_refused_but_the_unit_circle_itself_is_allowed() {
        let in_hole = f64::from_bits(max_contiguous_pole_radius().to_bits() + 1);
        assert_eq!(
            validate_stage_roots(&StageRoots {
                zero_r: in_hole,
                ..valid()
            }),
            RootValidity::ZeroRadius
        );
        assert_eq!(
            validate_stage_roots(&StageRoots {
                zero_r: 1.0,
                ..valid()
            }),
            RootValidity::Ok
        );
    }
    #[test]
    fn a_zero_on_the_unit_circle_round_trips_exactly() {
        let roots = StageRoots {
            pole_hz: 1000.0,
            pole_r: 0.9,
            zero_hz: 3000.0,
            zero_r: max_encodable_zero_radius(),
            scale: 1.0,
        };
        assert_eq!(validate_stage_roots(&roots), RootValidity::Ok);
        let back = roots_from_words(words_from_roots(&roots)).expect("conjugate row");
        assert!(
            (back.zero_r - 1.0).abs() < 1e-12,
            "zero r came back {}",
            back.zero_r
        );
    }
    #[test]
    fn the_limits_agree_with_the_crate_constants() {
        let lim = authoring_limits();
        assert_eq!(lim.sample_rate_hz, crate::compiler::DEFAULT_AUTHORING_SR);
        assert_eq!(lim.authoring_freq_max_hz, crate::compiler::FREQ_MAX);
        assert_eq!(lim.display_freq_min_hz, DEFAULT_AUTHORING_SR / 2048.0);
        assert_eq!(lim.display_freq_max_hz, DEFAULT_AUTHORING_SR / 2.0);
        assert_eq!(lim.scale_max, COMBINE_K);
        assert!(lim.authoring_freq_max_hz < lim.display_freq_max_hz);
    }
    #[test]
    fn identity_roots_validate_and_encode_to_the_exact_identity_row() {
        assert_eq!(
            validate_stage_roots(&StageRoots::IDENTITY),
            RootValidity::Ok
        );
        let words = words_from_roots(&StageRoots::IDENTITY);
        let biquad = crate::minifloat::stage_words_to_biquad(words);
        assert_eq!(
            biquad,
            [1.0, 0.0, 0.0, 0.0, 0.0],
            "identity row is not the identity biquad"
        );
    }
    fn valid() -> StageRoots {
        StageRoots {
            pole_hz: 1000.0,
            pole_r: 0.95,
            zero_hz: 3000.0,
            zero_r: 0.5,
            scale: 1.0,
        }
    }
    #[test]
    fn each_violation_returns_its_own_distinct_reason() {
        let lim = authoring_limits();
        assert_eq!(validate_stage_roots(&valid()), RootValidity::Ok);
        let cases: [(StageRoots, RootValidity); 8] = [
            (
                StageRoots {
                    pole_hz: f64::NAN,
                    ..valid()
                },
                RootValidity::NotFinite,
            ),
            (
                StageRoots {
                    zero_r: f64::INFINITY,
                    ..valid()
                },
                RootValidity::NotFinite,
            ),
            (
                StageRoots {
                    pole_hz: lim.display_freq_min_hz * 0.5,
                    ..valid()
                },
                RootValidity::DisplayDomainLow,
            ),
            (
                StageRoots {
                    pole_hz: lim.display_freq_max_hz * 1.001,
                    ..valid()
                },
                RootValidity::DisplayDomainHigh,
            ),
            (
                StageRoots {
                    pole_hz: 0.5 * (lim.authoring_freq_max_hz + lim.display_freq_max_hz),
                    ..valid()
                },
                RootValidity::AuthoringFreq,
            ),
            (
                StageRoots {
                    pole_r: 1.5,
                    ..valid()
                },
                RootValidity::PoleRadius,
            ),
            (
                StageRoots {
                    zero_r: 1.0000001,
                    ..valid()
                },
                RootValidity::ZeroRadius,
            ),
            (
                StageRoots {
                    scale: lim.scale_max + 0.1,
                    ..valid()
                },
                RootValidity::Scale,
            ),
        ];
        for (roots, expected) in cases {
            assert_eq!(validate_stage_roots(&roots), expected, "for {roots:?}");
        }
    }
    #[test]
    fn a_root_at_the_origin_is_not_judged_on_its_angle() {
        let at_origin = StageRoots {
            pole_hz: 0.0,
            pole_r: 0.0,
            ..valid()
        };
        assert_eq!(validate_stage_roots(&at_origin), RootValidity::Ok);
    }
    #[test]
    fn negative_radii_are_rejected_rather_than_folded() {
        assert_eq!(
            validate_stage_roots(&StageRoots {
                pole_r: -0.1,
                ..valid()
            }),
            RootValidity::PoleRadius
        );
        assert_eq!(
            validate_stage_roots(&StageRoots {
                zero_r: -0.1,
                ..valid()
            }),
            RootValidity::ZeroRadius
        );
    }
}
#[cfg(test)]
mod recompile {
    use super::*;
    #[test]
    fn the_same_rate_is_bit_exact_passthrough() {
        let roots = StageRoots {
            pole_hz: 1_200.0,
            pole_r: 0.96,
            zero_hz: 900.0,
            zero_r: 0.82,
            scale: 0.8,
        };
        let w = words_from_roots_at(&roots, 44_100.0);
        assert_eq!(recompile_stage_words(w, 44_100.0, 44_100.0), w);
    }
    #[test]
    fn geometry_survives_the_rate_change() {
        let roots = StageRoots {
            pole_hz: 2_137.0,
            pole_r: 0.985,
            zero_hz: 3_411.0,
            zero_r: 1.0,
            scale: 0.72,
        };
        for to in [48_000.0, 96_000.0, 192_000.0] {
            let datum = words_from_roots_at(&roots, 44_100.0);
            let compiled = recompile_stage_words(datum, 44_100.0, to);
            assert_ne!(compiled, datum, "{to} Hz must be a different packed row");
            let back = roots_from_words_at(compiled, to).expect("conjugate row");
            let cents = |a: f64, b: f64| ((a / b).ln() / 2.0f64.ln() * 1200.0).abs();
            assert!(
                cents(back.pole_hz, roots.pole_hz) < 10.0,
                "pole moved to {} Hz at {to} Hz",
                back.pole_hz
            );
            assert!(
                cents(back.zero_hz, roots.zero_hz) < 10.0,
                "zero moved to {} Hz at {to} Hz",
                back.zero_hz
            );
            assert!((back.pole_r - roots.pole_r).abs() < 1e-3);
            assert!(
                (back.zero_r - 1.0).abs() < 1e-9,
                "traveling null left the circle"
            );
        }
    }
    #[test]
    fn real_root_rows_pass_through_verbatim() {
        let w_real = [
            encode(0.7),
            encode(0.75),
            encode(0.2),
            encode(0.75),
            encode(0.25),
        ];
        assert!(roots_from_words_at(w_real, 44_100.0).is_none());
        assert_eq!(recompile_stage_words(w_real, 44_100.0, 96_000.0), w_real);
    }
    #[test]
    fn frequencies_above_the_target_ceiling_are_clamped_not_wrapped() {
        let roots = StageRoots {
            pole_hz: 20_000.0,
            pole_r: 0.9,
            zero_hz: 0.0,
            zero_r: 0.0,
            scale: 1.0,
        };
        let datum = words_from_roots_at(&roots, 44_100.0);
        let down = recompile_stage_words(datum, 44_100.0, 32_000.0);
        let back = roots_from_words_at(down, 32_000.0).expect("conjugate row");
        assert!(
            (back.pole_hz - 32_000.0 * 0.49).abs() < 50.0,
            "expected the authoring ceiling, got {} Hz",
            back.pole_hz
        );
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    use crate::compiler::{biquad_to_words, stage_biquad};
    use crate::minifloat::stage_words_to_biquad;
    fn root_sweep() -> Vec<StageRoots> {
        let mut out = Vec::new();
        let freqs = [
            40.0, 90.0, 200.0, 440.0, 1000.0, 1200.0, 2500.0, 4500.0, 8000.0, 14000.0, 18000.0,
        ];
        let radii = [0.5, 0.7, 0.85, 0.95, 0.99, 0.999, 0.9999];
        let scales = [0.05, 0.25, 1.0, 2.0, 3.9];
        for (i, &pf) in freqs.iter().enumerate() {
            for &pr in &radii {
                let zf = freqs[(i + 3) % freqs.len()];
                let zr = radii[(i + 2) % radii.len()];
                out.push(StageRoots {
                    pole_hz: pf,
                    pole_r: pr,
                    zero_hz: zf,
                    zero_r: zr,
                    scale: scales[i % scales.len()],
                });
            }
        }
        out
    }
    #[test]
    fn words_are_a_fixed_point() {
        for r in root_sweep() {
            let w1 = words_from_roots(&r);
            match roots_from_words(w1) {
                Some(r2) => {
                    let w2 = words_from_roots(&r2);
                    assert_eq!(
                        w1, w2,
                        "words drifted for {r:?}: {w1:04x?} -> {r2:?} -> {w2:04x?}"
                    );
                }
                None => {
                    let near_dc = (r.pole_r < 0.85 && r.pole_hz < 250.0)
                        || (r.zero_r < 0.85 && r.zero_hz < 250.0);
                    assert!(near_dc, "unexpected refusal for authored {r:?}");
                }
            }
        }
    }
    #[test]
    fn quantization_bounds() {
        let mut max_dr = 0.0f64;
        let mut max_scale_db = 0.0f64;
        let mut max_resonant = 0.0f64;
        let mut max_mid = 0.0f64;
        let mut max_high_freq = 0.0f64;
        let mut collapses: Vec<(f64, f64)> = Vec::new();
        for r in root_sweep() {
            let Some(r2) = roots_from_words(words_from_roots(&r)) else {
                if r.pole_r < 0.85 && r.pole_hz < 250.0 {
                    collapses.push((r.pole_hz, r.pole_r));
                }
                if r.zero_r < 0.85 && r.zero_hz < 250.0 {
                    collapses.push((r.zero_hz, r.zero_r));
                }
                continue;
            };
            max_dr = max_dr
                .max((r2.pole_r - r.pole_r).abs())
                .max((r2.zero_r - r.zero_r).abs());
            max_scale_db = max_scale_db.max((20.0 * (r2.scale / r.scale).log10()).abs());
            let cents = |a: f64, b: f64| ((a / b).ln() / 2.0f64.ln() * 1200.0).abs();
            for (hz, rr, hz2) in [
                (r.pole_hz, r.pole_r, r2.pole_hz),
                (r.zero_hz, r.zero_r, r2.zero_hz),
            ] {
                if hz2 <= 0.0 {
                    collapses.push((hz, rr));
                    continue;
                }
                let e = cents(hz2, hz);
                if rr >= 0.95 {
                    max_resonant = max_resonant.max(e);
                }
                if rr >= 0.85 && hz >= 200.0 {
                    max_mid = max_mid.max(e);
                }
                if hz >= 440.0 {
                    max_high_freq = max_high_freq.max(e);
                }
            }
        }
        println!(
            "stage_law quantization: resonant(r≥.95) {max_resonant:.2}c · mid(r≥.85,f≥200) {max_mid:.2}c · high-freq(f≥440) {max_high_freq:.2}c · radius {max_dr:.2e} · scale {max_scale_db:.5} dB"
        );
        println!("angle-collapsed-to-DC roots (low r · low hz): {collapses:?}");
        assert!(
            max_resonant < 10.0,
            "resonant roots above 10 cents: {max_resonant}"
        );
        assert!(max_mid < 7.0, "mid-radius roots above 7 cents: {max_mid}");
        assert!(
            max_high_freq < 5.0,
            "≥440 Hz roots above 5 cents: {max_high_freq}"
        );
        assert!(max_dr < 5e-4, "radius quantization above 5e-4");
        assert!(max_scale_db < 0.01, "scale quantization above 0.01 dB");
        for (hz, r) in &collapses {
            assert!(
                *r < 0.85 && *hz < 250.0,
                "unexpected DC collapse at {hz} Hz r {r}"
            );
        }
    }
    #[test]
    fn consistent_with_biquad_to_words() {
        for r in root_sweep() {
            assert_eq!(
                words_from_roots(&r),
                biquad_to_words(r.biquad()),
                "law vs biquad_to_words diverged for {r:?}"
            );
        }
    }
    #[test]
    fn identity_is_exact() {
        let w = words_from_roots(&StageRoots::IDENTITY);
        let bq = stage_words_to_biquad(w);
        assert_eq!(bq, [1.0, 0.0, 0.0, 0.0, 0.0], "identity words {w:04x?}");
        let g = geometry_from_words(w);
        assert_eq!(g.pole, RootPair::Degenerate);
        assert_eq!(g.zero, RootPair::Degenerate);
        assert_eq!(g.scale, 1.0);
    }
    #[test]
    fn legacy_gain_disagreement_pinned() {
        let legacy = stage_biquad(&[1.0, 1200.0, 0.95, 1.0, 1.0, 4500.0, 0.90]);
        let law = StageRoots {
            pole_hz: 1200.0,
            pole_r: 0.95,
            zero_hz: 4500.0,
            zero_r: 0.90,
            scale: 1.0,
        };
        let law_b0 = roots_from_words(words_from_roots(&law)).unwrap().scale;
        let disagreement_db = 20.0 * (law_b0 / legacy[0]).log10();
        println!(
            "legacy b0 {:.6} · law b0 {law_b0:.6} · disagreement {disagreement_db:.2} dB",
            legacy[0]
        );
        assert!((legacy[0] - 0.082192).abs() < 1e-4, "legacy b0 drifted");
        assert!(
            (disagreement_db - 21.70).abs() < 0.05,
            "the pinned 21.70 dB disagreement changed: {disagreement_db:.3} dB"
        );
    }
    #[test]
    fn real_root_rows_refused_not_clamped() {
        let w_real = [
            encode(0.7),
            encode(0.75),
            encode(0.2),
            encode(0.75),
            encode(0.25),
        ];
        assert_eq!(roots_from_words(w_real), None, "real pair must be refused");
        match geometry_from_words(w_real).zero {
            RootPair::RealPair { root_a, root_b } => {
                assert!(
                    (root_a * root_b - 0.25).abs() < 1e-3,
                    "product {}",
                    root_a * root_b
                );
                assert!(
                    (root_a + root_b + 1.55).abs() < 1e-3,
                    "sum {}",
                    root_a + root_b
                );
            }
            other => panic!("expected RealPair, got {other:?}"),
        }
        let w_conj = [
            encode(0.3),
            encode(0.75),
            encode(0.2),
            encode(0.75),
            encode(0.25),
        ];
        assert!(roots_from_words(w_conj).is_some());
    }
    fn declared_stage_rows() -> Vec<([u16; 5], String)> {
        const BODY: &[u8; 240] = include_bytes!("../tests/fixtures/sf_mouth_frame.body240");
        let mut rows = Vec::with_capacity(65_560);
        for (row_index, chunk) in BODY.chunks_exact(10).enumerate() {
            let mut words = [0u16; 5];
            for (word_index, bytes) in chunk.chunks_exact(2).enumerate() {
                words[word_index] = u16::from_le_bytes([bytes[0], bytes[1]]);
            }
            rows.push((words, format!("cleanroom-body#{row_index}")));
        }
        let mut state = 0x6D2B_79F5u32;
        for row_index in 0..65_536usize {
            let mut words = [0u16; 5];
            for word in &mut words {
                state = state.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
                *word = (state >> 16) as u16;
            }
            rows.push((words, format!("packed-sweep#{row_index}")));
        }
        rows
    }
    #[test]
    fn declared_geometry_round_trip() {
        let rows = declared_stage_rows();
        assert_eq!(rows.len(), 65_560, "declared packed sweep changed");
        let mut conjugate = 0usize;
        let mut real = 0usize;
        let mut mismatches = Vec::new();
        for (w, src) in &rows {
            match roots_from_words(*w) {
                Some(r) => {
                    conjugate += 1;
                    let w2 = words_from_roots(&r);
                    if w2 != *w {
                        mismatches.push((src.clone(), *w, w2));
                    }
                }
                None => {
                    real += 1;
                    let g = geometry_from_words(*w);
                    let has_real = matches!(g.pole, RootPair::RealPair { .. })
                        || matches!(g.zero, RootPair::RealPair { .. });
                    assert!(has_real, "{src}: refused but no real pair found");
                }
            }
        }
        println!(
            "declared geometry: {} stage rows · {conjugate} conjugate · {real} real-root (refused) · {} round-trip mismatches",
            rows.len(),
            mismatches.len()
        );
        for (src, w, w2) in mismatches.iter().take(10) {
            println!("  MISMATCH {src}: {w:04x?} -> {w2:04x?}");
        }
        assert!(
            mismatches.is_empty(),
            "{} conjugate rows failed the word-identity round trip",
            mismatches.len()
        );
    }
}

pub const GRID_LOW_BYTE: u16 = 0xFC;

pub fn snap_radius(r: f64) -> f64 {
    if !(0.0..1.0).contains(&r) {
        return r;
    }
    let mut best = r;
    let mut err = f64::MAX;
    for hi in 0..256u16 {
        let w = (hi << 8) | GRID_LOW_BYTE;
        let q = 1.0 - decode(w);
        if q <= 0.0 || q > 1.0 {
            continue;
        }
        let cand = q.sqrt();
        if cand >= 1.0 {
            continue;
        }
        let d = (cand - r).abs();
        if d < err {
            err = d;
            best = cand;
        }
    }
    best
}

pub fn snap_frequency(hz: f64, radius: f64, sample_rate_hz: f64) -> f64 {
    snap_frequency_within(hz, radius, sample_rate_hz, 0.0, f64::INFINITY)
}

pub fn snap_frequency_within(
    hz: f64,
    radius: f64,
    sample_rate_hz: f64,
    lo: f64,
    hi: f64,
) -> f64 {
    if radius <= 0.0 || radius >= 1.0 || hz <= 0.0 {
        return hz;
    }
    let c3 = 1.0 - radius * radius;
    let mut best = hz;
    let mut err = f64::MAX;
    for step in 0..256u16 {
        let w = (step << 8) | GRID_LOW_BYTE;
        let c2 = 4.0 * decode(w) + c3;
        let cw = (2.0 - c2) / (2.0 * radius);
        if !(-1.0..=1.0).contains(&cw) {
            continue;
        }
        let cand = cw.acos() * sample_rate_hz / TAU;
        if cand < lo || cand > hi {
            continue;
        }
        let d = (cand / hz).log2().abs();
        if d < err {
            err = d;
            best = cand;
        }
    }
    best
}
