use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::{geometry_from_words_at, RootPair};

const SR: f64 = 39_062.5;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 2 {
        eprintln!("usage: real_pair_census <presets dir>");
        std::process::exit(2);
    }
    let mut bins: Vec<_> = std::fs::read_dir(&args[1])
        .expect("presets dir")
        .flatten()
        .map(|e| e.path())
        .collect();
    bins.sort();
    for path in &bins {
        let bytes = std::fs::read(path).unwrap();
        let Ok(packed) = PackedCorners::from_body_bytes(&bytes) else {
            continue;
        };
        let mut real_poles = 0;
        let mut real_zeros = 0;
        let mut degenerate = 0;
        for ci in 0..4 {
            for si in 0..6 {
                let g = geometry_from_words_at(packed.words[ci][si], SR);
                match g.pole {
                    RootPair::RealPair { .. } => real_poles += 1,
                    RootPair::Degenerate => degenerate += 1,
                    RootPair::Conjugate { .. } => {}
                }
                match g.zero {
                    RootPair::RealPair { .. } => real_zeros += 1,
                    RootPair::Degenerate => degenerate += 1,
                    RootPair::Conjugate { .. } => {}
                }
            }
        }
        let name = path.file_stem().unwrap().to_string_lossy();
        if real_poles + real_zeros + degenerate > 0 {
            println!(
                "{name}: {real_poles} real-pair poles · {real_zeros} real-pair zeros · {degenerate} degenerate"
            );
        } else {
            println!("{name}: all conjugate");
        }
    }
}
