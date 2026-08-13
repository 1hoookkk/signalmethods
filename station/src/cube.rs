use eframe::egui::{Painter, Pos2, Rect, Vec2};

use crate::body::{Body, FRAME_NAME};
use crate::paint::{curve, decades, fill, frame, label, label_right, shrink};
use crate::theme;

/// The eight frames as two stacked planes, floor and ceiling, exactly as the
/// runtime addresses them. A square draws four and says the far plane is a copy.
pub fn draw(p: &Painter, r: Rect, b: &Body, selected: usize) -> Vec<(Rect, usize)> {
    let cols = if b.is_cube() { 2 } else { 1 };
    let head = 16.0;
    let gap = 8.0;
    let cw = (r.width() - gap * (cols - 1) as f32) / cols as f32;
    let ch = (r.height() - head - gap * 3.0) / 4.0;
    let mut hits = Vec::new();

    for c in 0..cols {
        let x = r.left() + c as f32 * (cw + gap);
        label(
            p,
            Pos2::new(x, r.top()),
            if c == 0 { "Z0  FLOOR" } else { "Z1  CEILING" },
            10.0,
            theme::DIM,
        );
        for row in 0..4 {
            let i = c * 4 + row;
            let card = Rect::from_min_size(
                Pos2::new(x, r.top() + head + row as f32 * (ch + gap)),
                Vec2::new(cw, ch),
            );
            hits.push((card, i));
            fill(p, card, theme::PANEL);
            frame(p, card, if i == selected { theme::CORNER[i] } else { theme::RULE });

            let plot = shrink(card, 8.0, 16.0);
            let db = b.frame_total(i);
            let lo = db.iter().cloned().fold(f64::MAX, f64::min);
            let hi = db.iter().cloned().fold(f64::MIN, f64::max);
            decades(p, plot, 20.0, 20_000.0);
            curve(p, plot, &db, lo - 2.0, hi + 2.0, theme::CORNER[i], 1.6);

            label(
                p,
                card.left_top() + Vec2::new(6.0, 3.0),
                format!("C{:02}  {}", i + 1, FRAME_NAME[i]),
                10.0,
                theme::INK,
            );
            label_right(
                p,
                Pos2::new(card.right() - 6.0, card.top() + 3.0),
                format!("{:+.0} dB", hi),
                10.0,
                theme::DIM,
            );
        }
    }
    if !b.is_cube() {
        label(
            p,
            Pos2::new(r.left() + cw + gap, r.top() + 24.0),
            "square\n\nno third axis\n\nthe far plane\nis a copy",
            10.0,
            theme::DIM,
        );
    }
    hits
}
