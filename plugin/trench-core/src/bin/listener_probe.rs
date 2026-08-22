//! The LISTENER proof: feed a vocal melody through a real body (TYSON_WAH and
//! friends), render with the pitch follower on vs off, and measure the filter's
//! gain at the voice's instantaneous fundamental.
//!
//! Follow should hold gain@f0 roughly constant (the filter chases the melody);
//! static should let it wander with the notes. Runtime evidence on the real
//! path — packed bytes, true sample rate.
//!
//! Usage: listener-probe <body.body240> [out_prefix]

use std::io::Write;
use trench_core::engine::FilterEngine;
use trench_core::Cartridge;

const SR: f64 = 44_100.0;
const FRAME: usize = 512; // = LISTEN_HOP: one tracker analysis per frame

fn main() {
    let body_path = std::env::args().nth(1).expect("usage: listener-probe <body.body240> [prefix]");
    let prefix = std::env::args().nth(2).unwrap_or_else(|| "listener".to_string());
    let bytes = std::fs::read(&body_path).expect("read body");
    let name = std::path::Path::new(&body_path)
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("body");
    // A vocal phrase: A3 C4 E4 G4 A4, a breath, then back down. Vibrato + a
    // harmonic-rich spectrum so the tracker sees a voice-like signal.
    let melody: [(f64, f64); 10] = [
        (220.00, 0.6), (261.63, 0.6), (329.63, 0.6), (392.00, 0.6), (440.00, 0.6),
        (0.0, 0.4),    // breath — the follower must hold
        (392.00, 0.6), (329.63, 0.6), (261.63, 0.6), (220.00, 0.6),
    ];
    let input = synth_phrase(&melody);

    let render = |listen: bool| -> (Vec<f32>, Vec<(f64, f64, f64, [f64; 6])>) {
        // (t, f0_known, f0_tracked, gains[1..=6] at k*f0) per frame
        let mut eng = FilterEngine::new();
        eng.prepare(SR);
        eng.debug.agc_enabled = false;
        eng.debug.dc_block_enabled = false;
        eng.debug.spatial_enabled = false;
        // Cap off: isolate the follow's spectral behaviour from the broadband
        // accident insurance (level-only; engages differently per transpose).
        let cart = Cartridge::from_body_bytes(name, &bytes, 1.0).expect("load body");
        eng.load_cartridge(cart);
        if listen {
            eng.set_listener(1.0, 80.0);
        }
        let mut out = Vec::with_capacity(input.len());
        let mut frames = Vec::new();
        let mut t = 0.0f64;
        for chunk in input.chunks(FRAME) {
            let mut l = chunk.to_vec();
            let mut r = chunk.to_vec();
            eng.process_block(&mut l, &mut r, 0.5, 0.5);
            let f0_known = f0_at(&melody, t);
            let f0_tracked = eng.listener_f0();
            let mut gains = [f64::NAN; 6];
            if f0_known > 0.0 {
                for k in 1..=6 {
                    gains[k - 1] = 20.0 * goertzel(&l, f0_known * k as f64).max(1.0e-9).log10();
                }
            }
            frames.push((t, f0_known, f0_tracked, gains));
            out.extend_from_slice(&l);
            t += FRAME as f64 / SR;
        }
        (out, frames)
    };

    let (on_out, on_frames) = render(true);
    let (off_out, off_frames) = render(false);

    // Tracker quality: % of voiced frames within 3% of the known f0.
    let mut locked = 0usize;
    let mut voiced_count = 0usize;
    for f in &on_frames {
        if f.1 > 0.0 && f.2 > 0.0 {
            voiced_count += 1;
            if (f.2 - f.1).abs() < 0.03 * f.1 {
                locked += 1;
            }
        }
    }
    let lock_pct = 100.0 * locked as f64 / voiced_count.max(1) as f64;

    // Metric per harmonic: mean gain at k*f0 over voiced frames, follower vs
    // static. The body's own response shape tells which harmonic it colors.
    let harm_delta = |frames: &[(f64, f64, f64, [f64; 6])]| -> Vec<f64> {
        (0..6)
            .map(|k| {
                let v: Vec<f64> = frames
                    .iter()
                    .filter(|x| x.1 > 0.0 && x.3[k].is_finite())
                    .map(|x| x.3[k])
                    .collect();
                mean(&v)
            })
            .collect()
    };
    let on_harms = harm_delta(&on_frames);
    let off_harms = harm_delta(&off_frames);

    write_wav_f32(&format!("{prefix}_in.wav"), &input, SR as u32);
    write_wav_f32(&format!("{prefix}_on.wav"), &on_out, SR as u32);
    write_wav_f32(&format!("{prefix}_off.wav"), &off_out, SR as u32);
    let mut csv = std::fs::File::create(format!("{prefix}_track.csv")).expect("csv");
    writeln!(
        csv,
        "t_sec,f0_known,f0_tracked,g1_on,g1_off,g2_on,g2_off,g3_on,g3_off,g4_on,g4_off,g5_on,g5_off,g6_on,g6_off"
    )
    .unwrap();
    for (on, off) in on_frames.iter().zip(off_frames.iter()) {
        writeln!(
            csv,
            "{:.4},{:.2},{:.2},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3}",
            on.0,
            on.1,
            on.2,
            on.3[0], off.3[0],
            on.3[1], off.3[1],
            on.3[2], off.3[2],
            on.3[3], off.3[3],
            on.3[4], off.3[4],
            on.3[5], off.3[5],
        )
        .unwrap();
    }

    let cart = Cartridge::from_body_bytes(name, &bytes, 1.0).expect("load body");
    let corner = cart.interpolate(0.5, 0.5, 0.0);
    let resp = |hz: f64| trench_core::response::biquad_cascade_mag_db(&corner, hz, SR);
    println!("body: {name} ({})", bytes.len());
    println!(
        "static response at morph 0.5: 150 Hz {:.1} dB, 300 {:.1}, 600 {:.1}, 1.2k {:.1}, 2.4k {:.1}, 4.8k {:.1}",
        resp(150.0), resp(300.0), resp(600.0), resp(1200.0), resp(2400.0), resp(4800.0)
    );
    println!("tracker locked within 3% of the known f0 in {lock_pct:.1}% of voiced frames");
    println!("mean gain at k*f0 (follow / static / delta dB):");
    for k in 1..=6 {
        let d = on_harms[k - 1] - off_harms[k - 1];
        println!(
            "  {}x  {:+.2} / {:+.2} / {:+.2}",
            k, on_harms[k - 1], off_harms[k - 1], d
        );
    }
    println!("wrote {prefix}_in.wav {prefix}_on.wav {prefix}_off.wav {prefix}_track.csv");
}

