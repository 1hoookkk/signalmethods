#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod domain;
mod engine;
mod lab;
mod services;
mod session;
mod ui;

use eframe::egui;

fn main() -> eframe::Result {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let mut case = String::from("workstation");
    let mut fixture = String::from("talking_hedz");
    let mut shot: Option<std::path::PathBuf> = None;
    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "--ui-case" => {
                i += 1;
                if let Some(c) = args.get(i) {
                    case = c.clone();
                }
            }
            "--fixture" => {
                i += 1;
                if let Some(f) = args.get(i) {
                    fixture = f.clone();
                }
            }
            "--shot" => {
                i += 1;
                shot = args.get(i).map(std::path::PathBuf::from);
            }
            _ => {}
        }
        i += 1;
    }

    let mut services = services::Services::new();
    let session = match lab::session_for(&mut services, &fixture) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    };
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([1600.0, 900.0])
            .with_title(format!("TRENCH — {case} · {fixture}")),
        ..Default::default()
    };
    let l = lab::Lab {
        screen: if case == "workstation" {
            lab::Screen::Home
        } else {
            lab::Screen::Fit
        },
        case_name: case,
        shot,
        frames: 0,
        taken: false,
    };
    eframe::run_native(
        "TRENCH",
        options,
        Box::new(move |_cc| {
            Ok(Box::new(ui::app::App {
                session,
                services,
                lab: l,
            }))
        }),
    )
}
