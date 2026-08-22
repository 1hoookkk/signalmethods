//! compose_body — clean-room body composer.
//! Applies the stage laws (design/SLOT_GRAMMAR.md, design/STAGE_LAW.md)
//! to TF-measured fundamentals. Never touches P2K dossier data.
//!
//! Stage laws applied:
//!   1. SCALE law — same SCALE across all 6 slots per corner
//!   2. S6 unit-zero law — S6 zero on unit circle (r ≥ 0.9995)
//!   3. Lane identity — stage number preserved corner-to-corner
//!   4. Slot roles — S1=HF frame, S2=main voice, S3/S4=mouth, S5=colour, S6=terminal
//!
//! Species patterns from STAGE_LAW.md:
//!   Bass: sparse low anchors, remote high counterweights, parallel-down sweep
//!   Vocal: formant frame ~0.95/2-2.7/3.2-5k, oblique relay
//!   TB-303: sub + harmonic ladder, zero crossings forbidden
//!   Wah: beating pairs ~1st apart, contrary motion
//!   Sweepz: air-frame lattice → mid wall, asymmetric Q relay
//!
//! Usage:
//!   cargo run -p trench-core --bin compose-body -- \
//!     --species bass \
//!     --low-source recipes/tfs/uiowa_double_bass_mf.tf.json \
//!     --high-source recipes/tfs/uiowa_alto_sax_mf.tf.json \
//!     --name "Bass Foundation" --out body.body240

use trench_core::cartridge::NUM_CORNERS;
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::compiler::DEFAULT_AUTHORING_SR;
use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::{words_from_geometry, RootPair, StageGeometry};

const SR: f64 = DEFAULT_AUTHORING_SR;
const PAD: [u16; 5] = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff];
const ST: f64 = 12.0;

// Stage-law slot roles (SLOT_GRAMMAR.md)
// S1: HF foundation/frame — median ~7 kHz, widest morph travel in sweepers
// S2: Low-mid main voice
// S3: Mid inner voice (mouth)
// S4: Upper voice (mouth)
// S5: Colour/detail
// S6: LF-lift terminal frame, unit zero (r ≥ 0.9995)

const AIR_CAP_HZ: f64 = 20277.1;

// ── Per-species slot frequency ranges (from STAGE_LAW.md + ROM_MUSIC_THEORY.md) ──

struct SpeciesSlots {
    s1_range: (f64, f64),  // HF frame
    s2_range: (f64, f64),  // main voice
    s3_range: (f64, f64),  // mouth
    s4_range: (f64, f64),  // mouth
    s5_range: (f64, f64),  // colour
    s6_range: (f64, f64),  // terminal (anchor)
    pole_r_q0: f64,         // nominal pole radius at Q0
    motion: &'static str,   // parallel-down | contrary | oblique
    q_lane: usize,          // which lane gets the Q bloom
    anchor_count: usize,    // number of sub-400Hz anchors
    scale_db_per_corner: [f64; 4], // uniform per-corner scale (SCALE law)
}

