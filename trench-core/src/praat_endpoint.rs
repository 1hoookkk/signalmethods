use crate::cascade::{NUM_COEFFS, NUM_STAGES};
use crate::minifloat::stage_words_to_biquad;
use crate::stage_law::{
    authoring_limits_at, validate_stage_roots_at, words_from_roots_at, RootValidity, StageRoots,
};
use std::f64::consts::PI;

pub const TARGET_POINTS: usize = 1024;
pub const TARGET_LO_HZ: f64 = 40.0;
pub const TARGET_HI_HZ: f64 = 16_000.0;
pub const BACKGROUND_FWHM_OCTAVES: f64 = 1.0;
pub const RESIDUAL_MIN_RMS_IMPROVEMENT_DB: f64 = 0.25;
pub const RESIDUAL_MIN_RELATIVE_IMPROVEMENT: f64 = 0.03;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FormantLane {
    pub frequency_hz: f64,
    pub bandwidth_hz: f64,
}

#[derive(Clone, Debug)]
pub struct EndpointFit {
    pub roots: [StageRoots; NUM_STAGES],
    pub words: [[u16; NUM_COEFFS]; NUM_STAGES],
    pub target_rms_db: f64,
    pub intended_packed_rms_db: f64,
    pub residual_used: bool,
    pub residual_improvement_db: f64,
}

pub fn pole_from_frequency_bandwidth(
    frequency_hz: f64,
    bandwidth_hz: f64,
    sample_rate_hz: f64,
) -> Option<(f64, f64)> {
    if !frequency_hz.is_finite()
        || !bandwidth_hz.is_finite()
        || !sample_rate_hz.is_finite()
        || frequency_hz <= 0.0
        || bandwidth_hz <= 0.0
        || sample_rate_hz <= 0.0
    {
        return None;
    }
    Some((
        2.0 * PI * frequency_hz / sample_rate_hz,
        (-PI * bandwidth_hz / sample_rate_hz).exp(),
    ))
}

fn reflected_index(mut index: isize, len: usize) -> usize {
    debug_assert!(len > 1);
    let last = len as isize - 1;
    while index < 0 || index > last {
        index = if index < 0 { -index } else { 2 * last - index };
    }
    index as usize
}

fn interpolate_curve(curve: &[(f64, f64)], frequency_hz: f64) -> Option<f64> {
    if curve.len() < 2 {
        return None;
    }
    let upper = curve.partition_point(|(frequency, _)| *frequency < frequency_hz);
    if upper == 0 {
        return Some(curve[0].1);
    }
    if upper >= curve.len() {
        return Some(curve[curve.len() - 1].1);
    }
    let (f0, y0) = curve[upper - 1];
    let (f1, y1) = curve[upper];
    let denominator = f1.log2() - f0.log2();
    if denominator.abs() < 1e-15 {
        return Some(y0);
    }
    let t = ((frequency_hz.log2() - f0.log2()) / denominator).clamp(0.0, 1.0);
    Some(y0 + t * (y1 - y0))
}

pub fn prepare_pitch_corrected_ltas(source: &[(f64, f64)]) -> Option<[(f64, f64); TARGET_POINTS]> {
    if source.len() < 2
        || source
            .iter()
            .any(|(frequency, db)| !frequency.is_finite() || *frequency <= 0.0 || !db.is_finite())
        || source.windows(2).any(|pair| pair[1].0 <= pair[0].0)
    {
        return None;
    }
    let mut frequency = [0.0; TARGET_POINTS];
    let mut raw = [0.0; TARGET_POINTS];
    for index in 0..TARGET_POINTS {
        let t = index as f64 / (TARGET_POINTS - 1) as f64;
        frequency[index] = TARGET_LO_HZ * (TARGET_HI_HZ / TARGET_LO_HZ).powf(t);
        raw[index] = interpolate_curve(source, frequency[index])?;
    }

    let step_octaves = (TARGET_HI_HZ / TARGET_LO_HZ).log2() / (TARGET_POINTS - 1) as f64;
    let sigma_octaves = BACKGROUND_FWHM_OCTAVES / (2.0 * (2.0 * 2.0f64.ln()).sqrt());
    let sigma_samples = sigma_octaves / step_octaves;
    let radius = (4.0 * sigma_samples).ceil() as isize;
    let mut background = [0.0; TARGET_POINTS];
    for index in 0..TARGET_POINTS {
        let mut weighted = 0.0;
        let mut weight_sum = 0.0;
        for offset in -radius..=radius {
            let x = offset as f64 / sigma_samples;
            let weight = (-0.5 * x * x).exp();
            weighted += weight * raw[reflected_index(index as isize + offset, TARGET_POINTS)];
            weight_sum += weight;
        }
        background[index] = weighted / weight_sum;
    }
    Some(std::array::from_fn(|index| {
        (frequency[index], raw[index] - background[index])
    }))
}

