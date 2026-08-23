//! Motion envelope for the 33 factory bodies.
//!
//! Every frozen interior point is provably stable — an interior coefficient
//! row is a convex blend of stable corner sections. That proof says nothing
//! about MOTION: a wheel stepping between two stable rows re-uses the filter
//! state across the step, and a state carried into a new pole geometry can
//! grow. This suite drives Morph with instant squares (2/20/200 Hz), full-range
//! sines (0.5/5/50 Hz) and a single 0.3 s step, on pink-ish noise and on a lone
//! impulse, at both Q ends, and compares the moving peak against the loudest
//! frozen point of the same body.
//!
//! The cascade runs linear here (BITE at zero and the section nonlinearity
//! bypassed, no AGC, no output saturation, no DC block, no width) so growth
//! reads as gain instead of being swallowed by the ±2.0 state ceiling. `unst`
//! counts runs where a stage went non-finite and the cascade muted itself —
//! without that column a divergence would read as silence, not as a NaN.

use std::path::PathBuf;
use trench_core::{Cartridge, FilterEngine};

const SR: f64 = 48_000.0;
const N: usize = 48_000;
const HOST_BLOCK: usize = 512;
const IMPULSE_AT: usize = 14_400;
const WINDOW: usize = 4_800;
const TRAJECTORIES: [&str; 7] = ["sq2", "sq20", "sq200", "sin.5", "sin5", "sin50", "step"];
const FROZEN: [f64; 5] = [0.0, 0.25, 0.5, 0.75, 1.0];
const QS: [f64; 2] = [0.0, 1.0];

enum Morph<'a> {
    Held(f64),
    Moving(&'a [f32]),
}

fn dbfs(v: f64) -> f64 {
    20.0 * v.abs().max(1.0e-15).log10()
}

fn morph_at(name: &str, t: f64) -> f64 {
    use std::f64::consts::TAU;
    let square = |hz: f64| if (t * hz).fract() < 0.5 { 0.0 } else { 1.0 };
    let sine = |hz: f64| 0.5 - 0.5 * (TAU * hz * t).cos();
    match name {
        "sq2" => square(2.0),
        "sq20" => square(20.0),
        "sq200" => square(200.0),
        "sin.5" => sine(0.5),
        "sin5" => sine(5.0),
        "sin50" => sine(50.0),
        "step" => {
            if (0.3..0.6).contains(&t) {
                1.0
            } else {
                0.0
            }
        }
        other => panic!("unknown trajectory {other}"),
    }
}

fn trajectory(name: &str) -> Vec<f32> {
    (0..N)
        .map(|i| morph_at(name, i as f64 / SR) as f32)
        .collect()
}

fn noise() -> Vec<f32> {
    use std::f64::consts::TAU;
    let alpha = 1.0 - (-TAU * 500.0 / SR).exp();
    let mut seed: u32 = 0x1BAD_F00D;
    let mut y = 0.0f64;
    let mut raw = Vec::with_capacity(N);
    for _ in 0..N {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        let white = (seed as f64 / u32::MAX as f64) * 2.0 - 1.0;
        y += alpha * (white - y);
        raw.push(y);
    }
    let peak = raw.iter().fold(0.0f64, |a, b| a.max(b.abs()));
    raw.iter().map(|s| (s / peak * 0.25) as f32).collect()
}

fn impulse() -> Vec<f32> {
    let mut v = vec![0.0f32; N];
    v[IMPULSE_AT] = 0.5;
    v
}

fn bodies() -> Vec<(String, Vec<u8>)> {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../ref/presets")
        .canonicalize()
        .expect("preset directory");
    let mut paths: Vec<PathBuf> = std::fs::read_dir(&dir)
        .expect("read preset directory")
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.extension().and_then(|e| e.to_str()) == Some("bin"))
        .collect();
    paths.sort();
    paths
        .into_iter()
        .map(|p| {
            let name = p.file_stem().unwrap().to_string_lossy().into_owned();
            let bytes = std::fs::read(&p).expect("read body");
            assert_eq!(bytes.len(), 240, "{name} is not a 240-byte body");
            (name, bytes)
        })
        .collect()
}

fn render(body: &[u8], input: &[f32], q: f64, morph: Morph) -> (Vec<f32>, bool) {
    let cart = Cartridge::from_body_bytes("stress", body, 1.0).expect("body load");
    let mut e = FilterEngine::new();
    e.prepare(SR);
    e.debug.agc_enabled = false;
    e.debug.dc_block_enabled = false;
    e.debug.saturation_enabled = false;
    e.debug.spatial_enabled = false;
    e.debug.nonlinearity_enabled = false;
    e.set_amount(1.0);
    e.load_cartridge(cart);
    let mut left = input.to_vec();
    let mut right = input.to_vec();
    let mut off = 0usize;
    while off < left.len() {
        let len = (left.len() - off).min(HOST_BLOCK);
        let (l, r) = (&mut left[off..off + len], &mut right[off..off + len]);
        match morph {
            Morph::Held(m) => e.process_block(l, r, m, q),
            Morph::Moving(t) => e.process_trajectory(l, r, &t[off..off + len], q),
        }
        off += len;
    }
    let unstable = e.take_instability_flag();
    (left, unstable)
}

