use author::frame::LaneLaw;
use trench_core::arma_endpoint::ArmaFit;
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::stage_law::{
    authoring_limits_at, max_contiguous_pole_radius, words_from_roots_at, StageRoots,
};

use crate::domain::document::lane_is_empty;
use crate::engine::response::{row_db, SR};

const MAX_ITERS: usize = 120;
const STRIDE: usize = 3;

#[derive(Clone, Copy)]
struct Param {
    section: usize,
    kind: usize,
}

fn hz_bounds() -> (f64, f64) {
    (30.0, 0.49 * SR)
}

fn r_max(kind: usize) -> f64 {
    if kind == 1 {
        max_contiguous_pole_radius() - 1e-7
    } else {
        1.0
    }
}

fn to_u(kind: usize, v: f64) -> f64 {
    match kind {
        0 | 2 => v.max(1.0).ln(),
        _ => (1.0 - v.min(0.999_999_4)).ln(),
    }
}

fn from_u(kind: usize, u: f64) -> f64 {
    match kind {
        0 | 2 => u.exp(),
        _ => 1.0 - u.exp(),
    }
}

fn get(l: &StageRoots, kind: usize) -> f64 {
    match kind {
        0 => l.pole_hz,
        1 => l.pole_r,
        2 => l.zero_hz,
        _ => l.zero_r,
    }
}

fn set(l: &mut StageRoots, kind: usize, v: f64) {
    match kind {
        0 => l.pole_hz = v,
        1 => l.pole_r = v,
        2 => l.zero_hz = v,
        _ => l.zero_r = v,
    }
}

fn project(kind: usize, v: f64) -> f64 {
    let (flo, fhi) = hz_bounds();
    match kind {
        0 | 2 => v.clamp(flo, fhi),
        _ => {
            let v = v.clamp(0.05, r_max(kind));
            if kind == 3 && v > 0.999_999 {
                1.0
            } else {
                v
            }
        }
    }
}

fn stage_curve(lane: &StageRoots, freqs: &[f64], out: &mut [f64]) {
    let row = lane.biquad_at(SR);
    for (o, &f) in out.iter_mut().zip(freqs.iter()) {
        *o = row_db(&row, f, SR);
    }
}

fn solve(a: &mut Vec<Vec<f64>>, b: &mut Vec<f64>) -> Option<Vec<f64>> {
    let n = b.len();
    for col in 0..n {
        let mut piv = col;
        for row in col + 1..n {
            if a[row][col].abs() > a[piv][col].abs() {
                piv = row;
            }
        }
        if a[piv][col].abs() < 1e-14 {
            return None;
        }
        a.swap(col, piv);
        b.swap(col, piv);
        for row in col + 1..n {
            let f = a[row][col] / a[col][col];
            for k in col..n {
                a[row][k] -= f * a[col][k];
            }
            b[row] -= f * b[col];
        }
    }
    let mut x = vec![0.0; n];
    for col in (0..n).rev() {
        let mut acc = b[col];
        for k in col + 1..n {
            acc -= a[col][k] * x[k];
        }
        x[col] = acc / a[col][col];
    }
    Some(x)
}

fn model(lanes: &[StageRoots; NUM_STAGES], freqs: &[f64]) -> Vec<f64> {
    let rows: Vec<[f64; 5]> = lanes
        .iter()
        .filter(|l| !lane_is_empty(l))
        .map(|l| l.biquad_at(SR))
        .collect();
    freqs
        .iter()
        .map(|&f| rows.iter().map(|r| row_db(r, f, SR)).sum())
        .collect()
}

fn rms_after_gain(target: &[f64], y: &[f64]) -> (f64, f64) {
    let g = target
        .iter()
        .zip(y.iter())
        .map(|(t, y)| t - y)
        .sum::<f64>()
        / target.len() as f64;
    let acc: f64 = target
        .iter()
        .zip(y.iter())
        .map(|(t, y)| {
            let r = t - y - g;
            r * r
        })
        .sum();
    ((acc / target.len() as f64).sqrt(), g)
}