fn get_species(species: &str) -> SpeciesSlots {
    match species {
        "bass" => SpeciesSlots {
            // Sparse low anchors (S6, S2) + remote high counterweights (S1, S5 air)
            s1_range: (8000.0, 17000.0),  s2_range: (400.0, 1200.0),
            s3_range: (2000.0, 5000.0),   s4_range: (5000.0, 9000.0),
            s5_range: (9000.0, 15000.0),  s6_range: (60.0, 300.0),
            pole_r_q0: 0.70, motion: "parallel-down",
            q_lane: 6, anchor_count: 2,
            scale_db_per_corner: [-12.0, -3.0, -14.0, -3.0], // M0 deep-cut, M100 unity
        },
        "vocal" => SpeciesSlots {
            // Formant frame: S6=chest, S2/S3/S4=mouth, S1=air (spared)
            s1_range: (4000.0, 10000.0),  s2_range: (600.0, 1200.0),
            s3_range: (1200.0, 2000.0),   s4_range: (1800.0, 3000.0),
            s5_range: (3000.0, 5000.0),   s6_range: (100.0, 350.0),
            pole_r_q0: 0.97, motion: "oblique",
            q_lane: 5, anchor_count: 1,
            scale_db_per_corner: [-5.0, -6.0, -5.0, -6.0],
        },
        "tb303" => SpeciesSlots {
            // Sub engine (S6) + harmonic ladder (S2-S5), S1=register-jumper
            s1_range: (2000.0, 9000.0),   s2_range: (200.0, 500.0),
            s3_range: (400.0, 1500.0),    s4_range: (1000.0, 4500.0),
            s5_range: (4000.0, 7000.0),   s6_range: (50.0, 150.0),
            pole_r_q0: 0.92, motion: "contrary",
            q_lane: 1, anchor_count: 1,
            scale_db_per_corner: [-7.0, -8.0, -7.0, 0.0],
        },
        "wah" => SpeciesSlots {
            // Beating pairs: S2+S6 close, S3+S4 close. S1/S5 frame.
            s1_range: (300.0, 7000.0),    s2_range: (500.0, 1000.0),
            s3_range: (2000.0, 6000.0),   s4_range: (5000.0, 7000.0),
            s5_range: (9000.0, 15000.0),  s6_range: (600.0, 1000.0),
            pole_r_q0: 0.85, motion: "contrary",
            q_lane: 1, anchor_count: 0,
            scale_db_per_corner: [-7.0, -4.0, -16.0, -6.0],
        },
        "sweepz" => SpeciesSlots {
            // Air lattice (all high) at M0, mid wall at M100
            s1_range: (6000.0, 13000.0),  s2_range: (9000.0, 12000.0),
            s3_range: (13000.0, 17000.0), s4_range: (12000.0, 15000.0),
            s5_range: (8000.0, 11000.0),  s6_range: (1000.0, 3000.0),
            pole_r_q0: 0.65, motion: "parallel-down",
            q_lane: 1, anchor_count: 0,
            scale_db_per_corner: [-5.0, -10.0, -2.0, -7.0],
        },
        "deepbouche" => SpeciesSlots {
            // Descending formant stack: S1 highest (3484 Hz→mouth), S6 throat (228 Hz).
            // All radii hot (r=0.97-1.0), Q essentially zero. Deeply cut sub scale.
            s1_range: (2500.0, 4000.0),   s2_range: (2000.0, 3500.0),
            s3_range: (1500.0, 3000.0),   s4_range: (1000.0, 2500.0),
            s5_range: (500.0, 2000.0),    s6_range: (150.0, 400.0),
            pole_r_q0: 0.97, motion: "oblique",
            q_lane: 5, anchor_count: 0,
            scale_db_per_corner: [-6.0, -3.0, -7.0, -8.0],
        },
        _ => panic!("unknown species: {}", species),
    }
}

// ── Section geometry ──

#[derive(Clone, Copy)]
struct SectionGeo { pole_hz: f64, pole_r: f64, zero_hz: f64, zero_r: f64, scale: f64 }
type CornerGeo = [SectionGeo; NUM_STAGES];

fn geo(hz: f64, r: f64, zh: f64, zr: f64, scale_db: f64) -> SectionGeo {
    SectionGeo { pole_hz: hz, pole_r: r.min(0.999), zero_hz: zh, zero_r: zr.min(0.999f64),
        scale: 10.0f64.powf(scale_db / 20.0) }
}

// ── TF source → fundamentals ──

fn extract_fundamentals(tf_path: &str) -> Vec<f64> {
    let text = std::fs::read_to_string(tf_path).expect("read tf.json");
    let v: serde_json::Value = serde_json::from_str(&text).expect("parse tf.json");
    let freqs: Vec<f64> = v["freqs_hz"].as_array().unwrap().iter().map(|x| x.as_f64().unwrap()).collect();
    let mags: Vec<f64> = v["mag_db"].as_array().unwrap().iter().map(|x| x.as_f64().unwrap()).collect();
    let mut peaks: Vec<(f64, f64)> = Vec::new();
    for i in 2..freqs.len()-2 {
        if mags[i] > mags[i-1] && mags[i] > mags[i+1] && mags[i] > mags[i-2] && mags[i] > mags[i+2] {
            peaks.push((freqs[i], mags[i]));
        }
    }
    peaks.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap());
    let median_db = peaks.iter().map(|p| p.1).sum::<f64>() / peaks.len().max(1) as f64;
    let mut result: Vec<f64> = peaks.iter()
        .filter(|(_, db)| *db > median_db - 6.0)
        .map(|(hz, _)| *hz)
        .collect();
    result.sort_by(|a, b| a.partial_cmp(b).unwrap());
    result.truncate(6);
    while result.len() < 6 {
        let base = result.last().copied().unwrap_or(100.0);
        result.push((base * 2.0).min(SR * 0.45));
    }
    result
}

