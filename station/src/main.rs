mod app;
mod model;
mod ui;
mod views;
mod views_cube;
mod views_ingest;
mod views_perceptual;
mod views_runtime;

use std::path::PathBuf;

use model::project::Project;
use model::topology::Topology;

fn main() -> eframe::Result<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let laws_path = PathBuf::from(
        args.get(1)
            .cloned()
            .unwrap_or_else(|| "station/laws.json".into()),
    );

    // An object may be named on the command line; without one the Station opens
    // on an empty project rather than refusing to start.
    let project = match args.first() {
        Some(a) => {
            let path = PathBuf::from(a);
            let name = path
                .file_stem()
                .map(|s| s.to_string_lossy().to_string())
                .unwrap_or_else(|| "imported".into());
            match std::fs::read(&path)
                .map_err(|e| format!("{}: {e}", path.display()))
                .and_then(|raw| Project::from_packed(name, &raw, 44_100.0))
            {
                Ok(p) => p,
                Err(e) => {
                    eprintln!("{e}");
                    std::process::exit(2);
                }
            }
        }
        None => Project::blank("untitled", Topology::packed_runtime(), 7),
    };

    let options = eframe::NativeOptions {
        viewport: eframe::egui::ViewportBuilder::default()
            .with_inner_size([1440.0, 900.0])
            .with_min_inner_size([1100.0, 700.0])
            .with_title("Station"),
        ..Default::default()
    };
    eframe::run_native(
        "Station",
        options,
        Box::new(|_| Ok(Box::new(app::Station::new(project, laws_path)))),
    )
}
