//! The BLOOM proof: a drum-pop train through a real body at the true rate,
//! with ONLY the PREAMP destination armed (ENV->MORPH amount 0).
//!
//! Evidence printed:
//!   (a) the desk drive tracks the follower per control block,
//!   (b) the saturation really changes — odd-harmonic distortion measured at
//!       the hit and in the tail, bloom on vs off, on identical input through
//!       an identical (parked, linear) cascade,
//!   (c) bloom_amount 0 renders BIT-IDENTICAL to bloom never armed.
//!
//! Usage: bloom-probe <body.body240> [out_prefix]

use std::io::Write;
use trench_core::engine::{FilterEngine, InputMode};
use trench_core::env::ENV_HOP;
use trench_core::Cartridge;

const SR: f64 = 48_000.0;
const MORPH: f64 = 0.35;
const Q: f64 = 0.75;
/// The PREAMP knob position the user parked. BLOOM offsets from here.
const BASE_PREAMP: f32 = 0.30;
const BLOOM: f32 = 0.50;
const HITS: usize = 8;
const HIT_PERIOD_S: f64 = 0.6;
/// Drum tone: 8.333 ms period, so every measurement window is whole cycles.
const F0: f64 = 120.0;
/// Hit peak. The desk multiplies by (1 + drive*99), so this parks the material
/// on the knee of `trench_saturate` at the base drive — where a drive change
/// is audible rather than already fully clipped.
const HIT_PEAK: f64 = 0.030;
/// Quiet tail bed, 30 dB down: what the follower relaxes to.
const TAIL_PEAK: f64 = 0.001;

