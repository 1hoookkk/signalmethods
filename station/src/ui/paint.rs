//! Drawing primitives. Every mark the Station puts on screen goes through this
//! file; egui supplies a painter and nothing else.

use eframe::egui::{Align2, Color32, Painter, Pos2, Rect, Stroke, Vec2};

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
    p.rect_stroke(
        r,
        0.0,
        Stroke::new(1.0, c),
        eframe::egui::StrokeKind::Inside,
    );
}

pub fn text(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::LEFT_TOP, s, theme::mono(size), c)
}

pub fn text_right(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::RIGHT_TOP, s, theme::mono(size), c)
}

pub fn text_center(p: &Painter, at: Pos2, s: impl ToString, size: f32, c: Color32) -> Rect {
    p.text(at, Align2::CENTER_CENTER, s, theme::mono(size), c)
}

/// Width of a monospace run, measured through the font rather than guessed.
pub fn text_width(p: &Painter, s: &str, size: f32) -> f32 {
    p.layout_no_wrap(s.to_string(), theme::mono(size), theme::INK)
        .size()
        .x
}

pub fn char_width(p: &Painter, size: f32) -> f32 {
    text_width(p, "0", size)
}

pub fn line_height(p: &Painter, size: f32) -> f32 {
    p.layout_no_wrap("0".into(), theme::mono(size), theme::INK)
        .size()
        .y
}

/// A short tick-ended rule, the Station's section divider.
pub fn divider(p: &Painter, a: Pos2, b: Pos2, c: Color32) {
    hairline(p, a, b, c);
    p.circle_filled(a, 1.5, c);
    p.circle_filled(b, 1.5, c);
}

/// Maps a frequency onto a log x-axis.
pub fn x_of_hz(r: Rect, hz: f64, lo: f64, hi: f64) -> f32 {
    r.left() + ((hz / lo).log10() / (hi / lo).log10()).clamp(0.0, 1.0) as f32 * r.width()
}

/// Decade rules with labels, so the plot reads as a measurement.
pub fn frequency_rules(p: &Painter, r: Rect, lo: f64, hi: f64, labels: bool) {
    for &d in &[20.0, 100.0, 1_000.0, 10_000.0] {
        if d < lo || d > hi {
            continue;
        }
        let x = x_of_hz(r, d, lo, hi);
        hairline(
            p,
            Pos2::new(x, r.top()),
            Pos2::new(x, r.bottom()),
            theme::mix(theme::BG, theme::RULE, 0.55),
        );
        if labels {
            let s = if d >= 1_000.0 {
                format!("{:.0}k", d / 1000.0)
            } else {
                format!("{d:.0}")
            };
            text(
                p,
                Pos2::new(x + 3.0, r.bottom() - 11.0),
                s,
                theme::T_MICRO,
                theme::FAINT,
            );
        }
    }
}

/// Horizontal dB rules at a readable spacing for the given span.
pub fn db_rules(p: &Painter, r: Rect, lo_db: f64, hi_db: f64, labels: bool) {
    let span = hi_db - lo_db;
    if span <= 0.0 {
        return;
    }
    let step = [1.0, 3.0, 6.0, 12.0, 24.0, 48.0, 96.0]
        .into_iter()
        .find(|s| span / s <= 8.0)
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
                theme::RULE_HI
            } else {
                theme::mix(theme::BG, theme::RULE, 0.55)
            },
        );
        if labels {
            text(
                p,
                Pos2::new(r.left() + 2.0, y + 1.0),
                format!("{v:+.0}"),
                theme::T_MICRO,
                theme::FAINT,
            );
        }
        v += step;
    }
}

pub fn y_of_db(r: Rect, db: f64, lo: f64, hi: f64) -> f32 {
    let t = ((db - lo) / (hi - lo)).clamp(0.0, 1.0) as f32;
    r.bottom() - t * r.height()
}

/// A response curve over a log-frequency grid.
// A plot needs its rect, its data, and both axis spans; bundling them into a
// struct would only move the same values one level down.
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

/// Filled area under a curve, the reference language's response fill.
#[allow(clippy::too_many_arguments)]
pub fn curve_fill(
    p: &Painter,
    r: Rect,
    grid: &[f64],
    db: &[f64],
    lo_hz: f64,
    hi_hz: f64,
    lo_db: f64,
    hi_db: f64,
    c: Color32,
) {
    if db.len() < 2 {
        return;
    }
    // Vertical hairlines rather than a mesh: exact, cheap, and it keeps the
    // fill visually subordinate to the curve itself.
    let faded = theme::mix(theme::BG, c, 0.16);
    for (i, &v) in db.iter().enumerate() {
        let hz = grid.get(i).copied().unwrap_or(lo_hz);
        let x = x_of_hz(r, hz, lo_hz, hi_hz);
        let y = y_of_db(r, v, lo_db, hi_db);
        if y < r.bottom() {
            line(p, Pos2::new(x, y), Pos2::new(x, r.bottom()), faded, 1.5);
        }
    }
}

/// A small index chip, e.g. `C03`, as used across the reference language.
pub fn chip(p: &Painter, at: Pos2, s: &str, c: Color32) -> Rect {
    let w = text_width(p, s, theme::T_MICRO) + 8.0;
    let r = Rect::from_min_size(at, Vec2::new(w, 13.0));
    fill(p, r, theme::mix(theme::BG, c, 0.18));
    outline(p, r, theme::mix(theme::BG, c, 0.5));
    text(p, r.min + Vec2::new(4.0, 2.0), s, theme::T_MICRO, c);
    r
}