pub fn fit(
    pairs: &[(f64, f64)],
    start: [StageRoots; NUM_STAGES],
    laws: [LaneLaw; NUM_STAGES],
) -> Option<ArmaFit> {
    let mut params: Vec<Param> = Vec::new();
    for si in 0..NUM_STAGES {
        if !laws[si].writable || lane_is_empty(&start[si]) {
            continue;
        }
        for kind in 0..4 {
            let present = if kind < 2 {
                start[si].pole_r > 0.0
            } else {
                start[si].zero_r > 0.0
            };
            if present && laws[si].freedom[kind] {
                params.push(Param { section: si, kind });
            }
        }
    }
    if params.is_empty() {
        return None;
    }
    let freqs: Vec<f64> = pairs.iter().step_by(STRIDE).map(|&(f, _)| f).collect();
    let target: Vec<f64> = pairs.iter().step_by(STRIDE).map(|&(_, db)| db).collect();

    let (mut best, mut best_rms) = descend(&freqs, &target, start, &params);
    let movable_poles: Vec<usize> = params
        .iter()
        .filter(|p| p.kind == 0)
        .map(|p| p.section)
        .collect();
    let mut kicked: Vec<usize> = Vec::new();
    for _ in 0..movable_poles.len() {
        let y = model(&best, &freqs);
        let g = target.iter().zip(y.iter()).map(|(t, y)| t - y).sum::<f64>() / freqs.len() as f64;
        let peak = freqs
            .iter()
            .zip(target.iter().zip(y.iter()))
            .max_by(|a, b| (a.1 .0 - a.1 .1 - g).total_cmp(&(b.1 .0 - b.1 .1 - g)))
            .map(|(&f, _)| f);
        let Some(f_star) = peak else { break };
        let candidate = movable_poles
            .iter()
            .filter(|si| !kicked.contains(si))
            .min_by(|&&a, &&b| {
                let ra = residual_at(&freqs, &target, &y, g, best[a].pole_hz);
                let rb = residual_at(&freqs, &target, &y, g, best[b].pole_hz);
                ra.abs().total_cmp(&rb.abs())
            })
            .copied();
        let Some(si) = candidate else { break };
        kicked.push(si);
        let mut trial = best;
        trial[si].pole_hz = f_star.clamp(hz_bounds().0, hz_bounds().1);
        let (t, t_rms) = descend(&freqs, &target, trial, &params);
        if t_rms < best_rms - 1e-6 {
            best = t;
            best_rms = t_rms;
            kicked.clear();
        }
    }
    let _ = best_rms;
    Some(finish(best, &params, pairs))
}

fn residual_at(freqs: &[f64], target: &[f64], y: &[f64], g: f64, hz: f64) -> f64 {
    let mut k = 0;
    let mut dist = f64::INFINITY;
    for (i, &f) in freqs.iter().enumerate() {
        let d = (f / hz).ln().abs();
        if d < dist {
            dist = d;
            k = i;
        }
    }
    target[k] - y[k] - g
}

