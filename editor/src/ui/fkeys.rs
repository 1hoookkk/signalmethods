use eframe::egui::{Align2, Id, Key, Painter, Pos2, Rect, Sense, Ui};

use crate::ui::{paint, theme};

pub const FKEY_H: f32 = 24.0;

pub fn draw(
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    keys: &[(Key, &str, &str)],
) -> Option<usize> {
    let mut fired = None;
    let gap = 6.0;
    let w = ((rect.width() - gap * (keys.len() as f32 - 1.0)) / keys.len() as f32).min(112.0);
    for (i, (key, kname, label)) in keys.iter().enumerate() {
        let b = Rect::from_min_size(
            Pos2::new(rect.left() + i as f32 * (w + gap), rect.top()),
            eframe::egui::vec2(w, rect.height()),
        );
        let inner = paint::raised(painter, b);
        paint::label(
            painter,
            Pos2::new(inner.left() + 4.0, inner.center().y),
            Align2::LEFT_CENTER,
            kname,
            theme::SMALL,
            theme::INK_DIM,
        );
        paint::label(
            painter,
            Pos2::new(inner.right() - 5.0, inner.center().y),
            Align2::RIGHT_CENTER,
            label,
            theme::SMALL,
            theme::INK,
        );
        let resp = ui.interact(b, Id::new(("fkey", i)), Sense::click());
        if resp.clicked() || ui.input(|inp| inp.key_pressed(*key)) {
            fired = Some(i);
        }
    }
    fired
}
