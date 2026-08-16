use author::{envelope, formants};
use serde_json::json;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: mouth_formants <dvtd dir> <out.json>");
        std::process::exit(2);
    }
    let files = envelope::scan(std::path::Path::new(&args[1]));
    let grid = envelope::grid();
    let mut mouths = Vec::new();
    for (file, path) in &files {
        let name = file.trim_end_matches("-vvtf-measured.txt").to_string();
        let curve = match envelope::read_vvtf(path) {
            Ok(c) => envelope::on_grid(&c),
            Err(e) => {
                eprintln!("{e}");
                continue;
            }
        };
        let peaks: Vec<serde_json::Value> = formants::peaks(&grid, &curve)
            .iter()
            .map(|p| {
                json!({
                    "hz": (p.hz * 10.0).round() / 10.0,
                    "db": (p.db * 100.0).round() / 100.0,
                    "bandwidth_hz": (p.bandwidth_hz * 10.0).round() / 10.0,
                })
            })
            .collect();
        println!("{name}: {} peaks", peaks.len());
        mouths.push(json!({ "name": name, "peaks": peaks }));
    }
    let doc = json!({
        "schema": "trench-mouth-formants-v1",
        "source": "peak-picked from the measured DVTD vocal-tract transfer functions on the fixed 1024-point log grid",
        "method": "5-point smoothed local maxima, prominence >= 6 dB, 120..5500 Hz, bandwidth from the -3 dB width, peaks closer than 3 st merged",
        "mouths": mouths,
    });
    std::fs::write(&args[2], serde_json::to_string_pretty(&doc).unwrap()).unwrap();
    println!("wrote {} ({} mouths)", args[2], files.len());
}
