use egui::{pos2, vec2, Align2, Rect, Sense, Stroke, StrokeKind, Ui};

use crate::theme::Theme;

pub enum Action {
    SelectCorner(usize),
    SetRide([f32; 3]),
}

pub struct Model<'a> {
    pub corner: usize,
    pub ride: [f32; 3],
    pub seated: &'a [bool; 8],
    pub planes: usize,
}

fn corner_index(plane: usize, k: usize) -> usize {
    k | (plane << 2)
}

fn mark_center(rect: Rect, k: usize) -> egui::Pos2 {
    let m = (k & 1) as f32;
    let q = ((k >> 1) & 1) as f32;
    pos2(
        rect.left() + rect.width() * (0.18 + 0.64 * m),
        rect.bottom() - rect.height() * (0.18 + 0.64 * q),
    )
}

pub fn show(ui: &mut Ui, theme: &Theme, model: &Model<'_>) -> Option<Action> {
    let mut action = None;
    let side = ui.available_width().min(132.0);

    for plane in (0..model.planes).rev() {
        let (rect, surface) = ui.allocate_exact_size(vec2(side, side), Sense::click_and_drag());
        let label = if plane == 1 { "POSITION Z1" } else { "POSITION Z0" };
        surface.widget_info(|| egui::WidgetInfo::labeled(egui::WidgetType::Other, true, label));

        let p = ui.painter_at(rect);
        p.rect_filled(rect, theme.radius(), theme.well.bg);
        p.rect_stroke(
            rect,
            theme.radius(),
            Stroke::new(theme.metrics.border_px, theme.well.rule),
            StrokeKind::Inside,
        );
        p.text(
            pos2(rect.left() + 4.0, rect.top() + 3.0),
            Align2::LEFT_TOP,
            if plane == 1 { "Z1" } else { "Z0" },
            theme.num(),
            theme.well.dim,
        );

        let weight = if plane == 1 { model.ride[2] } else { 1.0 - model.ride[2] };
        if weight > 0.001 {
            let cx = rect.left() + rect.width() * (0.18 + 0.64 * model.ride[0]);
            let cy = rect.bottom() - rect.height() * (0.18 + 0.64 * model.ride[1]);
            let ink = theme.chrome.select_ink.gamma_multiply(weight.clamp(0.15, 1.0));
            let hair = Stroke::new(1.0, ink);
            p.line_segment([pos2(rect.left() + 2.0, cy), pos2(rect.right() - 2.0, cy)], hair);
            p.line_segment([pos2(cx, rect.top() + 2.0), pos2(cx, rect.bottom() - 2.0)], hair);
            p.circle_filled(pos2(cx, cy), 3.0, ink);
        }

        for k in 0..4 {
            let index = corner_index(plane, k);
            let seated = model.seated[index];
            let current = index == model.corner;
            let center = mark_center(rect, k);
            let hit = Rect::from_center_size(center, vec2(30.0, 20.0));
            let mark = ui.interact(
                hit,
                ui.id().with(("corner", index)),
                if seated { Sense::click() } else { Sense::hover() },
            );
            mark.widget_info(|| {
                egui::WidgetInfo::selected(
                    egui::WidgetType::Button,
                    seated,
                    current,
                    format!("C{index}"),
                )
            });
            if mark.clicked() {
                action = Some(Action::SelectCorner(index));
            }
            let ink = if seated { theme.well.ink } else { theme.well.dim };
            if seated {
                p.circle_filled(center, 3.5, ink);
            } else {
                p.circle_stroke(center, 3.5, Stroke::new(1.0, ink));
            }
            if current {
                p.circle_stroke(center, 6.5, Stroke::new(1.5, theme.chrome.select_ink));
            }
            p.text(
                pos2(center.x, center.y + 6.0),
                Align2::CENTER_TOP,
                format!("C{index}"),
                theme.num(),
                ink,
            );
        }

        if surface.dragged() {
            if let Some(at) = surface.interact_pointer_pos() {
                let m = ((at.x - rect.left()) / rect.width() - 0.18) / 0.64;
                let q = ((rect.bottom() - at.y) / rect.height() - 0.18) / 0.64;
                action = Some(Action::SetRide([
                    m.clamp(0.0, 1.0),
                    q.clamp(0.0, 1.0),
                    model.ride[2],
                ]));
            }
        }
        ui.add_space(theme.metrics.gap);
    }

    if model.planes > 1 {
        let mut z = model.ride[2];
        ui.spacing_mut().slider_width = side - 52.0;
        if ui
            .add(egui::Slider::new(&mut z, 0.0..=1.0).text("Z").fixed_decimals(2))
            .changed()
        {
            action = Some(Action::SetRide([model.ride[0], model.ride[1], z]));
        }
    }

    ui.label(
        egui::RichText::new(format!(
            "M {:.2}  Q {:.2}  Z {:.2}",
            model.ride[0], model.ride[1], model.ride[2]
        ))
        .font(theme.num()),
    );

    action
}
