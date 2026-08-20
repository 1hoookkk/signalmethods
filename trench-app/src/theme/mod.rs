pub mod flat;

use egui::{Color32, CornerRadius, FontFamily, FontId, Stroke};

pub const STAGES: usize = 7;

pub struct Chrome {
    pub face: Color32,
    pub face_hi: Color32,
    pub face_lo: Color32,
    pub rule: Color32,
    pub ink: Color32,
    pub ink_dim: Color32,
    pub select_fill: Color32,
    pub select_ink: Color32,
}

pub struct Well {
    pub bg: Color32,
    pub rule: Color32,
    pub grid: Color32,
    pub ink: Color32,
    pub dim: Color32,
}

pub struct DataInk {
    pub stage: [Color32; STAGES],
    pub target: Color32,
    pub target_fill: Color32,
    pub live: Color32,
    pub preview: Color32,
    pub candidate: Color32,
    pub error: Color32,
    pub crown: Color32,
}

pub struct Metrics {
    pub corner_radius: u8,
    pub border_px: f32,
    pub bar_h: f32,
    pub row_h: f32,
    pub cell_h: f32,
    pub gap: f32,
    pub well_pad: f32,
}

pub struct Theme {
    pub name: &'static str,
    pub chrome: Chrome,
    pub well: Well,
    pub data: DataInk,
    pub metrics: Metrics,
}

pub fn theme(name: &str) -> Theme {
    match name {
        _ => flat::theme(),
    }
}

impl Theme {
    pub fn radius(&self) -> CornerRadius {
        CornerRadius::same(self.metrics.corner_radius)
    }

    pub fn num(&self) -> FontId {
        FontId::new(13.0, FontFamily::Monospace)
    }

    pub fn label(&self) -> FontId {
        FontId::new(12.0, FontFamily::Proportional)
    }

    pub fn head(&self) -> FontId {
        FontId::new(16.0, FontFamily::Proportional)
    }

    pub fn big(&self) -> FontId {
        FontId::new(14.0, FontFamily::Monospace)
    }

    pub fn selection_stroke(&self, on: bool) -> Stroke {
        if on {
            Stroke::new(self.metrics.border_px * 2.0, self.chrome.select_ink)
        } else {
            Stroke::new(self.metrics.border_px, self.chrome.rule)
        }
    }

    pub fn bar_frame(&self) -> egui::Frame {
        egui::Frame::NONE
            .fill(self.chrome.face)
            .inner_margin(egui::Margin::symmetric(6, 2))
    }

    pub fn dock_frame(&self) -> egui::Frame {
        egui::Frame::NONE
            .fill(self.chrome.face)
            .inner_margin(egui::Margin::same(6))
    }

    pub fn apply(&self, ctx: &egui::Context) {
        let radius = self.radius();
        let border = self.metrics.border_px;
        let m = &self.metrics;
        ctx.all_styles_mut(|s| {
            s.visuals = egui::Visuals::light();
            s.visuals.panel_fill = self.chrome.face;
            s.visuals.window_fill = self.chrome.face;
            s.visuals.extreme_bg_color = self.well.bg;
            s.visuals.faint_bg_color = self.chrome.face_lo;
            s.visuals.window_shadow = egui::epaint::Shadow::NONE;
            s.visuals.popup_shadow = egui::epaint::Shadow::NONE;
            s.visuals.window_corner_radius = radius;
            s.visuals.menu_corner_radius = radius;
            s.visuals.window_stroke = Stroke::new(border, self.chrome.rule);
            s.visuals.button_frame = true;
            s.visuals.striped = false;
            s.visuals.slider_trailing_fill = true;
            s.visuals.handle_shape = egui::style::HandleShape::Rect { aspect_ratio: 0.30 };
            s.visuals.selection.bg_fill = self.chrome.select_fill;
            s.visuals.selection.stroke = Stroke::new(border, self.chrome.select_ink);

            for w in [
                &mut s.visuals.widgets.noninteractive,
                &mut s.visuals.widgets.inactive,
                &mut s.visuals.widgets.hovered,
                &mut s.visuals.widgets.active,
                &mut s.visuals.widgets.open,
            ] {
                w.corner_radius = radius;
                w.expansion = 0.0;
                w.bg_stroke = Stroke::new(border, self.chrome.rule);
                w.fg_stroke = Stroke::new(1.0, self.chrome.ink);
            }
            s.visuals.widgets.noninteractive.bg_stroke = Stroke::new(border, self.chrome.rule);
            s.visuals.widgets.noninteractive.fg_stroke = Stroke::new(1.0, self.chrome.ink_dim);
            s.visuals.widgets.inactive.bg_fill = self.chrome.face;
            s.visuals.widgets.inactive.weak_bg_fill = self.chrome.face;
            s.visuals.widgets.hovered.bg_fill = self.chrome.face_hi;
            s.visuals.widgets.hovered.weak_bg_fill = self.chrome.face_hi;
            s.visuals.widgets.active.bg_fill = self.chrome.face_lo;
            s.visuals.widgets.active.weak_bg_fill = self.chrome.face_lo;
            s.visuals.widgets.open.bg_fill = self.chrome.face_lo;
            s.visuals.widgets.open.weak_bg_fill = self.chrome.face_lo;

            s.spacing.item_spacing = egui::vec2(m.gap, m.gap);
            s.spacing.button_padding = egui::vec2(8.0, 5.0);
            s.spacing.interact_size = egui::vec2(0.0, 22.0);
            s.spacing.slider_width = 96.0;
            s.spacing.indent = 10.0;

            for (style, font) in [
                (egui::TextStyle::Small, FontId::new(10.0, FontFamily::Proportional)),
                (egui::TextStyle::Body, self.label()),
                (egui::TextStyle::Button, self.label()),
                (egui::TextStyle::Monospace, self.big()),
                (egui::TextStyle::Heading, FontId::new(12.0, FontFamily::Proportional)),
            ] {
                s.text_styles.insert(style, font);
            }
        });
    }
}
