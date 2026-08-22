//! RENDER QA — debug renders for the 2026-08-10 verdicts (Tyson: "renders
//! are the way we debug"). Three questions, each answered by an A/B pair
//! through the REAL engine:
//!
//!   1. JANK    — hard phrase steps raw vs spread over 1 ms, on drums.
//!   2. KEY     — parked wheel, key snap off vs C minor, on brass.
//!   3. CORNERS — the new full-throw step phrase, on brass and on drums,
//!                key C minor, matching the plugin's rendering exactly.
//!
//! cargo run --release -p trench-core --bin render-qa
//! Writes WAVs to dev/experiments/render_qa/.

use trench_core::cartridge::Cartridge;
use trench_core::engine::FilterEngine;

const SR: f64 = 48_000.0;
const SECS: f64 = 6.0;
const KEY_C_MINOR: i32 = 1;

// CORNERS as shipped in plugin/source/dsp/Movement.h (curated 2026-08-10:
// chant plateaus with one flip): 16 hard steps over 8 beats, travel 1.0.
const CORNERS: [f32; 16] = [
    -1.0, -1.0, 1.0, -1.0,  0.0, 0.0, 0.0, 0.0,
     1.0,  1.0, 1.0,  1.0, -1.0, -1.0, 1.0, 1.0,
];
fn corners_morph(n: usize, slew_1ms: bool, bpm: f64) -> Vec<f32> {
    let step_secs = 60.0 / bpm / 2.0; // eighth notes on the material's grid
    let max_step = (1.0 / (0.001 * SR)) as f32;
    let mut out = Vec::with_capacity(n);
    let mut prev = (0.5 + CORNERS[0]).clamp(0.0, 1.0);
    for i in 0..n {
        let step = ((i as f64 / SR) / step_secs) as usize % CORNERS.len();
        let target = (0.5 + CORNERS[step]).clamp(0.0, 1.0);
        prev = if slew_1ms {
            prev + (target - prev).clamp(-max_step, max_step)
        } else {
            target
        };
        out.push(prev);
    }
    out
}

/// Real trap drums: the D.Rich loop assembled by scratchpad make_loop.py
/// (PCM16 mono 48k). Real transients or the diagnosis is mismatched.
fn drums(n: usize) -> Vec<f32> {
    let raw = std::fs::read("dev/experiments/render_qa/drum_loop_drich.wav")
        .expect("dev/experiments/render_qa/drum_loop_drich.wav (run make_loop.py)");
    let data = &raw[44..];
    let loop_len = data.len() / 2;
    (0..n)
        .map(|i| {
            let j = (i % loop_len) * 2;
            i16::from_le_bytes([data[j], data[j + 1]]) as f32 / 32768.0
        })
        .collect()
}

/// Brass-ish: C3 + C4 saws, C3 loud (Tyson's audition rig), slow attack,
/// bright harmonics from a touch of soft clip.
fn brass(n: usize) -> Vec<f32> {
    let (c3, c4) = (130.8128, 261.6256);
    let (mut p3, mut p4) = (0.0f64, 0.0f64);
    let mut audio = Vec::with_capacity(n);
    for i in 0..n {
        let t = i as f64 / SR;
        let attack = (t / 0.06).min(1.0);
        let x = (2.0 * p3 - 1.0) * 0.5 + (2.0 * p4 - 1.0) * 0.25;
        audio.push(((x * 1.8).tanh() * 0.4 * attack) as f32);
        p3 += c3 / SR;
        p4 += c4 / SR;
        if p3 >= 1.0 { p3 -= 1.0; }
        if p4 >= 1.0 { p4 -= 1.0; }
    }
    audio
}

