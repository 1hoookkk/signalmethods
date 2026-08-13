use std::path::PathBuf;

use eframe::egui::{self, Key, Pos2, Rect, Vec2};

use crate::body::Body;
use crate::law::{self, Law};
use crate::paint::{fill, label, label_right};
use crate::{cube, laws_panel, theme};

pub struct Station {
    pub body: Body,
    pub laws: Vec<Law>,
    pub laws_path: PathBuf,
    pub selected: usize,
    pub m: f32,
    pub q: f32,
    pub z: f32,
    pub note: String,
}

impl Station {
    pub fn new(body: Body, laws_path: PathBuf) -> Self {
        let (laws, note) = match law::load(&laws_path) {
            Ok(l) => (l, String::new()),
            Err(e) => (Vec::new(), e),
        };
        Self { body, laws, laws_path, selected: 0, m: 0.5, q: 0.0, z: 0.0, note }
    }

    fn reload(&mut self) {
        match law::load(&self.laws_path) {
            Ok(l) => {
                self.note = format!("{} laws reloaded", l.len());
                self.laws = l;
            }
            Err(e) => self.note = e,
        }
    }

    fn keys(&mut self, ctx: &egui::Context) {
        ctx.input(|i| {
            if i.key_pressed(Key::R) {
                self.reload();
            }
            let step = if i.modifiers.shift { 0.01 } else { 0.05 };
            if i.key_down(Key::ArrowRight) { self.m = (self.m + step).min(1.0); }
            if i.key_down(Key::ArrowLeft) { self.m = (self.m - step).max(0.0); }
            if i.key_pressed(Key::Q) { self.q = if self.q > 0.5 { 0.0 } else { 1.0 }; }
            if i.key_pressed(Key::T) { self.z = if self.z > 0.5 { 0.0 } else { 1.0 }; }
        });
    }
}

impl eframe::App for Station {
    fn update(&mut self, ctx: &egui::Context, _f: &mut eframe::Frame) {
        self.keys(ctx);
        let frame = egui::Frame { fill: theme::BG, ..Default::default() };
        egui::CentralPanel::default().frame(frame).show(ctx, |ui| {
            let (resp, p) = ui.allocate_painter(ui.available_size(), egui::Sense::click_and_drag());
            let full = resp.rect;
            fill(&p, full, theme::BG);

            let head = Rect::from_min_size(full.min, Vec2::new(full.width(), 26.0));
            label(&p, head.left_top() + Vec2::new(10.0, 6.0), "STATION", 13.0, theme::INK);
            label(
                &p,
                head.left_top() + Vec2::new(84.0, 8.0),
                format!(
                    "{}   {} bytes   {} sections   {} frames   {}",
                    self.body.path.file_name().unwrap_or_default().to_string_lossy(),
                    self.body.bytes,
                    self.body.stages(),
                    if self.body.is_cube() { 8 } else { 4 },
                    if self.body.is_cube() { "cube" } else { "square" },
                ),
                10.0,
                theme::DIM,
            );
            label_right(
                &p,
                Pos2::new(full.right() - 10.0, head.top() + 8.0),
                if self.note.is_empty() {
                    "arrows MORPH   Q  T2   R reload laws".into()
                } else {
                    self.note.clone()
                },
                10.0,
                theme::DIM,
            );

            let body_area = Rect::from_min_max(
                Pos2::new(full.left() + 10.0, head.bottom() + 4.0),
                Pos2::new(full.right() - 10.0, full.bottom() - 10.0),
            );
            let laws_w = body_area.width() * 0.34;
            let cube_w = body_area.width() - laws_w - 10.0;

            let cube_r = Rect::from_min_size(
                body_area.min,
                Vec2::new(cube_w, body_area.height()),
            );
            let hits = cube::draw(&p, cube_r, &self.body, self.selected);


            let laws_r = Rect::from_min_size(
                Pos2::new(cube_r.right() + 10.0, body_area.top()),
                Vec2::new(laws_w, body_area.height()),
            );
            let readings = law::read_all(&self.body, &self.laws, self.selected);
            laws_panel::draw(&p, laws_r, &readings, &self.laws_path.display().to_string());

            if let Some(pos) = resp.interact_pointer_pos() {
                for (rect, i) in hits {
                    if rect.contains(pos) {
                        self.selected = i;
                        let (m, q, z) = Body::frame_axes(i);
                        self.m = m;
                        self.q = q;
                        self.z = z;
                    }
                }
            }
        });
        ctx.request_repaint();
    }
}
