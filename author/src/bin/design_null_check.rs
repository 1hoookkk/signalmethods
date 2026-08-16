use author::design::{compile, decompile, pack, Alphabet};
use trench_core::minifloat::PackedCorners;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 2 {
        eprintln!("usage: design_null_check <presets dir>");
        std::process::exit(2);
    }
    let mut bins: Vec<_> = std::fs::read_dir(&args[1])
        .expect("presets dir")
        .flatten()
        .map(|e| e.path())
        .collect();
    bins.sort();
    let alphabet = Alphabet::empty();
    let tmp = std::env::temp_dir().join("trench-design-null-check.json");
    let mut nulled = 0;
    let mut total = 0;
    for path in &bins {
        let bytes = std::fs::read(path).unwrap();
        let Ok(packed) = PackedCorners::from_body_bytes(&bytes) else {
            continue;
        };
        total += 1;
        let name = path.file_stem().unwrap().to_string_lossy().into_owned();
        let doc = decompile(&packed, &name);
        std::fs::write(&tmp, serde_json::to_string(&doc).unwrap()).unwrap();
        match compile(&tmp, &alphabet) {
            Ok(design) => {
                if pack(&design).words == packed.words {
                    nulled += 1;
                } else {
                    println!("{name}: recompiled words differ");
                }
            }
            Err(e) => println!("{name}: {e}"),
        }
    }
    println!("DESIGN NULL: {nulled} of {total} factory presets round-trip bit-exact through the design form");
}
