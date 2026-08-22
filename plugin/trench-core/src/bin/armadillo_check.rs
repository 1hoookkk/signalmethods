//! Verify TRENCH's stored encoding against Rossum's own published law.
//!
//! Source: Dave Rossum, "The 'ARMAdillo' Coefficient Encoding Scheme for Digital
//! Audio Filters", E-mu Systems (ref/patents/rossum_armadillo_coefficient_encoding.pdf).
//!
//! The paper states the resonance field k2 is encoded so that the resulting peak
//! height in dB is linear in k2:
//!
//!     p = 8.68 * k2 + 6.02
//!
//! That constant pair is derivable: for a complex pole b2 = R^2, so
//!     1 - b2 = (1-R)(1+R) ~ 2(1-R)   for R near 1
//!     p = 20 log10( 1/(1-R) ) = 20 log10(2) + 20 log10( 1/(1-b2) )
//! giving intercept 20 log10 2 = 6.0206 dB, and if 1-b2 = e^-k2 the slope is
//! 20 log10 e = 8.6859 dB per unit k2.
//!
//! In TRENCH's format a2 = 1 - decode(word[3]), so decode(word[3]) IS Rossum's
//! (1 - b2). This walks the real word space and checks the law holds.

use trench_core::minifloat::decode;

const ROSSUM_SLOPE: f64 = 8.68;
const ROSSUM_INTERCEPT: f64 = 6.02;

fn main() {
    println!("Rossum ARMAdillo law:  p = {ROSSUM_SLOPE} * k2 + {ROSSUM_INTERCEPT}");
    println!("TRENCH field under test: word[3] (pole r^2), a2 = 1 - decode(word)\n");

    let mut rows = Vec::new();
    for word in 0u32..=0xFFFF {
        let d = decode(word as u16); // == Rossum's (1 - b2)
        if !(d > 0.0 && d < 1.0) {
            continue;
        }
        let a2 = 1.0 - d;
        if a2 <= 0.0 {
            continue;
        }
        let r = a2.sqrt();
        if !(r > 0.0 && r < 1.0) {
            continue;
        }
        let p_actual = 20.0 * (1.0 / (1.0 - r)).log10();
        let k2 = -d.ln();
        let p_rossum = ROSSUM_SLOPE * k2 + ROSSUM_INTERCEPT;
        rows.push((word as u16, d, r, k2, p_actual, p_rossum));
    }

    // Least-squares fit of p_actual against k2 over the resonant range Rossum
    // describes ("for significant resonance both R and b2 will be near unity").
    let fit: Vec<_> = rows.iter().filter(|r| r.2 >= 0.5).collect();
    let n = fit.len() as f64;
    let (sx, sy): (f64, f64) = fit.iter().fold((0.0, 0.0), |a, r| (a.0 + r.3, a.1 + r.4));
    let (mx, my) = (sx / n, sy / n);
    let (num, den): (f64, f64) = fit.iter().fold((0.0, 0.0), |a, r| {
        (a.0 + (r.3 - mx) * (r.4 - my), a.1 + (r.3 - mx).powi(2))
    });
    let slope = num / den;
    let intercept = my - slope * mx;

    println!("measured over {} words with pole radius >= 0.5:", fit.len());
    println!(
        "  slope     {slope:.4}   (Rossum {ROSSUM_SLOPE},   exact 20*log10(e) = {:.4})",
        20.0 * std::f64::consts::E.log10()
    );
    println!(
        "  intercept {intercept:.4}   (Rossum {ROSSUM_INTERCEPT},   exact 20*log10(2) = {:.4})\n",
        20.0 * 2.0f64.log10()
    );

    let worst = fit.iter().map(|r| (r.4 - r.5).abs()).fold(0.0f64, f64::max);
    println!("  worst |p_actual - p_rossum| across that range: {worst:.3} dB");
    println!("  (the paper allows ~1 dB: \"errors introduced by such a scheme are");
    println!("   in the neighborhood of 1 dB\")\n");

    println!("  spot values");
    println!(
        "  {:>8}  {:>10}  {:>8}  {:>7}  {:>9}  {:>9}",
        "word", "1-b2", "R", "k2", "p actual", "p Rossum"
    );
    for &target in &[0.9, 0.99, 0.999, 0.9999] {
        if let Some(r) = fit.iter().min_by(|a, b| {
            (a.2 - target)
                .abs()
                .partial_cmp(&(b.2 - target).abs())
                .unwrap()
        }) {
            println!(
                "  0x{:04x}  {:10.7}  {:8.5}  {:7.3}  {:8.2} dB {:8.2} dB",
                r.0, r.1, r.2, r.3, r.4, r.5
            );
        }
    }

    // Rossum's second claim: k1 encodes frequency linearly in musical octaves.
    // Octave number Omega = log2(f / 20), f from the pole angle.
    println!("\n  and the octave claim: equal steps in the encoded field should be");
    println!("  equal musical intervals, not equal Hz.");
    let sr = 44100.0;
    println!(
        "  {:>8}  {:>12}  {:>10}  {:>12}",
        "exponent", "decode", "f @44.1k", "cents/step"
    );
    let mut prev: Option<(i32, f64)> = None;
    for e in 4..=14 {
        let word = ((e as u32) << 12 | 0x800) as u16;
        let d = decode(word);
        // treat the field as an angle-ish magnitude for the interval check
        let f = d * sr / 2.0;
        let cents = prev.map(|(_, pf)| 1200.0 * (f / pf).log2());
        println!(
            "  {e:>8}  {d:12.8}  {f:10.2}  {:>12}",
            cents
                .map(|c| format!("{c:+.1}"))
                .unwrap_or_else(|| "-".into())
        );
        prev = Some((e, f));
    }
}
