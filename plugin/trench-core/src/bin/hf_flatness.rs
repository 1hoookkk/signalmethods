//! Morpheus authoring law: formant bodies are PEAKS ON A FLAT BACKGROUND,
//! not a lowpass with bumps.
//!
//! Morpheus Operation Manual, Ch.11 "Dipthong Filters":
//!   "the resonances do not have the traditional overall low pass effect that a
//!    true vocal resonance would have. Instead, they are placed at the same
//!    frequency as the resonances would be found in a true vowel, but THE
//!    RESPONSE AT HIGH FREQUENCIES IS ESSENTIALLY FLAT to allow high frequencies
//!    of the signals to get through the filter."
//!
//! This scores every shipping body against that law: how far the 8-16 kHz shelf
//! sits below the body's own peak. A big drop means the body is eating the top
//! of the signal - which is what "dull" and "weak" sound like.

use trench_core::cartridge::Cartridge;
use trench_core::response::biquad_cascade_mag_db;

const HOST_SR: f64 = 48_000.0;

fn main() {
    let dir = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "plugin/presets/bodies".into());
    let mut entries: Vec<_> = std::fs::read_dir(&dir)
        .expect("read bodies dir")
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|p| p.extension().and_then(|s| s.to_str()) == Some("body240"))
        .collect();
    entries.sort();

    println!("Morpheus law: peaks on a flat background, HF essentially flat.");
    println!("drop = how far the 8-16 kHz shelf sits below the body's own peak.\n");

    let mut scored: Vec<(f64, String, f64, f64)> = Vec::new();
    for path in &entries {
        let bytes = match std::fs::read(path) {
            Ok(b) => b,
            Err(_) => continue,
        };
        let cart = match Cartridge::from_body_bytes("b", &bytes, 1.0) {
            Ok(c) => c,
            Err(_) => continue,
        };
        // worst corner: the one that eats the most top end
        let mut worst_drop = f64::NEG_INFINITY;
        let mut worst_peak = 0.0;
        let mut worst_hf = 0.0;
        for &(m, q) in &[(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)] {
            let corner = cart.packed.interpolate_biquad(m, q, 0.0);
            let mut peak = f64::NEG_INFINITY;
            for i in 0..400 {
                let f = 40.0 * (18_000.0f64 / 40.0).powf(i as f64 / 399.0);
                let d = biquad_cascade_mag_db(&corner, f, HOST_SR);
                if d.is_finite() && d > peak {
                    peak = d;
                }
            }
            let mut hf = 0.0;
            let mut n = 0;
            for i in 0..60 {
                let f = 8000.0 * (16_000.0f64 / 8000.0).powf(i as f64 / 59.0);
                let d = biquad_cascade_mag_db(&corner, f, HOST_SR);
                if d.is_finite() {
                    hf += d;
                    n += 1;
                }
            }
            if n == 0 || !peak.is_finite() {
                continue;
            }
            hf /= n as f64;
            let drop = peak - hf;
            if drop > worst_drop {
                worst_drop = drop;
                worst_peak = peak;
                worst_hf = hf;
            }
        }
        if worst_drop.is_finite() {
            let name = path.file_stem().unwrap().to_string_lossy().to_string();
            scored.push((worst_drop, name, worst_peak, worst_hf));
        }
    }

    scored.sort_by(|a, b| b.0.partial_cmp(&a.0).unwrap());
    let n = scored.len();
    let over = |t: f64| scored.iter().filter(|s| s.0 > t).count();
    println!("scored {n} bodies");
    println!(
        "  HF drop  > 40 dB : {:3}  ({:.0}%)   badly lowpassed",
        over(40.0),
        100.0 * over(40.0) as f64 / n as f64
    );
    println!(
        "  HF drop  > 24 dB : {:3}  ({:.0}%)",
        over(24.0),
        100.0 * over(24.0) as f64 / n as f64
    );
    println!(
        "  HF drop  > 12 dB : {:3}  ({:.0}%)",
        over(12.0),
        100.0 * over(12.0) as f64 / n as f64
    );
    println!(
        "  HF drop <= 12 dB : {:3}  ({:.0}%)   Morpheus-legal\n",
        n - over(12.0),
        100.0 * (n - over(12.0)) as f64 / n as f64
    );

    println!("worst 12 - these eat the most top end:");
    for (drop, name, peak, hf) in scored.iter().take(12) {
        println!("  {drop:7.1} dB   peak {peak:+7.1}  hf {hf:+8.1}   {name}");
    }
    println!("\nbest 12 - closest to the authored law:");
    for (drop, name, peak, hf) in scored.iter().rev().take(12) {
        println!("  {drop:7.1} dB   peak {peak:+7.1}  hf {hf:+8.1}   {name}");
    }
}