fn descend(
    freqs: &[f64],
    target: &[f64],
    start: [StageRoots; NUM_STAGES],
    params: &[Param],
) -> ([StageRoots; NUM_STAGES], f64) {
    let m = freqs.len();
    let n = params.len();
    let mut lanes = start;
    let mut y = model(&lanes, freqs);
    let (mut best_rms, _) = rms_after_gain(target, &y);
    let mut best = lanes;
    let mut lambda = 1e-3;
    let h = 1e-4;
    let mut stall = 0;

    for _ in 0..MAX_ITERS {
        let mut cols: Vec<Vec<f64>> = Vec::with_capacity(n);
        for p in params {
            let u0 = to_u(p.kind, get(&lanes[p.section], p.kind));
            let mut plus = lanes[p.section];
            set(&mut plus, p.kind, project(p.kind, from_u(p.kind, u0 + h)));
            let mut minus = lanes[p.section];
            set(&mut minus, p.kind, project(p.kind, from_u(p.kind, u0 - h)));
            let mut cp = vec![0.0; m];
            let mut cm = vec![0.0; m];
            stage_curve(&plus, freqs, &mut cp);
            stage_curve(&minus, freqs, &mut cm);
            let col: Vec<f64> = cp
                .iter()
                .zip(cm.iter())
                .map(|(a, b)| (a - b) / (2.0 * h))
                .collect();
            let mean = col.iter().sum::<f64>() / m as f64;
            cols.push(col.into_iter().map(|v| v - mean).collect());
        }
        let g = target
            .iter()
            .zip(y.iter())
            .map(|(t, y)| t - y)
            .sum::<f64>()
            / m as f64;
        let resid: Vec<f64> = target
            .iter()
            .zip(y.iter())
            .map(|(t, y)| t - y - g)
            .collect();

        let mut ata = vec![vec![0.0; n]; n];
        let mut atr = vec![0.0; n];
        for i in 0..n {
            for j in i..n {
                let dot: f64 = cols[i].iter().zip(cols[j].iter()).map(|(a, b)| a * b).sum();
                ata[i][j] = dot;
                ata[j][i] = dot;
            }
            atr[i] = cols[i].iter().zip(resid.iter()).map(|(a, r)| a * r).sum();
        }

        let mut improved = false;
        for _ in 0..8 {
            let mut a = ata.clone();
            for (i, row) in a.iter_mut().enumerate() {
                row[i] += lambda * (ata[i][i].max(1e-9));
            }
            let mut b = atr.clone();
            let Some(delta) = solve(&mut a, &mut b) else {
                lambda *= 4.0;
                continue;
            };
            let mut trial = lanes;
            for (p, d) in params.iter().zip(delta.iter()) {
                let u = to_u(p.kind, get(&trial[p.section], p.kind)) + d;
                set(&mut trial[p.section], p.kind, project(p.kind, from_u(p.kind, u)));
            }
            let ty = model(&trial, freqs);
            let (trial_rms, _) = rms_after_gain(target, &ty);
            if trial_rms < best_rms {
                lanes = trial;
                y = ty;
                best = trial;
                let gain_now = best_rms - trial_rms;
                best_rms = trial_rms;
                lambda = (lambda / 3.0).max(1e-9);
                improved = true;
                if gain_now < 1e-5 {
                    stall += 1;
                } else {
                    stall = 0;
                }
                break;
            }
            lambda *= 4.0;
            if lambda > 1e7 {
                return (best, best_rms);
            }
        }
        if !improved || stall >= 8 {
            break;
        }
    }
    (best, best_rms)
}

fn finish(
    mut lanes: [StageRoots; NUM_STAGES],
    params: &[Param],
    pairs: &[(f64, f64)],
) -> ArmaFit {
    let freqs: Vec<f64> = pairs.iter().map(|&(f, _)| f).collect();
    let target: Vec<f64> = pairs.iter().map(|&(_, db)| db).collect();
    let y = model(&lanes, &freqs);
    let (_, g) = rms_after_gain(&target, &y);
    let movable: Vec<usize> = {
        let mut v: Vec<usize> = params.iter().map(|p| p.section).collect();
        v.sort_unstable();
        v.dedup();
        v
    };
    let lim = authoring_limits_at(SR);
    if !movable.is_empty() {
        let per = g / movable.len() as f64;
        for &si in &movable {
            let s = (lanes[si].scale * 10f64.powf(per / 20.0)).clamp(lim.scale_min, lim.scale_max);
            lanes[si].scale = s;
        }
    }
    let y2 = model(&lanes, &freqs);
    let acc: f64 = target
        .iter()
        .zip(y2.iter())
        .map(|(t, y)| (t - y) * (t - y))
        .sum();
    let rms = (acc / target.len() as f64).sqrt();
    let mut words = [[0u16; NUM_COEFFS]; NUM_STAGES];
    for (w, lane) in words.iter_mut().zip(lanes.iter()) {
        *w = words_from_roots_at(lane, SR);
    }
    let used = lanes.iter().filter(|l| !lane_is_empty(l)).count();
    ArmaFit {
        roots: lanes,
        words,
        target_rms_db: rms,
        intended_packed_rms_db: rms,
        sections_used: used,
    }
}
