use eframe::egui::{Align2, Color32, FontId, Painter, Pos2, Rect, Shape, Stroke};

use crate::ui::theme;

pub const FREQ_LO: f64 = 40.0;
pub const FREQ_HI: f64 = 16_000.0;

pub fn log_x(hz: f64, rect: Rect) -> f32 {
    let t = (hz.max(FREQ_LO) / FREQ_LO).log2() / (FREQ_HI / FREQ_LO).log2();
    rect.left() + (t.clamp(0.0, 1.0) as f32) * rect.width()
}

pub fn hz_at_x(x: f32, rect: Rect) -> f64 {
    let t = ((x - rect.left()) / rect.width()).clamp(0.0, 1.0) as f64;
    FREQ_LO * (FREQ_HI / FREQ_LO).powf(t)
}

pub fn db_y(db: f64, rect: Rect, lo: f64, hi: f64) -> f32 {
    let t = ((db - lo) / (hi - lo)).clamp(0.0, 1.0) as f32;
    rect.bottom() - t * rect.height()
}

pub fn thin_curve(painter: &Painter, points: &[Pos2], color: Color32, width: f32) {
    if points.len() < 2 {
        return;
    }
    painter.add(Shape::line(points.to_vec(), Stroke::new(width, color)));
}

pub fn x3_trace(
    painter: &Painter,
    well: Rect,
    lo: f64,
    hi: f64,
    color: Color32,
    sample: &dyn Fn(f64) -> f64,
) {
    let bins = (well.width().round() as usize).max(192);
    let n = bins * 4;
    let mut raw: Vec<(f32, f32, f32)> = Vec::with_capacity(bins);
    for b in 0..bins {
        let mut peak = f64::NAN;
        for k in 0..4 {
            let i = b * 4 + k;
            let frac = i as f64 / (n - 1) as f64;
            let f = FREQ_LO * (FREQ_HI / FREQ_LO).powf(frac);
            let db = sample(f);
            if db.is_nan() {
                continue;
            }
            if peak.is_nan() || db.abs() > peak.abs() {
                peak = db;
            }
        }
        if peak.is_nan() {
            continue;
        }
        let yt = ((hi - peak) / (hi - lo)).clamp(-0.25, 1.25);
        let x_raw = well.left() + (b as f32 / (bins - 1) as f32) * well.width();
        raw.push((
            x_raw,
            x_raw.floor() + 0.5,
            well.top() + yt as f32 * well.height(),
        ));
    }
    let count = raw.len();
    if count < 2 {
        return;
    }
    let mut pts: Vec<Pos2> = Vec::with_capacity(count);
    for i in 0..count {
        let before = raw[i.saturating_sub(1)].2;
        let after = raw[(i + 1).min(count - 1)].2;
        let (x_raw, x_lock, y_raw) = raw[i];
        if (after - before).abs() < 0.15 {
            pts.push(Pos2::new(x_lock, y_raw.floor() + 0.5));
        } else {
            pts.push(Pos2::new(x_raw, y_raw));
        }
    }
    painter.add(Shape::line(pts, Stroke::new(1.1, color)));
}

pub fn label(painter: &Painter, pos: Pos2, anchor: Align2, text: &str, size: f32, color: Color32) {
    painter.text(pos, anchor, text, FontId::monospace(size), color);
}

fn edge(painter: &Painter, rect: Rect, tl: Color32, br: Color32) {
    painter.line_segment([rect.left_top(), rect.right_top()], Stroke::new(1.0, tl));
    painter.line_segment([rect.left_top(), rect.left_bottom()], Stroke::new(1.0, tl));
    painter.line_segment([rect.left_bottom(), rect.right_bottom()], Stroke::new(1.0, br));
    painter.line_segment([rect.right_top(), rect.right_bottom()], Stroke::new(1.0, br));
}

pub fn sunken(painter: &Painter, rect: Rect) -> Rect {
    edge(painter, rect, theme::CHROME_DEEP, theme::CHROME_LT);
    edge(painter, rect.shrink(1.0), theme::CHROME_DK, theme::CHROME);
    rect.shrink(2.0)
}

pub fn raised(painter: &Painter, rect: Rect) -> Rect {
    edge(painter, rect, theme::CHROME_LT, theme::CHROME_DEEP);
    edge(painter, rect.shrink(1.0), theme::CHROME, theme::CHROME_DK);
    rect.shrink(2.0)
}

pub fn field(painter: &Painter, rect: Rect) -> Rect {
    let inner = sunken(painter, rect);
    painter.rect_filled(inner, 0.0, theme::FIELD);
    inner
}

pub fn well(painter: &Painter, rect: Rect) -> Rect {
    let inner = sunken(painter, rect);
    painter.rect_filled(inner, 0.0, theme::WELL);
    inner
}

pub const BANNER_H: f32 = 22.0;

pub fn banner(painter: &Painter, rect: Rect, title: &str, tag: &str) -> Rect {
    let inner = raised(painter, rect);
    painter.rect_filled(inner, 0.0, theme::TITLEBAR);
    label(
        painter,
        Pos2::new(inner.center().x, inner.center().y),
        Align2::CENTER_CENTER,
        title,
        theme::TITLE,
        theme::CHROME_LT,
    );
    label(
        painter,
        Pos2::new(inner.right() - 5.0, inner.center().y),
        Align2::RIGHT_CENTER,
        tag,
        theme::SMALL,
        theme::faded(theme::CHROME_LT, 170),
    );
    inner
}

pub fn group(painter: &Painter, rect: Rect, title: &str) -> Rect {
    edge(painter, rect, theme::CHROME_DK, theme::CHROME_LT);
    edge(painter, rect.shrink(1.0), theme::CHROME_LT, theme::CHROME_DK);
    if !title.is_empty() {
        let w = title.len() as f32 * 6.4 + 10.0;
        let tr = Rect::from_min_size(
            Pos2::new(rect.left() + 8.0, rect.top() - 6.0),
            eframe::egui::vec2(w, 12.0),
        );
        painter.rect_filled(tr, 0.0, theme::CHROME);
        label(
            painter,
            tr.center(),
            Align2::CENTER_CENTER,
            title,
            theme::SMALL,
            theme::INK_DIM,
        );
    }
    rect.shrink(2.0)
}
