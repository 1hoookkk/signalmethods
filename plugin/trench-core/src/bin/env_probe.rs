//! The ENV macro proof: a drum-pop train through a real body at the true rate,
//! rendered with the envelope follower on vs off.
//!
//! Evidence printed: the follower pops on hits and relaxes in tails; the MORPH
//! offset obeys `(env - ref) * amount` clamped to +-1; and the LIVE cascade
//! coefficients — the ones actually filtering the audio — describe a different
//! curve at the hit than in the tail. The audio itself is measured too.
//!
//! Usage: env-probe <body.body240> [out_prefix]

use std::io::Write;
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::cartridge::CornerData;
use trench_core::engine::FilterEngine;
use trench_core::env::ENV_HOP;
use trench_core::Cartridge;

const SR: f64 = 48_000.0;
const MORPH: f64 = 0.15; // parked low so the pops have somewhere to travel
const Q: f64 = 0.75;
const AMOUNT: f32 = 1.0;
const RELEASE_MS: f32 = 200.0;
const HITS: usize = 8;
const HIT_PERIOD_S: f64 = 0.6;
const PROBE_HZ: [f64; 10] = [
    60.0, 120.0, 250.0, 500.0, 900.0, 1500.0, 2400.0, 3800.0, 6000.0, 9000.0,
];

