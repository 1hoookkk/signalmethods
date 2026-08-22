// Author a cube by section, write it native, and prove it.
//
// The design is in the sections. A section is a row that exists at all eight
// frames — pole, zero, scale, eight times. Seven sections x 8 frames x 5 words
// = 280 words = 560 bytes. This binary reads that table, writes those bytes,
// and then tries to catch itself lying about them.
//
// Five falsifiable outputs, none of which the legacy 6x4 path can produce:
//
//   order            2 x (sections carrying a conjugate pole), per frame
//   round-trip       to_native_bytes -> from_body_bytes, all 280 words
//   legacy refusal   the body must NOT be legacy-representable
//   section 7        nulled against the identity: how much it carries, in dB
//   Transform 2      frame z=0 against z=1: how far the third axis moves it
//
// The plot is the gate, and it is drawn by the house inspector, which now
// reads a 560-byte cube: three axis rides, seven sections, eight frames, and
// the running cascade where a zero eating a peak is visible.
//
//   cube TABLE.json
//   python tools/inspect_body.py TABLE.body 44100 TABLE.png
//
// No listening step.
//
// Frame index is `m | q<<1 | z<<2`, which is how E-MU numbers frames: 1-based
// with Morph varying fastest, frame 1 the all-axes-zero frame, the third axis
// the "rear" plane (ref/morpheus_manual_vocabulary.md).

use serde::Deserialize;
use std::f64::consts::PI;

use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, IDENTITY_STAGE, NUM_CORNERS};
use trench_core::response::{biquad_cascade_mag_db, log_frequency_grid};
use trench_core::stage_law::{
    authoring_limits_at, geometry_from_words_at, radius_survives_encoding,
    validate_stage_roots_at, words_from_roots_at, RootPair, RootValidity, StageRoots,
};

#[derive(Deserialize)]
struct Frame {
    pole_hz: f64,
    pole_bw_hz: f64,
    zero_hz: f64,
    zero_bw_hz: f64,
    #[serde(default)]
    gain_db: f64,
}

#[derive(Deserialize)]
struct Section {
    #[serde(default)]
    label: String,
    #[serde(default)]
    identity: bool,
    #[serde(default)]
    frames: Vec<Frame>,
}

#[derive(Deserialize)]
struct Table {
    name: String,
    sample_rate_hz: f64,
    sections: Vec<Section>,
}

/// Rossum's radial law read backwards: a pole bandwidth in Hz is a radius.
fn radius_from_bw(bw_hz: f64, fs: f64) -> f64 {
    (-PI * bw_hz / fs).exp()
}

const FRAME_NAMES: [&str; NUM_CORNERS] = [
    "m0 q0 z0", "m1 q0 z0", "m0 q1 z0", "m1 q1 z0",
    "m0 q0 z1", "m1 q0 z1", "m0 q1 z1", "m1 q1 z1",
];

fn build(table: &Table) -> Result<PackedCorners, String> {
    let fs = table.sample_rate_hz;
    let lim = authoring_limits_at(fs);
    if table.sections.len() != NUM_STAGES {
        return Err(format!(
            "a cube has {NUM_STAGES} sections; the table has {}",
            table.sections.len()
        ));
    }
    let mut words = [[IDENTITY_STAGE; NUM_STAGES]; NUM_CORNERS];
    for (si, section) in table.sections.iter().enumerate() {
        if section.identity {
            continue;
        }
        if section.frames.len() != NUM_CORNERS {
            return Err(format!(
                "section {} ({}) has {} frames; a cube has {NUM_CORNERS}",
                si + 1,
                section.label,
                section.frames.len()
            ));
        }
        for (ci, f) in section.frames.iter().enumerate() {
            let roots = StageRoots {
                pole_hz: f.pole_hz,
                pole_r: radius_from_bw(f.pole_bw_hz, fs),
                zero_hz: f.zero_hz,
                zero_r: radius_from_bw(f.zero_bw_hz, fs),
                scale: 10.0f64.powf(f.gain_db / 20.0),
            };
            let where_ = format!("section {} frame {} [{}]", si + 1, ci, FRAME_NAMES[ci]);
            match validate_stage_roots_at(&roots, fs) {
                RootValidity::Ok => {}
                bad => {
                    return Err(format!(
                        "{where_}: refused by the encoder ({bad:?}). \
                         pole {:.1} Hz, zero {:.1} Hz; the domain is {:.2}..{:.1} Hz \
                         and the authoring ceiling is {:.1} Hz",
                        f.pole_hz,
                        f.zero_hz,
                        lim.display_freq_min_hz,
                        lim.display_freq_max_hz,
                        lim.authoring_freq_max_hz
                    ))
                }
            }
            // The encoder is not monotone in r: there is a hole just below
            // 1 - r^2 = 2^-15 where a radius collapses onto the unit circle.
            // Test the actual value, never a threshold.
            if !radius_survives_encoding(roots.pole_r) {
                return Err(format!(
                    "{where_}: pole bandwidth {:.1} Hz gives r = {:.6}, which the \
                     encoder cannot hold — it would land on the unit circle",
                    f.pole_bw_hz, roots.pole_r
                ));
            }
            words[ci][si] = words_from_roots_at(&roots, fs);
        }
    }
    Ok(PackedCorners { words })
}

