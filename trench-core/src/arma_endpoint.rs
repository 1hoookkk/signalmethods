//! Source-agnostic ARMA endpoint authoring.
//!
//! Where `praat_endpoint` needs Praat's F1–F5 lane identity and can therefore
//! only measure throats, this module fits any measured magnitude spectrum:
//! six pole/zero sections are placed and refined jointly against the target,
//! with no lane or vowel assumptions.  Poles grab the resonances, zeros grab
//! the anti-resonances.  Roots are validated and packed by `stage_law`, so a
//! root the words cannot hold is never proposed.

use crate::cascade::{NUM_COEFFS, NUM_STAGES};
use crate::minifloat::stage_words_to_biquad;
use crate::praat_endpoint::{biquad_db, optimal_gain_and_rms, rms_with_embedded_gain, stage_db};
use crate::stage_law::{
    authoring_limits_at, validate_stage_roots_at, words_from_roots_at, RootValidity, StageRoots,
};

/// Fallback dominant-root radius when the residual is too featureless to
/// measure a width; the paired root always starts loose.
const NO_PINS: [bool; NUM_STAGES] = [false; NUM_STAGES];
const SEED_DOMINANT_R: f64 = 0.90;
const SEED_PAIRED_R: f64 = 0.50;
/// A refined section that moves the fit by less than this is not material and
/// stays identity, keeping the body's unused sections clean.
const SECTION_MIN_RMS_IMPROVEMENT_DB: f64 = 0.05;
/// Reseat sweeps: how many times every section may be pulled out and reseeded
/// at the strongest remaining residual to escape a greedy local minimum.
const RESEAT_SWEEPS: usize = 6;

#[derive(Clone, Debug)]
pub struct ArmaFit {
    pub roots: [StageRoots; NUM_STAGES],
    pub words: [[u16; NUM_COEFFS]; NUM_STAGES],
    pub target_rms_db: f64,
    pub intended_packed_rms_db: f64,
    pub sections_used: usize,
}

/// Coordinate descent over all four root placements of the editable sections.
/// Frequencies walk in octaves, radii walk multiplicatively in (1 - r) so the
/// resolution matches how sharply a root near the circle shapes the response.
/// Every trial is judged by `validate_stage_roots_at`, so the search space is
/// exactly the encoder's.
fn optimise_roots(
    roots: &mut [StageRoots; NUM_STAGES],
    editable_sections: &[usize],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    pinned_pole_hz: &[bool; NUM_STAGES],
    pinned_zero_hz: &[bool; NUM_STAGES],
) -> f64 {
    let limits = authoring_limits_at(sample_rate_hz);
    let freq_ceiling = limits.display_freq_max_hz.min(limits.authoring_freq_max_hz);
    let mut best_rms = optimal_gain_and_rms(roots, target, sample_rate_hz).1;
    for level in 0..9 {
        let frequency_step_octaves = 0.5 / 2.0f64.powi(level);
        let radius_factor = 1.0 + 0.5 / 2.0f64.powi(level);
        let mut changed = true;
        let mut passes = 0;
        while changed && passes < 12 {
            changed = false;
            passes += 1;
            for &section in editable_sections {
                for parameter in 0..4 {
                    if parameter == 0 && pinned_pole_hz[section] {
                        continue;
                    }
                    if parameter == 2 && pinned_zero_hz[section] {
                        continue;
                    }
                    for direction in [-1.0f64, 1.0] {
                        let before = roots[section];
                        let mut trial = before;
                        match parameter {
                            0 => {
                                trial.pole_hz = (before.pole_hz
                                    * 2.0f64.powf(direction * frequency_step_octaves))
                                .clamp(limits.display_freq_min_hz, freq_ceiling);
                            }
                            1 => {
                                let distance = (1.0 - before.pole_r).max(1e-6);
                                trial.pole_r = (1.0 - distance * radius_factor.powf(-direction))
                                    .clamp(0.0, 1.0);
                            }
                            2 => {
                                trial.zero_hz = (before.zero_hz
                                    * 2.0f64.powf(direction * frequency_step_octaves))
                                .clamp(limits.display_freq_min_hz, freq_ceiling);
                            }
                            _ => {
                                let distance = (1.0 - before.zero_r).max(1e-6);
                                trial.zero_r = (1.0 - distance * radius_factor.powf(-direction))
                                    .clamp(0.0, 1.0);
                            }
                        }
                        if validate_stage_roots_at(&trial, sample_rate_hz) != RootValidity::Ok {
                            continue;
                        }
                        roots[section] = trial;
                        let rms = optimal_gain_and_rms(roots, target, sample_rate_hz).1;
                        if rms + 1e-12 < best_rms {
                            best_rms = rms;
                            changed = true;
                        } else {
                            roots[section] = before;
                        }
                    }
                }
            }
        }
    }
    best_rms
}

