//! carve-ab — settle the TPT question on real geometry.
//!
//! The synthetic single-stage probe in `tpt.rs` said DF2 carries a ring through
//! an abrupt coefficient change cleanly and TPT dumps it. That was one stage of
//! b=[1,0,0] at radius 0.995. The bodies that actually matter are six stages of
//! near-unit-circle ZEROS with deliberately tame poles, driven by the aimer.
//!
//! So: run DX_carve through both runtimes, per sample, aimer in the loop, and
//! compare against the shipping control-rate path.
//!
//!   cargo run -p trench-core --bin carve-ab --release -- bodies/candidates/DX_carve.body240

use trench_core::cascade::{Cascade, BLOCK_SIZE};
use trench_core::env::{Aimer, ENV_HOP};
use trench_core::minifloat::PackedCorners;
use trench_core::tpt::TptCascade;

const SR: f64 = 48_000.0;
const N: usize = 48_000; // one second

/// Deterministic noise — no wallclock, so the run is reproducible.
struct Lcg(u64);
impl Lcg {
    fn next(&mut self) -> f64 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        ((self.0 >> 11) as f64 / (1u64 << 53) as f64) * 2.0 - 1.0
    }
}

/// Two-pole bandpass, used to shape the "ess" bursts.
struct Bp { b0: f64, a1: f64, a2: f64, x1: f64, x2: f64, y1: f64, y2: f64 }
impl Bp {
    fn new(hz: f64, q: f64) -> Self {
        let w = 2.0 * std::f64::consts::PI * hz / SR;
        let alpha = w.sin() / (2.0 * q);
        let a0 = 1.0 + alpha;
        Self { b0: alpha / a0, a1: -2.0 * w.cos() / a0, a2: (1.0 - alpha) / a0,
               x1: 0.0, x2: 0.0, y1: 0.0, y2: 0.0 }
    }
    fn run(&mut self, x: f64) -> f64 {
        let y = self.b0 * (x - self.x2) - self.a1 * self.y1 - self.a2 * self.y2;
        self.x2 = self.x1; self.x1 = x; self.y2 = self.y1; self.y1 = y;
        y
    }
}

/// A voice: steady low tone, with sibilant bursts every 200 ms.
fn test_signal() -> Vec<f32> {
    let mut rng = Lcg(0x5EED_1234);
    let mut bp = Bp::new(7_000.0, 2.0);
    (0..N)
        .map(|i| {
            let t = i as f64 / SR;
            let voice = 0.30 * (2.0 * std::f64::consts::PI * 200.0 * t).sin();
            // 60 ms of 7 kHz-centred noise at the top of every 200 ms.
            let phase = t % 0.200;
            let gate = if phase < 0.060 {
                let e = (phase / 0.060).min(1.0);
                (std::f64::consts::PI * e).sin()
            } else {
                0.0
            };
            let ess = bp.run(rng.next()) * 6.0 * gate;
            (voice + ess) as f32
        })
        .collect()
}

/// Energy above 14 kHz — above the aimer's search band and above the notch, so
/// nothing in the signal or the filter's job puts it there. Artefact only.
fn spurious_hf(v: &[f64]) -> f64 {
    let rms = (v.iter().map(|y| y * y).sum::<f64>() / v.len() as f64).sqrt();
    let mut hf = 0.0;
    let mut bin = 14_000.0;
    while bin < 23_000.0 {
        let (mut re, mut im) = (0.0, 0.0);
        for (n, &y) in v.iter().enumerate() {
            let w = 2.0 * std::f64::consts::PI * bin * n as f64 / SR;
            re += y * w.cos();
            im -= y * w.sin();
        }
        hf += (re * re + im * im) / (v.len() as f64).powi(2);
        bin += 500.0;
    }
    20.0 * (hf.sqrt() / rms.max(1e-18)).log10()
}

fn main() {
    let path = std::env::args().nth(1)
        .unwrap_or_else(|| "bodies/candidates/DX_carve.body240".into());
    let bytes = std::fs::read(&path).unwrap_or_else(|e| panic!("read {path}: {e}"));
    let packed = PackedCorners::from_body_bytes(&bytes).expect("body240");

    let x = test_signal();

    // The aimer drives both wheels, exactly as the engine wires it.
    let mut aim = Aimer::new(SR);
    aim.set_range(2_000.0, 12_000.0);

    let mut df2_ps = Cascade::new();      // DF2, retuned every sample
    let mut tpt_ps = TptCascade::new();   // TPT, retuned every sample
    let mut df2_cr = Cascade::new();      // DF2, control rate + per-sample ramp

    let (mut out_df2, mut out_tpt, mut out_ref) =
        (Vec::with_capacity(N), Vec::with_capacity(N), Vec::with_capacity(N));
    let (mut morph, mut q) = (0.0f64, 0.0f64);
    let mut wheel_travel = 0.0f64;
    let (mut q_min, mut q_max) = (1.0f64, 0.0f64);
    let mut morph_max = 0.0f64;

    for i in 0..N {
        if i % ENV_HOP == 0 {
            let end = (i + ENV_HOP).min(N);
            aim.advance(&x[i..end]);
            let (m, qq) = (aim.excess(), aim.q_position());
            wheel_travel += (qq - q).abs();
            q = qq;
            morph = m;
            q_min = q_min.min(q);
            q_max = q_max.max(q);
            morph_max = morph_max.max(morph);
        }
        // Both per-sample paths see the identical geometry.
        let rows = packed.interpolate_biquad(morph as f32, q as f32, 0.0);
        df2_ps.snap_targets(&rows);
        tpt_ps.set_rows(&rows);
        if i % BLOCK_SIZE == 0 {
            df2_cr.set_targets(&rows, BLOCK_SIZE);
        }
        out_df2.push(f64::from(df2_ps.tick(x[i])));
        out_tpt.push(f64::from(tpt_ps.tick(x[i])));
        out_ref.push(f64::from(df2_cr.tick(x[i])));
    }

    // Skip the first 100 ms: aimer priming, not steady behaviour.
    let s = (SR * 0.1) as usize;
    let dry: Vec<f64> = x[s..].iter().map(|&v| f64::from(v)).collect();

    let diff: Vec<f64> = out_df2[s..].iter().zip(&out_tpt[s..]).map(|(a, b)| a - b).collect();
    let d_rms = (diff.iter().map(|v| v * v).sum::<f64>() / diff.len() as f64).sqrt();
    let o_rms = (out_df2[s..].iter().map(|v| v * v).sum::<f64>() / (N - s) as f64).sqrt();

    println!("\nbody: {path}");
    println!("aimer: Q swept {q_min:.3}..{q_max:.3} ({wheel_travel:.1} wheel units travelled), \
              MORPH peaked {morph_max:.3}");
    println!("\nspurious energy above 14 kHz, relative to each output's own level:");
    println!("  dry signal            {:8.2} dB   (the floor this can reach)", spurious_hf(&dry));
    println!("  DF2  per-sample       {:8.2} dB", spurious_hf(&out_df2[s..]));
    println!("  TPT  per-sample       {:8.2} dB", spurious_hf(&out_tpt[s..]));
    println!("  DF2  control rate     {:8.2} dB   (what ships today)", spurious_hf(&out_ref[s..]));
    println!("\nDF2 vs TPT difference: {:.2} dB below the output",
             20.0 * (d_rms / o_rms.max(1e-18)).log10());
}