fn main() {
    let body_path = std::env::args()
        .nth(1)
        .expect("usage: bloom-probe <body.body240> [prefix]");
    let prefix = std::env::args().nth(2).unwrap_or_else(|| "bloom".to_string());
    let bytes = std::fs::read(&body_path).expect("read body");
    let name = std::path::Path::new(&body_path)
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("body");
    let input = drum_pops();

    // (t, level, reference, bloom_offset, preamp_drive)
    type Trace = Vec<(f64, f64, f64, f64, f64)>;
    let render = |bloom: Option<f32>| -> (Vec<f32>, Trace) {
        let mut eng = FilterEngine::new();
        eng.prepare(SR);
        // Level machinery off: the only thing allowed to move is the desk.
        eng.debug.agc_enabled = false;
        eng.debug.dc_block_enabled = false;
        eng.debug.spatial_enabled = false;
        let cart = Cartridge::from_body_bytes(name, &bytes, 1.0).expect("load body");
        eng.load_cartridge(cart);
        eng.set_input_mode(InputMode::MackieDeskSlam);
        eng.set_input_preamp(BASE_PREAMP);
        eng.set_env(0.0, 200.0); // ENV->MORPH OFF. Bloom alone.
        if let Some(b) = bloom {
            eng.set_bloom(b);
        }
        let mut out = Vec::with_capacity(input.len());
        let mut trace: Trace = Vec::new();
        for (h, chunk) in input.chunks(ENV_HOP).enumerate() {
            let mut l = chunk.to_vec();
            let mut r = chunk.to_vec();
            eng.process_block(&mut l, &mut r, MORPH, Q);
            trace.push((
                (h * ENV_HOP) as f64 / SR,
                eng.env_level(),
                eng.env_reference(),
                eng.bloom_offset(),
                f64::from(eng.preamp_drive()),
            ));
            out.extend_from_slice(&l);
        }
        (out, trace)
    };

    let (on_out, on_trace) = render(Some(BLOOM));
    let (zero_out, zero_trace) = render(Some(0.0));
    let (never_out, _) = render(None);

    println!("body: {name} ({} bytes) @ {SR} Hz", bytes.len());
    println!(
        "parked MORPH {MORPH:.2} / Q {Q:.2}; PREAMP base {BASE_PREAMP}; \
         ENV->MORPH amount 0.0; BLOOM {BLOOM}"
    );
    println!("material: {HITS} hits, {F0:.0} Hz, peak {HIT_PEAK} / tail bed {TAIL_PEAK} (-30 dB)");

    // --- (a) the drive tracks the follower ---
    println!("\n(a) desk drive per hit (base {BASE_PREAMP}), bloom on:");
    println!("   hit   t_hit    level    ref    bloom_off   drive@hit   drive@tail   drive off");
    for h in 0..HITS {
        let t0 = h as f64 * HIT_PERIOD_S;
        let t1 = t0 + HIT_PERIOD_S;
        let win: Vec<&(f64, f64, f64, f64, f64)> =
            on_trace.iter().filter(|x| x.0 >= t0 && x.0 < t1).collect();
        if win.is_empty() {
            continue;
        }
        let pop = win.iter().max_by(|a, b| a.4.total_cmp(&b.4)).unwrap();
        let tail = win.last().unwrap();
        let off_tail = zero_trace
            .iter()
            .filter(|x| x.0 >= t0 && x.0 < t1)
            .last()
            .unwrap();
        println!(
            "   {:>3}  {:>6.3}s {:>7.3} {:>6.3}  {:>+9.3}  {:>10.4}  {:>10.4}  {:>9.4}",
            h + 1,
            t0,
            pop.1,
            pop.2,
            pop.3,
            pop.4,
            tail.4,
            off_tail.4
        );
    }
    let d_hi = on_trace.iter().map(|x| x.4).fold(f64::MIN, f64::max);
    let d_lo = on_trace.iter().map(|x| x.4).fold(f64::MAX, f64::min);
    println!("    drive range {d_lo:.4} .. {d_hi:.4}   (bloom off: pinned at {BASE_PREAMP})");
    let law = on_trace
        .iter()
        .map(|x| (x.3 - (x.1 - x.2) * BLOOM as f64).abs())
        .fold(0.0f64, f64::max);
    println!("    law |bloom_offset - (level-ref)*bloom| max = {law:.3e} over {} blocks", on_trace.len());
    println!("    corr(level, drive) = {:.3}", corr(&on_trace));
    println!(
        "    (the drive reaches the offset through the PREAMP de-zip — 5 ms — so it\n     \
         lags the follower by design, the same lag a hand on the knob gets)"
    );

    // --- (b) the saturation actually changed ---
    // Identical input, identical parked linear cascade: any harmonic change is
    // the desk. Windows are whole cycles of F0.
    println!("\n(b) odd-harmonic distortion, bloom on vs off (dB re fundamental):");
    println!("        window          drive in window      h1      h3      h5      h7    THD%");
    for (label, off_s, len_s) in [
        ("hit  (5-55ms)", 0.005, 0.050),
        ("tail (450-500ms)", 0.450, 0.050),
    ] {
        for (tag, out, trace) in [("bloom on ", &on_out, &on_trace), ("bloom off", &zero_out, &zero_trace)]
        {
            let (mut h1, mut h3, mut h5, mut h7, mut thd, mut n) = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            let (mut d_lo, mut d_hi) = (f64::MAX, f64::MIN);
            for h in 4..HITS {
                let t0 = h as f64 * HIT_PERIOD_S + off_s;
                let s0 = (t0 * SR) as usize;
                let s1 = (s0 + (len_s * SR) as usize).min(out.len());
                if s0 >= s1 {
                    continue;
                }
                let seg = &out[s0..s1];
                let f = goertzel(seg, F0).max(1.0e-12);
                h1 += db(f);
                h3 += db(goertzel(seg, F0 * 3.0) / f);
                h5 += db(goertzel(seg, F0 * 5.0) / f);
                h7 += db(goertzel(seg, F0 * 7.0) / f);
                let mut sum = 0.0;
                for k in [3.0, 5.0, 7.0, 9.0, 11.0] {
                    let m = goertzel(seg, F0 * k);
                    sum += m * m;
                }
                thd += 100.0 * sum.sqrt() / f;
                for x in trace.iter().filter(|x| x.0 >= t0 && x.0 < t0 + len_s) {
                    d_lo = d_lo.min(x.4);
                    d_hi = d_hi.max(x.4);
                }
                n += 1.0;
            }
            println!(
                "  {label:<16} {tag}  {:>6.4}..{:<6.4} {:>7.2} {:>7.2} {:>7.2} {:>7.2} {:>7.2}",
                d_lo,
                d_hi,
                h1 / n,
                h3 / n,
                h5 / n,
                h7 / n,
                thd / n
            );
        }
    }
    println!("    (input and cascade are identical between the two renders — the delta is the desk)");
    let rms_on = rms(&on_out);
    let rms_off = rms(&zero_out);
    println!(
        "    full-render RMS: bloom on {rms_on:.6}  bloom off {rms_off:.6}  ({:+.2} dB)",
        20.0 * (rms_on / rms_off).log10()
    );

    // --- (c) bloom 0 is an exact bypass ---
    let identical = zero_out == never_out;
    let bytes_identical = zero_out
        .iter()
        .zip(never_out.iter())
        .all(|(a, b)| a.to_bits() == b.to_bits());
    println!("\n(c) bloom_amount 0 vs bloom never armed:");
    println!("    samples equal: {identical}   bit-identical: {bytes_identical}   ({} samples)", zero_out.len());
    println!(
        "    bloom on differs from bloom off: {}",
        on_out.iter().zip(zero_out.iter()).any(|(a, b)| a != b)
    );

    write_wav_f32(&format!("{prefix}_in.wav"), &input, SR as u32);
    write_wav_f32(&format!("{prefix}_on.wav"), &on_out, SR as u32);
    write_wav_f32(&format!("{prefix}_off.wav"), &zero_out, SR as u32);
    let mut csv = std::fs::File::create(format!("{prefix}_trace.csv")).expect("csv");
    writeln!(csv, "t_sec,level,reference,bloom_offset,drive_on,drive_off").unwrap();
    for (on, off) in on_trace.iter().zip(zero_trace.iter()) {
        writeln!(csv, "{:.5},{:.4},{:.4},{:.4},{:.5},{:.5}", on.0, on.1, on.2, on.3, on.4, off.4).unwrap();
    }
    println!("\nwrote {prefix}_in.wav {prefix}_on.wav {prefix}_off.wav {prefix}_trace.csv");
}