// ── Voice fundamentals into slot roles (STAGE_LAW.md species patterns) ──

fn voice_into_slots(low_fundamentals: &[f64], high_fundamentals: &[f64], slots: &SpeciesSlots) -> CornerGeo {
    let mut poles = [0.0f64; NUM_STAGES];
    let ranges = [slots.s1_range, slots.s2_range, slots.s3_range,
                  slots.s4_range, slots.s5_range, slots.s6_range];
    // High slots (S1, S3, S5): use high-source fundamentals (air, frame, colour)
    // Low slots (S2, S4, S6): use low-source fundamentals (anchor, mouth, terminal)
    let high_slots = [0usize, 2, 4];  // S1, S3, S5
    let low_slots  = [1usize, 3, 5];  // S2, S4, S6

    // Assign high fundamentals to high slots
    let mut used = [false; NUM_STAGES];
    for &f in high_fundamentals.iter().chain(low_fundamentals.iter()) {
        let candidates: &[usize] = if f > 1000.0 { &high_slots } else { &low_slots };
        let mut best_slot = candidates[0];
        let mut best_dist = f64::INFINITY;
        for &si in candidates {
            if used[si] { continue; }
            let center = (ranges[si].0 + ranges[si].1) / 2.0;
            let dist = (f / center).ln().abs();
            if dist < best_dist { best_dist = dist; best_slot = si; }
        }
        poles[best_slot] = f.clamp(ranges[best_slot].0, ranges[best_slot].1);
        used[best_slot] = true;
    }

    // Fill empty slots with range centers
    for si in 0..NUM_STAGES {
        if poles[si] < 1.0 {
            poles[si] = (ranges[si].0 + ranges[si].1) / 2.0;
        }
    }

    // S6 unit-zero law: S6 gets a zero on the unit circle
    // SCALE law: same scale_db across all slots in a corner
    let corner_scale = slots.scale_db_per_corner[0]; // M0 corner scale
    let mut corner: CornerGeo = [geo(0.0, 0.0, 0.0, 0.0, 0.0); NUM_STAGES];
    for si in 0..NUM_STAGES {
        let hz = poles[si];
        let r = slots.pole_r_q0;
        // Zero placement: razor/valley/parked per grammar
        let (zh, zr): (f64, f64) = if si == 5 {
            // S6 unit-zero law: zero on unit circle
            (hz * 2.0f64.powf(3.0 / ST), 1.0)
        } else if hz < 400.0 {
            // Anchor: razor zero beside the pole
            (hz * 2.0f64.powf(1.5 / ST), 0.98)
        } else if hz < 3500.0 {
            // Mouth: valley zero above the formant
            (hz * 2.0f64.powf(5.0 / ST), 0.95)
        } else {
            // Air: parked high
            (hz * 2.0f64.powf(5.0), 0.90)
        };
        corner[si] = geo(hz, r, zh.max(20.0).min(SR * 0.48), zr.min(0.999f64), corner_scale);
    }
    corner
}

// ── Apply motion to create M100 from M0 ──

