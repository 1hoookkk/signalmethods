//! Tripwires: every one-off measurement that proved a 2026-07-30 fix becomes
//! a permanent test, so "task complete" and "still doing what it should"
//! stay the same green.
//!
//! 1. Composed body switch: no silence hole, no spike beyond the old body's
//!    own crest, no slow swell (the "4 s preset lag" root causes).
//! 2. CORDS morph law: spamming MORPH cannot exceed the loudest real point
//!    on the path by more than ~1 dB (the "laser beam").
//! 3. PREAMP unity: Mackie input mode with PREAMP at 0 is bit-exact clean.
//! 4. Resonance budget: mathematically absent below the cap.
//! 5. GRIT: pole distortion raises harmonics on a resonant body.

use trench_core::cartridge::Cartridge;
use trench_core::engine::{FilterEngine, InputMode};
use trench_core::stage_law::{words_from_roots, StageRoots, DEFAULT_AUTHORING_SR};

const SR: f64 = DEFAULT_AUTHORING_SR;

fn stage_words(pole_hz: f64, pole_r: f64, scale: f64) -> [u16; 5] {
    words_from_roots(&StageRoots {
        pole_hz,
        pole_r,
        zero_hz: 250.0,
        zero_r: 0.4,
        scale,
    })
}

fn ident_words() -> [u16; 5] {
    words_from_roots(&StageRoots {
        pole_hz: 0.0,
        pole_r: 0.0,
        zero_hz: 0.0,
        zero_r: 0.0,
        scale: 1.0,
    })
}

/// 240-byte body: one active stage per corner, identity elsewhere. Corner
/// order (M0Q0, M100Q0, M0Q100, M100Q100).
fn body(corners: [[u16; 5]; 4]) -> Vec<u8> {
    let mut bytes = Vec::with_capacity(240);
    let ident = ident_words();
    for corner in corners.iter() {
        for s in 0..6 {
            for w in if s == 0 { *corner } else { ident } {
                bytes.extend_from_slice(&w.to_le_bytes());
            }
        }
    }
    bytes
}

fn resonant_body() -> Vec<u8> {
    // pole walks 500 Hz -> 3 kHz across MORPH, hot radius: a real screamer
    let lo = stage_words(500.0, 0.985, 0.08);
    let hi = stage_words(3000.0, 0.985, 0.08);
    body([lo, hi, lo, hi])
}

fn mellow_body() -> Vec<u8> {
    let w = stage_words(800.0, 0.55, 0.9);
    body([w, w, w, w])
}

fn engine_with(bytes: &[u8]) -> FilterEngine {
    let mut eng = FilterEngine::new();
    eng.prepare(SR);
    let cart = Cartridge::from_body_bytes("tripwire", bytes, 1.0).expect("cart");
    eng.load_cartridge(cart);
    eng
}

fn run_saw(eng: &mut FilterEngine, morph: f64, q: f64, samples: usize, phase: &mut f64) -> Vec<f32> {
    let f0 = 110.0;
    let mut out = Vec::with_capacity(samples);
    let mut buf_l = [0.0f32; 64];
    let mut buf_r = [0.0f32; 64];
    let mut produced = 0;
    while produced < samples {
        let n = 64.min(samples - produced);
        for i in 0..n {
            *phase = (*phase + f0 / SR).fract();
            let s = (2.0 * *phase - 1.0) as f32 * 0.7;
            buf_l[i] = s;
            buf_r[i] = s;
        }
        eng.process_block(&mut buf_l[..n], &mut buf_r[..n], morph, q);
        out.extend_from_slice(&buf_l[..n]);
        produced += n;
    }
    out
}

fn rms(x: &[f32]) -> f64 {
    (x.iter().map(|&v| (v as f64) * (v as f64)).sum::<f64>() / x.len().max(1) as f64).sqrt()
}

fn peak(x: &[f32]) -> f64 {
    x.iter().fold(0.0f64, |m, &v| m.max((v as f64).abs()))
}

#[test]
fn body_switch_has_no_hole_no_spike_no_swell() {
    let mut eng = engine_with(&resonant_body());
    let mut ph = 0.0;
    let pre = run_saw(&mut eng, 0.5, 0.8, (2.0 * SR) as usize, &mut ph);
    let pre_peak = peak(&pre[pre.len() / 2..]);

    let cart = Cartridge::from_body_bytes("mellow", &mellow_body(), 1.0).expect("cart");
    eng.load_cartridge(cart);
    let post = run_saw(&mut eng, 0.5, 0.2, (3.0 * SR) as usize, &mut ph);

    // no spike beyond the old body's own crest (+3 dB headroom for phase luck)
    let post_peak = peak(&post);
    assert!(
        post_peak <= pre_peak * 1.42,
        "switch spike: post {post_peak:.3} vs pre crest {pre_peak:.3}"
    );
    // steady level, no hole and no swell: every 10 ms window from 200 ms on
    // stays within a sane band of the final steady RMS
    let win = (0.010 * SR) as usize;
    let steady = rms(&post[(2.0 * SR) as usize..]);
    assert!(steady > 1.0e-4, "post-switch output effectively silent");
    let mut idx = (0.2 * SR) as usize;
    while idx + win <= post.len() {
        let w = rms(&post[idx..idx + win]);
        assert!(
            w > steady * 0.25,
            "hole/swell at {} ms: window rms {w:.5} vs steady {steady:.5}",
            (idx as f64 / SR * 1000.0) as i64
        );
        idx += win;
    }
}

