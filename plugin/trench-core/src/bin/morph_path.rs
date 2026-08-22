//! Is a morph a STRAIGHT LINE on Rossum's ARMAdillo plot?
//!
//! The encoding is linear in musical octaves and linear in dB of resonance, and
//! the runtime linearly interpolates the encoded words. So the path a pole takes
//! from one corner to another should be a straight line in (octave, dB) space -
//! which would mean the plot predicts the audible trajectory by eye.
//!
//! This walks the real packed interpolation and measures the deviation from the
//! straight line between the endpoints.

use trench_core::cartridge::Cartridge;
use trench_core::minifloat::stage_words_to_biquad;

const SR: f64 = 48_000.0;
const REF_HZ: f64 = 20.0;

/// ARMAdillo coordinates: angle = musical octave number, radius = dB of resonance.
fn arma_coords(a1: f64, a2: f64) -> Option<(f64, f64)> {
    if a2 <= 1.0e-12 {
        return None;
    }
    let r = a2.sqrt();
    if !(r > 1.0e-6 && r < 1.0) {
        return None;
    }
    let cos_t = -a1 / (2.0 * r);
    if cos_t.abs() > 1.0 {
        return None; // real pair: no meaningful octave number
    }
    let theta = cos_t.acos();
    let hz = theta * SR / (2.0 * std::f64::consts::PI);
    if hz < REF_HZ {
        return None;
    }
    Some(((hz / REF_HZ).log2(), 20.0 * (1.0 / (1.0 - r)).log10()))
}

fn main() {
    let mut args = std::env::args().skip(1);
    let path = args.next().expect("usage: morph-path <body240>");
    let bytes = std::fs::read(&path).expect("read body");
    let cart = Cartridge::from_body_bytes("p", &bytes, 1.0).expect("cartridge");

    println!("body: {path}");
    println!("morph M0 -> M100 at Q=0, per stage, walking the REAL packed interpolation.");
    println!("deviation = distance from the straight line between the endpoints,");
    println!("in ARMAdillo coordinates (octaves across, dB of resonance up).\n");
    println!(
        "{:>5}  {:>9} {:>8}   {:>9} {:>8}   {:>10} {:>10}",
        "stage", "oct@M0", "dB@M0", "oct@M100", "dB@M100", "max dev", "as % span"
    );

    let mut worst_overall: f64 = 0.0;
    for stage in 0..6 {
        let ends: Vec<Option<(f64, f64)>> = [0.0f32, 1.0]
            .iter()
            .map(|&m| {
                let w = cart.packed.interpolate_words(m, 0.0, 0.0)[stage];
                let b = stage_words_to_biquad(w);
                arma_coords(b[3], b[4])
            })
            .collect();
        let (Some(a), Some(z)) = (ends[0], ends[1]) else {
            println!("{stage:>5}  {:>9}", "(real pair / out of range)");
            continue;
        };
        let span = ((z.0 - a.0).powi(2) + (z.1 - a.1).powi(2)).sqrt();
        let mut max_dev: f64 = 0.0;
        for step in 1..20 {
            let t = step as f64 / 20.0;
            let w = cart.packed.interpolate_words(t as f32, 0.0, 0.0)[stage];
            let b = stage_words_to_biquad(w);
            let Some(p) = arma_coords(b[3], b[4]) else {
                continue;
            };
            // perpendicular distance from the straight line a->z
            let dev = if span > 1.0e-9 {
                ((z.0 - a.0) * (a.1 - p.1) - (a.0 - p.0) * (z.1 - a.1)).abs() / span
            } else {
                ((p.0 - a.0).powi(2) + (p.1 - a.1).powi(2)).sqrt()
            };
            max_dev = max_dev.max(dev);
        }
        worst_overall = worst_overall.max(if span > 1.0e-9 { max_dev / span } else { 0.0 });
        println!(
            "{stage:>5}  {:>9.3} {:>8.1}   {:>9.3} {:>8.1}   {:>10.4} {:>9.2}%",
            a.0,
            a.1,
            z.0,
            z.1,
            max_dev,
            if span > 1.0e-9 {
                100.0 * max_dev / span
            } else {
                0.0
            }
        );
    }
    println!(
        "\nworst deviation across all stages: {:.2}% of its own span",
        100.0 * worst_overall
    );
    println!("under a few percent = the plot predicts the audible path by eye.");
}
