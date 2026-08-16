#[derive(Clone, Copy, Debug)]
pub struct Peak {
    pub hz: f64,
    pub db: f64,
    pub bandwidth_hz: f64,
}

pub const VOICE_LO_HZ: f64 = 120.0;
pub const VOICE_HI_HZ: f64 = 5_500.0;
const PROMINENCE_DB: f64 = 6.0;
const BANDWIDTH_MIN_HZ: f64 = 30.0;
const BANDWIDTH_MAX_HZ: f64 = 600.0;

pub fn peaks(grid: &[f64], curve: &[f64]) -> Vec<Peak> {
    let n = grid.len().min(curve.len());
    if n < 16 {
        return Vec::new();
    }
    let smooth: Vec<f64> = (0..n)
        .map(|i| {
            let lo = i.saturating_sub(2);
            let hi = (i + 3).min(n);
            curve[lo..hi].iter().sum::<f64>() / (hi - lo) as f64
        })
        .collect();

    let mut found = Vec::new();
    for i in 1..n - 1 {
        if !(VOICE_LO_HZ..=VOICE_HI_HZ).contains(&grid[i]) {
            continue;
        }
        if smooth[i] <= smooth[i - 1] || smooth[i] < smooth[i + 1] {
            continue;
        }
        let mut left_min = smooth[i];
        for k in (0..i).rev() {
            left_min = left_min.min(smooth[k]);
            if smooth[k] > smooth[i] {
                break;
            }
        }
        let mut right_min = smooth[i];
        for k in i + 1..n {
            right_min = right_min.min(smooth[k]);
            if smooth[k] > smooth[i] {
                break;
            }
        }
        if smooth[i] - left_min.max(right_min) < PROMINENCE_DB {
            continue;
        }
        let half = smooth[i] - 3.0;
        let mut lo_hz = grid[0];
        for k in (0..i).rev() {
            if smooth[k] <= half {
                lo_hz = grid[k];
                break;
            }
        }
        let mut hi_hz = grid[n - 1];
        for k in i + 1..n {
            if smooth[k] <= half {
                hi_hz = grid[k];
                break;
            }
        }
        let bandwidth = (hi_hz - lo_hz).clamp(BANDWIDTH_MIN_HZ, BANDWIDTH_MAX_HZ);
        found.push(Peak {
            hz: grid[i],
            db: curve[i],
            bandwidth_hz: bandwidth,
        });
    }
    found.sort_by(|a, b| a.hz.total_cmp(&b.hz));
    let mut merged: Vec<Peak> = Vec::new();
    for p in found {
        if let Some(last) = merged.last_mut() {
            if 12.0 * (p.hz / last.hz).log2() < 3.0 {
                if p.db > last.db {
                    *last = p;
                }
                continue;
            }
        }
        merged.push(p);
    }
    merged
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::envelope;

    #[test]
    fn known_resonances_are_found_with_sane_bandwidths() {
        let grid = envelope::grid();
        let f = [700.0, 1_200.0, 2_600.0];
        let bw = [90.0, 120.0, 200.0];
        let curve: Vec<f64> = grid
            .iter()
            .map(|&hz| {
                f.iter()
                    .zip(&bw)
                    .map(|(&fc, &b)| {
                        let x = (hz - fc) / (b / 2.0);
                        10.0 * (1.0 / (1.0 + x * x)).log10() + 8.0
                    })
                    .sum::<f64>()
            })
            .collect();
        let found = peaks(&grid, &curve);
        assert!(found.len() >= 3, "found {:?}", found);
        for (&fc, &b) in f.iter().zip(&bw) {
            let hit = found
                .iter()
                .find(|p| (p.hz / fc).log2().abs() < 0.1)
                .unwrap_or_else(|| panic!("no peak near {fc}: {found:?}"));
            assert!(
                hit.bandwidth_hz > b * 0.3 && hit.bandwidth_hz < b * 4.0,
                "bandwidth at {fc}: {} vs true {b}",
                hit.bandwidth_hz
            );
        }
    }
}
