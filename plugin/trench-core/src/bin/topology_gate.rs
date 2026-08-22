use std::path::PathBuf;
use trench_core::cascade::{Cascade, NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::PackedCorners;

const LEGACY_STAGES: usize = 6;
const IDENTITY_WORDS: [u16; 5] = [0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF];
const GRID: usize = 11;
const RENDER_SAMPLES: usize = 4096;
const RENDER_POINTS: [(f32, f32); 5] = [
    (0.0, 0.0),
    (1.0, 0.0),
    (0.0, 1.0),
    (1.0, 1.0),
    (0.37, 0.61),
];

/// The frozen reference set. New work goes elsewhere; these must never move.
const REFERENCE_DIRS: [(&str, &str); 3] = [
    ("ref/presets", "bin"),
    ("filters/bodies", "body240"),
    ("plugin/assets/bodies", "body240"),
];

fn corpus() -> Vec<PathBuf> {
    let mut out = Vec::new();
    for (dir, ext) in REFERENCE_DIRS {
        let Ok(rd) = std::fs::read_dir(dir) else {
            continue;
        };
        for entry in rd.flatten() {
            let p = entry.path();
            if p.extension().map(|e| e == ext).unwrap_or(false) {
                out.push(p);
            }
        }
    }
    out.sort();
    out
}

fn signal(n: usize) -> Vec<f32> {
    let mut rng = 0x2545_F491_4F6C_DD1Du64;
    (0..n)
        .map(|i| {
            rng = rng
                .wrapping_mul(6364136223846793005)
                .wrapping_add(1442695040888963407);
            let noise = ((rng >> 40) as f64 / (1u64 << 23) as f64) - 1.0;
            let tone = (i as f64 * 0.0731).sin();
            ((noise * 0.5 + tone * 0.5) * 0.25) as f32
        })
        .collect()
}

fn main() {
    let dst = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "topology_gate.bin".to_string());
    let files = corpus();
    let mut blob: Vec<u8> = Vec::new();
    let mut loaded = 0usize;
    let mut refused: Vec<String> = Vec::new();
    let mut tail_checked = 0usize;

    for path in &files {
        let name = path.file_name().unwrap().to_string_lossy().to_string();
        let bytes = std::fs::read(path).unwrap();
        let packed = match PackedCorners::from_body_bytes(&bytes) {
            Ok(p) => p,
            Err(e) => {
                refused.push(format!("{name}: {e}"));
                continue;
            }
        };
        loaded += 1;
        blob.extend_from_slice(name.as_bytes());
        blob.push(0);

        for mi in 0..GRID {
            for qi in 0..GRID {
                let m = mi as f32 / (GRID - 1) as f32;
                let q = qi as f32 / (GRID - 1) as f32;
                let words = packed.interpolate_words(m, q, 0.0);
                for si in 0..LEGACY_STAGES {
                    for wi in 0..NUM_COEFFS {
                        blob.extend_from_slice(&words[si][wi].to_le_bytes());
                    }
                }
                for si in LEGACY_STAGES..NUM_STAGES {
                    assert_eq!(
                        words[si], IDENTITY_WORDS,
                        "{name} m={m} q={q}: stage {si} is not the identity stage"
                    );
                    tail_checked += 1;
                }
            }
        }

        for (m, q) in RENDER_POINTS {
            let rows = packed.interpolate_biquad(m, q, 0.0);
            let mut cascade = Cascade::new();
            cascade.snap_targets(&rows);
            let mut buf = signal(RENDER_SAMPLES);
            cascade.process_block_mono(&mut buf);
            for s in &buf {
                blob.extend_from_slice(&s.to_bits().to_le_bytes());
            }
        }
    }

    std::fs::write(&dst, &blob).unwrap();
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for b in &blob {
        h ^= *b as u64;
        h = h.wrapping_mul(0x0000_0100_0000_01b3);
    }
    println!("topology gate");
    println!("  NUM_STAGES        {NUM_STAGES}");
    println!("  corners           {}", packed_corner_count());
    println!("  bodies found      {}", files.len());
    println!("  bodies loaded     {loaded}");
    println!("  bodies refused    {}", refused.len());
    for r in &refused {
        println!("      {r}");
    }
    println!("  identity-tail rows checked {tail_checked}");
    println!("  bytes             {}", blob.len());
    println!("  fnv1a64           {h:016x}");
    println!("  wrote             {dst}");
}

fn packed_corner_count() -> usize {
    std::mem::size_of::<PackedCorners>() / (NUM_STAGES * NUM_COEFFS * 2)
}
