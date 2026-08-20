use egui::{Align, Layout, RichText, Ui};
use trench_core::stage_law::RootPair;

use crate::document::Corner;
use crate::theme::Theme;

pub enum Action {
    SetPoleHz(f64),
    SetPoleR(f64),
    SetZeroHz(f64),
    SetZeroR(f64),
    ToggleHold,
    Clear,
}

pub struct Model<'a> {
    pub corner: usize,
    pub corner_data: &'a Corner,
    pub section: Option<usize>,
}

fn field(
    ui: &mut Ui,
    theme: &Theme,
    label: &str,
    value: f64,
    speed: f64,
    range: std::ops::RangeInclusive<f64>,
    decimals: usize,
    editable: bool,
) -> Option<f64> {
    ui.label(
        RichText::new(label)
            .font(theme.label())
            .color(theme.chrome.ink_dim),
    );
    let mut v = value;
    let mut changed = None;
    let r = ui.add_enabled(
        editable,
        egui::DragValue::new(&mut v)
            .speed(speed)
            .range(range)
            .fixed_decimals(decimals),
    );
    if r.changed() {
        changed = Some(v);
    }
    changed
}

pub fn show(ui: &mut Ui, theme: &Theme, model: &Model<'_>) -> Option<Action> {
    let mut action = None;

    ui.horizontal(|ui| {
        ui.label(RichText::new(format!("C{}", model.corner)).font(theme.head()).strong());
        ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
            ui.label(
                RichText::new(
                    model
                        .corner_data
                        .target
                        .as_ref()
                        .map(|t| t.name.clone())
                        .unwrap_or_else(|| "no target".into()),
                )
                .font(theme.num())
                .color(theme.chrome.ink_dim),
            );
        });
    });
    ui.separator();

    let Some(si) = model.section else {
        ui.add_space(6.0);
        ui.label(
            RichText::new("no section")
                .font(theme.label())
                .color(theme.chrome.ink_dim),
        );
        return None;
    };

    let s = model.corner_data.sections[si];
    let ink = theme.data.stage[si % theme.data.stage.len()];
    let held = s.hold.any();

    ui.horizontal(|ui| {
        ui.label(
            RichText::new(format!("S{}", si + 1))
                .font(theme.head())
                .strong()
                .color(ink),
        );
        ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
            ui.label(
                RichText::new(if held { "HELD" } else { "FREE" })
                    .font(theme.num())
                    .color(if held {
                        theme.chrome.select_ink
                    } else {
                        theme.chrome.ink_dim
                    }),
            );
        });
    });
    ui.add_space(4.0);

    let editable = !held;
    match s.geometry.pole {
        RootPair::Conjugate { hz, r } => {
            if let Some(v) = field(ui, theme, "POLE", hz, 4.0, 40.0..=16_000.0, 1, editable) {
                action = Some(Action::SetPoleHz(v));
            }
            if let Some(v) = field(ui, theme, "r", r, 0.0004, 0.0..=0.9999, 4, editable) {
                action = Some(Action::SetPoleR(v));
            }
        }
        RootPair::RealPair { root_a, root_b } => {
            ui.label(RichText::new("POLE").font(theme.label()).color(theme.chrome.ink_dim));
            ui.label(
                RichText::new(format!("real {root_a:+.5} {root_b:+.5}"))
                    .font(theme.num())
                    .color(theme.chrome.ink),
            );
        }
        RootPair::Degenerate => {
            ui.label(RichText::new("POLE").font(theme.label()).color(theme.chrome.ink_dim));
            ui.label(RichText::new("—").font(theme.num()).color(theme.chrome.ink_dim));
        }
    }
    ui.add_space(4.0);

    match s.geometry.zero {
        RootPair::Conjugate { hz, r } => {
            if let Some(v) = field(ui, theme, "ZERO", hz, 4.0, 40.0..=16_000.0, 1, editable) {
                action = Some(Action::SetZeroHz(v));
            }
            if let Some(v) = field(ui, theme, "r", r, 0.0004, 0.0..=1.0, 4, editable) {
                action = Some(Action::SetZeroR(v));
            }
        }
        RootPair::RealPair { root_a, root_b } => {
            ui.label(RichText::new("ZERO").font(theme.label()).color(theme.chrome.ink_dim));
            ui.label(
                RichText::new(format!("real {root_a:+.5} {root_b:+.5}"))
                    .font(theme.num())
                    .color(theme.chrome.ink),
            );
        }
        RootPair::Degenerate => {
            ui.label(RichText::new("ZERO").font(theme.label()).color(theme.chrome.ink_dim));
            ui.label(RichText::new("—").font(theme.num()).color(theme.chrome.ink_dim));
        }
    }
    ui.add_space(4.0);

    ui.label(RichText::new("SCALE").font(theme.label()).color(theme.chrome.ink_dim));
    ui.label(
        RichText::new(format!("{:+.2} dB", 20.0 * s.geometry.scale.max(1e-9).log10()))
            .font(theme.num())
            .color(theme.chrome.ink),
    );

    ui.add_space(8.0);
    ui.horizontal(|ui| {
        if ui
            .add(egui::Button::new(if held { "FREE" } else { "HOLD" }).min_size(egui::vec2(74.0, 24.0)))
            .clicked()
        {
            action = Some(Action::ToggleHold);
        }
        if ui
            .add(egui::Button::new("CLEAR").min_size(egui::vec2(74.0, 24.0)))
            .clicked()
        {
            action = Some(Action::Clear);
        }
    });

    action
}
