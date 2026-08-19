use crate::cascade::{NUM_COEFFS, NUM_STAGES};
use crate::minifloat::stage_words_to_biquad;
use crate::praat_endpoint::{biquad_db, optimal_gain_and_rms, rms_with_embedded_gain, stage_db};
use crate::stage_law::{
    authoring_limits_at, validate_stage_roots_at, words_from_roots_at, RootValidity, StageRoots,
};

const NO_PINS: [bool; NUM_STAGES] = [false; NUM_STAGES];

pub type Freedom = [[bool; 4]; NUM_STAGES];

pub const FREE: Freedom = [[true; 4]; NUM_STAGES];

pub type Zones = [[f64; 2]; NUM_STAGES];

pub const NO_ZONES: Zones = [[0.0, f64::INFINITY]; NUM_STAGES];

pub type Watch<'a> = &'a mut dyn FnMut(&[StageRoots; NUM_STAGES], f64);

fn unwatched() -> impl FnMut(&[StageRoots; NUM_STAGES], f64) {
    |_, _| {}
}

fn freedom_from_pins(pole: &[bool; NUM_STAGES], zero: &[bool; NUM_STAGES]) -> Freedom {
    let mut f = FREE;
    for section in 0..NUM_STAGES {
        f[section][0] = !pole[section];
        f[section][2] = !zero[section];
    }
    f
}
const SEED_DOMINANT_R: f64 = 0.90;
const SEED_PAIRED_R: f64 = 0.50;
const SECTION_MIN_RMS_IMPROVEMENT_DB: f64 = 0.05;
const RESEAT_SWEEPS: usize = 6;

#[derive(Clone, Debug)]
pub struct ArmaFit {
    pub roots: [StageRoots; NUM_STAGES],
    pub words: [[u16; NUM_COEFFS]; NUM_STAGES],
    pub target_rms_db: f64,
    pub intended_packed_rms_db: f64,
    pub sections_used: usize,
    pub grown: [Option<usize>; NUM_STAGES],
}

