use author::recipes::{corner_geometry, corner_to_json, CORNERS};
use serde_json::json;
use trench_core::cascade::NUM_STAGES;
use trench_core::minifloat::{PackedCorners, IDENTITY_STAGE, NUM_CORNERS};
use trench_core::stage_law::{geometry_from_words_at, words_from_geometry_at};

const SR: f64 = 39_062.5;
const LEGACY_STAGES: usize = 6;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: regen_recipes <architectures dir> <presets dir>");
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

    let mut written = 0;
    for path in &arch {
        let stem = path.file_stem().unwrap().to_string_lossy().into_owned();
        let index = &stem[..7];
        let Some(bin_path) = bins.iter().find(|b| {
            b.file_name()
                .is_some_and(|n| n.to_string_lossy().starts_with(index))
        }) else {
            println!("{stem}: no factory binary — left untouched");
            continue;
        };
        let factory = std::fs::read(bin_path).unwrap();
        let packed = PackedCorners::from_body_bytes(&factory).expect("factory body decodes");
        let old: serde_json::Value =
            serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();

        let sections: Vec<serde_json::Value> = (0..LEGACY_STAGES)
            .map(|si| {
                let corners: serde_json::Map<String, serde_json::Value> = CORNERS
                    .iter()
                    .enumerate()
                    .map(|(ci, corner)| {
                        let g = geometry_from_words_at(packed.words[ci][si], SR);
                        (corner.to_string(), corner_to_json(&g))
                    })
                    .collect();
                json!({ "slot": si + 1, "corners": corners })
            })
            .collect();
        let doc = json!({
            "schema": "trench-architecture-v2",
            "index": old.get("index").cloned().unwrap_or_default(),
            "name": old.get("name").cloned().unwrap_or_default(),
            "x3_type": old.get("x3_type").cloned().unwrap_or_default(),
            "datum_sr_hz": SR,
            "source": old.get("source").cloned().unwrap_or_default(),
            "provenance": "regenerated from the factory preset bytes; geometry round-trips bit-exact",
            "sections": sections,
        });

        let mut words = [[IDENTITY_STAGE; NUM_STAGES]; NUM_CORNERS];
        let mut verified = true;
        for (ci, corner) in CORNERS.iter().enumerate() {
            for (si, section) in doc["sections"].as_array().unwrap().iter().enumerate() {
                let Some(g) = corner_geometry(section, corner) else {
                    verified = false;
                    continue;
                };
                words[ci][si] = words_from_geometry_at(&g, SR);
                words[ci + 4][si] = words[ci][si];
            }
        }
        let ours = PackedCorners { words }
            .to_legacy_bytes()
            .expect("regenerated body is legacy-representable");
        if !verified || ours[..] != factory[..] {
            println!("{stem}: DOES NOT NULL — file left untouched");
            continue;
        }
        std::fs::write(path, serde_json::to_string_pretty(&doc).unwrap()).unwrap();
        written += 1;
    }
    println!("regenerated {written} of {} recipes, each null-verified", arch.len());
}
