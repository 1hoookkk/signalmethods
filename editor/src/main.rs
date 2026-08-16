#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod domain;
mod engine;
mod lab;
mod services;
mod session;
mod ui;

use eframe::egui;

fn main() -> eframe::Result {
    let services = services::Services::new();
    let session = session::state::Session::new();

    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([1600.0, 960.0])
            .with_min_inner_size([1200.0, 700.0])
            .with_title("TRENCH WORKSTATION // Z-PLANE AUTHORING"),
        ..Default::default()
    };

    eframe::run_native(
        "TRENCH",
        options,
        Box::new(move |_cc| {
            Ok(Box::new(ui::app::App {
                session,
                services,
            }))
        }),
    )
}
