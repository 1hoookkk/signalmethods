mod app;
mod model;
mod ui;
mod view;

use std::path::PathBuf;

use model::project::Project;

fn main() -> eframe::Result<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();

    // `--new 4d` / `--new cube` starts on a new object of that form.
    if let Some(i) = args.iter().position(|a| a == "--new") {
        let form = match args.get(i + 1).map(|s| s.as_str()) {
            Some("cube") => model::object::ObjectForm::Cube,
            _ => model::object::ObjectForm::Square,
        };
        let project = Project::new_object(format!("untitled.{}", form.extension()), form, 44_100.0);
        return run(project);
    }

    // A named object opens on start; without one the Station opens empty.
    let project = match args.first() {
        Some(a) => {
            let path = PathBuf::from(a);
            let name = path
                .file_name()
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
        None => Project::empty(),
    };

    run(project)
}

fn run(project: Project) -> eframe::Result<()> {
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
        Box::new(|cc| {
            ui::theme::install_fonts(&cc.egui_ctx);
            Ok(Box::new(app::Station::new(project)))
        }),
    )
}
