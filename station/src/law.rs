use std::path::Path;

use serde::Deserialize;

use crate::body::Body;

/// One law, as written in laws.json. The station never invents a bound: it
/// measures the quantity and reports it against the span you set.
#[derive(Clone, Deserialize)]
pub struct Law {
    #[serde(default = "frame_scope")]
    pub on: String,
    pub q: String,
    pub min: f64,
    pub max: f64,
    #[serde(default)]
    pub why: String,
}

fn frame_scope() -> String {
    "frame".into()
}

#[derive(Deserialize)]
struct File {
    laws: Vec<Law>,
}

pub struct Reading {
    pub law: Law,
    pub value: f64,
    pub where_: String,
    pub ok: bool,
}

pub fn load(path: &Path) -> Result<Vec<Law>, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let f: File = serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
    Ok(f.laws)
}

fn median(v: &mut Vec<f64>) -> f64 {
    v.sort_by(|a, b| a.partial_cmp(b).unwrap());
    if v.is_empty() { 0.0 } else { v[v.len() / 2] }
}

fn peaks(db: &[f64], prom: f64) -> usize {
    let w = 12usize;
    (2..db.len().saturating_sub(2))
        .filter(|&i| {
            if !(db[i] > db[i - 1] && db[i] >= db[i + 1]) {
                return false;
            }
            let lo = db[i.saturating_sub(w)..i].iter().cloned().fold(f64::MAX, f64::min);
            let hi = db[i..(i + w).min(db.len())].iter().cloned().fold(f64::MAX, f64::min);
            db[i] - lo.min(hi) >= prom
        })
        .count()
}

/// Every quantity a law may name. Add one here and it is immediately available
/// to laws.json; nothing else needs to change.
pub fn measure(b: &Body, q: &str, frame: usize) -> Option<f64> {
    let ns = b.stages();
    let db = b.frame_total(frame);
    let band: Vec<f64> = b
        .grid
        .iter()
        .zip(&db)
        .filter(|(f, _)| **f > 60.0 && **f < 16_000.0)
        .map(|(_, d)| *d)
        .collect();
    let mut sorted = band.clone();
    let med = median(&mut sorted);
    let mean_in = |lo: f64, hi: f64| -> f64 {
        let v: Vec<f64> = b
            .grid
            .iter()
            .zip(&db)
            .filter(|(f, _)| **f > lo && **f < hi)
            .map(|(_, d)| *d)
            .collect();
        if v.is_empty() { 0.0 } else { v.iter().sum::<f64>() / v.len() as f64 }
    };

    Some(match q {
        "order" => (0..ns)
            .filter(|&s| b.roots(frame, s).0.map(|(_, r)| r > 0.0).unwrap_or(false))
            .count() as f64
            * 2.0,
        "crown_db" => band.iter().cloned().fold(f64::MIN, f64::max) - med,
        "dip_db" => med - band.iter().cloned().fold(f64::MAX, f64::min),
        "tilt_db" => mean_in(60.0, 300.0) - mean_in(4_000.0, 12_000.0),
        "peaks" => peaks(&db, 4.0) as f64,
        "gap_oct" => {
            let mut hz: Vec<f64> = (0..ns).filter_map(|s| b.roots(frame, s).0.map(|(h, _)| h)).collect();
            hz.sort_by(|a, c| a.partial_cmp(c).unwrap());
            hz.windows(2)
                .map(|w| (w[1] / w[0]).log2())
                .fold(f64::MAX, f64::min)
        }
        "zero_min_st" | "zero_max_st" => {
            let off: Vec<f64> = (0..ns)
                .filter_map(|s| match b.roots(frame, s) {
                    (Some((p, _)), Some((z, _))) if p > 20.0 && z > 20.0 => Some(12.0 * (z / p).log2()),
                    _ => None,
                })
                .collect();
            if off.is_empty() {
                return None;
            }
            if q == "zero_min_st" {
                off.iter().cloned().fold(f64::MAX, |a, x| a.min(x.abs()))
            } else {
                off.iter().cloned().fold(0.0, |a, x| a.max(x.abs()))
            }
        }
        "scale_spread_db" => {
            let s: Vec<f64> = (0..ns).map(|s| 20.0 * b.scale(frame, s).max(1e-9).log10()).collect();
            s.iter().cloned().fold(f64::MIN, f64::max) - s.iter().cloned().fold(f64::MAX, f64::min)
        }
        _ => return None,
    })
}

/// Body-scope quantities look across the wheel rather than at one frame.
pub fn measure_body(b: &Body, q: &str) -> Option<(f64, String)> {
    let ns = b.stages();
    match q {
        "meet_st" | "meet_at" => {
            let mut best = (f64::MAX, 0.0f64, 0usize, 0usize);
            for i in 0..=80 {
                let m = i as f32 / 80.0;
                let hz: Vec<Option<f64>> = (0..ns)
                    .map(|s| {
                        let w = b.packed.interpolate_words(m, 0.0, 0.0)[s];
                        match geometry_pole(w, b.rate) {
                            Some(h) if h > 20.0 => Some(h),
                            _ => None,
                        }
                    })
                    .collect();
                for a in 0..ns {
                    for c in (a + 1)..ns {
                        if let (Some(x), Some(y)) = (hz[a], hz[c]) {
                            let d = (12.0 * (y / x).log2()).abs();
                            if d < best.0 {
                                best = (d, m as f64, a + 1, c + 1);
                            }
                        }
                    }
                }
            }
            if best.0 == f64::MAX {
                return None;
            }
            let note = format!("S{} and S{} at MORPH {:.0}", best.2, best.3, best.1 * 100.0);
            Some(if q == "meet_st" { (best.0, note) } else { (best.1, note) })
        }
        _ => None,
    }
}

fn geometry_pole(words: [u16; 5], rate: f64) -> Option<f64> {
    match geometry_from_words_at_pole(words, rate) {
        RootPairHz::Hz(h) => Some(h),
        RootPairHz::None => None,
    }
}

enum RootPairHz {
    Hz(f64),
    None,
}

fn geometry_from_words_at_pole(words: [u16; 5], rate: f64) -> RootPairHz {
    use trench_core::stage_law::{geometry_from_words_at, RootPair};
    match geometry_from_words_at(words, rate).pole {
        RootPair::Conjugate { hz, .. } => RootPairHz::Hz(hz),
        _ => RootPairHz::None,
    }
}

pub fn read_all(b: &Body, laws: &[Law], frame: usize) -> Vec<Reading> {
    laws
        .iter()
        .filter_map(|l| {
            let (v, w) = if l.on == "body" {
                let (v, note) = measure_body(b, &l.q)?;
                (v, note)
            } else {
                (measure(b, &l.q, frame)?, crate::body::FRAME_NAME[frame].to_string())
            };
            Some(Reading {
                law: l.clone(),
                value: v,
                where_: w,
                ok: v >= l.min && v <= l.max,
            })
        })
        .collect()
}
