use egui::{vec2, RichText, Ui};

use crate::document::CORNERS;
use crate::theme::Theme;

pub enum Action {
    SelectCorner(usize),
}

pub struct Model<'a> {
    pub corner: usize,
    pub ride: [f32; 3],
    pub seated: &'a [bool; CORNERS],
    pub corners_shown: usize,
}

pub fn show(ui: &mut Ui, theme: &Theme, model: &Model<'_>) -> Option<Action> {
    let mut action = None;
    let cell = vec2(38.0, theme.metrics.cell_h);

    ui.spacing_mut().item_spacing.x = 2.0;

    for i in 0..model.corners_shown {
        let on = i == model.corner;
        let text = RichText::new(format!("C{i}")).font(theme.num()).color(
            if model.seated[i] {
                theme.chrome.ink
            } else {
                theme.chrome.ink_dim
            },
        );
        if ui
            .add(
                egui::Button::new(text)
                    .min_size(cell)
                    .fill(if on {
                        theme.chrome.face_lo
                    } else {
                        theme.chrome.face
                    })
                    .stroke(theme.selection_stroke(on))
                    .corner_radius(theme.radius()),
            )
            .clicked()
        {
            action = Some(Action::SelectCorner(i));
        }
    }

    ui.add_space(14.0);
    ui.label(
        RichText::new(format!(
            "M {:.2}   Q {:.2}   Z {:.2}",
            model.ride[0], model.ride[1], model.ride[2]
        ))
        .font(theme.num())
        .color(theme.chrome.ink_dim),
    );

    action
}
