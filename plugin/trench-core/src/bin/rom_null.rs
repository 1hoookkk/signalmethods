//! Null TRENCH against the E-MU ROM table itself - no X3, no FL, no host state.
//!
//! Reference = the ROM's own 240 bytes evaluated at 44,100 Hz with NO rate
//! adaptation. That is E-MU's definition of the filter, straight from the
//! binary. Everything else is nulled against it.

use trench_core::cartridge::Cartridge;
use trench_core::engine::FilterEngine;

const BLOCK: usize = 128;

fn read_wav(path: &str) -> (u32, Vec<f32>) {
    let d = std::fs::read(path).expect("read wav");
    let (mut i, mut sr, mut ch, mut bits) = (12usize, 44100u32, 1u16, 32u16);
    let mut data: &[u8] = &[];
    while i + 8 <= d.len() {
        let id = &d[i..i + 4];
        let sz = u32::from_le_bytes(d[i + 4..i + 8].try_into().unwrap()) as usize;
        let body = &d[i + 8..(i + 8 + sz).min(d.len())];
        if id == b"fmt " {
            ch = u16::from_le_bytes(body[2..4].try_into().unwrap());
            sr = u32::from_le_bytes(body[4..8].try_into().unwrap());
            bits = u16::from_le_bytes(body[14..16].try_into().unwrap());
        } else if id == b"data" {
            data = body;
        }
        i += 8 + sz + (sz & 1);
    }
    let mono: Vec<f32> = if bits == 32 {
        data.chunks_exact(4)
            .map(|c| f32::from_le_bytes(c.try_into().unwrap()))
            .step_by(ch as usize)
            .collect()
    } else {
        data.chunks_exact(2)
            .map(|c| i16::from_le_bytes(c.try_into().unwrap()) as f32 / 32768.0)
            .step_by(ch as usize)
            .collect()
    };
    (sr, mono)
}

/// Render `dry` through `body` at the requested host rate. Packed words are
/// consumed verbatim; authoring owns their rate.
fn render(body: &[u8], dry: &[f32], rate: f64, adapt_to: Option<f64>, m: f64, q: f64) -> Vec<f32> {
    let mut eng = FilterEngine::new();
    eng.prepare(rate);
    let _ = adapt_to;
    let cart = Cartridge::from_body_bytes("n", body, 1.0).expect("cartridge");
    eng.load_cartridge(cart);
    let mut l = dry.to_vec();
    let mut r = dry.to_vec();
    let mut off = 0;
    while off < l.len() {
        let n = BLOCK.min(l.len() - off);
        let (a, b) = (&mut l[off..off + n], &mut r[off..off + n]);
        eng.process_block(a, b, m, q);
        off += n;
    }
    l
}

/// Best-fit gain, then residual relative to the reference, in dB.
/// -inf is a perfect null; 0 dB means the residual is as loud as the signal.
fn null_db(reference: &[f32], test: &[f32]) -> (f64, f64) {
    let n = reference.len().min(test.len());
    let (mut rt, mut tt) = (0.0f64, 0.0f64);
    for i in 0..n {
        rt += reference[i] as f64 * test[i] as f64;
        tt += (test[i] as f64).powi(2);
    }
    let g = if tt > 0.0 { rt / tt } else { 0.0 };
    let (mut res, mut refp) = (0.0f64, 0.0f64);
    for i in 0..n {
        let d = reference[i] as f64 - g * test[i] as f64;
        res += d * d;
        refp += (reference[i] as f64).powi(2);
    }
    (
        10.0 * (res / refp.max(1e-30)).log10(),
        20.0 * g.abs().max(1e-30).log10(),
    )
}

fn main() {
    let mut a = std::env::args().skip(1);
    let body_path = a.next().expect("usage: rom-null <body240> <dry.wav>");
    let dry_path = a.next().expect("usage: rom-null <body240> <dry.wav>");
    let body = std::fs::read(&body_path).expect("read body");
    let (_, dry) = read_wav(&dry_path);

    println!("body: {body_path}");
    println!("dry : {dry_path}  ({} frames)\n", dry.len());
    println!("reference = ROM words evaluated at 44,100 Hz, no rate adaptation");
    println!("(that is E-MU's own definition of this filter)\n");

    for &(m, q, label) in &[
        (0.47, 1.0, "M47 Q100"),
        (0.00, 1.0, "M0  Q100"),
        (0.50, 0.0, "M50 Q0  "),
    ] {
        let reference = render(&body, &dry, 44100.0, None, m, q);
        println!("--- {label} ---");
        for &(rate, adapt, what) in &[
            (44100.0, None, "raw words @ 44,100   (identity check)"),
            (48000.0, None, "raw words @ 48,000"),
        ] {
            let test = render(&body, &dry, rate, adapt, m, q);
            let (null, gain) = null_db(&reference, &test);
            println!("  {what:36}  null {null:7.2} dB   gain fit {gain:+6.2} dB");
        }
        println!();
    }
    println!("a real null is -20 dB or deeper; 0 dB means no cancellation at all");
}