/// The gain-corrected residual (target minus fit) at every target point.
fn residual_curve(
    roots: &[StageRoots; NUM_STAGES],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
) -> Vec<(f64, f64)> {
    let (gain_db, _) = optimal_gain_and_rms(roots, target, sample_rate_hz);
    target
        .iter()
        .map(|&(frequency, target_db)| {
            let response = roots
                .iter()
                .map(|stage| stage_db(stage, frequency, sample_rate_hz))
                .sum::<f64>()
                + gain_db;
            (frequency, target_db - response)
        })
        .collect()
}

/// Seed one section at the strongest residual.  The dominant root's radius is
/// estimated from the residual's half-height width around that point, so a
/// sharp resonance starts sharp and descent only has to trim it.
fn seed_from_residual(
    roots: &[StageRoots; NUM_STAGES],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
) -> StageRoots {
    let residual = residual_curve(roots, target, sample_rate_hz);
    let peak = residual
        .iter()
        .enumerate()
        .max_by(|a, b| a.1 .1.abs().total_cmp(&b.1 .1.abs()))
        .map(|(index, _)| index)
        .unwrap_or(0);
    seed_at_index(&residual, peak, sample_rate_hz)
}

/// Seed one section at a chosen residual point (radius from half-height width).
fn seed_at_index(residual: &[(f64, f64)], peak: usize, sample_rate_hz: f64) -> StageRoots {
    let (peak_hz, peak_db) = residual[peak];
    let half = peak_db.abs() * 0.5;
    let sign = peak_db.signum();
    let mut lo_hz = residual[0].0;
    for index in (0..peak).rev() {
        if residual[index].1 * sign < half {
            lo_hz = residual[index].0;
            break;
        }
    }
    let mut hi_hz = residual[residual.len() - 1].0;
    for entry in residual.iter().skip(peak + 1) {
        if entry.1 * sign < half {
            hi_hz = entry.0;
            break;
        }
    }
    let bandwidth_hz = (hi_hz - lo_hz).max(1.0);
    let dominant_r = (-std::f64::consts::PI * bandwidth_hz / sample_rate_hz)
        .exp()
        .clamp(SEED_PAIRED_R, 0.995);
    let (pole_r, zero_r) = if peak_db >= 0.0 {
        (dominant_r, SEED_PAIRED_R)
    } else {
        (SEED_PAIRED_R, dominant_r)
    };
    StageRoots {
        pole_hz: peak_hz,
        pole_r,
        zero_hz: peak_hz,
        zero_r,
        scale: 1.0,
    }
}

/// Fit six sections to a measured magnitude target: sections are seeded one at
/// a time at the strongest residual (pole-dominant above the fit, zero-dominant
/// below it), each addition followed by joint refinement of everything placed
/// so far, then reseat sweeps let any section escape a greedy local minimum.
/// Sections that do not materially improve the fit stay identity.
pub fn fit_arma(target: &[(f64, f64)], sample_rate_hz: f64) -> Option<ArmaFit> {
    fit_arma_pinned(target, sample_rate_hz, &[])
}

/// `fit_arma` with pole frequencies locked to measured resonances.
///
/// Each entry of `pinned_hz` claims one section: its pole frequency is seeded
/// there and never moves — descent may only shape that pole's radius, its
/// zero, and scale. Remaining sections are fitted freely as in `fit_arma`.
/// Serial-cascade honesty is preserved: every trial is still judged on the
/// whole-cascade response, the caller just supplies the ground truth for
/// where the resonances sit.
pub fn fit_arma_pinned(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    pinned_hz: &[f64],
) -> Option<ArmaFit> {
    fit_arma_pinned_pairs(target, sample_rate_hz, pinned_hz, &[])
}