fn synth_phrase(notes: &[(f64, f64)]) -> Vec<f32> {
    let mut out = Vec::new();
    for &(f0, dur) in notes {
        if f0 <= 0.0 {
            out.resize(out.len() + (dur * SR) as usize, 0.0);
            continue;
        }
        let n = (dur * SR) as usize;
        let fade = (0.008 * SR) as usize;
        let kmax = (0.45 * SR / f0) as usize;
        let mut phase = 0.0f64;
        for i in 0..n {
            let t = i as f64 / SR;
            // 5 Hz vibrato, ±0.35%, and harmonic rolloff 1/k — voice-like.
            let f = f0 * (1.0 + 0.0035 * (2.0 * std::f64::consts::PI * 5.0 * t).sin());
            phase += 2.0 * std::f64::consts::PI * f / SR;
            let mut s = 0.0f64;
            for k in 1..=kmax {
                s += (phase * k as f64).sin() / k as f64;
            }
            let env = (i.min(fade) as f64 / fade as f64)
                .min((n - 1 - i).min(fade) as f64 / fade as f64)
                .clamp(0.0, 1.0);
            out.push((s * 0.45 * env) as f32);
        }
    }
    out
}

fn f0_at(notes: &[(f64, f64)], t: f64) -> f64 {
    let mut acc = 0.0;
    for &(f0, dur) in notes {
        if t < acc + dur {
            return f0;
        }
        acc += dur;
    }
    0.0
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

fn mean(x: &[f64]) -> f64 {
    if x.is_empty() {
        return 0.0;
    }
    x.iter().sum::<f64>() / x.len() as f64
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
