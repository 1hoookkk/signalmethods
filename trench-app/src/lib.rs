pub mod app;
pub mod audio;
pub mod derived;
pub mod document;
pub mod fit;
pub mod gpu;
pub mod library;
pub mod plot;
pub mod theme;
pub mod view;

pub fn run() -> eframe::Result {
    let options = eframe::NativeOptions {
        viewport: eframe::egui::ViewportBuilder::default()
            .with_inner_size([1600.0, 1000.0])
            .with_min_inner_size([1100.0, 700.0])
            .with_title("TRENCH"),
        ..Default::default()
    };
    eframe::run_native(
        "trench",
        options,
        Box::new(|cc| Ok(Box::new(app::Author::new(cc)))),
    )
}