/// `fit_arma_pinned` with the zero frequency of a section frozen as well.
///
/// `pinned_zero_hz[s]`, when finite and positive, claims section `s`'s zero:
/// it is seeded there and descent may not move it. A zero pin without a pole
/// pin at the same index is allowed. Entries that are 0.0 or non-finite leave
/// that section's zero free. This is the caller stating the whole geometry —
/// where the resonance sits and where its notch sits — and leaving only the
/// radii and gains to the fitter.
pub fn fit_arma_pinned_pairs(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    pinned_hz: &[f64],
    pinned_zero_hz: &[f64],
) -> Option<ArmaFit> {
    if target.len() < 32
        || target
            .iter()
            .any(|(frequency, db)| !frequency.is_finite() || *frequency <= 0.0 || !db.is_finite())
        || !sample_rate_hz.is_finite()
        || sample_rate_hz <= 0.0
    {
        return None;
    }

    if pinned_hz.len() > NUM_STAGES
        || pinned_hz.iter().any(|hz| !hz.is_finite() || *hz <= 0.0)
        || pinned_zero_hz.len() > NUM_STAGES
        || pinned_zero_hz.iter().any(|hz| !hz.is_finite() || *hz < 0.0)
    {
        return None;
    }

    let mut pinned_zero = [false; NUM_STAGES];
    let mut zero_hz_of = [0.0f64; NUM_STAGES];
    for (section, &hz) in pinned_zero_hz.iter().enumerate() {
        if hz > 0.0 {
            pinned_zero[section] = true;
            zero_hz_of[section] = hz;
        }
    }

    let mut roots = [StageRoots::IDENTITY; NUM_STAGES];
    let mut pinned = [false; NUM_STAGES];
    let mut active: Vec<usize> = Vec::new();
    let mut best_rms = optimal_gain_and_rms(&roots, target, sample_rate_hz).1;

    // Pinned sections first: seed each at its measured frequency (radius from
    // the residual's width there), then joint-refine with the pole Hz frozen.
    let limits = authoring_limits_at(sample_rate_hz);
    let freq_ceiling = limits.display_freq_max_hz.min(limits.authoring_freq_max_hz);
    for (section, &hz) in pinned_hz.iter().enumerate() {
        let hz = hz.clamp(limits.display_freq_min_hz, freq_ceiling);
        let residual = residual_curve(&roots, target, sample_rate_hz);
        let nearest = residual
            .iter()
            .enumerate()
            .min_by(|a, b| (a.1 .0 - hz).abs().total_cmp(&(b.1 .0 - hz).abs()))
            .map(|(index, _)| index)
            .unwrap_or(0);
        let mut seed = seed_at_index(&residual, nearest, sample_rate_hz);
        seed.pole_hz = hz;
        seed.zero_hz = if pinned_zero[section] {
            zero_hz_of[section].clamp(limits.display_freq_min_hz, freq_ceiling)
        } else {
            hz
        };
        // a pinned section exists to hold a resonance: pole-dominant always
        if seed.pole_r < seed.zero_r {
            std::mem::swap(&mut seed.pole_r, &mut seed.zero_r);
        }
        if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
            seed.pole_r = seed.pole_r.min(SEED_DOMINANT_R);
            seed.zero_r = seed.zero_r.min(SEED_DOMINANT_R);
            if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
                return None;
            }
        }
        roots[section] = seed;
        pinned[section] = true;
        active.push(section);
        best_rms = optimise_roots(
            &mut roots,
            &active,
            target,
            sample_rate_hz,
            &pinned,
            &pinned_zero,
        );
    }

    for section in pinned_hz.len()..NUM_STAGES {
        let mut seed = seed_from_residual(&roots, target, sample_rate_hz);
        if pinned_zero[section] {
            seed.zero_hz = zero_hz_of[section].clamp(limits.display_freq_min_hz, freq_ceiling);
        }
        if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
            seed.pole_r = seed.pole_r.min(SEED_DOMINANT_R);
            seed.zero_r = seed.zero_r.min(SEED_DOMINANT_R);
            if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
                break;
            }
        }
        // A seed can dig a hole descent cannot climb out of; the flipped
        // orientation (zero-dominant vs pole-dominant) often can.
        let flipped = StageRoots {
            pole_r: seed.zero_r,
            zero_r: seed.pole_r,
            ..seed
        };
        let before = roots;
        let before_rms = best_rms;
        let mut accepted = false;
        for candidate in [seed, flipped] {
            if validate_stage_roots_at(&candidate, sample_rate_hz) != RootValidity::Ok {
                continue;
            }
            roots = before;
            roots[section] = candidate;
            if !accepted {
                active.push(section);
                accepted = true;
            }
            let rms = optimise_roots(
                &mut roots,
                &active,
                target,
                sample_rate_hz,
                &pinned,
                &pinned_zero,
            );
            if before_rms - rms >= SECTION_MIN_RMS_IMPROVEMENT_DB {
                best_rms = rms;
                break;
            }
            best_rms = before_rms;
        }
        if best_rms == before_rms {
            roots = before;
            if accepted {
                active.pop();
            }
            break;
        }
    }
    if active.is_empty() {
        return None;
    }

    // Reseat: a greedily placed section can be stranded once later sections
    // reshape the residual.  Pull each one out, reseed it at the strongest
    // remaining residual, refit jointly, and keep whichever cascade is better.
    for _ in 0..RESEAT_SWEEPS {
        let mut improved = false;
        for &section in active.clone().iter() {
            if pinned[section] {
                continue;
            }
            let before = roots;
            let before_rms = best_rms;
            roots[section] = StageRoots::IDENTITY;
            let mut seed = seed_from_residual(&roots, target, sample_rate_hz);
            if pinned_zero[section] {
                seed.zero_hz = zero_hz_of[section].clamp(limits.display_freq_min_hz, freq_ceiling);
            }
            if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
                seed.pole_r = seed.pole_r.min(SEED_DOMINANT_R);
                seed.zero_r = seed.zero_r.min(SEED_DOMINANT_R);
            }
            if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
                roots = before;
                continue;
            }
            roots[section] = seed;
            let rms = optimise_roots(
                &mut roots,
                &active,
                target,
                sample_rate_hz,
                &pinned,
                &pinned_zero,
            );
            if rms + 1e-9 < before_rms {
                best_rms = rms;
                improved = true;
            } else {
                roots = before;
            }
        }
        if !improved {
            break;
        }
    }

    // Measured targets keep their tilt, so the optimal gain can exceed what a
    // single SCALE word holds (COMBINE_K).  Spread it evenly across the active
    // sections; SCALE is pure broadband level and cannot change contrast.
    let (gain_db, _) = optimal_gain_and_rms(&roots, target, sample_rate_hz);
    let per_section = 10.0f64.powf(gain_db / 20.0 / active.len() as f64);
    for &section in &active {
        roots[section].scale = per_section;
        if validate_stage_roots_at(&roots[section], sample_rate_hz) != RootValidity::Ok {
            return None;
        }
    }
    let target_rms_db = rms_with_embedded_gain(&roots, target, sample_rate_hz);
    let mut words = [[0u16; NUM_COEFFS]; NUM_STAGES];
    for section in 0..NUM_STAGES {
        if validate_stage_roots_at(&roots[section], sample_rate_hz) != RootValidity::Ok {
            return None;
        }
        words[section] = words_from_roots_at(&roots[section], sample_rate_hz);
    }

    let intended_packed_rms_db = (target
        .iter()
        .map(|&(frequency, _)| {
            let intended = roots
                .iter()
                .map(|stage| stage_db(stage, frequency, sample_rate_hz))
                .sum::<f64>();
            let packed = words
                .iter()
                .map(|stage| biquad_db(&stage_words_to_biquad(*stage), frequency, sample_rate_hz))
                .sum::<f64>();
            (intended - packed).powi(2)
        })
        .sum::<f64>()
        / target.len() as f64)
        .sqrt();

    Some(ArmaFit {
        roots,
        words,
        target_rms_db,
        intended_packed_rms_db,
        sections_used: active.len(),
    })
}

