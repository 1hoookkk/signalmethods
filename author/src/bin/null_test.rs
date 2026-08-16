use author::recipes::{corner_geometry, CORNERS};
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{stage_words_to_biquad, PackedCorners, IDENTITY_STAGE, NUM_CORNERS};
use trench_core::stage_law::words_from_geometry_at;

const SR: f64 = 39_062.5;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: null_test <architectures dir> <presets dir>");
        std::process::exit(2);
    }
    let mut arch: Vec<_> = std::fs::read_dir(&args[1])
        .expect("architectures dir")
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x == "json"))
        .collect();
    arch.sort();
    let bins: Vec<_> = std::fs::read_dir(&args[2])
        .expect("presets dir")
        .flatten()
        .map(|e| e.path())
        .collect();

    let mut nulled = 0;
    let mut total = 0;
    for path in &arch {
        let stem = path.file_stem().unwrap().to_string_lossy();
        let index = &stem[..7];
        let Some(bin_path) = bins.iter().find(|b| {
            b.file_name()
                .is_some_and(|n| n.to_string_lossy().starts_with(index))
        }) else {
            println!("{stem}: no factory binary");
            continue;
        };
        let doc: serde_json::Value =
            serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
        let Some(sections) = doc.get("sections").and_then(|v| v.as_array()) else {
            println!("{stem}: no sections");
            continue;
        };
        let mut words = [[IDENTITY_STAGE; NUM_STAGES]; NUM_CORNERS];
        let mut complete = true;
        for (ci, corner) in CORNERS.iter().enumerate() {
            for (si, section) in sections.iter().enumerate().take(NUM_STAGES) {
                let Some(g) = corner_geometry(section, corner) else {
                    complete = false;
                    continue;
                };
                words[ci][si] = words_from_geometry_at(&g, SR);
                words[ci + 4][si] = words[ci][si];
            }
        }
        if !complete {
            println!("{stem}: incomplete recipe");
            continue;
        }
        total += 1;
        let packed = PackedCorners { words };
        let Some(ours) = packed.to_legacy_bytes() else {
            println!("{stem}: not legacy-representable");
            continue;
        };
        let factory = std::fs::read(bin_path).unwrap();
        if ours[..] == factory[..] {
            nulled += 1;
            continue;
        }
        let mut diff_words = 0;
        let mut max_delta = 0i32;
        for k in (0..240).step_by(2) {
            let a = u16::from_le_bytes([ours[k], ours[k + 1]]);
            let b = u16::from_le_bytes([factory[k], factory[k + 1]]);
            if a != b {
                diff_words += 1;
                max_delta = max_delta.max((a as i32 - b as i32).abs());
            }
        }
        let theirs = PackedCorners::from_body_bytes(&factory).unwrap();
        let mut max_db = 0.0f64;
        for ci in 0..4 {
            let mut a_rows = [[0.0; NUM_COEFFS]; NUM_STAGES];
            let mut b_rows = [[0.0; NUM_COEFFS]; NUM_STAGES];
            for si in 0..NUM_STAGES {
                a_rows[si] = stage_words_to_biquad(packed.words[ci][si]);
                b_rows[si] = stage_words_to_biquad(theirs.words[ci][si]);
            }
            for k in 0..256 {
                let hz = 40.0 * (16_000.0f64 / 40.0).powf(k as f64 / 255.0);
                let w = std::f64::consts::TAU * hz / SR;
                let (cw, sw) = (w.cos(), w.sin());
                let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
                let db = |rows: &[[f64; NUM_COEFFS]; NUM_STAGES]| -> f64 {
                    rows.iter()
                        .map(|r| {
                            let nr = r[0] + r[1] * cw + r[2] * c2;
                            let ni = -(r[1] * sw + r[2] * s2);
                            let dr = 1.0 + r[3] * cw + r[4] * c2;
                            let di = -(r[3] * sw + r[4] * s2);
                            10.0 * ((nr * nr + ni * ni).max(1e-30)
                                / (dr * dr + di * di).max(1e-30))
                            .log10()
                        })
                        .sum()
                };
                max_db = max_db.max((db(&a_rows) - db(&b_rows)).abs());
            }
        }
        println!(
            "{stem}: {diff_words}/120 words differ · max word delta {max_delta} · response delta {max_db:.4} dB"
        );
    }
    println!();
    println!("NULL: {nulled} of {total} presets are bit-identical from the high-level form");
}