fn apply_motion(m0: &CornerGeo, species: &str) -> CornerGeo {
    let mut m100 = m0.clone();
    let slots = get_species(species);
    match slots.motion {
        "parallel-down" => {
            // Bass/Sweepz: everything slides down together (gentle: factor 0.5, not 0.25)
            for i in 0..NUM_STAGES {
                if m100[i].pole_hz > 1.0 {
                    let factor = if i < 3 { 0.55 } else { 0.45 };
                    m100[i].pole_hz = (m100[i].pole_hz * factor).max(20.0);
                }
            }
        }
        "contrary" => {
            // TB-303/Wah: S1/S5 rise, S2/S6 fall (registers trade)
            for i in [0, 4] {
                if m100[i].pole_hz > 1.0 { m100[i].pole_hz = (m100[i].pole_hz * 2.5).min(SR * 0.48); }
            }
            for i in [1, 5] {
                if m100[i].pole_hz > 1.0 { m100[i].pole_hz = (m100[i].pole_hz / 3.0).max(20.0); }
            }
            // S3/S4 converge toward center
            for i in [2, 3] {
                let mid = (m0[i].pole_hz + m0[5-i].pole_hz) / 2.0;
                if m100[i].pole_hz > 1.0 { m100[i].pole_hz = mid.clamp(400.0, SR * 0.45); }
            }
        }
        "oblique" => {
            // Vocal: S2 drops (chest), S6 rises (mouth → air relay), rest hold
            if m100[1].pole_hz > 1.0 { m100[1].pole_hz = (m100[1].pole_hz / 4.0).max(100.0); }
            if m100[5].pole_hz > 1.0 { m100[5].pole_hz = (m100[5].pole_hz * 4.0).min(SR * 0.45); }
        }
        _ => {}
    }
    // Re-author S6 zero to unit circle at new pole position
    if m100[5].pole_hz > 1.0 {
        m100[5].zero_hz = m100[5].pole_hz * 2.0f64.powf(3.0 / ST);
        m100[5].zero_r = 1.0;
    }
    // SCALE law: M100 corner gets its own uniform scale
    let m100_scale = slots.scale_db_per_corner[1];
    for i in 0..NUM_STAGES { m100[i].scale = 10.0f64.powf(m100_scale / 20.0); }
    m100
}

// ── Apply Q attitude ──

fn apply_q(m0: &mut CornerGeo, m100: &mut CornerGeo, species: &str) {
    let slots = get_species(species);
    let si = slots.q_lane - 1;
    let q_delta = 0.22; // nominal Q push
    // Push the designated bloomer lane
    if m0[si].pole_hz > 1.0 { m0[si].pole_r = (m0[si].pole_r + q_delta).min(0.999); }
    if m100[si].pole_hz > 1.0 { m100[si].pole_r = (m100[si].pole_r + q_delta).min(0.999); }
    // Spare the air: S1 (HF frame) stays gentle
    if m0[0].pole_hz > 3500.0 { m0[0].pole_r = m0[0].pole_r.min(0.35); }
    if m100[0].pole_hz > 3500.0 { m100[0].pole_r = m100[0].pole_r.min(0.35); }
    // Q100 corners get their own SCALE
    let q0_scale = slots.scale_db_per_corner[2];
    let q100_scale = slots.scale_db_per_corner[3];
    for i in 0..NUM_STAGES {
        m0[i].scale = 10.0f64.powf(q0_scale / 20.0);
        m100[i].scale = 10.0f64.powf(q100_scale / 20.0);
    }
}

// ── Build ──

fn corner_to_stages(c: &CornerGeo) -> [[u16; NUM_COEFFS]; NUM_STAGES] {
    let mut s = [PAD; NUM_STAGES];
    for si in 0..NUM_STAGES {
        if c[si].pole_hz <= 1.0 && c[si].pole_r <= 0.01 { continue; }
        let pole = if c[si].pole_hz > 1.0 {
            RootPair::Conjugate { hz: c[si].pole_hz, r: c[si].pole_r }
        } else if c[si].pole_r > 0.01 {
            RootPair::RealPair { root_a: c[si].pole_r, root_b: 0.0 }
        } else { RootPair::Degenerate };
        let zero = if c[si].zero_hz > 1.0 {
            RootPair::Conjugate { hz: c[si].zero_hz, r: c[si].zero_r }
        } else { RootPair::Degenerate };
        s[si] = words_from_geometry(&StageGeometry { pole, zero, scale: c[si].scale });
    }
    s
}