fn render(body: &[u8], morph: &[f32], input: &[f32], key: i32, q: f64) -> (Vec<f32>, Vec<f32>) {
    let mut e = FilterEngine::new();
    e.prepare(SR);
    e.load_cartridge(Cartridge::from_body_bytes("qa", body, 1.0).unwrap());
    e.set_key_snap(key);
    let n = input.len();
    let (mut l, mut r) = (input.to_vec(), input.to_vec());
    let mut off = 0;
    while off < n {
        let len = 512.min(n - off);
        let (lhs, rhs) = (&mut l[off..off + len], &mut r[off..off + len]);
        e.process_trajectory(lhs, rhs, &morph[off..off + len], q);
        off += len;
    }
    (l, r)
}

fn write_wav(path: &std::path::Path, l: &[f32], r: &[f32]) {
    let n = l.len();
    let mut b = Vec::with_capacity(44 + n * 4);
    let dl = (n * 4) as u32;
    b.extend_from_slice(b"RIFF");
    b.extend_from_slice(&(36 + dl).to_le_bytes());
    b.extend_from_slice(b"WAVEfmt ");
    b.extend_from_slice(&16u32.to_le_bytes());
    b.extend_from_slice(&1u16.to_le_bytes());
    b.extend_from_slice(&2u16.to_le_bytes());
    b.extend_from_slice(&(SR as u32).to_le_bytes());
    b.extend_from_slice(&((SR as u32) * 4).to_le_bytes());
    b.extend_from_slice(&4u16.to_le_bytes());
    b.extend_from_slice(&16u16.to_le_bytes());
    b.extend_from_slice(b"data");
    b.extend_from_slice(&dl.to_le_bytes());
    let peak = l
        .iter()
        .chain(r.iter())
        .fold(0.0f32, |m, &s| m.max(s.abs()))
        .max(1.0e-9);
    let g = 0.7079 / peak; // -3 dBFS
    for i in 0..n {
        b.extend_from_slice(&(((l[i] * g).clamp(-1.0, 1.0) * 32767.0) as i16).to_le_bytes());
        b.extend_from_slice(&(((r[i] * g).clamp(-1.0, 1.0) * 32767.0) as i16).to_le_bytes());
    }
    std::fs::write(path, b).unwrap();
    println!("  {}", path.display());
}

/// PCM16 mono 48k reader (the decoded sample beats and the assembled loop).
fn read_pcm16(path: &str, n: usize) -> Vec<f32> {
    let raw = std::fs::read(path).expect(path);
    let data = &raw[44..];
    let len = data.len() / 2;
    (0..n.min(len))
        .map(|i| i16::from_le_bytes([data[i * 2], data[i * 2 + 1]]) as f32 / 32768.0)
        .collect()
}

fn main() {
    let out = std::path::Path::new("dev/experiments/render_qa");
    std::fs::create_dir_all(out).unwrap();
    let body = std::fs::read("presets_ship_v1/bodies/acid_vox.body240")
        .expect("presets_ship_v1/bodies/acid_vox.body240");

    // Program material verdict (Tyson 2026-08-10 "those renders suck" /
    // "synthesised test pattern 01"): render REAL D.Rich beats, step grid
    // matched to each beat's measured tempo. Renders are music or nothing.
    let beats = [
        ("beat1", "dev/experiments/render_qa/samplebeat_48k.wav", 97.0),
        ("beat2", "dev/experiments/render_qa/samplebeat2_48k.wav", 89.5),
    ];
    for (name, path, bpm) in beats {
        println!("{name} ({bpm} bpm):");
        let input = read_pcm16(path, usize::MAX);
        let n = input.len();
        let smooth = corners_morph(n, true, bpm);
        let (l, r) = render(&body, &smooth, &input, KEY_C_MINOR, 0.85);
        write_wav(&out.join(format!("{name}_CORNERS_keyC.wav")), &l, &r);
        // Same pattern with RAW steps — the jank A/B on real music.
        let raw = corners_morph(n, false, bpm);
        let (l, r) = render(&body, &raw, &input, KEY_C_MINOR, 0.85);
        write_wav(&out.join(format!("{name}_CORNERS_keyC_RAWSTEPS.wav")), &l, &r);
    }
}
