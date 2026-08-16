use trench_core::arma_endpoint::{fit_arma, FREE};
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::stage_words_to_biquad;
use trench_core::stage_law::{authoring_limits_at, validate_stage_roots_at, words_from_roots_at, RootValidity, StageRoots};
use std::path::Path;
use std::time::Instant;

const DATUM_SR: f64 = 39_062.5;
const TARGET_POINTS: usize = 1024;
const TARGET_LO_HZ: f64 = 40.0;
const TARGET_HI_HZ: f64 = 16_000.0;

fn log_grid(n: usize, lo: f64, hi: f64) -> Vec<f64> {
    (0..n)
        .map(|i| lo * (hi / lo).powf(i as f64 / (n - 1) as f64))
        .collect()
}

fn resample_curve(raw_freqs: &[f64], raw_dbs: &[f64], grid: &[f64]) -> Vec<(f64, f64)> {
    grid.iter()
        .map(|&f| {
            let upper = raw_freqs.partition_point(|&rf| rf < f);
            let db = if upper == 0 {
                raw_dbs[0]
            } else if upper >= raw_freqs.len() {
                raw_dbs[raw_dbs.len() - 1]
            } else {
                let f0 = raw_freqs[upper - 1];
                let f1 = raw_freqs[upper];
                let y0 = raw_dbs[upper - 1];
                let y1 = raw_dbs[upper];
                let denom = f1.log2() - f0.log2();
                if denom.abs() < 1e-15 {
                    y0
                } else {
                    let t = ((f.log2() - f0.log2()) / denom).clamp(0.0, 1.0);
                    y0 + t * (y1 - y0)
                }
            };
            (f, db)
        })
        .collect()
}

fn load_vvtf(path: &Path) -> (Vec<f64>, Vec<f64>) {
    let content = std::fs::read_to_string(path).expect("read file");
    let mut freqs = Vec::new();
    let mut dbs = Vec::new();
    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with("freq") || line.starts_with('#') {
            continue;
        }
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() < 2 {
            continue;
        }
        if let (Ok(f), Ok(lin_mag)) = (parts[0].parse::<f64>(), parts[1].parse::<f64>()) {
            if f > 0.0 && lin_mag.is_finite() {
                freqs.push(f);
                let db = 20.0 * lin_mag.max(1e-12).log10();
                dbs.push(db);
            }
        }
    }
    (freqs, dbs)
}

fn eval_stage_db(roots: &StageRoots, freq_hz: f64, sr: f64) -> f64 {
    let w = 2.0 * std::f64::consts::PI * freq_hz / sr;
    let cos_w = w.cos();
    let cos_2w = (2.0 * w).cos();
    let p_w = 2.0 * std::f64::consts::PI * roots.pole_hz / sr;
    let z_w = 2.0 * std::f64::consts::PI * roots.zero_hz / sr;
    let b1 = -2.0 * roots.zero_r * z_w.cos();
    let b2 = roots.zero_r * roots.zero_r;
    let a1 = -2.0 * roots.pole_r * p_w.cos();
    let a2 = roots.pole_r * roots.pole_r;
    let num = 1.0 + b1 * b1 + b2 * b2 + 2.0 * (b1 + b1 * b2) * cos_w + 2.0 * b2 * cos_2w;
    let den = 1.0 + a1 * a1 + a2 * a2 + 2.0 * (a1 + a1 * a2) * cos_w + 2.0 * a2 * cos_2w;
    10.0 * (roots.scale * roots.scale * (num / den.max(1e-15))).max(1e-12).log10()
}

fn eval_biquad_db(coeffs: &[f64; 5], freq_hz: f64, sr: f64) -> f64 {
    let w = 2.0 * std::f64::consts::PI * freq_hz / sr;
    let cos_w = w.cos();
    let cos_2w = (2.0 * w).cos();
    let (b0, b1, b2, a1, a2) = (coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4]);
    let num = b0 * b0 + b1 * b1 + b2 * b2 + 2.0 * (b0 * b1 + b1 * b2) * cos_w + 2.0 * b0 * b2 * cos_2w;
    let den = 1.0 + a1 * a1 + a2 * a2 + 2.0 * (a1 + a1 * a2) * cos_w + 2.0 * a2 * cos_2w;
    10.0 * (num / den.max(1e-15)).max(1e-12).log10()
}

