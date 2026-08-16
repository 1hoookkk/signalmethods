use eframe::egui::{Align2, Color32, FontId, Id, Painter, Pos2, Rect, Sense, Stroke, Ui, Vec2};
use crate::ui::theme;

// ============================================================================
// CUSTOM MOTIF / EDA PAINTER UTILITIES (ZERO DEFAULT WIDGETS)
// ============================================================================

/// 3D Raised Bevel Panel (Motif Button / Chrome Header)
pub fn raised(painter: &Painter, rect: Rect) -> Rect {
    painter.rect_filled(rect, 0.0, theme::CHROME);
    painter.line_segment([rect.left_top(), rect.right_top()], Stroke::new(1.5, theme::BEVEL_HI));
    painter.line_segment([rect.left_top(), rect.left_bottom()], Stroke::new(1.5, theme::BEVEL_HI));
    painter.line_segment([rect.right_top(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_LO));
    painter.line_segment([rect.left_bottom(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_LO));
    rect.shrink(2.0)
}

/// 3D Sunken Bevel Panel (Deep Engineering Well / Inset Canvas)
pub fn sunken(painter: &Painter, rect: Rect) -> Rect {
    painter.rect_filled(rect, 0.0, theme::WELL_BG);
    painter.line_segment([rect.left_top(), rect.right_top()], Stroke::new(1.5, theme::BEVEL_LO));
    painter.line_segment([rect.left_top(), rect.left_bottom()], Stroke::new(1.5, theme::BEVEL_LO));
    painter.line_segment([rect.right_top(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_HI));
    painter.line_segment([rect.left_bottom(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_HI));
    rect.shrink(2.0)
}

/// Tactile Motif Button
pub fn motif_button(
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    id: Id,
    text: &str,
    active: bool,
) -> bool {
    let resp = ui.interact(rect, id, Sense::click());
    let pressed = resp.is_pointer_button_down_on() || active;

    if pressed {
        // Sunken pressed state
        painter.rect_filled(rect, 0.0, theme::PANEL_BG);
        painter.line_segment([rect.left_top(), rect.right_top()], Stroke::new(1.5, theme::BEVEL_LO));
        painter.line_segment([rect.left_top(), rect.left_bottom()], Stroke::new(1.5, theme::BEVEL_LO));
        painter.line_segment([rect.right_top(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_HI));
        painter.line_segment([rect.left_bottom(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_HI));
    } else {
        // Raised unpressed state
        painter.rect_filled(rect, 0.0, theme::CHROME);
        painter.line_segment([rect.left_top(), rect.right_top()], Stroke::new(1.5, theme::BEVEL_HI));
        painter.line_segment([rect.left_top(), rect.left_bottom()], Stroke::new(1.5, theme::BEVEL_HI));
        painter.line_segment([rect.right_top(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_LO));
        painter.line_segment([rect.left_bottom(), rect.right_bottom()], Stroke::new(1.5, theme::BEVEL_LO));
    }

    let text_pos = if pressed {
        rect.center() + Vec2::new(1.0, 1.0)
    } else {
        rect.center()
    };

    painter.text(
        text_pos,
        Align2::CENTER_CENTER,
        text,
        FontId::monospace(9.5),
        if active { theme::TITLEBAR } else { Color32::BLACK },
    );

    resp.clicked()
}

/// Logarithmic Frequency Axis Mapping: [40 Hz .. 16 kHz] -> Screen X
pub fn log_x(hz: f64, rect: Rect) -> f32 {
    let min_f = 40.0f64;
    let max_f = 16_000.0f64;
    let norm = ((hz.max(min_f).min(max_f) / min_f).ln() / (max_f / min_f).ln()) as f32;
    rect.left() + norm * rect.width()
}

/// Screen X -> Logarithmic Frequency Hz
pub fn hz_at_x(x: f32, rect: Rect) -> f64 {
    let t = ((x - rect.left()) / rect.width()).clamp(0.0, 1.0) as f64;
    40.0 * (16_000.0 / 40.0f64).powf(t)
}

/// Decibel Axis Mapping: [lo_db .. hi_db] -> Screen Y
pub fn db_y(db: f64, rect: Rect, lo: f64, hi: f64) -> f32 {
    let norm = ((db - lo) / (hi - lo)).clamp(0.0, 1.0) as f32;
    rect.bottom() - norm * rect.height()
}

/// High-Precision Engineering Graticule Grid
pub fn draw_graticule(painter: &Painter, rect: Rect, lo_db: f64, hi_db: f64) {
    // 1. Frequency Vertical Grid Lines
    let freqs = [100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0];
    for &f in &freqs {
        let x = log_x(f, rect);
        let is_major = (f == 1000.0) || (f == 100.0) || (f == 10000.0);
        let col = if is_major { theme::GRATICULE_HI } else { theme::GRATICULE_DIM };
        painter.line_segment([Pos2::new(x, rect.top()), Pos2::new(x, rect.bottom())], Stroke::new(1.0, col));

        // Frequency labels at bottom
        let label = if f >= 1000.0 {
            format!("{:.0}k", f / 1000.0)
        } else {
            format!("{:.0}", f)
        };
        painter.text(
            Pos2::new(x, rect.bottom() - 3.0),
            Align2::CENTER_BOTTOM,
            label,
            FontId::monospace(7.5),
            theme::TEXT_DIM,
        );
    }

    // 2. Decibel Horizontal Grid Lines
    let mut db_step = (hi_db / 10.0).floor() * 10.0;
    while db_step >= lo_db {
        let y = db_y(db_step, rect, lo_db, hi_db);
        let is_zero = db_step == 0.0;
        let col = if is_zero { theme::GRATICULE_HI } else { theme::GRATICULE_DIM };
        painter.line_segment([Pos2::new(rect.left(), y), Pos2::new(rect.right(), y)], Stroke::new(1.0, col));

        // Decibel label at left
        painter.text(
            Pos2::new(rect.left() + 3.0, y - 2.0),
            Align2::LEFT_BOTTOM,
            format!("{:+.0}dB", db_step),
            FontId::monospace(7.5),
            theme::TEXT_DIM,
        );
        db_step -= 10.0;
    }
}

/// Continuous Response Curve Trace
pub fn draw_curve<F>(
    painter: &Painter,
    rect: Rect,
    lo_db: f64,
    hi_db: f64,
    color: Color32,
    stroke_width: f32,
    eval: &F,
) where
    F: Fn(f64) -> f64,
{
    let num_pts = 160;
    let mut pts = Vec::with_capacity(num_pts);
    for i in 0..num_pts {
        let t = i as f64 / (num_pts - 1) as f64;
        let hz = 40.0 * (16_000.0 / 40.0f64).powf(t);
        let db = eval(hz);
        let x = rect.left() + (t as f32) * rect.width();
        let y = db_y(db, rect, lo_db, hi_db);
        pts.push(Pos2::new(x, y));
    }
    if pts.len() >= 2 {
        painter.add(eframe::egui::Shape::line(pts, Stroke::new(stroke_width, color)));
    }
}
