use eframe::egui::{Align2, Painter, Pos2, Rect, Stroke, Ui};

use trench_core::cascade::NUM_STAGES;

use crate::engine::response::{row_db, SR};
use crate::session::state::Session;
use crate::ui::{paint, theme};

const PAD: f32 = 14.0;

fn cell(painter: &Painter, rect: Rect, mark: &str) -> Rect {
    let inner = paint::well(painter, rect);
    paint::label(
        painter,
        Pos2::new(inner.left() + 4.0, inner.top() + 3.0),
        Align2::LEFT_TOP,
        mark,
        theme::SMALL,
        theme::WELL_DIM,
    );
    inner
}

fn trace(painter: &Painter, inner: Rect, vals: &dyn Fn(f64) -> f64, ink: eframe::egui::Color32) {
    let mut lo = f64::INFINITY;
    let mut hi = f64::NEG_INFINITY;
    for i in 0..64 {
        let f = paint::FREQ_LO * (paint::FREQ_HI / paint::FREQ_LO).powf(i as f64 / 63.0);
        let db = vals(f);
        lo = lo.min(db);
        hi = hi.max(db);
    }
    lo -= 4.0;
    hi += 4.0;
    if hi - lo < 18.0 {
        let mid = (hi + lo) / 2.0;
        lo = mid - 9.0;
        hi = mid + 9.0;
    }
    let plot = Rect::from_min_max(
        Pos2::new(inner.left(), inner.top() + 5.0),
        Pos2::new(inner.right(), inner.bottom() - 3.0),
    );
    let zero_y = paint::db_y(0.0, plot, lo, hi);
    if zero_y > plot.top() && zero_y < plot.bottom() {
        painter.line_segment(
            [Pos2::new(plot.left(), zero_y), Pos2::new(plot.right(), zero_y)],
            Stroke::new(1.0, theme::GRATICULE),
        );
    }
    paint::x3_trace(&painter.with_clip_rect(inner), plot, lo, hi, ink, vals);
}

pub fn draw(session: &Session, ui: &mut Ui) {
    let rect = ui.max_rect();
    let painter = ui.painter().clone();
    painter.rect_filled(rect, 0.0, theme::CHROME);
    let Some(rows) = session.current_rows() else {
        return;
    };
    let gap = 8.0;
    let cols = NUM_STAGES as f32;
    let w = (rect.width() - PAD * 2.0 - gap * (cols - 1.0)) / cols;
    let h = ((rect.height() - PAD * 2.0 - gap * 2.0) / 3.0).min(w * 0.62);
    let top0 = rect.center().y - (h * 3.0 + gap * 2.0) / 2.0;
    let (preview, mask) = session.preview();
    let all_modes = crate::engine::modes::modes(&preview, SR);
    for si in 0..NUM_STAGES {
        let x = rect.left() + PAD + si as f32 * (w + gap);
        let ink = theme::LANES[si % 7];
        let own = cell(
            &painter,
            Rect::from_min_size(Pos2::new(x, top0), eframe::egui::vec2(w, h)),
            &(if mask[si] {
                format!("p {}", si + 1)
            } else {
                format!("{}", si + 1)
            }),
        );
        let row = rows[si];
        let identity = row == [1.0, 0.0, 0.0, 0.0, 0.0];
        if identity {
            paint::label(
                &painter,
                own.center(),
                Align2::CENTER_CENTER,
                "—",
                theme::BODY,
                theme::WELL_DIM,
            );
        } else {
            trace(&painter, own, &move |hz| row_db(&row, hz, SR), ink);
        }
        let mode_cell = cell(
            &painter,
            Rect::from_min_size(
                Pos2::new(x, top0 + h + gap),
                eframe::egui::vec2(w, h),
            ),
            &format!("mode {}", si + 1),
        );
        if let Some(m) = all_modes.iter().find(|m| m.section == si) {
            let vals = |hz: f64| m.db_at(hz, SR);
            trace(&painter, mode_cell, &vals, theme::faded(ink, 230));
        } else {
            paint::label(
                &painter,
                mode_cell.center(),
                Align2::CENTER_CENTER,
                "—",
                theme::BODY,
                theme::WELL_DIM,
            );
        }
        let sofar = cell(
            &painter,
            Rect::from_min_size(
                Pos2::new(x, top0 + (h + gap) * 2.0),
                eframe::egui::vec2(w, h),
            ),
            &format!("1..{}", si + 1),
        );
        let upto: Vec<[f64; 5]> = rows[..=si].to_vec();
        trace(
            &painter,
            sofar,
            &move |hz| upto.iter().map(|r| row_db(r, hz, SR)).sum(),
            if si + 1 == NUM_STAGES { theme::NOW } else { theme::faded(ink, 210) },
        );
    }
}