fn calc_gain_and_rms(roots: &[StageRoots; NUM_STAGES], target: &[(f64, f64)], sr: f64) -> (f64, f64) {
    let n = target.len() as f64;
    let mut sum_diff = 0.0;
    let responses: Vec<f64> = target.iter().map(|&(f, _)| {
        roots.iter().map(|s| eval_stage_db(s, f, sr)).sum::<f64>()
    }).collect();

    for i in 0..target.len() {
        sum_diff += target[i].1 - responses[i];
    }
    let gain_db = sum_diff / n;
    let mut sum_sq = 0.0;
    for i in 0..target.len() {
        let err = target[i].1 - (responses[i] + gain_db);
        sum_sq += err * err;
    }
    (gain_db, (sum_sq / n).sqrt())
}

// ----------------------------------------------------------------------------
// LEVENBERG-MARQUARDT (LM) CASCADE FITTER PROTOTYPE
// ----------------------------------------------------------------------------

fn lm_refine_cascade(
    roots: &mut [StageRoots; NUM_STAGES],
    active_sections: &[usize],
    target: &[(f64, f64)],
    sample_rate_hz: f64,
    freedom: &[[bool; 4]; NUM_STAGES],
    max_iters: usize,
) -> f64 {
    let n_pts = target.len();
    let limits = authoring_limits_at(sample_rate_hz);
    let freq_ceiling = limits.display_freq_max_hz.min(limits.authoring_freq_max_hz);
    let freq_floor = limits.display_freq_min_hz;
    let r_ceil = limits.pole_radius_max;

    let mut lambda = 1e-2;
    let mut best_rms = calc_gain_and_rms(roots, target, sample_rate_hz).1;

    let mut param_map = Vec::new();
    for &s in active_sections {
        for p in 0..4 {
            if freedom[s][p] {
                param_map.push((s, p));
            }
        }
    }
    let n_params = param_map.len();
    if n_params == 0 {
        return best_rms;
    }

    let mut j_mat = vec![0.0; n_pts * n_params];
    let mut residual = vec![0.0; n_pts];

    for _iter in 0..max_iters {
        let (gain_db, current_rms) = calc_gain_and_rms(roots, target, sample_rate_hz);
        best_rms = current_rms;

        for i in 0..n_pts {
            let (f, t_db) = target[i];
            let resp = roots.iter().map(|s| eval_stage_db(s, f, sample_rate_hz)).sum::<f64>() + gain_db;
            residual[i] = t_db - resp;
        }

        for (p_idx, &(s, p_type)) in param_map.iter().enumerate() {
            let before = roots[s];
            let mut trial = before;
            let eps = 1e-4;

            match p_type {
                0 => {
                    trial.pole_hz = (before.pole_hz * (1.0 + eps)).clamp(freq_floor, freq_ceiling);
                }
                1 => {
                    let d = (1.0 - before.pole_r).max(1e-6);
                    trial.pole_r = (1.0 - d * (1.0 - eps)).clamp(0.0, r_ceil);
                }
                2 => {
                    trial.zero_hz = (before.zero_hz * (1.0 + eps)).clamp(freq_floor, freq_ceiling);
                }
                3 => {
                    let d = (1.0 - before.zero_r).max(1e-6);
                    trial.zero_r = (1.0 - d * (1.0 - eps)).clamp(0.0, 1.0);
                }
                _ => {}
            }

            if validate_stage_roots_at(&trial, sample_rate_hz) != RootValidity::Ok {
                for i in 0..n_pts {
                    j_mat[i * n_params + p_idx] = 0.0;
                }
                continue;
            }

            for i in 0..n_pts {
                let f = target[i].0;
                let db_before = eval_stage_db(&before, f, sample_rate_hz);
                let db_after = eval_stage_db(&trial, f, sample_rate_hz);
                j_mat[i * n_params + p_idx] = (db_after - db_before) / eps;
            }
        }

        let mut jtj = vec![0.0; n_params * n_params];
        let mut jtr = vec![0.0; n_params];

        for i in 0..n_pts {
            let r_i = residual[i];
            let row_offset = i * n_params;
            for p1 in 0..n_params {
                let j_ip1 = j_mat[row_offset + p1];
                jtr[p1] += j_ip1 * r_i;
                for p2 in 0..n_params {
                    jtj[p1 * n_params + p2] += j_ip1 * j_mat[row_offset + p2];
                }
            }
        }

        let mut a_mat = jtj.clone();
        for p in 0..n_params {
            a_mat[p * n_params + p] += lambda * (jtj[p * n_params + p] + 1e-6);
        }

        let delta = solve_linear_system(&a_mat, &jtr, n_params);
        if let Some(delta) = delta {
            let mut trial_roots = *roots;
            for (p_idx, &(s, p_type)) in param_map.iter().enumerate() {
                let step = delta[p_idx];
                let before = trial_roots[s];
                let mut trial = before;
                match p_type {
                    0 => {
                        trial.pole_hz = (before.pole_hz * (1.0 + step.clamp(-0.5, 0.5))).clamp(freq_floor, freq_ceiling);
                    }
                    1 => {
                        let d = (1.0 - before.pole_r).max(1e-6);
                        trial.pole_r = (1.0 - d * (1.0 - step.clamp(-0.5, 0.5))).clamp(0.0, r_ceil);
                    }
                    2 => {
                        trial.zero_hz = (before.zero_hz * (1.0 + step.clamp(-0.5, 0.5))).clamp(freq_floor, freq_ceiling);
                    }
                    3 => {
                        let d = (1.0 - before.zero_r).max(1e-6);
                        trial.zero_r = (1.0 - d * (1.0 - step.clamp(-0.5, 0.5))).clamp(0.0, 1.0);
                    }
                    _ => {}
                }
                if validate_stage_roots_at(&trial, sample_rate_hz) == RootValidity::Ok {
                    trial_roots[s] = trial;
                }
            }

            let (_, trial_rms) = calc_gain_and_rms(&trial_roots, target, sample_rate_hz);
            if trial_rms < best_rms {
                *roots = trial_roots;
                best_rms = trial_rms;
                lambda *= 0.3;
            } else {
                lambda *= 2.5;
            }
        } else {
            lambda *= 4.0;
        }

        if lambda > 1e6 {
            break;
        }
    }

    best_rms
}

