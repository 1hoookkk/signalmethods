use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::{geometry_from_words_at, words_from_geometry_at};

const SR: f64 = 39_062.5;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 2 {
        eprintln!("usage: roundtrip_check <presets dir>");
        std::process::exit(2);
    }
    let mut bins: Vec<_> = std::fs::read_dir(&args[1])
        .expect("presets dir")
        .flatten()
        .map(|e| e.path())
        .collect();
    bins.sort();
    let mut exact = 0;
    let mut total = 0;
    for path in &bins {
        let bytes = std::fs::read(path).unwrap();
        let Ok(packed) = PackedCorners::from_body_bytes(&bytes) else {
            continue;
        };
        total += 1;
        let mut diffs = Vec::new();
        for ci in 0..4 {
            for si in 0..6 {
                let g = geometry_from_words_at(packed.words[ci][si], SR);
                let back = words_from_geometry_at(&g, SR);
                for wi in 0..5 {
                    if back[wi] != packed.words[ci][si][wi] {
                        diffs.push((ci, si, wi, packed.words[ci][si][wi], back[wi]));
                    }
                }
            }
        }
        let name = path.file_stem().unwrap().to_string_lossy();
        if diffs.is_empty() {
            exact += 1;
        } else {
            let (ci, si, wi, a, b) = diffs[0];
            println!(
                "{name}: {} words drift · first at corner {ci} stage {si} word {wi}: {a:#06x} -> {b:#06x}",
                diffs.len()
            );
        }
    }
    println!();
    println!("ROUNDTRIP: {exact} of {total} presets bit-exact through geometry");
}