fn optimise_roots(
    roots: &mut [StageRoots; NUM_STAGES],
    editable_sections: &[usize],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    freedom: &Freedom,
    zones: &Zones,
    watch: Watch,
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
                    if !freedom[section][parameter] {
                        continue;
                    }
                    for direction in [-1.0f64, 1.0] {
                        let before = roots[section];
                        let mut trial = before;
                        match parameter {
                            0 => {
                                let lo = limits.display_freq_min_hz.max(zones[section][0]);
                                let hi = freq_ceiling.min(zones[section][1]);
                                if hi < lo {
                                    continue;
                                }
                                trial.pole_hz = (before.pole_hz
                                    * 2.0f64.powf(direction * frequency_step_octaves))
                                .clamp(lo, hi);
                            }
                            1 => {
                                let distance = (1.0 - before.pole_r).max(1e-6);
                                trial.pole_r = (1.0 - distance * radius_factor.powf(-direction))
                                    .clamp(0.0, 1.0);
                            }
                            2 => {
                                let lo = limits.display_freq_min_hz.max(zones[section][0]);
                                let hi = freq_ceiling.min(zones[section][1]);
                                if hi < lo {
                                    continue;
                                }
                                trial.zero_hz = (before.zero_hz
                                    * 2.0f64.powf(direction * frequency_step_octaves))
                                .clamp(lo, hi);
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
                            watch(roots, best_rms);
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

pub fn fit_arma(target: &[(f64, f64)], sample_rate_hz: f64) -> Option<ArmaFit> {
    fit_arma_pinned(target, sample_rate_hz, &[])
}

pub fn fit_arma_pinned(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    pinned_hz: &[f64],
) -> Option<ArmaFit> {
    fit_arma_pinned_pairs(target, sample_rate_hz, pinned_hz, &[])
}

pub fn fit_arma_pinned_pairs(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    pinned_hz: &[f64],
    pinned_zero_hz: &[f64],
) -> Option<ArmaFit> {
    fit_arma_pinned_pairs_watched(
        target,
        sample_rate_hz,
        pinned_hz,
        pinned_zero_hz,
        &mut unwatched(),
    )
}

pub fn fit_arma_pinned_pairs_watched(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    pinned_hz: &[f64],
    pinned_zero_hz: &[f64],
    watch: Watch,
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
            &freedom_from_pins(&pinned, &pinned_zero),
            &NO_ZONES,
            watch,
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
                &freedom_from_pins(&pinned, &pinned_zero),
                &NO_ZONES,
                watch,
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
                &freedom_from_pins(&pinned, &pinned_zero),
                &NO_ZONES,
                watch,
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
        grown: [None; NUM_STAGES],
    })
}

pub fn fit_arma_refine(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    seed: &[StageRoots; NUM_STAGES],
    pinned_pole: &[bool; NUM_STAGES],
) -> Option<ArmaFit> {
    let mut freedom = FREE;
    for section in 0..NUM_STAGES {
        freedom[section][0] = !pinned_pole[section];
    }
    fit_arma_planned(target, sample_rate_hz, seed, &freedom, &[true; NUM_STAGES], &NO_ZONES)
}

pub fn fit_arma_lane(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    seed: &[StageRoots; NUM_STAGES],
    lane: usize,
    freedom_row: &[bool; 4],
    zone: &[f64; 2],
) -> Option<ArmaFit> {
    if lane >= NUM_STAGES
        || target.len() < 32
        || target
            .iter()
            .any(|(frequency, db)| !frequency.is_finite() || *frequency <= 0.0 || !db.is_finite())
        || !sample_rate_hz.is_finite()
        || sample_rate_hz <= 0.0
    {
        return None;
    }
    let limits = authoring_limits_at(sample_rate_hz);
    let freq_ceiling = limits.display_freq_max_hz.min(limits.authoring_freq_max_hz);
    let lo = limits.display_freq_min_hz.max(zone[0]);
    let hi = freq_ceiling.min(zone[1]);
    if hi < lo {
        return None;
    }
    let mut cleared = *seed;
    cleared[lane] = StageRoots::IDENTITY;
    let residual = residual_curve(&cleared, target, sample_rate_hz);
    let peak = residual
        .iter()
        .enumerate()
        .filter(|(_, (frequency, _))| (lo..=hi).contains(frequency))
        .max_by(|a, b| a.1 .1.abs().total_cmp(&b.1 .1.abs()))
        .map(|(index, _)| index)?;
    let mut fresh = seed_at_index(&residual, peak, sample_rate_hz);
    fresh.pole_hz = fresh.pole_hz.clamp(lo, hi);
    fresh.zero_hz = fresh.zero_hz.clamp(lo, hi);
    let tight = {
        let mut t = fresh;
        if t.pole_r >= t.zero_r {
            t.pole_r = t.pole_r.max(SEED_DOMINANT_R);
        } else {
            t.zero_r = t.zero_r.max(SEED_DOMINANT_R);
        }
        t
    };
    let flipped = StageRoots {
        pole_r: fresh.zero_r,
        zero_r: fresh.pole_r,
        ..fresh
    };

    let banded: Vec<(f64, f64)>;
    let fit_target: &[(f64, f64)] = if zone[1].is_finite() {
        banded = target
            .iter()
            .copied()
            .filter(|(frequency, _)| (lo..=hi).contains(frequency))
            .collect();
        if banded.len() >= 8 {
            &banded
        } else {
            target
        }
    } else {
        target
    };

    let mut freedom = FREE;
    freedom[lane] = *freedom_row;
    let mut zones = NO_ZONES;
    zones[lane] = *zone;
    let mut best: Option<([StageRoots; NUM_STAGES], f64)> = None;
    for candidate in [fresh, tight, flipped] {
        let mut candidate = candidate;
        if validate_stage_roots_at(&candidate, sample_rate_hz) != RootValidity::Ok {
            candidate.pole_r = candidate.pole_r.min(SEED_DOMINANT_R);
            candidate.zero_r = candidate.zero_r.min(SEED_DOMINANT_R);
            if validate_stage_roots_at(&candidate, sample_rate_hz) != RootValidity::Ok {
                continue;
            }
        }
        let mut trial = cleared;
        trial[lane] = candidate;
        let rms = optimise_roots(
            &mut trial,
            &[lane],
            fit_target,
            sample_rate_hz,
            &freedom,
            &zones,
            &mut unwatched(),
        );
        if best.as_ref().is_none_or(|(_, b)| rms < *b) {
            best = Some((trial, rms));
        }
    }
    let (mut roots, _) = best?;

    let (gain_db, _) = optimal_gain_and_rms(&roots, target, sample_rate_hz);
    roots[lane].scale = 10.0f64.powf(gain_db / 20.0).min(limits.scale_max);
    if validate_stage_roots_at(&roots[lane], sample_rate_hz) != RootValidity::Ok {
        return None;
    }
    let target_rms_db = rms_with_embedded_gain(&roots, target, sample_rate_hz);
    let mut words = [[0u16; NUM_COEFFS]; NUM_STAGES];
    let mut sections_used = 0;
    for section in 0..NUM_STAGES {
        if validate_stage_roots_at(&roots[section], sample_rate_hz) != RootValidity::Ok {
            return None;
        }
        if roots[section].pole_r > 0.0 || roots[section].zero_r > 0.0 {
            sections_used += 1;
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
        sections_used,
        grown: [None; NUM_STAGES],
    })
}

pub fn fit_arma_planned(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    seed: &[StageRoots; NUM_STAGES],
    freedom: &Freedom,
    writable: &[bool; NUM_STAGES],
    zones: &Zones,
) -> Option<ArmaFit> {
    fit_arma_planned_watched(
        target,
        sample_rate_hz,
        seed,
        freedom,
        writable,
        &[true; NUM_STAGES],
        zones,
        &[],
        &mut unwatched(),
    )
}

pub fn fit_arma_planned_watched(
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    seed: &[StageRoots; NUM_STAGES],
    freedom: &Freedom,
    writable: &[bool; NUM_STAGES],
    grow: &[bool; NUM_STAGES],
    zones: &Zones,
    candidates: &[Vec<StageRoots>],
    watch: Watch,
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
    let mut open: Vec<usize> = Vec::new();
    for section in 0..NUM_STAGES {
        if !writable[section] {
            continue;
        }
        roots[section].scale = 1.0;
        let is_identity = roots[section].pole_r <= 0.0 && roots[section].zero_r <= 0.0;
        if is_identity {
            roots[section] = StageRoots::IDENTITY;
            if grow[section] {
                open.push(section);
            }
            continue;
        }
        if validate_stage_roots_at(&roots[section], sample_rate_hz) != RootValidity::Ok {
            return None;
        }
        active.push(section);
    }
    let mut best_rms = if active.is_empty() {
        optimal_gain_and_rms(&roots, target, sample_rate_hz).1
    } else {
        optimise_roots(&mut roots, &active, target, sample_rate_hz, freedom, zones, watch)
    };
    let mut grown = [None; NUM_STAGES];
    for section in open {
        let before = roots;
        let before_rms = best_rms;
        let mut seeds = vec![seed_from_residual(&roots, target, sample_rate_hz)];
        if let Some(list) = candidates.get(section) {
            seeds.extend(list.iter().copied());
        }
        let mut winner: Option<(usize, [StageRoots; NUM_STAGES], f64)> = None;
        for (index, mut seed) in seeds.into_iter().enumerate() {
            seed.scale = 1.0;
            if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
                seed.pole_r = seed.pole_r.min(SEED_DOMINANT_R);
                seed.zero_r = seed.zero_r.min(SEED_DOMINANT_R);
                if validate_stage_roots_at(&seed, sample_rate_hz) != RootValidity::Ok {
                    continue;
                }
            }
            let mut trial = before;
            trial[section] = seed;
            active.push(section);
            let rms =
                optimise_roots(&mut trial, &active, target, sample_rate_hz, freedom, zones, watch);
            active.pop();
            if winner.as_ref().map_or(true, |(_, _, best)| rms < *best) {
                winner = Some((index, trial, rms));
            }
        }
        if let Some((index, trial, rms)) = winner {
            if before_rms - rms >= SECTION_MIN_RMS_IMPROVEMENT_DB {
                roots = trial;
                active.push(section);
                best_rms = rms;
                grown[section] = Some(index);
            }
        }
    }
    if active.is_empty() {
        return None;
    }

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
        grown,
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
    fn sequential_lane_fits_claim_jobs_one_at_a_time() {
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
        let empty = [StageRoots::IDENTITY; NUM_STAGES];
        let open = [true; 4];
        let wide = [0.0, f64::INFINITY];
        let step1 = fit_arma_lane(&target, sample_rate, &empty, 0, &open, &wide).unwrap();
        assert_eq!(step1.sections_used, 1);
        let step2 = fit_arma_lane(&target, sample_rate, &step1.roots, 1, &open, &wide).unwrap();
        assert_eq!(step2.sections_used, 2);
        let (a, b) = (step2.roots[0], step1.roots[0]);
        assert!(
            a.pole_hz == b.pole_hz
                && a.pole_r == b.pole_r
                && a.zero_hz == b.zero_hz
                && a.zero_r == b.zero_r,
            "fitting lane 1 disturbed lane 0"
        );
        let mut writable = [false; NUM_STAGES];
        writable[0] = true;
        writable[1] = true;
        let polished =
            fit_arma_planned(&target, sample_rate, &step2.roots, &FREE, &writable, &NO_ZONES)
                .unwrap();
        for known_hz in [700.0, 2_100.0] {
            let claimed = polished.roots[..2]
                .iter()
                .any(|s| s.pole_r > 0.9 && (s.pole_hz / known_hz).log2().abs() < 0.3);
            assert!(claimed, "no lane claimed the {known_hz} Hz job after polish");
        }
        assert!(
            polished.target_rms_db < 1.5,
            "RMS was {}",
            polished.target_rms_db
        );
    }

    #[test]
    fn a_corridor_forces_the_lane_to_its_fenced_job() {
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
        let empty = [StageRoots::IDENTITY; NUM_STAGES];
        let open = [true; 4];
        let fence = [1_500.0, 3_000.0];
        let wide = [0.0, f64::INFINITY];
        let step1 = fit_arma_lane(&target, sample_rate, &empty, 0, &open, &fence).unwrap();
        assert!(
            (fence[0]..=fence[1]).contains(&step1.roots[0].pole_hz),
            "the pole left its corridor: {}",
            step1.roots[0].pole_hz
        );
        let step2 = fit_arma_lane(&target, sample_rate, &step1.roots, 1, &open, &wide).unwrap();
        let redeal1 = fit_arma_lane(&target, sample_rate, &step2.roots, 0, &open, &fence).unwrap();
        let redeal2 = fit_arma_lane(&target, sample_rate, &redeal1.roots, 1, &open, &wide).unwrap();
        let mut writable = [false; NUM_STAGES];
        writable[0] = true;
        writable[1] = true;
        let mut zones = NO_ZONES;
        zones[0] = fence;
        let polished =
            fit_arma_planned(&target, sample_rate, &redeal2.roots, &FREE, &writable, &zones)
                .unwrap();
        let fenced = polished.roots[0].pole_hz;
        assert!(
            (fence[0]..=fence[1]).contains(&fenced),
            "the fenced pole escaped after polish: {fenced}"
        );
        assert!(
            (fenced / 2_100.0).log2().abs() < 0.4,
            "the fenced lane missed the in-fence job: {fenced}"
        );
        assert!(
            (polished.roots[1].pole_hz / 700.0).log2().abs() < 0.4,
            "the open lane missed the out-of-fence job: {}",
            polished.roots[1].pole_hz
        );
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