/// Hits with quiet tails: a flat-topped 120 Hz drum tone so every measurement
/// window is whole cycles, then a fast fall into a -30 dB bed.
fn drum_pops() -> Vec<f32> {
    let n = (HITS as f64 * HIT_PERIOD_S * SR) as usize;
    let mut out = vec![0.0f32; n];
    for (i, s) in out.iter_mut().enumerate() {
        let t = i as f64 / SR;
        let phase = t % HIT_PERIOD_S;
        let env = if phase < 0.001 {
            TAIL_PEAK + (HIT_PEAK - TAIL_PEAK) * (phase / 0.001)
        } else if phase < 0.060 {
            HIT_PEAK
        } else {
            TAIL_PEAK + (HIT_PEAK - TAIL_PEAK) * (-(phase - 0.060) / 0.010).exp()
        };
        *s = (env * (core::f64::consts::TAU * F0 * t).sin()) as f32;
    }
    out
}

fn corr(trace: &[(f64, f64, f64, f64, f64)]) -> f64 {
    let n = trace.len() as f64;
    let mx = trace.iter().map(|x| x.1).sum::<f64>() / n;
    let my = trace.iter().map(|x| x.4).sum::<f64>() / n;
    let (mut sxy, mut sxx, mut syy) = (0.0, 0.0, 0.0);
    for x in trace {
        let (a, b) = (x.1 - mx, x.4 - my);
        sxy += a * b;
        sxx += a * a;
        syy += b * b;
    }
    sxy / (sxx * syy).sqrt().max(1.0e-30)
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