fn peak_of(out: &[f32]) -> f64 {
    out.iter().fold(0.0f64, |a, b| a.max(b.abs() as f64))
}

fn nonfinite(out: &[f32]) -> usize {
    out.iter().filter(|s| !s.is_finite()).count()
}

fn rms(out: &[f32]) -> f64 {
    let sum: f64 = out.iter().map(|s| (*s as f64) * (*s as f64)).sum();
    (sum / out.len() as f64).sqrt()
}

fn tail_ratio_db(out: &[f32]) -> f64 {
    let after = rms(&out[IMPULSE_AT..IMPULSE_AT + WINDOW]);
    let last = rms(&out[N - WINDOW..]);
    dbfs(last / after.max(1.0e-15))
}

fn median(mut v: Vec<f64>) -> f64 {
    v.sort_by(|a, b| a.partial_cmp(b).unwrap());
    v[v.len() / 2]
}

#[test]
fn modulation_envelope_over_the_factory_bank() {
    let signals = [("noise", noise()), ("impulse", impulse())];
    let paths: Vec<(&str, Vec<f32>)> = TRAJECTORIES.iter().map(|n| (*n, trajectory(n))).collect();

    println!(
        "{:<24}{:>5}{:>6}{:>10}{:>11}{:>11}{:>10}  {}",
        "body", "nan", "unst", "peak_db", "frozen_db", "motion_db", "tail_db", "worst_case"
    );

    let mut peaks = Vec::new();
    let mut frozens = Vec::new();
    let mut motions = Vec::new();
    let mut tails = Vec::new();
    let mut nan_total = 0usize;
    let mut unst_total = 0usize;

    for (name, body) in bodies() {
        let mut nan = 0usize;
        let mut unst = 0usize;
        let mut peak = 0.0f64;
        let mut frozen_peak = 0.0f64;
        let mut tail = f64::NEG_INFINITY;
        let mut worst = String::new();

        for &q in &QS {
            for (signal_name, signal) in &signals {
                for (path_name, path) in &paths {
                    let (out, unstable) = render(&body, signal, q, Morph::Moving(path));
                    nan += nonfinite(&out);
                    unst += usize::from(unstable);
                    let p = peak_of(&out);
                    if p > peak {
                        peak = p;
                        worst = format!("q{q:.0}/{path_name}/{signal_name}");
                    }
                    if *signal_name == "impulse" {
                        tail = tail.max(tail_ratio_db(&out));
                    }
                }
                for &m in &FROZEN {
                    let (out, unstable) = render(&body, signal, q, Morph::Held(m));
                    nan += nonfinite(&out);
                    unst += usize::from(unstable);
                    frozen_peak = frozen_peak.max(peak_of(&out));
                }
            }
        }

        let peak_db = dbfs(peak);
        let frozen_db = dbfs(frozen_peak);
        let motion_db = peak_db - frozen_db;
        println!(
            "{name:<24}{nan:>5}{unst:>6}{peak_db:>10.2}{frozen_db:>11.2}{motion_db:>11.2}{tail:>10.2}  {worst}"
        );

        assert_eq!(nan, 0, "{name}: {nan} non-finite output samples");
        assert_eq!(unst, 0, "{name}: {unst} runs muted by a non-finite stage");
        peaks.push(peak_db);
        frozens.push(frozen_db);
        motions.push(motion_db);
        tails.push(tail);
        nan_total += nan;
        unst_total += unst;
    }

    println!(
        "{:<24}{:>5}{:>6}{:>10.2}{:>11.2}{:>11.2}{:>10.2}",
        "median",
        "-",
        "-",
        median(peaks.clone()),
        median(frozens.clone()),
        median(motions.clone()),
        median(tails.clone())
    );
    println!(
        "{:<24}{:>5}{:>6}{:>10.2}{:>11.2}{:>11.2}{:>10.2}",
        "max",
        nan_total,
        unst_total,
        peaks.iter().cloned().fold(f64::NEG_INFINITY, f64::max),
        frozens.iter().cloned().fold(f64::NEG_INFINITY, f64::max),
        motions.iter().cloned().fold(f64::NEG_INFINITY, f64::max),
        tails.iter().cloned().fold(f64::NEG_INFINITY, f64::max)
    );
}