/// Order is 2 x the sections carrying a conjugate pole, counted per frame from
/// the words as stored — not from the table that made them.
fn order_of(packed: &PackedCorners, ci: usize, fs: f64) -> usize {
    (0..NUM_STAGES)
        .filter(|&si| {
            matches!(
                geometry_from_words_at(packed.words[ci][si], fs).pole,
                RootPair::Conjugate { r, .. } if r > 0.0
            )
        })
        .count()
        * 2
}

fn cascade_db(packed: &PackedCorners, m: f32, q: f32, z: f32, grid: &[f64], fs: f64) -> Vec<f64> {
    let rows = packed.interpolate_biquad(m, q, z);
    grid.iter()
        .map(|&f| biquad_cascade_mag_db(&rows, f, fs))
        .collect()
}

fn max_abs_delta(a: &[f64], b: &[f64]) -> f64 {
    a.iter()
        .zip(b)
        .map(|(x, y)| (x - y).abs())
        .fold(0.0f64, f64::max)
}

fn run() -> Result<i32, String> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        return Err("usage: cube TABLE.json".into());
    }
    let table_path = &args[1];
    let text = std::fs::read_to_string(table_path).map_err(|e| format!("{table_path}: {e}"))?;
    let table: Table = serde_json::from_str(&text).map_err(|e| format!("{table_path}: {e}"))?;
    let fs = table.sample_rate_hz;

    let packed = build(&table)?;
    let bytes = packed.to_native_bytes();

    println!("cube  {}  @ {:.0} Hz", table.name, fs);
    for (si, s) in table.sections.iter().enumerate() {
        let tag = if s.identity { "identity" } else { &s.label };
        println!("  S{}  {tag}", si + 1);
    }
    println!();

    let mut failures = 0;

    // 1. order, per frame, read back out of the words
    let orders: Vec<usize> = (0..NUM_CORNERS).map(|ci| order_of(&packed, ci, fs)).collect();
    let want = 2 * NUM_STAGES;
    let ok = orders.iter().all(|&o| o == want);
    failures += !ok as i32;
    println!(
        "  order           {} {}",
        orders
            .iter()
            .map(|o| o.to_string())
            .collect::<Vec<_>>()
            .join(" "),
        if ok {
            format!("— {want}th order at every frame")
        } else {
            format!("— FAIL, wanted {want} at all {NUM_CORNERS} frames")
        }
    );

    // 2. the 560-byte round-trip
    let back = PackedCorners::from_body_bytes(&bytes)?;
    let total = NUM_CORNERS * NUM_STAGES * NUM_COEFFS;
    let same = (0..NUM_CORNERS)
        .flat_map(|c| (0..NUM_STAGES).flat_map(move |s| (0..NUM_COEFFS).map(move |w| (c, s, w))))
        .filter(|&(c, s, w)| packed.words[c][s][w] == back.words[c][s][w])
        .count();
    failures += (same != total) as i32;
    println!(
        "  round-trip      {same}/{total} words identical through {} bytes {}",
        bytes.len(),
        if same == total { "" } else { "— FAIL" }
    );

    // 3. this body must not fit in the legacy container
    let legacy = packed.is_legacy_representable();
    failures += legacy as i32;
    println!(
        "  legacy refusal  {} — it uses the seventh section and the third axis",
        if legacy { "FAIL, it still fits in 240 bytes" } else { "refused" }
    );

    let grid = log_frequency_grid(20.0, 20_000.0, 2048);

    // 4. section 7 nulled against the identity
    let mut without = packed.clone();
    for ci in 0..NUM_CORNERS {
        without.words[ci][NUM_STAGES - 1] = IDENTITY_STAGE;
    }
    let s7 = max_abs_delta(
        &cascade_db(&packed, 0.0, 0.0, 0.0, &grid, fs),
        &cascade_db(&without, 0.0, 0.0, 0.0, &grid, fs),
    );
    failures += (s7 <= 1.0) as i32;
    println!(
        "  section 7       {s7:.1} dB against the identity {}",
        if s7 > 1.0 { "— it carries signal" } else { "— FAIL, it is silent" }
    );

    // 5. the third axis
    let z = max_abs_delta(
        &cascade_db(&packed, 0.0, 0.0, 0.0, &grid, fs),
        &cascade_db(&packed, 0.0, 0.0, 1.0, &grid, fs),
    );
    failures += (z <= 1.0) as i32;
    println!(
        "  Transform 2     {z:.1} dB from z=0 to z=1 {}",
        if z > 1.0 { "— the third axis is live" } else { "— FAIL, it is flat" }
    );

    let out = std::path::Path::new(table_path)
        .with_extension("body")
        .to_string_lossy()
        .into_owned();
    std::fs::write(&out, bytes).map_err(|e| format!("{out}: {e}"))?;
    println!("  wrote           {out}  ({} bytes)", bytes.len());

    println!();
    println!(
        "{}",
        if failures == 0 {
            "the cube is real. now look at the plate."
        } else {
            "FAILED"
        }
    );
    Ok(failures.min(1))
}

fn main() {
    match run() {
        Ok(code) => std::process::exit(code),
        Err(e) => {
            eprintln!("cube: {e}");
            std::process::exit(2);
        }
    }
}
