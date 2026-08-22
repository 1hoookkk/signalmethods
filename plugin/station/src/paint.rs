use eframe::egui::{Align2, Color32, Painter, Pos2, Rect, Stroke, Vec2};

use crate::theme;

pub fn hairline(p: &Painter, a: Pos2, b: Pos2, c: Color32) {
    p.line_segment([a, b], Stroke::new(1.0, c));
}

/// Box drawn as four hairlines. Nothing here uses an egui widget: every mark on
/// the screen is placed by this file.
pub fn frame(p: &Painter, r: Rect, c: Color32) {
    hairline(p, r.left_top(), r.right_top(), c);
    hairline(p, r.right_top(), r.right_bottom(), c);
    hairline(p, r.right_bottom(), r.left_bottom(), c);
    hairline(p, r.left_bottom(), r.left_top(), c);
}

pub fn label(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) {
    p.text(at, Align2::LEFT_TOP, s, theme::mono(size), c);
}

pub fn label_right(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) {
    p.text(at, Align2::RIGHT_TOP, s, theme::mono(size), c);
}

/// Ticked rule along the bottom of a plot: decade marks only, no numbers, so it
/// reads as a scale and not as a chart.
pub fn decades(p: &Painter, r: Rect, lo: f64, hi: f64) {
    let mut d = 10.0f64;
    while d < hi {
        if d > lo {
            let x = r.left() + ((d / lo).log10() / (hi / lo).log10()) as f32 * r.width();
            hairline(
                p,
                Pos2::new(x, r.bottom()),
                Pos2::new(x, r.bottom() - 4.0),
                theme::RULE,
            );
        }
        d *= 10.0;
    }
}

/// A response curve in dB over a log-frequency grid, fitted to `r`.
pub fn curve(p: &Painter, r: Rect, db: &[f64], lo_db: f64, hi_db: f64, c: Color32, w: f32) {
    if db.len() < 2 || hi_db <= lo_db {
        return;
    }
    let sx = r.width() / (db.len() - 1) as f32;
    let map = |v: f64| -> f32 {
        let t = ((v - lo_db) / (hi_db - lo_db)).clamp(0.0, 1.0) as f32;
        r.bottom() - t * r.height()
    };
    let mut prev = Pos2::new(r.left(), map(db[0]));
    for (i, &v) in db.iter().enumerate().skip(1) {
        let next = Pos2::new(r.left() + i as f32 * sx, map(v));
        p.line_segment([prev, next], Stroke::new(w, c));
        prev = next;
    }
}

/// Horizontal bar showing a value inside its allowed span, with the span's ends
/// marked. Used by the law list; it is a readout, never a control.
pub fn gauge(p: &Painter, r: Rect, v: f64, lo: f64, hi: f64, ok: bool) {
    let pad = (hi - lo).abs().max(1e-9) * 0.35;
    let (a, b) = (lo - pad, hi + pad);
    let at = |x: f64| r.left() + (((x - a) / (b - a)).clamp(0.0, 1.0) as f32) * r.width();
    hairline(
        p,
        Pos2::new(r.left(), r.center().y),
        Pos2::new(r.right(), r.center().y),
        theme::RULE,
    );
    for e in [lo, hi] {
        hairline(
            p,
            Pos2::new(at(e), r.top()),
            Pos2::new(at(e), r.bottom()),
            theme::DIM,
        );
    }
    let c = if ok { theme::GOOD } else { theme::BAD };
    p.circle_filled(Pos2::new(at(v), r.center().y), 3.0, c);
}

pub fn fill(p: &Painter, r: Rect, c: Color32) {
    p.rect_filled(r, 0.0, c);
}

pub fn shrink(r: Rect, x: f32, y: f32) -> Rect {
    Rect::from_min_size(
        r.min + Vec2::new(x, y),
        Vec2::new((r.width() - 2.0 * x).max(0.0), (r.height() - 2.0 * y).max(0.0)),
    )
}