pub(crate) fn stage_db(roots: &StageRoots, frequency_hz: f64, sample_rate_hz: f64) -> f64 {
    let coefficients = roots.biquad_at(sample_rate_hz);
    biquad_db(&coefficients, frequency_hz, sample_rate_hz)
}

pub(crate) fn biquad_db(coefficients: &[f64; 5], frequency_hz: f64, sample_rate_hz: f64) -> f64 {
    let w = 2.0 * PI * frequency_hz / sample_rate_hz;
    let (cw, sw, c2, s2) = (w.cos(), w.sin(), (2.0 * w).cos(), (2.0 * w).sin());
    let numerator_re = coefficients[0] + coefficients[1] * cw + coefficients[2] * c2;
    let numerator_im = -(coefficients[1] * sw + coefficients[2] * s2);
    let denominator_re = 1.0 + coefficients[3] * cw + coefficients[4] * c2;
    let denominator_im = -(coefficients[3] * sw + coefficients[4] * s2);
    10.0 * ((numerator_re * numerator_re + numerator_im * numerator_im)
        / (denominator_re * denominator_re + denominator_im * denominator_im).max(1e-30))
    .max(1e-30)
    .log10()
}

pub(crate) fn optimal_gain_and_rms(
    roots: &[StageRoots; NUM_STAGES],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
) -> (f64, f64) {
    let mut difference_sum = 0.0;
    let mut shape = Vec::with_capacity(target.len());
    for &(frequency, target_db) in target {
        let response = roots
            .iter()
            .map(|stage| stage_db(stage, frequency, sample_rate_hz))
            .sum::<f64>();
        shape.push(response);
        difference_sum += target_db - response;
    }
    let gain_db = difference_sum / target.len() as f64;
    let rms = target
        .iter()
        .zip(shape)
        .map(|((_, target_db), response)| (response + gain_db - target_db).powi(2))
        .sum::<f64>()
        / target.len() as f64;
    let rms = rms.sqrt();
    (gain_db, rms)
}

pub(crate) fn rms_with_embedded_gain(
    roots: &[StageRoots; NUM_STAGES],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
) -> f64 {
    (target
        .iter()
        .map(|&(frequency, target_db)| {
            let response = roots
                .iter()
                .map(|stage| stage_db(stage, frequency, sample_rate_hz))
                .sum::<f64>();
            (response - target_db).powi(2)
        })
        .sum::<f64>()
        / target.len() as f64)
        .sqrt()
}