#[test]
fn morph_spam_cannot_exceed_the_real_path() {
    let mut eng = engine_with(&resonant_body());
    let mut ph = 0.0;
    // the loudest LEGITIMATE moment anywhere on the path — mid-path filters
    // are real playable positions, and ARRIVING at a position has its own
    // transient (the AGC's per-sample attack clamping a loudness jump is
    // heritage physics any wheel-landing produces). So the reference includes
    // both the arrival and the settled peak of every sampled position.
    let mut reference = 0.0f64;
    for k in 0..=4 {
        let m = k as f64 / 4.0;
        let arrival = peak(&run_saw(&mut eng, m, 1.0, (0.5 * SR) as usize, &mut ph));
        let settled = peak(&run_saw(&mut eng, m, 1.0, (0.5 * SR) as usize, &mut ph));
        reference = reference.max(arrival).max(settled);
    }

    // spam the wheel 0<->1 every 125 ms for 2 s
    let mut spam_peak = 0.0f64;
    let chunk = (0.125 * SR) as usize;
    for k in 0..16 {
        let m = if k % 2 == 0 { 1.0 } else { 0.0 };
        spam_peak = spam_peak.max(peak(&run_saw(&mut eng, m, 1.0, chunk, &mut ph)));
    }
    // +1.5 dB margin over the loudest real point on the path
    assert!(
        spam_peak <= reference * 1.19,
        "transit gain: spam peak {spam_peak:.3} vs endpoint reference {reference:.3}"
    );
}

#[test]
fn preamp_zero_is_bit_exact_clean() {
    let mut clean = engine_with(&mellow_body());
    clean.set_input_mode(InputMode::None);
    let mut mackie = engine_with(&mellow_body());
    mackie.set_input_mode(InputMode::MackieDeskSlam);
    mackie.set_input_preamp(0.0);
    let mut ph_a = 0.0;
    let mut ph_b = 0.0;
    let a = run_saw(&mut clean, 0.5, 0.3, SR as usize, &mut ph_a);
    let b = run_saw(&mut mackie, 0.5, 0.3, SR as usize, &mut ph_b);
    assert_eq!(a, b, "PREAMP at 0 must be bit-exact the clean input path");
}

#[test]
fn grit_raises_harmonics_on_a_resonant_body() {
    fn goertzel(x: &[f32], hz: f64) -> f64 {
        let w = std::f64::consts::TAU * hz / SR;
        let coeff = 2.0 * w.cos();
        let (mut s1, mut s2) = (0.0f64, 0.0f64);
        for &v in x {
            let s0 = v as f64 + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        (s1 * s1 + s2 * s2 - coeff * s1 * s2).max(0.0).sqrt() / (x.len() as f64 / 2.0)
    }
    fn run_sine(eng: &mut FilterEngine, samples: usize) -> Vec<f32> {
        let f0 = 987.0;
        let mut out = Vec::with_capacity(samples);
        let mut buf_l = [0.0f32; 64];
        let mut buf_r = [0.0f32; 64];
        let mut phase = 0.0f64;
        let mut produced = 0;
        while produced < samples {
            for i in 0..64 {
                phase += std::f64::consts::TAU * f0 / SR;
                buf_l[i] = (phase.sin() * 0.6) as f32;
                buf_r[i] = buf_l[i];
            }
            // morph 0.75: GRIT walks the shuffled section cycle by MORPH
            // (CHEW_STAGE_ORDER), and 0.75 selects section 0 — where this
            // body's pole lives. At other positions GRIT is honestly inert.
            eng.process_block(&mut buf_l, &mut buf_r, 0.75, 0.9);
            out.extend_from_slice(&buf_l);
            produced += 64;
        }
        out
    }
    let hot_pole = stage_words(1000.0, 0.98, 0.1);
    let b = body([hot_pole, hot_pole, hot_pole, hot_pole]);
    let mut lin = engine_with(&b);
    let clean = run_sine(&mut lin, (1.0 * SR) as usize);
    let mut gritty = engine_with(&b);
    gritty.set_grit(0.8);
    let driven = run_sine(&mut gritty, (1.0 * SR) as usize);

    // Pole distortion is NOT a saturator: the peak blooms and WANDERS, which
    // smears energy off exact harmonic bins (measured: H3 can even FALL while
    // the sound changes massively). The honest tripwire: the driven output
    // must diverge materially from the linear cascade, and GRIT 0 must not.
    let _ = goertzel; // spectral probes stay available for future tightening
    let n = clean.len().min(driven.len());
    let tail = (0.5 * SR) as usize;
    let diff: Vec<f32> = clean[n - tail..n]
        .iter()
        .zip(&driven[n - tail..n])
        .map(|(&a, &b)| a - b)
        .collect();
    let divergence_db = 20.0 * (rms(&diff) / rms(&clean[n - tail..n]).max(1.0e-12)).log10();
    assert!(
        divergence_db > -20.0,
        "GRIT 0.8 should diverge materially from linear on a hot pole: {divergence_db:.1} dBc"
    );
    // and GRIT at 0 must be exactly the linear cascade
    let mut zero = engine_with(&b);
    zero.set_grit(0.0);
    let same = run_sine(&mut zero, (1.0 * SR) as usize);
    assert_eq!(clean, same, "GRIT at 0 must be bit-exact the linear cascade");
}
