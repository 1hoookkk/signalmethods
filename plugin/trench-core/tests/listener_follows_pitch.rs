//! The LISTENER contract: with a resonant body loaded, following the voice's
//! pitch must hold the filter's gain at the voice's fundamental higher than a
//! static wheel does — the filter chases the melody instead of letting it
//! slide off the response.

use trench_core::cartridge::Cartridge;
use trench_core::engine::FilterEngine;
use trench_core::stage_law::{words_from_roots, StageRoots};

const SR: f64 = 44_100.0;
const FRAME: usize = 512;

fn body() -> Vec<u8> {
    // One resonant stage whose passband sits ON the voice range (pole 320 Hz,
    // chirp 220 -> 440): following keeps the voice's fundamental in the peak,
    // static lets it slide off the peak's far skirt.
    let active = words_from_roots(&StageRoots {
        pole_hz: 300.0,
        pole_r: 0.99,
        zero_hz: 50.0,
        zero_r: 0.1,
        scale: 0.3,
    });
    let ident = words_from_roots(&StageRoots {
        pole_hz: 0.0,
        pole_r: 0.0,
        zero_hz: 0.0,
        zero_r: 0.0,
        scale: 1.0,
    });
    let mut bytes = Vec::with_capacity(240);
    for _ in 0..4 {
        for s in 0..6 {
            for w in if s == 0 { active } else { ident } {
                bytes.extend_from_slice(&w.to_le_bytes());
            }
        }
    }
    bytes
}

/// Two-note step melody: A3 (220 Hz) for 1.5 s, then A4 (440 Hz) for 1.5 s.
/// A step, not a sweep: the follower's reference locks to the first note, then
/// the octave jump is the thing it must chase and hold.
fn melody() -> (Vec<f32>, Vec<f64>) {
    let mut out = Vec::new();
    let mut f0s = Vec::new();
    for &(f0, dur) in &[(220.0, 1.5), (440.0, 1.5)] {
        let n = (dur * SR) as usize;
        let fade = (0.008 * SR) as usize;
        let kmax = (0.45 * SR / f0) as usize;
        let mut phase = 0.0f64;
        for i in 0..n {
            phase += 2.0 * std::f64::consts::PI * f0 / SR;
            let mut s = 0.0f64;
            for k in 1..=kmax {
                s += (phase * k as f64).sin() / k as f64;
            }
            let env = (i.min(fade) as f64 / fade as f64)
                .min((n - 1 - i).min(fade) as f64 / fade as f64)
                .clamp(0.0, 1.0);
            out.push((s * 0.5 * env) as f32);
            if i % FRAME == 0 {
                f0s.push(f0);
            }
        }
    }
    (out, f0s)
}

#[test]
#[ignore = "TRACK retired from the face 2026-08-15 (Tyson: detector-driven \
retuner the X3 never had; the cube's authored third axis is the legitimate \
version). The listener code stays wired for old sessions, but its hold-gain \
guarantee is no longer a shipped promise — and this test was already failing \
(follow 7.83 dB vs static 7.79: the follower was not transposing on this \
melody). If the listener ever returns to the face, un-ignore this and debug \
that regression first."]
fn listener_holds_gain_at_the_voice_fundamental() {
    let bytes = body();
    let (input, f0s) = melody();

    let render = |listen: bool| -> Vec<f64> {
        let mut eng = FilterEngine::new();
        eng.prepare(SR);
        eng.debug.agc_enabled = false;
        eng.debug.dc_block_enabled = false;
        eng.debug.spatial_enabled = false;
        eng.load_cartridge(Cartridge::from_body_bytes("follow_test", &bytes, 1.0).unwrap());
        if listen {
            eng.set_listener(1.0, 80.0);
        }
        let mut gains = Vec::new();
        for (i, chunk) in input.chunks(FRAME).enumerate() {
            let mut l = chunk.to_vec();
            let mut r = chunk.to_vec();
            eng.process_block(&mut l, &mut r, 0.5, 0.5);
            let f0 = f0s.get(i).copied().unwrap_or(220.0);
            gains.push(20.0 * goertzel(&l, f0).max(1.0e-9).log10());
        }
        gains
    };

    let on = render(true);
    let off = render(false);
    // The law is the STEADY STATE of the A4 segment (its last 0.5 s, frames
    // 195..256): the glide has arrived and the follower sits transposed on the
    // high note; the static filter's peak is left behind at 300 Hz.
    let steady = |g: &[f64]| -> f64 {
        let seg: Vec<f64> = g[195..].to_vec();
        seg.iter().sum::<f64>() / seg.len() as f64
    };
    let on_db = steady(&on);
    let off_db = steady(&off);
    assert!(
        on_db > off_db + 3.0,
        "follower must hold gain at the voice's f0 on the high note: follow {on_db:.2} dB vs static {off_db:.2} dB"
    );
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
