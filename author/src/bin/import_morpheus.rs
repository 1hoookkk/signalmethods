use std::path::PathBuf;

use trench_core::cascade::NUM_STAGES;
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::{
    geometry_from_words_at, words_from_geometry_at, words_from_roots_at, RootPair, StageRoots,
    DEFAULT_AUTHORING_SR,
};

const R_MIN: f64 = 1e-6;

fn main() {
    let doc: serde_json::Value = serde_json::from_str(
        &std::fs::read_to_string("ref/morpheus/cubes_decoded.json")
            .expect("ref/morpheus/cubes_decoded.json"),
    )
    .expect("parse cubes_decoded.json");
    let out_dir = PathBuf::from("ref/morpheus/bodies");
    std::fs::create_dir_all(&out_dir).expect("create ref/morpheus/bodies");

    let sr = DEFAULT_AUTHORING_SR;
    let cubes = doc["cubes"].as_array().expect("cubes array");
    assert_eq!(cubes.len(), 289);

    let mut written = 0usize;
    let mut idem_fail = 0usize;
    let mut audit_fail: Vec<String> = Vec::new();
    let mut worst_cents = (0.0f64, String::new());
    let mut worst_dr = (0.0f64, String::new());
    let mut worst_scale_db = (0.0f64, String::new());
    let mut worst_coef = (0.0f64, String::new());
    let mut census = String::from("index\tname\tsections\taudit\tfailures\n");

    for cube in cubes {
        let index = cube["index"].as_u64().unwrap() as usize;
        let name = cube["name"].as_str().unwrap();
        let mcorners = cube["corners"].as_array().unwrap();

        let mut corners = [[StageRoots::IDENTITY; NUM_STAGES]; NUM_CORNERS];
        let mut max_sections = 0usize;
        for tc in 0..NUM_CORNERS {
            let (m, q, z) = (tc & 1, (tc >> 1) & 1, (tc >> 2) & 1);
            let mc = z | (m << 1) | (q << 2);
            let corner = &mcorners[mc];
            let gain = corner["gain"].as_f64().unwrap();
            let sections = corner["sections"].as_array().unwrap();
            let mut roots = [StageRoots::IDENTITY; NUM_STAGES];
            let mut active = Vec::new();
            for (si, sec) in sections.iter().enumerate() {
                let raw: Vec<i64> = sec["raw"]
                    .as_array()
                    .unwrap()
                    .iter()
                    .map(|v| v.as_i64().unwrap())
                    .collect();
                let (phz, pr) = (
                    sec["pole"]["hz"].as_f64().unwrap(),
                    sec["pole"]["r"].as_f64().unwrap().max(0.0),
                );
                let (zhz, zr) = (
                    sec["zero"]["hz"].as_f64().unwrap(),
                    sec["zero"]["r"].as_f64().unwrap().max(0.0),
                );
                let null_stage = (raw[0] == raw[2] && raw[1] == raw[3])
                    || (pr <= R_MIN && zr <= R_MIN);
                if null_stage {
                    continue;
                }
                let r = StageRoots {
                    pole_hz: if pr > R_MIN { phz } else { 0.0 },
                    pole_r: if pr > R_MIN { pr } else { 0.0 },
                    zero_hz: if zr > R_MIN { zhz } else { 0.0 },
                    zero_r: if zr > R_MIN { zr } else { 0.0 },
                    scale: 1.0,
                };
                roots[si] = r;
                active.push(si);
            }
            if active.is_empty() {
                roots[0].scale = gain;
            } else {
                let per = gain.powf(1.0 / active.len() as f64);
                for &si in &active {
                    roots[si].scale = per;
                }
            }
            max_sections = max_sections.max(active.len());
            corners[tc] = roots;
        }

        let mut words = [[[0u16; 5]; NUM_STAGES]; NUM_CORNERS];
        for (ci, corner) in corners.iter().enumerate() {
            for (si, r) in corner.iter().enumerate() {
                words[ci][si] = words_from_roots_at(r, sr);
            }
        }
        let packed = PackedCorners { words };

        for (ci, corner) in corners.iter().enumerate() {
            for (si, src) in corner.iter().enumerate() {
                let g = geometry_from_words_at(words[ci][si], sr);
                let again = words_from_geometry_at(&g, sr);
                if again != words[ci][si] {
                    idem_fail += 1;
                }
                let loc = format!("{name} c{ci} S{}", si + 1);
                if let RootPair::Conjugate { hz, r } = g.pole {
                    if src.pole_r > 0.9 && src.pole_hz > 20.0 && hz > 20.0 {
                        let cents = 1200.0 * (hz / src.pole_hz).log2();
                        if cents.abs() > worst_cents.0.abs() {
                            worst_cents = (cents, loc.clone());
                        }
                    }
                    if src.pole_r > R_MIN {
                        let dr = r - src.pole_r;
                        if dr.abs() > worst_dr.0.abs() {
                            worst_dr = (dr, loc.clone());
                        }
                    }
                }
                if let RootPair::Conjugate { hz, r } = g.zero {
                    if src.zero_r > 0.9 && src.zero_hz > 20.0 && hz > 20.0 {
                        let cents = 1200.0 * (hz / src.zero_hz).log2();
                        if cents.abs() > worst_cents.0.abs() {
                            worst_cents = (cents, loc.clone());
                        }
                    }
                    if src.zero_r > R_MIN {
                        let dr = r - src.zero_r;
                        if dr.abs() > worst_dr.0.abs() {
                            worst_dr = (dr, loc.clone());
                        }
                    }
                }
                let src_bq = src.biquad_at(sr);
                let back_bq = g.biquad_at(sr);
                for (a, b) in src_bq.iter().zip(back_bq.iter()) {
                    let d = (b - a).abs();
                    if d > worst_coef.0 {
                        worst_coef = (d, loc.clone());
                    }
                }
                if src.scale > 0.0 && g.scale > 0.0 {
                    let db = 20.0 * (g.scale / src.scale).log10();
                    if db.abs() > worst_scale_db.0.abs() {
                        worst_scale_db = (db, loc.clone());
                    }
                }
            }
        }

        let audit = author::body::audit(&packed, sr);
        let verdict = if audit.pass() { "pass" } else { "FAIL" };
        if !audit.pass() {
            audit_fail.push(format!("{name}: {}", audit.failures.join("; ")));
        }
        census.push_str(&format!(
            "{index}\t{name}\t{max_sections}\t{verdict}\t{}\n",
            audit.failures.join("; ")
        ));

        let safe: String = name
            .chars()
            .map(|c| if c.is_ascii_alphanumeric() || c == '.' || c == '-' { c } else { '_' })
            .collect();
        let path = out_dir.join(format!("{index:03}_{safe}.body"));
        std::fs::write(&path, packed.to_native_bytes()).expect("write body");
        written += 1;
    }

    std::fs::write(out_dir.join("import_census.tsv"), census).expect("write census");
    println!("wrote {written} bodies to ref/morpheus/bodies/");
    println!("re-encode idempotence failures: {idem_fail}");
    println!(
        "worst resonant-root (r>0.9) frequency error: {:+.2} cents at {}",
        worst_cents.0, worst_cents.1
    );
    println!("worst radius error: {:+.6} at {}", worst_dr.0, worst_dr.1);
    println!(
        "worst scale error: {:+.4} dB at {}",
        worst_scale_db.0, worst_scale_db.1
    );
    println!(
        "worst biquad coefficient error: {:.6} at {}",
        worst_coef.0, worst_coef.1
    );
    println!("audit failures: {}", audit_fail.len());
    for f in &audit_fail {
        println!("  {f}");
    }
}
