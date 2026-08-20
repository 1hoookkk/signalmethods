use egui::{vec2, RichText, Ui};

use crate::document::SECTIONS;
use crate::theme::Theme;

pub struct Model<'a> {
    pub selected: Option<usize>,
    pub unused: &'a [bool; SECTIONS],
    pub held: &'a [bool; SECTIONS],
}

pub fn show(ui: &mut Ui, theme: &Theme, model: &Model<'_>) -> Option<usize> {
    let mut picked = None;
    ui.horizontal(|ui| {
        ui.spacing_mut().item_spacing.x = 1.0;
        ui.label(RichText::new("IN").font(theme.num()).color(theme.chrome.ink_dim));
        let cell = vec2(64.0, theme.metrics.cell_h);
        for si in 0..SECTIONS {
            let current = model.selected == Some(si);
            let ink = if model.unused[si] {
                theme.data.stage[si].gamma_multiply(0.42)
            } else {
                theme.data.stage[si]
            };
            let text = RichText::new(format!("S{}", si + 1))
                .font(theme.num())
                .color(ink);
            let button = egui::Button::selectable(current, text)
                .min_size(cell)
                .fill(theme.well.bg)
                .stroke(if current {
                    egui::Stroke::new(1.5, theme.chrome.select_ink)
                } else {
                    egui::Stroke::new(theme.metrics.border_px, theme.well.rule)
                })
                .corner_radius(theme.radius());
            let r = ui.add(button);
            if r.clicked() {
                picked = Some(si);
            }
            if model.held[si] {
                let bar = egui::Rect::from_min_max(
                    egui::pos2(r.rect.left(), r.rect.bottom() - 2.0),
                    egui::pos2(r.rect.right(), r.rect.bottom()),
                );
                ui.painter().rect_filled(bar, 0, theme.chrome.select_ink);
            }
        }
        ui.label(RichText::new("OUT").font(theme.num()).color(theme.chrome.ink_dim));
    });
    picked
}
