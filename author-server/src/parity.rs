use serde_json::{json, Value};
use trench_core::minifloat::PackedCorners;

use crate::fit::row_db;

pub fn vectors(packed: &PackedCorners, n: usize, sr: f64) -> Value {
    let mut cases = Vec::new();
    let mut state: u64 = 0x5DEECE66D;
    let mut rand = || {
        state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        ((state >> 33) as f64) / ((1u64 << 31) as f64)
    };
    let mut coords: Vec<(f32, f32, f32)> = Vec::new();
    for m in [0.0f32, 1.0] {
        for q in [0.0f32, 1.0] {
            for z in [0.0f32, 1.0] {
                coords.push((m, q, z));
            }
        }
    }
    coords.push((0.5, 0.5, 0.5));
    for _ in 0..n {
        coords.push((rand() as f32, rand() as f32, rand() as f32));
    }
    let grid: Vec<f64> = (0..128)
        .map(|i| 40.0 * (16_000.0f64 / 40.0).powf(i as f64 / 127.0))
        .collect();
    for (m, q, z) in coords {
        let words = packed.interpolate_words(m, q, z);
        let rows = packed.interpolate_biquad(m, q, z);
        let curve: Vec<f64> = grid
            .iter()
            .map(|&hz| rows.iter().map(|r| row_db(r, hz, sr)).sum::<f64>())
            .collect();
        cases.push(json!({
            "m": m, "q": q, "z": z,
            "iwords": words,
            "rows": rows,
            "curve": curve,
        }));
    }
    json!({ "words": packed.words, "sr": sr, "cases": cases })
}

