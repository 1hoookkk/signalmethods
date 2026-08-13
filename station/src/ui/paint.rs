//! Drawing primitives. Every mark the Station puts on screen goes through here.

use eframe::egui::{Align2, Color32, Painter, Pos2, Rect, Stroke, StrokeKind, Vec2};

use super::theme;

pub fn fill(p: &Painter, r: Rect, c: Color32) {
    p.rect_filled(r, 0.0, c);
}

pub fn line(p: &Painter, a: Pos2, b: Pos2, c: Color32, w: f32) {
    p.line_segment([a, b], Stroke::new(w, c));
}

pub fn hairline(p: &Painter, a: Pos2, b: Pos2, c: Color32) {
    line(p, a, b, c, 1.0);
}

pub fn outline(p: &Painter, r: Rect, c: Color32) {
    p.rect_stroke(r, 0.0, Stroke::new(1.0, c), StrokeKind::Inside);
}

/// A titled region. No box: a heading and a rule beneath it, the way a figure
/// is captioned. Returns the content area.
pub fn panel(p: &Painter, r: Rect, title: &str) -> Rect {
    if title.is_empty() {
        return r;
    }
    label(p, r.min, title, theme::T_SMALL, theme::TEXT_DIM);
    hairline(
        p,
        Pos2::new(r.left(), r.top() + 17.0),
        Pos2::new(r.right(), r.top() + 17.0),
        theme::LINE,
    );
    Rect::from_min_max(Pos2::new(r.left(), r.top() + 26.0), r.max)
}

/// A crude miniplot: a trace on its own baseline, no axes, no frame. Used
/// wherever a shape has to be recognised rather than measured.
pub fn spark(p: &Painter, r: Rect, grid: &[f64], db: &[f64], lo_hz: f64, hi_hz: f64, c: Color32) {
    if db.len() < 2 {
        return;
    }
    let hi = db.iter().cloned().fold(f64::MIN, f64::max);
    let lo = db.iter().cloned().fold(f64::MAX, f64::min);
    let (lo, hi) = if (hi - lo).abs() < 1e-6 {
        (lo - 1.0, hi + 1.0)
    } else {
        (lo.max(hi - 72.0), hi)
    };
    hairline(
        p,
        Pos2::new(r.left(), r.bottom()),
        Pos2::new(r.right(), r.bottom()),
        theme::LINE,
    );
    let mut prev: Option<Pos2> = None;
    for (i, &v) in db.iter().enumerate() {
        let hz = grid.get(i).copied().unwrap_or(lo_hz);
        let pt = Pos2::new(x_of_hz(r, hz, lo_hz, hi_hz), y_of_db(r, v, lo, hi));
        if let Some(q) = prev {
            line(p, q, pt, c, 1.2);
        }
        prev = Some(pt);
    }
}

// -- text ---------------------------------------------------------------

/// Interface text.
pub fn label(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::LEFT_TOP, s, theme::ui(size), c)
}

pub fn label_right(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::RIGHT_TOP, s, theme::ui(size), c)
}

pub fn label_center(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::CENTER_CENTER, s, theme::ui(size), c)
}

/// Numeric text: coefficients, words, addresses, readings.
pub fn num(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::LEFT_TOP, s, theme::mono(size), c)
}

pub fn num_right(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::RIGHT_TOP, s, theme::mono(size), c)
}

pub fn label_width(p: &Painter, s: &str, size: f32) -> f32 {
    p.layout_no_wrap(s.to_string(), theme::ui(size), theme::TEXT)
        .size()
        .x
}

pub fn num_width(p: &Painter, s: &str, size: f32) -> f32 {
    p.layout_no_wrap(s.to_string(), theme::mono(size), theme::TEXT)
        .size()
        .x
}

pub fn char_width(p: &Painter, size: f32) -> f32 {
    num_width(p, "0", size)
}

pub fn line_height(p: &Painter, size: f32) -> f32 {
    p.layout_no_wrap("0".into(), theme::mono(size), theme::TEXT)
        .size()
        .y
}

/// A key above its value.
pub fn reading(p: &Painter, at: Pos2, key: &str, value: impl ToString, c: Color32) {
    label(p, at, key, theme::T_SMALL, theme::TEXT_FAINT);
    num(p, at + Vec2::new(0.0, 15.0), value, theme::T_BODY, c);
}

