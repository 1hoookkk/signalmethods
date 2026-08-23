//! A body switch transitions between filters (US 10,514,883 Fig. 9, method
//! 900): the outgoing filter keeps running and fades out while the new one
//! fades in over 50 ms. The switch must not spike, must not click, and must
//! land exactly on the new body once the fade is over.

use std::path::PathBuf;
use trench_core::{Cartridge, FilterEngine};

const SR: f64 = 48_000.0;
const HOST_BLOCK: usize = 512;
const BEFORE: usize = 48_000;
const AFTER: usize = 24_000;
const MORPH: f64 = 0.5;
const Q: f64 = 0.25;
const FADE: usize = 2_400;
const LANDED: usize = 4_800;
const STEP_WINDOW: usize = 96;
const STEADY: usize = 9_600;
const BODY_A: &str = "P2k_000_ace_of_bass";
const BODY_B: &str = "P2k_013_talking_hedz";

fn noise(n: usize) -> Vec<f32> {
    use std::f64::consts::TAU;
    let alpha = 1.0 - (-TAU * 500.0 / SR).exp();
    let mut seed: u32 = 0x1BAD_F00D;
    let mut y = 0.0f64;
    let mut raw = Vec::with_capacity(n);
    for _ in 0..n {
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

fn two_bodies() -> [(String, Vec<u8>); 2] {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../ref/presets")
        .canonicalize()
        .expect("preset directory");
    let load = |name: &str| {
        let p = dir.join(format!("{name}.bin"));
        let bytes = std::fs::read(&p).unwrap_or_else(|e| panic!("read {}: {e}", p.display()));
        assert_eq!(bytes.len(), 240, "{name} is not a 240-byte body");
        (name.to_string(), bytes)
    };
    [load(BODY_A), load(BODY_B)]
}

fn engine(body: &[u8], name: &str) -> FilterEngine {
    let mut e = FilterEngine::new();
    e.prepare(SR);
    e.debug.agc_enabled = false;
    e.debug.dc_block_enabled = false;
    e.debug.saturation_enabled = false;
    e.debug.spatial_enabled = false;
    e.debug.nonlinearity_enabled = false;
    e.set_amount(1.0);
    e.set_x3_movement(false);
    e.load_cartridge(Cartridge::from_body_bytes(name, body, 1.0).expect("body load"));
    e
}

fn run(e: &mut FilterEngine, input: &[f32]) -> Vec<f32> {
    let mut left = input.to_vec();
    let mut right = input.to_vec();
    let mut off = 0usize;
    while off < left.len() {
        let len = (left.len() - off).min(HOST_BLOCK);
        e.process_block(
            &mut left[off..off + len],
            &mut right[off..off + len],
            MORPH,
            Q,
        );
        off += len;
    }
    left
}

fn peak(x: &[f32]) -> f64 {
    x.iter().fold(0.0f64, |a, b| a.max(b.abs() as f64))
}

fn max_step(x: &[f32]) -> f64 {
    x.windows(2)
        .fold(0.0f64, |a, w| a.max((w[1] - w[0]).abs() as f64))
}

#[test]
fn a_body_switch_fades_instead_of_cutting() {
    let [(name_a, body_a), (name_b, body_b)] = two_bodies();
    let input = noise(BEFORE + AFTER);
    let (head, tail) = input.split_at(BEFORE);

    let mut e = engine(&body_a, &name_a);
    let out_head = run(&mut e, head);
    let mut r = engine(&body_a, &name_a);
    let out_head_ref = run(&mut r, head);
    assert_eq!(out_head, out_head_ref, "the two runs must share a history");

    let cart_b = Cartridge::from_body_bytes(&name_b, &body_b, 1.0).expect("body load");
    e.load_cartridge(cart_b);
    let out = run(&mut e, tail);

    let mut b = engine(&body_b, &name_b);
    let out_ref = run(&mut b, tail);

    let cart_b = Cartridge::from_body_bytes(&name_b, &body_b, 1.0).expect("body load");
    r.reload_cartridge(Box::new(cart_b));
    let out_hard = run(&mut r, tail);

    let peak_a = peak(&out_head[BEFORE - STEADY..]);
    let peak_b = peak(&out_ref[out_ref.len() - STEADY..]);
    let switch_peak = peak(&out[..FADE]);
    let limit = 1.5 * peak_a.max(peak_b);

    let land: f64 = out[LANDED..]
        .iter()
        .zip(out_ref[LANDED..].iter())
        .fold(0.0f64, |a, (x, y)| a.max((x - y).abs() as f64));

    let mut joint = vec![out_head[BEFORE - 1]];
    joint.extend_from_slice(&out[..STEP_WINDOW]);
    let switch_step = max_step(&joint);
    let steady_step = max_step(&out_head[BEFORE - STEADY..]);

    let mut hard = vec![out_head[BEFORE - 1]];
    hard.extend_from_slice(&out_hard[..STEP_WINDOW]);

    println!("bodies: {name_a} -> {name_b}");
    println!("steady peaks: A {peak_a:.6}  B {peak_b:.6}  limit {limit:.6}");
    println!(
        "switch peak (50 ms): {switch_peak:.6}   hard cut would be {:.6}",
        peak(&out_hard[..FADE])
    );
    println!("landed (100 ms on): max |test-ref| {land:.3e}");
    println!(
        "max step: switch (2 ms) {switch_step:.6}  steady {steady_step:.6}   hard cut would be {:.6}",
        max_step(&hard)
    );

    assert!(
        switch_peak <= limit,
        "switch peak {switch_peak:.6} exceeded 1.5x the steady peaks ({limit:.6})"
    );
    assert!(
        land < 1.0e-3,
        "transition did not land: max |test-ref| {land:.3e} after 100 ms"
    );
    assert!(
        switch_step < 3.0 * steady_step,
        "discontinuity at the switch: step {switch_step:.6} vs steady {steady_step:.6}"
    );
}
