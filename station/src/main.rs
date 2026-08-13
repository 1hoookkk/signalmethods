mod app;
mod body;
mod cube;
mod law;
mod laws_panel;
mod paint;
mod theme;

use std::path::PathBuf;

fn main() -> eframe::Result<()> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let body_path = PathBuf::from(a.first().cloned().unwrap_or_else(|| {
        "ref/presets/P2k_013_talking_hedz.bin".into()
    }));
    let laws_path = PathBuf::from(a.get(1).cloned().unwrap_or_else(|| "station/laws.json".into()));
    let body = match body::Body::load(&body_path, 44_100.0) {
        Ok(b) => b,
        Err(e) => { eprintln!("{e}"); std::process::exit(2); }
    };
    eframe::run_native(
        "station",
        eframe::NativeOptions::default(),
        Box::new(|_| Ok(Box::new(app::Station::new(body, laws_path)))),
    )
}