fn solve_linear_system(a: &[f64], b: &[f64], n: usize) -> Option<Vec<f64>> {
    let mut m = vec![vec![0.0; n + 1]; n];
    for i in 0..n {
        for j in 0..n {
            m[i][j] = a[i * n + j];
        }
        m[i][n] = b[i];
    }

    for i in 0..n {
        let mut pivot_row = i;
        for k in (i + 1)..n {
            if m[k][i].abs() > m[pivot_row][i].abs() {
                pivot_row = k;
            }
        }
        if m[pivot_row][i].abs() < 1e-12 {
            return None;
        }
        m.swap(i, pivot_row);

        for k in (i + 1)..n {
            let factor = m[k][i] / m[i][i];
            for j in i..=n {
                m[k][j] -= factor * m[i][j];
            }
        }
    }

    let mut x = vec![0.0; n];
    for i in (0..n).rev() {
        let mut sum = 0.0;
        for j in (i + 1)..n {
            sum += m[i][j] * x[j];
        }
        x[i] = (m[i][n] - sum) / m[i][i];
    }
    Some(x)
}

pub fn fit_arma_lm(target: &[(f64, f64)], sample_rate_hz: f64) -> Option<StageRootsFit> {
    let mut roots = [StageRoots::IDENTITY; NUM_STAGES];
    let mut active = Vec::new();
    let freedom = FREE;

    for s in 0..NUM_STAGES {
        let (gain_db, before_rms) = calc_gain_and_rms(&roots, target, sample_rate_hz);
        
        let residual: Vec<(f64, f64)> = target.iter().map(|&(f, t_db)| {
            let resp = roots.iter().map(|st| eval_stage_db(st, f, sample_rate_hz)).sum::<f64>() + gain_db;
            (f, t_db - resp)
        }).collect();

        let peak_idx = residual.iter().enumerate()
            .max_by(|a, b| a.1.1.abs().total_cmp(&b.1.1.abs()))
            .map(|(idx, _)| idx).unwrap_or(0);

        let (peak_hz, peak_db) = residual[peak_idx];
        let dominant_r = if peak_db.abs() > 10.0 { 0.96 } else { 0.90 };
        let (p_r, z_r) = if peak_db >= 0.0 { (dominant_r, 0.50) } else { (0.50, dominant_r) };

        let candidate = StageRoots {
            pole_hz: peak_hz,
            pole_r: p_r,
            zero_hz: peak_hz,
            zero_r: z_r,
            scale: 1.0,
        };

        if validate_stage_roots_at(&candidate, sample_rate_hz) != RootValidity::Ok {
            break;
        }

        roots[s] = candidate;
        active.push(s);

        let new_rms = lm_refine_cascade(&mut roots, &active, target, sample_rate_hz, &freedom, 25);
        if before_rms - new_rms < 0.03 {
            roots[s] = StageRoots::IDENTITY;
            active.pop();
            break;
        }
    }

    let final_rms = lm_refine_cascade(&mut roots, &active, target, sample_rate_hz, &freedom, 40);

    let (gain_db, _) = calc_gain_and_rms(&roots, target, sample_rate_hz);
    let per_section = 10.0f64.powf(gain_db / 20.0 / active.len().max(1) as f64);
    for &s in &active {
        roots[s].scale = per_section;
    }

    let mut words = [[0u16; NUM_COEFFS]; NUM_STAGES];
    for s in 0..NUM_STAGES {
        words[s] = words_from_roots_at(&roots[s], sample_rate_hz);
    }

    let intended_packed_rms = (target.iter().map(|&(f, _)| {
        let intended = roots.iter().map(|st| eval_stage_db(st, f, sample_rate_hz)).sum::<f64>();
        let packed = words.iter().map(|st| eval_biquad_db(&stage_words_to_biquad(*st), f, sample_rate_hz)).sum::<f64>();
        (intended - packed).powi(2)
    }).sum::<f64>() / target.len() as f64).sqrt();

    Some(StageRootsFit {
        roots,
        words,
        target_rms_db: final_rms,
        intended_packed_rms_db: intended_packed_rms,
        sections_used: active.len(),
    })
}