// -- plots --------------------------------------------------------------

pub fn x_of_hz(r: Rect, hz: f64, lo: f64, hi: f64) -> f32 {
    r.left() + ((hz / lo).log10() / (hi / lo).log10()).clamp(0.0, 1.0) as f32 * r.width()
}

pub fn y_of_db(r: Rect, db: f64, lo: f64, hi: f64) -> f32 {
    let t = ((db - lo) / (hi - lo)).clamp(0.0, 1.0) as f32;
    r.bottom() - t * r.height()
}

pub fn frequency_rules(p: &Painter, r: Rect, lo: f64, hi: f64, labels: bool) {
    for &d in &[
        20.0, 50.0, 100.0, 200.0, 500.0, 1_000.0, 2_000.0, 5_000.0, 10_000.0, 20_000.0,
    ] {
        if d < lo || d > hi {
            continue;
        }
        let major = matches!(d as i64, 100 | 1_000 | 10_000);
        let x = x_of_hz(r, d, lo, hi);
        hairline(
            p,
            Pos2::new(x, r.top()),
            Pos2::new(x, r.bottom()),
            theme::mix(
                theme::SURFACE_LO,
                theme::LINE,
                if major { 0.9 } else { 0.45 },
            ),
        );
        if labels && major {
            let s = if d >= 1_000.0 {
                format!("{:.0}k", d / 1000.0)
            } else {
                format!("{d:.0}")
            };
            num(
                p,
                Pos2::new(x + 3.0, r.bottom() - 14.0),
                s,
                theme::T_SMALL,
                theme::TEXT_FAINT,
            );
        }
    }
}

pub fn db_rules(p: &Painter, r: Rect, lo_db: f64, hi_db: f64, labels: bool) {
    let span = hi_db - lo_db;
    if span <= 0.0 {
        return;
    }
    let step = [3.0, 6.0, 12.0, 24.0, 48.0, 96.0]
        .into_iter()
        .find(|s| span / s <= 7.0)
        .unwrap_or(96.0);
    let mut v = (lo_db / step).ceil() * step;
    while v <= hi_db {
        let y = y_of_db(r, v, lo_db, hi_db);
        let zero = v.abs() < 1e-9;
        hairline(
            p,
            Pos2::new(r.left(), y),
            Pos2::new(r.right(), y),
            if zero {
                theme::LINE_HI
            } else {
                theme::mix(theme::SURFACE_LO, theme::LINE, 0.5)
            },
        );
        if labels {
            num(
                p,
                Pos2::new(r.left() + 3.0, y + 1.0),
                format!("{v:+.0}"),
                theme::T_SMALL,
                theme::TEXT_FAINT,
            );
        }
        v += step;
    }
}

#[allow(clippy::too_many_arguments)]
pub fn curve(
    p: &Painter,
    r: Rect,
    grid: &[f64],
    db: &[f64],
    lo_hz: f64,
    hi_hz: f64,
    lo_db: f64,
    hi_db: f64,
    c: Color32,
    w: f32,
) {
    if db.len() < 2 || hi_db <= lo_db {
        return;
    }
    let mut prev: Option<Pos2> = None;
    for (i, &v) in db.iter().enumerate() {
        let hz = grid.get(i).copied().unwrap_or(lo_hz);
        let pt = Pos2::new(x_of_hz(r, hz, lo_hz, hi_hz), y_of_db(r, v, lo_db, hi_db));
        if let Some(q) = prev {
            line(p, q, pt, c, w);
        }
        prev = Some(pt);
    }
}

/// A small index chip, e.g. `C03`.
pub fn chip(p: &Painter, at: Pos2, s: &str, c: Color32) -> Rect {
    let w = num_width(p, s, theme::T_SMALL) + 10.0;
    let r = Rect::from_min_size(at, Vec2::new(w, 17.0));
    fill(p, r, theme::mix(theme::SURFACE, c, 0.22));
    outline(p, r, theme::mix(theme::SURFACE, c, 0.55));
    num(p, r.min + Vec2::new(5.0, 2.0), s, theme::T_SMALL, c);
    r
}
