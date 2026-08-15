use eframe::egui;

use crate::lab::Lab;
use crate::services::Services;
use crate::session::state::Session;

pub struct App {
    pub session: Session,
    pub services: Services,
    pub lab: Lab,
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let Self {
            session,
            services,
            lab,
        } = self;
        services.jobs.poll(session, &mut services.audio);
        lab.frames += 1;
        egui::CentralPanel::default()
            .frame(egui::Frame::none())
            .show(ctx, |ui| {
                crate::lab::draw(session, &lab.case_name, ui);
            });
        if lab.shot.is_some() && !lab.taken {
            if lab.frames == 8 {
                ctx.send_viewport_cmd(egui::ViewportCommand::Screenshot);
            }
            let shot = ctx.input(|i| {
                i.raw.events.iter().find_map(|e| match e {
                    egui::Event::Screenshot { image, .. } => Some(image.clone()),
                    _ => None,
                })
            });
            if let Some(img) = shot {
                let path = lab.shot.clone().unwrap();
                match crate::lab::save_png(&path, &img) {
                    Ok(()) => println!("shot {}", path.display()),
                    Err(e) => eprintln!("shot failed: {e}"),
                }
                lab.taken = true;
            }
            ctx.request_repaint();
        }
    }
}