#[derive(Clone, Debug)]
pub struct StageRootsFit {
    pub roots: [StageRoots; NUM_STAGES],
    pub words: [[u16; NUM_COEFFS]; NUM_STAGES],
    pub target_rms_db: f64,
    pub intended_packed_rms_db: f64,
    pub sections_used: usize,
}

#[test]
fn benchmark_fitters_comparative() {
    println!("\n=======================================================");
    println!("  COMPARATIVE BENCHMARK: COORDINATE DESCENT vs LEVENBERG-MARQUARDT");
    println!("=======================================================");

    let grid = log_grid(TARGET_POINTS, TARGET_LO_HZ, TARGET_HI_HZ);

    // 1. Benchmark TalkingHedz M0_Q0
    let hedz_words = [
        [0x9753, 0x0346, 0x9354, 0x0000, 0x5619],
        [0x9793, 0x0890, 0x9458, 0x1113, 0x5619],
        [0x9773, 0x1569, 0x9602, 0x2014, 0x5619],
        [0x9969, 0x2348, 0x9723, 0x2971, 0x5619],
        [0x9479, 0x4606, 0x7501, 0x7921, 0x5619],
        [0x9912, 0x0199, 0xFFFF, 0x6396, 0x5619],
    ];
    let mut hedz_target = Vec::new();
    for &f in &grid {
        let db = hedz_words.iter().map(|w| eval_biquad_db(&stage_words_to_biquad(*w), f, DATUM_SR)).sum::<f64>();
        hedz_target.push((f, db));
    }

    let t0 = Instant::now();
    let fit_coord = fit_arma(&hedz_target, DATUM_SR).unwrap();
    let dt_coord = t0.elapsed();

    let t0 = Instant::now();
    let fit_lm = fit_arma_lm(&hedz_target, DATUM_SR).unwrap();
    let dt_lm = t0.elapsed();

    println!("\n--- TalkingHedz M0_Q0 Cold Fit ---");
    println!("Coordinate Descent: RMS = {:.3} dB | Quant Penalty = {:.4} dB | Wall Time = {:>6.1} ms | Stages = {}",
        fit_coord.target_rms_db, fit_coord.intended_packed_rms_db, dt_coord.as_secs_f64() * 1000.0, fit_coord.sections_used);
    println!("Levenberg-Marquardt: RMS = {:.3} dB | Quant Penalty = {:.4} dB | Wall Time = {:>6.1} ms | Stages = {}",
        fit_lm.target_rms_db, fit_lm.intended_packed_rms_db, dt_lm.as_secs_f64() * 1000.0, fit_lm.sections_used);

    // 2. Benchmark 10 DVTD Curves
    println!("\n--- 10 DVTD Measured Vocal Tract Curves ---");
    let dvtd_paths = [
        "../recipes/vocal/dvtd/subject-1/s1-01-bahn-tense-a/s1-01-bahn-tense-a-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-1/s1-02-beet-tense-e/s1-02-beet-tense-e-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-1/s1-03-tiere-tense-i/s1-03-tiere-tense-i-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-1/s1-04-boote-tense-o/s1-04-boote-tense-o-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-1/s1-05-bude-tense-u/s1-05-bude-tense-u-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-2/s2-01-bahn-tense-a/s2-01-bahn-tense-a-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-2/s2-02-beet-tense-e/s2-02-beet-tense-e-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-2/s2-03-tiere-tense-i/s2-03-tiere-tense-i-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-2/s2-04-boote-tense-o/s2-04-boote-tense-o-vvtf-measured.txt",
        "../recipes/vocal/dvtd/subject-2/s2-05-bude-tense-u/s2-05-bude-tense-u-vvtf-measured.txt",
    ];

    let mut coord_rms_sum = 0.0;
    let mut coord_time_sum = 0.0;
    let mut lm_rms_sum = 0.0;
    let mut lm_time_sum = 0.0;

    for (i, p_str) in dvtd_paths.iter().enumerate() {
        let p = Path::new(p_str);
        if !p.exists() { continue; }
        let (raw_f, raw_db) = load_vvtf(p);
        let target = resample_curve(&raw_f, &raw_db, &grid);

        let t0 = Instant::now();
        let fc = fit_arma(&target, DATUM_SR).unwrap();
        let dt_c = t0.elapsed();

        let t0 = Instant::now();
        let flm = fit_arma_lm(&target, DATUM_SR).unwrap();
        let dt_l = t0.elapsed();

        coord_rms_sum += fc.target_rms_db;
        coord_time_sum += dt_c.as_secs_f64();
        lm_rms_sum += flm.target_rms_db;
        lm_time_sum += dt_l.as_secs_f64();

        println!("  [Curve {:>2}] Coord: {:.3} dB ({:>5.0} ms) vs LM: {:.3} dB ({:>5.0} ms)",
            i + 1, fc.target_rms_db, dt_c.as_secs_f64() * 1000.0,
            flm.target_rms_db, dt_l.as_secs_f64() * 1000.0
        );
    }

    println!("DVTD 10-Curve Mean: Coord RMS = {:.3} dB ({:.1} ms total) vs LM RMS = {:.3} dB ({:.1} ms total)",
        coord_rms_sum / 10.0, coord_time_sum * 1000.0,
        lm_rms_sum / 10.0, lm_time_sum * 1000.0
    );

    // 3. Lane Freedom Test: 2 Lanes Locked + 1 Pinned Zero
    println!("\n--- Lane Freedom Test: 2 Lanes Fully Locked + 1 Pinned Zero ---");
    let mut freedom = FREE;
    freedom[0] = [false, false, false, false]; // Lane 0 fully locked
    freedom[1] = [false, false, false, false]; // Lane 1 fully locked
    freedom[2][2] = false; // Lane 2 Zero Frequency pinned

    let mut test_roots = fit_coord.roots;
    let locked_pole_0 = test_roots[0].pole_hz;
    let locked_zero_2 = test_roots[2].zero_hz;

    let lm_locked_rms = lm_refine_cascade(&mut test_roots, &[0, 1, 2, 3, 4, 5, 6], &hedz_target, DATUM_SR, &freedom, 30);
    println!("LM with 2 locked lanes + pinned zero:");
    println!("  RMS: {:.3} dB", lm_locked_rms);
    println!("  Lane 0 Pole HZ preserved: {:.1} Hz (was {:.1} Hz) -> {}", test_roots[0].pole_hz, locked_pole_0, test_roots[0].pole_hz == locked_pole_0);
    println!("  Lane 2 Zero HZ preserved: {:.1} Hz (was {:.1} Hz) -> {}", test_roots[2].zero_hz, locked_zero_2, test_roots[2].zero_hz == locked_zero_2);
}