fn main() {
    let body_path = std::env::args()
        .nth(1)
        .expect("usage: env-probe <body.body240> [prefix]");
    let prefix = std::env::args().nth(2).unwrap_or_else(|| "env".to_string());
    let bytes = std::fs::read(&body_path).expect("read body");
    let name = std::path::Path::new(&body_path)
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("body");
    let input = drum_pops();

    // (t, level, reference, offset, morph_position)
    type Trace = Vec<(f64, f64, f64, f64, f64)>;
    let render = |env_on: bool| -> (Vec<f32>, Trace, CornerData, CornerData) {
        let mut eng = FilterEngine::new();
        eng.prepare(SR);
        eng.debug.agc_enabled = false;
        eng.debug.dc_block_enabled = false;
        eng.debug.spatial_enabled = false;
        // Cap off: isolate the wheel travel from the broadband accident
        // insurance, which is level-only and engages differently per position.
        let cart = Cartridge::from_body_bytes(name, &bytes, 1.0).expect("load body");
        eng.load_cartridge(cart);
        if env_on {
            eng.set_env(AMOUNT, RELEASE_MS);
        }
        let mut out = Vec::with_capacity(input.len());
        let mut trace: Trace = Vec::new();
        // Live coefficients captured at the biggest offset (hit) and the
        // smallest (deepest tail), after the reference has centred.
        let mut hit_corner = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
        let mut tail_corner = hit_corner;
        let (mut best_hi, mut best_lo) = (f64::NEG_INFINITY, f64::INFINITY);
        let settle_s = 2.5;
        for (h, chunk) in input.chunks(ENV_HOP).enumerate() {
            let mut l = chunk.to_vec();
            let mut r = chunk.to_vec();
            eng.process_block(&mut l, &mut r, MORPH, Q);
            let t = (h * ENV_HOP) as f64 / SR;
            let m = eng.morph_position();
            trace.push((t, eng.env_level(), eng.env_reference(), eng.env_offset(), m));
            if t > settle_s {
                if m > best_hi {
                    best_hi = m;
                    hit_corner = live_corner(&eng);
                }
                if m < best_lo {
                    best_lo = m;
                    tail_corner = live_corner(&eng);
                }
            }
            out.extend_from_slice(&l);
        }
        (out, trace, hit_corner, tail_corner)
    };

    let (on_out, on_trace, hit_corner, tail_corner) = render(true);
    let (off_out, off_trace, _, _) = render(false);

    // --- the body's own morph travel, for reference ---
    let cart = Cartridge::from_body_bytes(name, &bytes, 1.0).expect("load body");
    println!("body: {name} ({} bytes) @ {SR} Hz", bytes.len());
    println!("parked at MORPH {MORPH:.2} / Q {Q:.2}, ENV amount {AMOUNT}, release {RELEASE_MS} ms");
    println!("\nbody morph travel at Q {Q:.2} (dB, packed runtime):");
    print_response_row("  morph 0.00", &cart.interpolate(0.0, Q, 0.0));
    print_response_row("  morph 0.50", &cart.interpolate(0.5, Q, 0.0));
    print_response_row("  morph 1.00", &cart.interpolate(1.0, Q, 0.0));

    // --- (a) the follower pops and relaxes ---
    println!("\n(a) follower per hit (after the 2 s reference has centred):");
    println!("   hit   t_hit    level@pop    ref     offset@pop   offset@tail   morph pop->tail");
    for h in 0..HITS {
        let t0 = h as f64 * HIT_PERIOD_S;
        let t1 = t0 + HIT_PERIOD_S;
        let win: Vec<&(f64, f64, f64, f64, f64)> =
            on_trace.iter().filter(|x| x.0 >= t0 && x.0 < t1).collect();
        if win.is_empty() {
            continue;
        }
        let pop = win.iter().max_by(|a, b| a.3.total_cmp(&b.3)).unwrap();
        let tail = win.last().unwrap();
        let m_pop = win.iter().map(|x| x.4).fold(f64::MIN, f64::max);
        println!(
            "   {:>3}  {:>6.3}s  {:>8.3}  {:>8.3}  {:>10.3}  {:>11.3}   {:.3} -> {:.3}",
            h + 1,
            t0,
            pop.1,
            pop.2,
            pop.3,
            tail.3,
            m_pop,
            tail.4
        );
    }

    // --- (b) the law ---
    let mut worst_law = 0.0f64;
    let mut clamped = 0usize;
    for x in &on_trace {
        let raw = (x.1 - x.2) * AMOUNT as f64;
        let expect = raw.clamp(-1.0, 1.0);
        worst_law = worst_law.max((x.3 - expect).abs());
        if raw.abs() > 1.0 {
            clamped += 1;
        }
    }
    let max_off = on_trace.iter().map(|x| x.3).fold(f64::MIN, f64::max);
    let min_off = on_trace.iter().map(|x| x.3).fold(f64::MAX, f64::min);
    let off_moved = off_trace.iter().any(|x| x.3 != 0.0 || x.4 != MORPH);
    println!(
        "\n(b) law |offset - clamp((level-ref)*amount, +-1)| max = {worst_law:.3e} over {} control blocks",
        on_trace.len()
    );
    println!("    offset range {min_off:+.3} .. {max_off:+.3}  ({clamped} blocks hit the +-1 clamp)");
    println!("    ENV off: offset always 0 and morph pinned at {MORPH:.2}: {}", !off_moved);

    // --- (c) the response actually moved (live coefficients) ---
    let mut worst_hz = 0.0;
    let mut worst_d = 0.0f64;
    for hz in PROBE_HZ {
        let d = mag_db(&hit_corner, hz) - mag_db(&tail_corner, hz);
        if d.abs() > worst_d.abs() {
            worst_d = d;
            worst_hz = hz;
        }
    }
    println!("\n(c) LIVE cascade coefficients, hit vs tail (the audio path's own filter):");
    print_response_row("    at hit ", &hit_corner);
    print_response_row("    at tail", &tail_corner);
    print!("    delta  ");
    for hz in PROBE_HZ {
        print!(
            " {:>7.1}",
            mag_db(&hit_corner, hz) - mag_db(&tail_corner, hz)
        );
    }
    println!("\n    biggest move: {worst_d:+.1} dB at {worst_hz:.0} Hz");
    let (mut sweep_hz, mut sweep_d) = (0.0f64, 0.0f64);
    for bin in 1..=512 {
        let hz = 20.0 * (24_000.0f64 / 20.0).powf(bin as f64 / 512.0);
        let d = mag_db(&hit_corner, hz) - mag_db(&tail_corner, hz);
        if d.abs() > sweep_d.abs() {
            sweep_d = d;
            sweep_hz = hz;
        }
    }
    println!("    dense sweep max |delta| = {sweep_d:+.1} dB at {sweep_hz:.0} Hz");

    // --- (d) the audio itself moved ---
    // Averaged over the settled hits (5..8), at each probe frequency: what the
    // input has to offer there, and what came out with ENV on vs off.
    println!("\n(d) rendered audio over the settled hit windows, ENV on vs off (dB):");
    println!("        Hz     input       on      off    delta");
    for hz in PROBE_HZ {
        let (mut gi, mut ga, mut gb) = (0.0, 0.0, 0.0);
        let mut n = 0.0;
        for h in 4..HITS {
            let s0 = (h as f64 * HIT_PERIOD_S * SR) as usize;
            let s1 = (s0 + (0.12 * SR) as usize).min(on_out.len());
            if s0 >= s1 {
                continue;
            }
            gi += db(goertzel(&input[s0..s1], hz));
            ga += db(goertzel(&on_out[s0..s1], hz));
            gb += db(goertzel(&off_out[s0..s1], hz));
            n += 1.0;
        }
        println!(
            "  {hz:>8.0}  {:>8.2} {:>8.2} {:>8.2}  {:>+7.2}",
            gi / n,
            ga / n,
            gb / n,
            (ga - gb) / n
        );
    }
    println!("    (sweep peak {sweep_hz:.0} Hz carries no drum energy — the audible move is where the material is)");
    let rms_on = rms(&on_out);
    let rms_off = rms(&off_out);
    println!("    full-render RMS: on {rms_on:.5}  off {rms_off:.5}");

    write_wav_f32(&format!("{prefix}_in.wav"), &input, SR as u32);
    write_wav_f32(&format!("{prefix}_on.wav"), &on_out, SR as u32);
    write_wav_f32(&format!("{prefix}_off.wav"), &off_out, SR as u32);
    let mut csv = std::fs::File::create(format!("{prefix}_trace.csv")).expect("csv");
    writeln!(csv, "t_sec,level,reference,offset,morph_on,morph_off").unwrap();
    for (on, off) in on_trace.iter().zip(off_trace.iter()) {
        writeln!(
            csv,
            "{:.5},{:.4},{:.4},{:.4},{:.4},{:.4}",
            on.0, on.1, on.2, on.3, on.4, off.4
        )
        .unwrap();
    }
    println!("\nwrote {prefix}_in.wav {prefix}_on.wav {prefix}_off.wav {prefix}_trace.csv");
}