/// Warm-start refinement: descend from `seed` instead of seeding from
/// residuals.  Every active seed section keeps its lane slot — no seeding, no
/// reseat sweeps, no lane reassignment — so correspondence with the other
/// corners survives by construction.  `pinned_pole[s]` freezes that lane's
/// pole frequency.  This is the SCULPT fast path: the seed is the corner's
/// current geometry and the target is the bent curve, so descent starts one
/// brush-stroke from the answer.
pub fn fit_arma_refine(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    seed: &[StageRoots; NUM_STAGES],
    pinned_pole: &[bool; NUM_STAGES],
) -> Option<ArmaFit> {
    if target.len() < 32
        || target
            .iter()
            .any(|(frequency, db)| !frequency.is_finite() || *frequency <= 0.0 || !db.is_finite())
        || !sample_rate_hz.is_finite()
        || sample_rate_hz <= 0.0
    {
        return None;
    }
    let mut roots = *seed;
    let mut active: Vec<usize> = Vec::new();
    for section in 0..NUM_STAGES {
        // strip embedded gain: it is re-spread after the descent
        roots[section].scale = 1.0;
        let is_identity = roots[section].pole_r <= 0.0 && roots[section].zero_r <= 0.0;
        if is_identity {
            roots[section] = StageRoots::IDENTITY;
            continue;
        }
        if validate_stage_roots_at(&roots[section], sample_rate_hz) != RootValidity::Ok {
            return None;
        }
        active.push(section);
    }
    if active.is_empty() {
        return None;
    }
    optimise_roots(
        &mut roots,
        &active,
        target,
        sample_rate_hz,
        pinned_pole,
        &NO_PINS,
    );

    let (gain_db, _) = optimal_gain_and_rms(&roots, target, sample_rate_hz);
    let per_section = 10.0f64.powf(gain_db / 20.0 / active.len() as f64);
    for &section in &active {
        roots[section].scale = per_section;
        if validate_stage_roots_at(&roots[section], sample_rate_hz) != RootValidity::Ok {
            return None;
        }
    }
    let target_rms_db = rms_with_embedded_gain(&roots, target, sample_rate_hz);
    let mut words = [[0u16; NUM_COEFFS]; NUM_STAGES];
    for section in 0..NUM_STAGES {
        if validate_stage_roots_at(&roots[section], sample_rate_hz) != RootValidity::Ok {
            return None;
        }
        words[section] = words_from_roots_at(&roots[section], sample_rate_hz);
    }
    let intended_packed_rms_db = (target
        .iter()
        .map(|&(frequency, _)| {
            let intended = roots
                .iter()
                .map(|stage| stage_db(stage, frequency, sample_rate_hz))
                .sum::<f64>();
            let packed = words
                .iter()
                .map(|stage| biquad_db(&stage_words_to_biquad(*stage), frequency, sample_rate_hz))
                .sum::<f64>();
            (intended - packed).powi(2)
        })
        .sum::<f64>()
        / target.len() as f64)
        .sqrt();
    Some(ArmaFit {
        roots,
        words,
        target_rms_db,
        intended_packed_rms_db,
        sections_used: active.len(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::praat_endpoint::{TARGET_HI_HZ, TARGET_LO_HZ, TARGET_POINTS};
    use crate::stage_law::roots_from_words_at;
    use std::f64::consts::PI;

    fn synthetic_target(roots: &[StageRoots; NUM_STAGES], sample_rate_hz: f64) -> Vec<(f64, f64)> {
        (0..TARGET_POINTS)
            .map(|index| {
                let t = index as f64 / (TARGET_POINTS - 1) as f64;
                let frequency = TARGET_LO_HZ * (TARGET_HI_HZ / TARGET_LO_HZ).powf(t);
                let db = roots
                    .iter()
                    .map(|stage| stage_db(stage, frequency, sample_rate_hz))
                    .sum();
                (frequency, db)
            })
            .collect()
    }

    #[test]
    fn flat_target_stays_near_identity() {
        let flat: Vec<_> = (0..TARGET_POINTS)
            .map(|index| {
                let t = index as f64 / (TARGET_POINTS - 1) as f64;
                (TARGET_LO_HZ * (TARGET_HI_HZ / TARGET_LO_HZ).powf(t), -3.0)
            })
            .collect();
        let fit = fit_arma(&flat, 48_000.0);
        // A constant offset is pure gain: either no material section survives
        // (None) or whatever survives fits to well under the seed threshold.
        if let Some(fit) = fit {
            assert!(fit.target_rms_db < 0.1, "RMS was {}", fit.target_rms_db);
        }
    }

    #[test]
    fn known_three_section_cascade_is_recovered_without_lanes() {
        let sample_rate = 48_000.0;
        let mut known = [StageRoots::IDENTITY; NUM_STAGES];
        known[0] = StageRoots {
            pole_hz: 500.0,
            pole_r: (-PI * 90.0 / sample_rate).exp(),
            zero_hz: 450.0,
            zero_r: 0.82,
            scale: 1.0,
        };
        known[1] = StageRoots {
            pole_hz: 1_500.0,
            pole_r: (-PI * 140.0 / sample_rate).exp(),
            zero_hz: 1_650.0,
            zero_r: 0.76,
            scale: 1.0,
        };
        known[2] = StageRoots {
            pole_hz: 2_600.0,
            pole_r: (-PI * 190.0 / sample_rate).exp(),
            zero_hz: 2_450.0,
            zero_r: 0.71,
            scale: 1.0,
        };
        let target = synthetic_target(&known, sample_rate);
        let fit = fit_arma(&target, sample_rate).unwrap();
        assert!(fit.target_rms_db < 0.5, "RMS was {}", fit.target_rms_db);
        assert!(fit.intended_packed_rms_db < 0.25);
        // Every known resonance must be claimed by some fitted pole.
        for known_hz in [500.0, 1_500.0, 2_600.0] {
            let claimed = fit
                .roots
                .iter()
                .any(|stage| stage.pole_r > 0.9 && (stage.pole_hz / known_hz).log2().abs() < 0.15);
            assert!(
                claimed,
                "no fitted pole near {known_hz} Hz: {:?}",
                fit.roots
            );
        }
        let scales: Vec<f64> = fit
            .roots
            .iter()
            .filter(|stage| stage.pole_r > 0.0 || stage.zero_r > 0.0)
            .map(|stage| stage.scale)
            .collect();
        assert!(
            scales
                .windows(2)
                .all(|pair| (pair[0] - pair[1]).abs() < 1e-12),
            "global gain is spread evenly, not shaped per-section: {scales:?}"
        );
    }

    #[test]
    fn pinned_poles_stay_exactly_where_they_are_pinned() {
        let sample_rate = 48_000.0;
        let mut known = [StageRoots::IDENTITY; NUM_STAGES];
        known[0] = StageRoots {
            pole_hz: 700.0,
            pole_r: 0.97,
            zero_hz: 500.0,
            zero_r: 0.7,
            scale: 1.0,
        };
        known[1] = StageRoots {
            pole_hz: 2_100.0,
            pole_r: 0.96,
            zero_hz: 2_600.0,
            zero_r: 0.7,
            scale: 1.0,
        };
        let target = synthetic_target(&known, sample_rate);
        // pin deliberately close-but-not-equal to the truth: the fit must hold
        // the pinned Hz (ground truth wins), not drift to the error minimum
        let pins = [710.0, 2_080.0];
        let fit = fit_arma_pinned(&target, sample_rate, &pins).unwrap();
        for (section, pin) in pins.iter().enumerate() {
            assert!(
                (fit.roots[section].pole_hz - pin).abs() < 1e-9,
                "section {section} pole moved off its pin: {} vs {pin}",
                fit.roots[section].pole_hz
            );
            assert!(fit.roots[section].pole_r > 0.5, "pinned pole went limp");
        }
        assert!(fit.target_rms_db < 1.5, "RMS was {}", fit.target_rms_db);
    }

    #[test]
    fn fitted_words_reload_to_the_fitted_roots() {
        let sample_rate = 96_000.0;
        let mut known = [StageRoots::IDENTITY; NUM_STAGES];
        known[0] = StageRoots {
            pole_hz: 800.0,
            pole_r: 0.97,
            zero_hz: 600.0,
            zero_r: 0.6,
            scale: 1.0,
        };
        let target = synthetic_target(&known, sample_rate);
        let fit = fit_arma(&target, sample_rate).unwrap();
        for section in 0..NUM_STAGES {
            let reloaded = roots_from_words_at(fit.words[section], sample_rate)
                .expect("fitted rows decode to conjugate or origin roots");
            if fit.roots[section].pole_r > 0.0 {
                assert!((reloaded.pole_hz - fit.roots[section].pole_hz).abs() < 1.0);
            }
        }
    }
}
