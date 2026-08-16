use eframe::egui;

use crate::services::Services;
use crate::session::command::Command;
use crate::session::state::Session;

pub struct App {
    pub session: Session,
    pub services: Services,
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let Self { session, services } = self;

        let mut cmds = egui::CentralPanel::default()
            .frame(egui::Frame::none())
            .show(ctx, |ui| crate::ui::workstation::draw(session, ui))
            .inner;

        // Global hotkeys
        ctx.input(|i| {
            if i.modifiers.command && i.key_pressed(egui::Key::Z) {
                cmds.push(if i.modifiers.shift {
                    Command::Redo
                } else {
                    Command::Undo
                });
            }
        });

        // Apply commands
        for cmd in cmds {
            if let Err(e) = crate::session::command::apply(session, services, cmd) {
                eprintln!("{e}");
                session.notice = Some((true, e));
            }
        }

        if session.audition.playing {
            ctx.request_repaint_after(std::time::Duration::from_millis(30));
        }
    }
}