fn optimise_zeros(
    roots: &mut [StageRoots; NUM_STAGES],
    editable_sections: &[usize],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
) -> f64 {
    let limits = authoring_limits_at(sample_rate_hz);
    let mut best_rms = optimal_gain_and_rms(roots, target, sample_rate_hz).1;
    for (frequency_step_octaves, radius_step) in [
        (0.5, 0.15),
        (0.25, 0.075),
        (0.125, 0.0375),
        (0.0625, 0.01875),
        (0.03125, 0.009375),
        (0.015625, 0.0046875),
        (0.0078125, 0.00234375),
    ] {
        let mut changed = true;
        let mut passes = 0;
        while changed && passes < 12 {
            changed = false;
            passes += 1;
            for &section in editable_sections {
                for parameter in 0..2 {
                    for direction in [-1.0, 1.0] {
                        let before = roots[section];
                        let mut trial = before;
                        if parameter == 0 {
                            trial.zero_hz = (before.zero_hz
                                * 2.0f64.powf(direction * frequency_step_octaves))
                            .clamp(
                                limits.display_freq_min_hz,
                                limits.display_freq_max_hz.min(limits.authoring_freq_max_hz),
                            );
                        } else {
                            trial.zero_r =
                                (before.zero_r + direction * radius_step).clamp(0.0, 1.0);
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

pub(crate) fn strongest_residual_frequency(
    roots: &[StageRoots; NUM_STAGES],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
) -> (f64, f64) {
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
        .max_by(|a, b| a.1.abs().total_cmp(&b.1.abs()))
        .unwrap_or((1_000.0, 0.0))
}

pub(crate) fn embed_global_gain(
    roots: &mut [StageRoots; NUM_STAGES],
    active_sections: &[usize],
    gain_db: f64,
    sample_rate_hz: f64,
) -> Option<()> {
    let &gain_section = active_sections.first()?;
    for &section in active_sections {
        roots[section].scale = 1.0;
    }
    roots[gain_section].scale = 10.0f64.powf(gain_db / 20.0);
    if validate_stage_roots_at(&roots[gain_section], sample_rate_hz) != RootValidity::Ok {
        return None;
    }
    Some(())
}

pub fn fit_endpoint(
    target: &[(f64, f64)],
    lanes: &[Option<FormantLane>; 5],
    sample_rate_hz: f64,
) -> Option<EndpointFit> {
    if target.len() < 32
        || target
            .iter()
            .any(|(frequency, db)| !frequency.is_finite() || *frequency <= 0.0 || !db.is_finite())
        || !sample_rate_hz.is_finite()
        || sample_rate_hz <= 0.0
    {
        return None;
    }

    let mut roots = [StageRoots::IDENTITY; NUM_STAGES];
    let mut active = Vec::new();
    for (section, lane) in lanes.iter().enumerate() {
        let Some(lane) = lane else { continue };
        let (_, pole_radius) =
            pole_from_frequency_bandwidth(lane.frequency_hz, lane.bandwidth_hz, sample_rate_hz)?;
        let candidate = StageRoots {
            pole_hz: lane.frequency_hz,
            pole_r: pole_radius,
            zero_hz: lane.frequency_hz,
            zero_r: pole_radius,
            scale: 1.0,
        };
        if validate_stage_roots_at(&candidate, sample_rate_hz) == RootValidity::Ok {
            roots[section] = candidate;
            active.push(section);
        }
    }
    if active.is_empty() {
        return None;
    }

    let rms_without_residual = optimise_zeros(&mut roots, &active, target, sample_rate_hz);
    let (residual_frequency, residual_error) =
        strongest_residual_frequency(&roots, target, sample_rate_hz);
    let before_residual = roots;
    roots[5] = if residual_error >= 0.0 {
        StageRoots {
            pole_hz: residual_frequency,
            pole_r: 0.90,
            zero_hz: residual_frequency,
            zero_r: 0.50,
            scale: 1.0,
        }
    } else {
        StageRoots {
            pole_hz: residual_frequency,
            pole_r: 0.50,
            zero_hz: residual_frequency,
            zero_r: 0.90,
            scale: 1.0,
        }
    };
    let mut residual_sections = active.clone();
    residual_sections.push(5);
    let rms_with_residual = optimise_zeros(&mut roots, &residual_sections, target, sample_rate_hz);
    let improvement = rms_without_residual - rms_with_residual;
    let residual_used = improvement >= RESIDUAL_MIN_RMS_IMPROVEMENT_DB
        && improvement / rms_without_residual.max(1e-12) >= RESIDUAL_MIN_RELATIVE_IMPROVEMENT;
    if !residual_used {
        roots = before_residual;
    } else {
        active.push(5);
    }

    let (gain_db, _) = optimal_gain_and_rms(&roots, target, sample_rate_hz);
    embed_global_gain(&mut roots, &active, gain_db, sample_rate_hz)?;
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

    Some(EndpointFit {
        roots,
        words,
        target_rms_db,
        intended_packed_rms_db,
        residual_used,
        residual_improvement_db: if residual_used { improvement } else { 0.0 },
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::stage_law::roots_from_words_at;

    fn synthetic_target(roots: &[StageRoots], sample_rate_hz: f64) -> Vec<(f64, f64)> {
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
    fn known_frequency_and_bandwidth_have_exact_pole_geometry() {
        let sample_rate = 48_000.0;
        let (angle, radius) = pole_from_frequency_bandwidth(700.0, 85.0, sample_rate).unwrap();
        assert_eq!(angle, 2.0 * PI * 700.0 / sample_rate);
        assert_eq!(radius, (-PI * 85.0 / sample_rate).exp());
    }

    #[test]
    fn packed_pole_round_trips_at_each_runtime_rate() {
        for rate in [44_100.0, 48_000.0, 88_200.0, 96_000.0, 176_400.0, 192_000.0] {
            let roots = StageRoots {
                pole_hz: 389.071_109,
                pole_r: (-PI * 187.863_537 / rate).exp(),
                zero_hz: 404.102_522,
                zero_r: 0.980_748_393,
                scale: 1.0,
            };
            let words = words_from_roots_at(&roots, rate);
            let decoded = roots_from_words_at(words, rate).expect("host packed row");
            assert!(
                (decoded.pole_hz - roots.pole_hz).abs() < 0.35,
                "{rate} Hz words decoded pole {} Hz instead of {} Hz",
                decoded.pole_hz,
                roots.pole_hz
            );
        }
    }

    #[test]
    fn host_authored_row_reloads_to_the_fitted_host_root() {
        let host_rate = 48_000.0;
        let fitted = StageRoots {
            pole_hz: 389.071_109,
            pole_r: (-PI * 187.863_537 / host_rate).exp(),
            zero_hz: 404.102_522,
            zero_r: 0.980_748_393,
            scale: 0.912_738_338,
        };
        let words = words_from_roots_at(&fitted, host_rate);
        let loaded = roots_from_words_at(words, host_rate).expect("host row");
        assert!((loaded.pole_hz - fitted.pole_hz).abs() < 0.3);
        assert!((loaded.pole_r - fitted.pole_r).abs() < 2.0e-4);
        assert!((loaded.scale - fitted.scale).abs() < 2.0e-3);
    }

    #[test]
    fn missing_lanes_stay_exact_identity() {
        let sample_rate = 44_100.0;
        let lanes = [
            Some(FormantLane {
                frequency_hz: 700.0,
                bandwidth_hz: 90.0,
            }),
            None,
            Some(FormantLane {
                frequency_hz: 2_500.0,
                bandwidth_hz: 180.0,
            }),
            None,
            None,
        ];
        let target = synthetic_target(
            &[
                StageRoots {
                    pole_hz: 700.0,
                    pole_r: (-PI * 90.0 / sample_rate).exp(),
                    zero_hz: 650.0,
                    zero_r: 0.80,
                    scale: 1.0,
                },
                StageRoots::IDENTITY,
                StageRoots {
                    pole_hz: 2_500.0,
                    pole_r: (-PI * 180.0 / sample_rate).exp(),
                    zero_hz: 2_800.0,
                    zero_r: 0.75,
                    scale: 1.0,
                },
                StageRoots::IDENTITY,
                StageRoots::IDENTITY,
                StageRoots::IDENTITY,
            ],
            sample_rate,
        );
        let fit = fit_endpoint(&target, &lanes, sample_rate).unwrap();
        assert_eq!(fit.roots[1], StageRoots::IDENTITY);
        assert_eq!(fit.roots[3], StageRoots::IDENTITY);
        assert_eq!(fit.roots[4], StageRoots::IDENTITY);
    }

    #[test]
    fn lane_identity_survives_endpoint_fitting() {
        let sample_rate = 48_000.0;
        let ay = [
            Some(FormantLane {
                frequency_hz: 700.0,
                bandwidth_hz: 90.0,
            }),
            Some(FormantLane {
                frequency_hz: 1_200.0,
                bandwidth_hz: 110.0,
            }),
            Some(FormantLane {
                frequency_hz: 2_500.0,
                bandwidth_hz: 180.0,
            }),
            Some(FormantLane {
                frequency_hz: 3_300.0,
                bandwidth_hz: 220.0,
            }),
            Some(FormantLane {
                frequency_hz: 4_400.0,
                bandwidth_hz: 300.0,
            }),
        ];
        let ee = [
            Some(FormantLane {
                frequency_hz: 300.0,
                bandwidth_hz: 70.0,
            }),
            Some(FormantLane {
                frequency_hz: 2_300.0,
                bandwidth_hz: 130.0,
            }),
            Some(FormantLane {
                frequency_hz: 3_000.0,
                bandwidth_hz: 190.0,
            }),
            Some(FormantLane {
                frequency_hz: 3_800.0,
                bandwidth_hz: 230.0,
            }),
            Some(FormantLane {
                frequency_hz: 4_700.0,
                bandwidth_hz: 320.0,
            }),
        ];
        let flat: Vec<_> = (0..TARGET_POINTS)
            .map(|index| {
                let t = index as f64 / (TARGET_POINTS - 1) as f64;
                (TARGET_LO_HZ * (TARGET_HI_HZ / TARGET_LO_HZ).powf(t), 0.0)
            })
            .collect();
        let ay_fit = fit_endpoint(&flat, &ay, sample_rate).unwrap();
        let ee_fit = fit_endpoint(&flat, &ee, sample_rate).unwrap();
        for lane in 0..5 {
            assert_eq!(ay_fit.roots[lane].pole_hz, ay[lane].unwrap().frequency_hz);
            assert_eq!(ee_fit.roots[lane].pole_hz, ee[lane].unwrap().frequency_hz);
        }
    }

    #[test]
    fn synthetic_known_pole_zero_target_is_recovered_as_a_curve() {
        let sample_rate = 96_000.0;
        let lanes = [
            Some(FormantLane {
                frequency_hz: 500.0,
                bandwidth_hz: 90.0,
            }),
            Some(FormantLane {
                frequency_hz: 1_500.0,
                bandwidth_hz: 140.0,
            }),
            Some(FormantLane {
                frequency_hz: 2_600.0,
                bandwidth_hz: 190.0,
            }),
            None,
            None,
        ];
        let mut known = [StageRoots::IDENTITY; NUM_STAGES];
        for section in 0..3 {
            let lane = lanes[section].unwrap();
            known[section] = StageRoots {
                pole_hz: lane.frequency_hz,
                pole_r: (-PI * lane.bandwidth_hz / sample_rate).exp(),
                zero_hz: lane.frequency_hz * [0.90, 1.10, 0.94][section],
                zero_r: [0.82, 0.76, 0.71][section],
                scale: 1.0,
            };
        }
        let target = synthetic_target(&known, sample_rate);
        let fit = fit_endpoint(&target, &lanes, sample_rate).unwrap();
        assert!(fit.target_rms_db < 0.75, "RMS was {}", fit.target_rms_db);
        assert!(fit.intended_packed_rms_db < 0.25);
        assert_eq!(
            fit.roots.iter().filter(|stage| stage.scale != 1.0).count(),
            1,
            "the fitted cascade has one global gain, not per-section gains"
        );
    }

    #[test]
    fn one_octave_reflected_gaussian_removes_a_broad_constant() {
        let source: Vec<_> = (0..300)
            .map(|index| {
                let t = index as f64 / 299.0;
                let frequency = 20.0f64 * (20_000.0f64 / 20.0f64).powf(t);
                (frequency, -12.0)
            })
            .collect();
        let target = prepare_pitch_corrected_ltas(&source).unwrap();
        assert!(target.iter().all(|(_, db)| db.abs() < 1e-10));
    }
}