fn live_corner(eng: &FilterEngine) -> CornerData {
    let mut c = [[0.0f32; NUM_COEFFS]; NUM_STAGES];
    let mut boost = 0.0f32;
    eng.get_coeffs_for_ui(&mut c, &mut boost);
    let mut out = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
    for (i, stage) in c.iter().enumerate() {
        for (j, &v) in stage.iter().enumerate() {
            out[i][j] = f64::from(v);
        }
    }
    out
}

fn mag_db(corner: &CornerData, hz: f64) -> f64 {
    trench_core::response::biquad_cascade_mag_db(corner, hz, SR)
}

fn print_response_row(label: &str, corner: &CornerData) {
    print!("{label}");
    for hz in PROBE_HZ {
        print!(" {:>7.1}", mag_db(corner, hz));
    }
    println!();
}

/// A kick/burst train: hard transient, fast body decay, quiet tail between —
/// exactly the material the ENV macro is built for.
fn drum_pops() -> Vec<f32> {
    let n = (HITS as f64 * HIT_PERIOD_S * SR) as usize;
    let mut out = vec![0.0f32; n];
    let mut state = 1u64;
    let mut noise = || {
        state = state.wrapping_mul(48_271) % 2_147_483_647;
        state as f64 / 1_073_741_823.5 - 1.0
    };
    for i in 0..n {
        let t = i as f64 / SR;
        let phase = t % HIT_PERIOD_S;
        // Body: a 65 Hz thump decaying in ~90 ms. Click: 4 ms of noise.
        let body = (core::f64::consts::TAU * 65.0 * phase).sin() * (-phase / 0.09).exp();
        let click = if phase < 0.004 {
            noise() * (1.0 - phase / 0.004)
        } else {
            0.0
        };
        // Quiet tail: a -32 dBFS bed so the follower has something to relax to.
        let bed = 0.025 * (core::f64::consts::TAU * 180.0 * t).sin() + 0.008 * noise();
        out[i] = ((body * 0.8 + click * 0.5) + bed).clamp(-1.0, 1.0) as f32;
    }
    out
}

fn goertzel(x: &[f32], hz: f64) -> f64 {
    let w = core::f64::consts::TAU * hz / SR;
    let coeff = 2.0 * w.cos();
    let (mut s1, mut s2) = (0.0f64, 0.0f64);
    for &v in x {
        let s0 = v as f64 + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    (s1 * s1 + s2 * s2 - coeff * s1 * s2).sqrt() / (x.len() as f64 / 2.0)
}

fn db(mag: f64) -> f64 {
    20.0 * mag.max(1.0e-12).log10()
}

fn rms(x: &[f32]) -> f64 {
    (x.iter().map(|&v| f64::from(v) * f64::from(v)).sum::<f64>() / x.len().max(1) as f64).sqrt()
}

fn write_wav_f32(path: &str, data: &[f32], fs: u32) {
    let mut f = std::fs::File::create(path).expect("create wav");
    let n = data.len() as u32;
    let byte_rate = fs * 4;
    let data_bytes = n * 4;
    let mut h = Vec::new();
    h.extend_from_slice(b"RIFF");
    h.extend_from_slice(&(36 + data_bytes).to_le_bytes());
    h.extend_from_slice(b"WAVE");
    h.extend_from_slice(b"fmt ");
    h.extend_from_slice(&16u32.to_le_bytes());
    h.extend_from_slice(&3u16.to_le_bytes());
    h.extend_from_slice(&1u16.to_le_bytes());
    h.extend_from_slice(&fs.to_le_bytes());
    h.extend_from_slice(&byte_rate.to_le_bytes());
    h.extend_from_slice(&4u16.to_le_bytes());
    h.extend_from_slice(&32u16.to_le_bytes());
    h.extend_from_slice(b"data");
    h.extend_from_slice(&data_bytes.to_le_bytes());
    f.write_all(&h).unwrap();
    for &s in data {
        f.write_all(&s.to_le_bytes()).unwrap();
    }
}
