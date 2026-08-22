//! BITE contract: drive 0 is BIT-EXACT the linear cascade; drive > 0 generates
//! harmonics between the stages (and only then).

use trench_core::cartridge::Cartridge;
use trench_core::engine::FilterEngine;
use trench_core::stage_law::{words_from_roots, StageRoots, DEFAULT_AUTHORING_SR};

fn test_body() -> Vec<u8> {
    // a resonant stage so the junction actually sees a hot peak
    let active = words_from_roots(&StageRoots {
        pole_hz: 1000.0,
        pole_r: 0.98,
        zero_hz: 250.0,
        zero_r: 0.5,
        scale: 0.1,
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

fn prepared_engine() -> FilterEngine {
    let mut eng = FilterEngine::new();
    eng.prepare(DEFAULT_AUTHORING_SR);
    // boost is a LINEAR output gain (0.0 = a silent body). The old 0.0 only
    // "worked" because the pre-2026-07-30 code glided output gain toward zero
    // slower than the render window; the snap-on-load path applies it honestly.
    let cart = Cartridge::from_body_bytes("bite_test", &test_body(), 1.0).expect("cart");
    eng.load_cartridge(cart);
    eng
}

fn render_sine(eng: &mut FilterEngine, hz: f64, blocks: usize, morph: f64) -> Vec<f32> {
    let mut out = Vec::new();
    let mut phase = 0.0f64;
    for _ in 0..blocks {
        let mut l = [0.0f32; 64];
        let mut r = [0.0f32; 64];
        for i in 0..64 {
            phase += core::f64::consts::TAU * hz / DEFAULT_AUTHORING_SR;
            l[i] = (phase.sin() * 0.5) as f32;
            r[i] = l[i];
        }
        eng.process_block(&mut l, &mut r, morph, 0.5);
        out.extend_from_slice(&l);
    }
    out
}

#[test]
fn drive_zero_is_bit_exact() {
    let mut a = prepared_engine();
    let mut b = prepared_engine();
    b.set_grit(0.0);
    assert_eq!(
        render_sine(&mut a, 987.0, 60, 0.5),
        render_sine(&mut b, 987.0, 60, 0.5),
        "drive 0 must be bit-exact the linear cascade"
    );
}

fn goertzel(x: &[f32], hz: f64) -> f64 {
    let w = core::f64::consts::TAU * hz / DEFAULT_AUTHORING_SR;
    let coeff = 2.0 * w.cos();
    let (mut s1, mut s2) = (0.0f64, 0.0f64);
    for &v in x {
        let s0 = v as f64 + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    (s1 * s1 + s2 * s2 - coeff * s1 * s2).sqrt() / (x.len() as f64 / 2.0)
}

#[test]
fn drive_generates_harmonics() {
    let f0 = 987.0;
    let mut lin = prepared_engine();
    let clean = render_sine(&mut lin, f0, 200, 0.5);
    let mut hot = prepared_engine();
    hot.set_grit(0.8);
    let driven = render_sine(&mut hot, f0, 200, 0.5);

    let tail = &clean[clean.len() - 4096..];
    let tail_hot = &driven[driven.len() - 4096..];
    let h1 = goertzel(tail_hot, f0).max(1e-12);
    let h3_clean = goertzel(tail, 3.0 * f0) / goertzel(tail, f0).max(1e-12);
    let h3_hot = goertzel(tail_hot, 3.0 * f0) / h1;
    let clean_db = 20.0 * h3_clean.max(1e-12).log10();
    let hot_db = 20.0 * h3_hot.max(1e-12).log10();
    // 20 dB was authored against the pre-2026-08-06 pole-distortion path; the
    // state-clipping mechanism that replaced it raises H3 by ~19 dB on this
    // stimulus. The tripwire is that GRIT still generates real harmonics, so
    // the margin is 15 — far above measurement noise, below the old law.
    assert!(
        hot_db > clean_db + 15.0,
        "drive 0.8 should raise H3 well above the linear floor: clean {clean_db:.1} dBc, hot {hot_db:.1} dBc"
    );
}

#[test]
fn grit_scales_the_one_mechanism() {
    // BITE is GRIT, and since 2026-08-06 state clipping replaced the separate
    // pole-distortion path: interstage drive and pole distortion are the same
    // knob. The tripwire is that GRIT still does something, monotonically.
    // morph 0.75 focuses the chew cycle on stage 0 - the test body's only
    // resonant stage, so the state clipper has something to push.
    let render = |g: f32| {
        let mut eng = prepared_engine();
        eng.set_grit(g);
        render_sine(&mut eng, 987.0, 120, 0.75)
    };
    let (a, b, c) = (render(0.0), render(0.5), render(0.9));

    let delta = |x: &[f32], y: &[f32]| {
        x.iter()
            .zip(y.iter())
            .map(|(&p, &q)| (p - q).abs())
            .fold(0.0f32, f32::max)
    };
    assert!(
        delta(&b, &a) > 1e-3,
        "GRIT 0.5 must differ from GRIT 0 (max delta {:.5})",
        delta(&b, &a)
    );
    assert!(
        delta(&c, &b) > 1e-3,
        "GRIT 0.9 must differ from GRIT 0.5 (max delta {:.5})",
        delta(&c, &b)
    );
    assert!(
        delta(&c, &a) > delta(&b, &a),
        "more GRIT must move further from linear: 0.9 {:.5} vs 0.5 {:.5}",
        delta(&c, &a),
        delta(&b, &a)
    );
}

#[test]
fn aliasing_probe_5k_drive08() {
    // 5 kHz sine, drive 0.8: harmonics 10k/15k live below Nyquist (19531);
    // 20k/25k/30k fold to 19062.5 / 14062.5 / 9062.5. Report dBc honestly.
    let mut eng = prepared_engine();
    eng.set_grit(0.8);
    let out = render_sine(&mut eng, 5000.0, 400, 0.5);
    let tail = &out[out.len() - 8192..];
    let h1 = goertzel(tail, 5000.0).max(1e-12);
    for (label, hz) in [
        ("H2 10k", 10000.0),
        ("H3 15k", 15000.0),
        ("fold H4 -> 19062.5", 19062.5),
        ("fold H5 -> 14062.5", 14062.5),
        ("fold H6 -> 9062.5", 9062.5),
    ] {
        let dbc = 20.0 * (goertzel(tail, hz) / h1).max(1e-12).log10();
        println!("{label}: {dbc:6.1} dBc");
    }
    println!("(gate: report only — the ear and the HD island own the verdict)");
}
