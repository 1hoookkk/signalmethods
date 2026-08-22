use eframe::egui::{Painter, Pos2, Rect, Vec2};

use crate::law::Reading;
use crate::paint::{fill, frame, gauge, label, label_right};
use crate::theme;

/// The laws you wrote, measured against the open body. The station states the
/// number and whether it sits inside your span. It never proposes one.
pub fn draw(p: &Painter, r: Rect, readings: &[Reading], laws_path: &str) {
    fill(p, r, theme::PANEL);
    frame(p, r, theme::RULE);
    let pass = readings.iter().filter(|x| x.ok).count();
    label(p, r.left_top() + Vec2::new(8.0, 4.0), "LAWS", 10.0, theme::DIM);
    label_right(
        p,
        Pos2::new(r.right() - 8.0, r.top() + 4.0),
        format!("{pass}/{} hold", readings.len()),
        10.0,
        if pass == readings.len() { theme::GOOD } else { theme::BAD },
    );

    let row_h = 30.0;
    for (i, rd) in readings.iter().enumerate() {
        let y = r.top() + 22.0 + i as f32 * row_h;
        if y + row_h > r.bottom() {
            break;
        }
        let c = if rd.ok { theme::GOOD } else { theme::BAD };
        label(p, Pos2::new(r.left() + 8.0, y), &rd.law.q, 11.0, theme::INK);
        label(
            p,
            Pos2::new(r.left() + 8.0, y + 13.0),
            format!("{}   {}", rd.where_, rd.law.why),
            9.0,
            theme::DIM,
        );
        label_right(
            p,
            Pos2::new(r.right() - 8.0, y),
            format!("{:.2}", rd.value),
            11.0,
            c,
        );
        let g = Rect::from_min_size(
            Pos2::new(r.right() - 150.0, y + 15.0),
            Vec2::new(142.0, 10.0),
        );
        gauge(p, g, rd.value, rd.law.min, rd.law.max, rd.ok);
    }

    label(
        p,
        Pos2::new(r.left() + 8.0, r.bottom() - 14.0),
        format!("R reloads {laws_path}"),
        9.0,
        theme::DIM,
    );
}