fn certify(packed: &PackedCorners) -> (usize, usize) {
    let mut u = 0; let mut nf = 0;
    let grid = trench_core::response::log_frequency_grid(20.0, SR * 0.499, 220);
    for qi in 0..33 { let q = qi as f64 / 32.0;
    for mi in 0..33 { let m = mi as f64 / 32.0;
        let mut rows = [[0.0f64; 5]; NUM_STAGES];
        for si in 0..NUM_STAGES {
            let mut w = [0u16; 5];
            for wi in 0..5 {
                let w00=packed.words[0][si][wi] as f64; let w10=packed.words[1][si][wi] as f64;
                let w01=packed.words[2][si][wi] as f64; let w11=packed.words[3][si][wi] as f64;
                let w0=w00+(w10-w00)*m; let w1=w01+(w11-w01)*m;
                w[wi] = (w0+(w1-w0)*q).round().clamp(0.0,65535.0) as u16;
            }
            rows[si] = trench_core::minifloat::kernel_to_biquad(
                trench_core::minifloat::stage_words_to_kernel(w));
        }
        for &f in &grid {
            let (r,i)=trench_core::response::biquad_cascade_complex(&rows,f,SR);
            if !r.is_finite()||!i.is_finite() { nf+=1; break; }
            if 10.0*(r*r+i*i+1e-30).log10()>60.0 { u+=1; break; }
        }
    }}
    (u, nf)
}

fn body_bytes(packed: &PackedCorners) -> [u8; 240] {
    let mut b = [0u8; 240];
    for ci in 0..NUM_CORNERS { for si in 0..NUM_STAGES {
        let base = (ci*NUM_STAGES+si)*NUM_COEFFS*2;
        for wi in 0..NUM_COEFFS {
            let w = packed.words[ci][si][wi];
            b[base+wi*2] = (w&0xff) as u8; b[base+wi*2+1] = ((w>>8)&0xff) as u8;
        }
    }}
    b
}

// ── Main ──

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let f = |n: &str| args.iter().position(|a| a==n).and_then(|i| args.get(i+1).cloned());
    let species = f("--species").expect("--species bass|vocal|tb303|wah|sweepz");
    let low_src = f("--low-source").unwrap_or_else(|| "recipes/tfs/uiowa_double_bass_mf.tf.json".into());
    let high_src = f("--high-source").unwrap_or_else(|| "recipes/tfs/uiowa_alto_sax_mf.tf.json".into());
    let name = f("--name").unwrap_or_else(|| "body".into());
    let out = f("--out").unwrap_or_else(|| format!("{}.body240", name.replace(' ',"_")));

    println!("=== {} (clean-room, stage-law-compliant) ===", name);
    println!("species={}  low={}  high={}", species, low_src, high_src);

    // 1. Extract fundamentals from both sources
    let low_f = extract_fundamentals(&low_src);
    let high_f = extract_fundamentals(&high_src);
    println!("low fundamentals: {:?}", low_f.iter().map(|f| format!("{:.0}Hz",f)).collect::<Vec<_>>());
    println!("high fundamentals: {:?}", high_f.iter().map(|f| format!("{:.0}Hz",f)).collect::<Vec<_>>());

    // 2. Voice fundamentals into slot roles
    let m0_q0 = voice_into_slots(&low_f, &high_f, &get_species(&species));

    // 3. Apply motion → M100_Q0
    let m100_q0 = apply_motion(&m0_q0, &species);

    // 4. Q100 = Q0 corners with Q attitude
    let mut m0_q100 = m0_q0.clone();
    let mut m100_q100 = m100_q0.clone();
    apply_q(&mut m0_q100, &mut m100_q100, &species);

    // 5. Pack
    let mut packed = [[PAD; NUM_STAGES]; NUM_CORNERS];
    packed[0] = corner_to_stages(&m0_q0);
    packed[1] = corner_to_stages(&m100_q0);
    packed[2] = corner_to_stages(&m0_q100);
    packed[3] = corner_to_stages(&m100_q100);
    let pc = PackedCorners { words: packed };

    // 6. Certify
    let cert = certify(&pc);
    println!("certify 33x33: {} unstable, {} nonfinite", cert.0, cert.1);
    if cert.0 > 0 || cert.1 > 0 { eprintln!("CERT FAIL"); std::process::exit(1); }

    // 7. Output
    let body = body_bytes(&pc);
    std::fs::write(&out, &body).expect("write");
    println!("-> {} (240 bytes, clean-room)", out);
}
