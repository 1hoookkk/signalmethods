//! MOTION LAB — the three per-sample movement experiments (2026-08-10).
//!
//! The audio-rate baseline removed the wheel's speed ceiling. These renders
//! probe the territory that opens up, through the REAL engine:
//!
//!   growl      — the wheel oscillates at audible rates (30..200 Hz): the
//!                filter's shape change becomes TONE (sidebands), not motion.
//!   cyclescan  — the wheel sweeps the whole body exactly once per waveform
//!                cycle, phase-locked: the morph path becomes the timbre of
//!                every cycle (phase-distortion with Z-plane shapes).
//!   hardfollow — the input's own envelope drives the wheel with ~0.25 ms
//!                lag: the signal modulates its own filter at near-audio rate.
//!
//! cargo run --release -p trench-core --bin motion-lab
//! Writes WAVs to dev/experiments/motion_lab/.

use trench_core::cartridge::Cartridge;
use trench_core::engine::FilterEngine;

const SR: f64 = 48_000.0;
const SECS: f64 = 6.0;
const F0: f64 = 55.0; // A1 — bass register, where the animal lives

fn saw_bass(n: usize) -> (Vec<f32>, Vec<f32>) {
    // The source and its exact cycle phase (for cyclescan): an 8th-note gated
    // saw so hardfollow has dynamics to chew on, others hear the held tone.
    let mut audio = Vec::with_capacity(n);
    let mut phase_out = Vec::with_capacity(n);
    let mut ph = 0.0f64;
    for i in 0..n {
        let t = i as f64 / SR;
        // gate: 8ths at 120bpm (0.25 s), 85% duty, 3 ms edges
        let pos = (t % 0.25) / 0.25;
        let gate = if pos < 0.85 {
            let a = (pos * 0.25 / 0.003).min(1.0);
            let r = ((0.85 - pos) * 0.25 / 0.003).min(1.0);
            a.min(r)
        } else {
            0.0
        };
        audio.push(((2.0 * ph - 1.0) * 0.35 * gate) as f32);
        phase_out.push(ph as f32);
        ph += F0 / SR;
        if ph >= 1.0 {
            ph -= 1.0;
        }
    }
    (audio, phase_out)
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

fn render(body: &[u8], morph: &[f32], input: &[f32]) -> (Vec<f32>, Vec<f32>) {
    let mut e = FilterEngine::new();
    e.prepare(SR);
    e.load_cartridge(Cartridge::from_body_bytes("lab", body, 1.0).unwrap());
    let n = input.len();
    let (mut l, mut r) = (input.to_vec(), input.to_vec());
    let mut off = 0;
    while off < n {
        let len = 512.min(n - off);
        let (ls, rs) = (&mut l[off..off + len], &mut r[off..off + len]);
        e.process_trajectory(ls, rs, &morph[off..off + len], 0.6);
        off += len;
    }
    (l, r)
}

fn main() {
    let out = std::path::Path::new("dev/experiments/motion_lab");
    std::fs::create_dir_all(out).unwrap();
    let n = (SR * SECS) as usize;
    let (input, cycle_phase) = saw_bass(n);

    let bodies = [
        ("acid_vox", "presets_ship_v1/bodies/acid_vox.body240"),
        ("moth", "presets_ship_v1/bodies/MOTH.body240"),
        ("hollow", "presets_ship_v1/bodies/HOLLOW.body240"),
    ];
    for (bname, bpath) in bodies {
        let body = match std::fs::read(bpath) {
            Ok(b) => b,
            Err(_) => {
                println!("skip {bname}: {bpath} not found");
                continue;
            }
        };
        println!("{bname}:");

        // dry-wheel reference for the A/B: parked at the sweet spot
        let parked = vec![0.5f32; n];
        let (l, r) = render(&body, &parked, &input);
        write_wav(&out.join(format!("{bname}_0_parked.wav")), &l, &r);

        // GROWL: the wheel as an audible oscillator. Rates chosen musically:
        // one octave below the note, the note's fifth, and a fast buzz.
        for rate in [27.5f64, 82.5, 165.0] {
            let morph: Vec<f32> = (0..n)
                .map(|i| {
                    let t = i as f64 / SR;
                    (0.5 + 0.45 * (core::f64::consts::TAU * rate * t).sin()) as f32
                })
                .collect();
            let (l, r) = render(&body, &morph, &input);
            write_wav(&out.join(format!("{bname}_growl_{}hz.wav", rate as u32)), &l, &r);
        }

        // CYCLESCAN: the wheel rides the waveform's own phase — the whole
        // body traversed once per cycle (up-and-back so the seam is smooth).
        let morph: Vec<f32> = cycle_phase
            .iter()
            .map(|&p| 1.0 - (2.0 * p - 1.0).abs())
            .collect();
        let (l, r) = render(&body, &morph, &input);
        write_wav(&out.join(format!("{bname}_cyclescan.wav")), &l, &r);

        // HARDFOLLOW: the signal's own envelope on the wheel, ~0.25 ms lag.
        let k = 1.0 - (-1.0 / (SR * 0.00025)).exp();
        let mut env = 0.0f64;
        let morph: Vec<f32> = input
            .iter()
            .map(|&x| {
                env += (x.abs() as f64 - env) * k;
                (0.15 + env * 2.4).clamp(0.0, 1.0) as f32
            })
            .collect();
        let (l, r) = render(&body, &morph, &input);
        write_wav(&out.join(format!("{bname}_hardfollow.wav")), &l, &r);
    }
    // ---- Confirmation renders (Tyson: "27hz sounds amazing" / "do those renders") ----

    // (1) The sub-octave growl law over EVERY shipping body: find the animals.
    println!("\nsub-octave growl, all bodies:");
    if let Ok(dir) = std::fs::read_dir("presets_ship_v1/bodies") {
        for entry in dir.flatten() {
            let path = entry.path();
            if path.extension().and_then(|e| e.to_str()) != Some("body240") {
                continue;
            }
            let name = path.file_stem().unwrap().to_string_lossy().to_lowercase();
            let Ok(body) = std::fs::read(&path) else { continue };
            let morph: Vec<f32> = (0..n)
                .map(|i| {
                    let t = i as f64 / SR;
                    (0.5 + 0.45 * (core::f64::consts::TAU * (F0 * 0.5) * t).sin()) as f32
                })
                .collect();
            let (l, r) = render(&body, &morph, &input);
            write_wav(&out.join(format!("suboct_{name}.wav")), &l, &r);
        }
    }

    // (2) A MOVING bassline with the growl locked to each note — proving the
    // "in tune" theory across pitch. Same law the GROWL button ships:
    // rate = note/2, phase continuous across note changes.
    println!("\npitch-locked bassline:");
    {
        let notes = [55.0f64, 55.0, 65.406, 48.999, 55.0, 82.407, 73.416, 65.406];
        let note_len = (SR * 0.75) as usize;
        let total = notes.len() * note_len;
        let mut bass = Vec::with_capacity(total);
        let mut morph = Vec::with_capacity(total);
        let (mut ph, mut gph) = (0.0f64, 0.0f64);
        for (k, &f) in notes.iter().enumerate() {
            for i in 0..note_len {
                let pos = i as f64 / note_len as f64;
                let edge = (pos / 0.01).min(((1.0 - pos) / 0.01).min(1.0));
                bass.push(((2.0 * ph - 1.0) * 0.35 * edge) as f32);
                ph += f / SR;
                if ph >= 1.0 { ph -= 1.0; }
                gph += (f * 0.5) / SR;
                if gph >= 1.0 { gph -= 1.0; }
                morph.push((0.5 + 0.45 * (core::f64::consts::TAU * gph).sin()) as f32);
                let _ = k;
            }
        }
        for (bname, bpath) in [("acid_vox", "presets_ship_v1/bodies/acid_vox.body240"),
                               ("moth", "presets_ship_v1/bodies/MOTH.body240")] {
            if let Ok(body) = std::fs::read(bpath) {
                let (l, r) = render(&body, &morph, &bass);
                write_wav(&out.join(format!("bassline_growl_{bname}.wav")), &l, &r);
            }
        }
    }

    // (3) The physics A/B at 27.5 Hz: the same raw cascade retuned per sample
    // as DF-II (the hardware's form), as zero-delay TPT, and as DF-II run at
    // 2x rate and band-limited back down. No AGC, no desk — only the
    // integration law differs, so what you hear IS the physics.
    println!("\nphysics A/B (raw cascade, acid_vox):");
    physics_ab(out, &input);

    // (4) THE CUBE (Tyson 2026-08-10, the Morpheus 8-corner form): a third
    // axis interpolating between TWO bodies' words — trilinear, entirely in
    // encoded space, per sample. Two 4-corner sheets = 8 corners. Here the
    // z axis triangles 0 -> 1 -> 0 over the render while Morph/Q hold still,
    // so what you hear IS the third axis.
    println!("\nthe cube (acid_vox <-> hollow, z sweep):");
    cube_scan(out, &input);

    println!("\ndone: drag anything in dev/experiments/motion_lab into FL");
}

fn cube_scan(out: &std::path::Path, input: &[f32]) {
    use trench_core::cartridge::Cartridge;
    use trench_core::cascade::Cascade;
    use trench_core::minifloat::lerp_u16;
    let (Ok(a), Ok(b)) = (
        std::fs::read("presets_ship_v1/bodies/acid_vox.body240"),
        std::fs::read("presets_ship_v1/bodies/HOLLOW.body240"),
    ) else {
        return;
    };
    let mut ca = Cartridge::from_body_bytes("a", &a, 1.0).unwrap();
    let mut cb = Cartridge::from_body_bytes("b", &b, 1.0).unwrap();
    ca.compile_at(SR);
    cb.compile_at(SR);
    let n = input.len();
    let mut cascade = Cascade::new();
    let out_l: Vec<f32> = input
        .iter()
        .enumerate()
        .map(|(i, &x)| {
            let t = i as f64 / n as f64;
            let z = (1.0 - (2.0 * t - 1.0).abs()) as f32; // 0 -> 1 -> 0
            let wa = ca.packed.interpolate_words(0.5, 0.6, 0.0);
            let wb = cb.packed.interpolate_words(0.5, 0.6, 0.0);
            let mut rows = [[0.0f64; 5]; 6];
            for s in 0..6 {
                let mut w = [0u16; 5];
                for k in 0..5 {
                    w[k] = lerp_u16(wa[s][k], wb[s][k], z);
                }
                rows[s] = trench_core::minifloat::stage_words_to_biquad(w);
            }
            cascade.snap_targets(&rows);
            cascade.tick(x)
        })
        .collect();
    write_wav(&out.join("cube_acidvox_to_hollow.wav"), &out_l, &out_l);
}

fn physics_ab(out: &std::path::Path, input48: &[f32]) {
    use trench_core::cartridge::Cartridge;
    use trench_core::cascade::Cascade;
    use trench_core::tpt::TptCascade;
    let Ok(body) = std::fs::read("presets_ship_v1/bodies/acid_vox.body240") else { return };
    let rows_at = |cart: &Cartridge, m: f64| cart.packed.interpolate_biquad(m as f32, 0.6, 0.0);
    let growl_morph = |i: usize, sr: f64| {
        let t = i as f64 / sr;
        0.5 + 0.45 * (core::f64::consts::TAU * 27.5 * t).sin()
    };
    // DF-II leg — the shipped law: whole coefficient set installed per sample.
    let mut cart = Cartridge::from_body_bytes("ab", &body, 1.0).unwrap();
    cart.compile_at(SR);
    {
        let mut c = Cascade::new();
        let out_l: Vec<f32> = input48
            .iter()
            .enumerate()
            .map(|(i, &x)| {
                c.snap_targets(&rows_at(&cart, growl_morph(i, SR)));
                c.tick(x)
            })
            .collect();
        write_wav(&out.join("physics_df2.wav"), &out_l, &out_l);
    }
    // ZDF leg — same rows, zero-delay topology, retuned per sample.
    {
        let mut c = TptCascade::new();
        let out_l: Vec<f32> = input48
            .iter()
            .enumerate()
            .map(|(i, &x)| {
                c.set_rows(&rows_at(&cart, growl_morph(i, SR)));
                c.tick(x)
            })
            .collect();
        write_wav(&out.join("physics_zdf.wav"), &out_l, &out_l);
    }
    // 2x leg — DF-II at 96k (source synthesized there, words recompiled
    // there), band-limited back to 48k with a windowed sinc.
    {
        let sr2 = SR * 2.0;
        let n2 = input48.len() * 2;
        let (input96, _) = {
            // same gated saw, synthesized natively at 96k
            let mut audio = Vec::with_capacity(n2);
            let mut ph = 0.0f64;
            for i in 0..n2 {
                let t = i as f64 / sr2;
                let pos = (t % 0.25) / 0.25;
                let gate = if pos < 0.85 {
                    ((pos * 0.25 / 0.003).min(1.0)).min(((0.85 - pos) * 0.25 / 0.003).min(1.0))
                } else {
                    0.0
                };
                audio.push(((2.0 * ph - 1.0) * 0.35 * gate) as f32);
                ph += F0 / sr2;
                if ph >= 1.0 { ph -= 1.0; }
            }
            (audio, ())
        };
        let mut cart2 = Cartridge::from_body_bytes("ab2", &body, 1.0).unwrap();
        cart2.compile_at(sr2);
        let mut c = Cascade::new();
        let hi: Vec<f32> = input96
            .iter()
            .enumerate()
            .map(|(i, &x)| {
                c.snap_targets(&rows_at(&cart2, growl_morph(i, sr2)));
                c.tick(x)
            })
            .collect();
        // 63-tap windowed-sinc lowpass at 21.6 kHz, then take every 2nd sample
        let taps = 63usize;
        let fc = 21_600.0 / sr2;
        let sinc: Vec<f64> = (0..taps)
            .map(|k| {
                let m = k as f64 - (taps as f64 - 1.0) / 2.0;
                let s = if m.abs() < 1.0e-9 {
                    2.0 * fc
                } else {
                    (core::f64::consts::TAU * fc * m).sin() / (core::f64::consts::PI * m)
                };
                let w = 0.54 - 0.46 * (core::f64::consts::TAU * k as f64 / (taps as f64 - 1.0)).cos();
                s * w
            })
            .collect();
        let out_l: Vec<f32> = (0..input48.len())
            .map(|j| {
                let centre = j * 2;
                let mut acc = 0.0f64;
                for (k, &h) in sinc.iter().enumerate() {
                    let idx = centre as i64 + k as i64 - (taps as i64 - 1) / 2;
                    if idx >= 0 && (idx as usize) < hi.len() {
                        acc += h * hi[idx as usize] as f64;
                    }
                }
                acc as f32
            })
            .collect();
        write_wav(&out.join("physics_df2_2x.wav"), &out_l, &out_l);
    }
}
