//! Custom-painted controls. No egui widget appears in the Station; these are
//! the whole vocabulary, built on `Ui::region` so they all behave alike.

use eframe::egui::{Color32, Pos2, Rect, Vec2};

use super::input::{Id, Response, Ui};
use super::paint::{self, fill, hairline, outline, text, text_right};
use super::theme;

/// A rectangular command. Returns true on the frame it is released inside.
pub fn button(ui: &mut Ui, id: Id, rect: Rect, label: &str, enabled: bool) -> bool {
    let r = if enabled {
        ui.region(id, rect)
    } else {
        Response::empty()
    };
    let (bg, edge, ink) = if !enabled {
        (theme::PANEL, theme::RULE, theme::FAINT)
    } else if r.held {
        (theme::mix(theme::PANEL, theme::ACCENT, 0.30), theme::ACCENT, theme::INK_HI)
    } else if r.hovered {
        (theme::PANEL_HI, theme::RULE_HI, theme::INK_HI)
    } else {
        (theme::PANEL, theme::RULE, theme::INK)
    };
    fill(ui.p, rect, bg);
    outline(ui.p, rect, edge);
    paint::text_center(ui.p, rect.center(), label, theme::T_SMALL, ink);
    r.clicked && enabled
}

/// A latching state, drawn as a marked cell rather than a checkbox.
pub fn toggle(ui: &mut Ui, id: Id, rect: Rect, label: &str, on: bool) -> bool {
    let r = ui.region(id, rect);
    let edge = if on {
        theme::ACCENT
    } else if r.hovered {
        theme::RULE_HI
    } else {
        theme::RULE
    };
    fill(
        ui.p,
        rect,
        if on {
            theme::mix(theme::PANEL, theme::ACCENT, 0.18)
        } else {
            theme::PANEL
        },
    );
    outline(ui.p, rect, edge);
    let mark = Rect::from_min_size(rect.min + Vec2::new(5.0, rect.height() * 0.5 - 3.0), Vec2::splat(6.0));
    fill(ui.p, mark, if on { theme::ACCENT } else { theme::FAINT });
    text(
        ui.p,
        Pos2::new(rect.left() + 17.0, rect.top() + rect.height() * 0.5 - 6.0),
        label,
        theme::T_SMALL,
        if on { theme::INK_HI } else { theme::INK },
    );
    r.clicked
}

/// One of a set. Returns true when this tab is chosen.
pub fn tab(ui: &mut Ui, id: Id, rect: Rect, index: &str, label: &str, active: bool) -> bool {
    let r = ui.region(id, rect);
    fill(ui.p, rect, if active { theme::mix(theme::PANEL, theme::ACCENT, 0.16) } else { theme::PANEL });
    outline(ui.p, rect, if active { theme::ACCENT } else if r.hovered { theme::RULE_HI } else { theme::RULE });
    text(
        ui.p,
        rect.min + Vec2::new(8.0, rect.height() * 0.5 - 5.0),
        index,
        theme::T_MICRO,
        if active { theme::ACCENT } else { theme::DIM },
    );
    text(
        ui.p,
        rect.min + Vec2::new(30.0, rect.height() * 0.5 - 6.0),
        label,
        theme::T_SMALL,
        if active { theme::INK_HI } else { theme::INK },
    );
    r.clicked
}

/// How a value maps onto its track.
#[derive(Clone, Copy, PartialEq)]
pub enum Scale {
    Linear,
    /// Log scale, for frequency. Both bounds must be positive.
    Log,
}

impl Scale {
    fn to_t(self, v: f64, lo: f64, hi: f64) -> f64 {
        match self {
            Scale::Linear => (v - lo) / (hi - lo),
            Scale::Log => (v.max(1e-12) / lo).log10() / (hi / lo).log10(),
        }
        .clamp(0.0, 1.0)
    }
    fn from_t(self, t: f64, lo: f64, hi: f64) -> f64 {
        let t = t.clamp(0.0, 1.0);
        match self {
            Scale::Linear => lo + (hi - lo) * t,
            Scale::Log => lo * (hi / lo).powf(t),
        }
    }
}

/// A horizontal track with a value. Drag anywhere on it; the value follows the
/// pointer absolutely rather than accumulating deltas, so it cannot drift.
/// Holding shift drags at a tenth of the rate for fine placement.
pub fn slider(
    ui: &mut Ui,
    id: Id,
    rect: Rect,
    value: f64,
    lo: f64,
    hi: f64,
    scale: Scale,
) -> Option<f64> {
    let r = ui.region(id, rect);
    let mut out = None;

    if r.pressed {
        ui.state.drag_origin = Some((id, r.pointer.unwrap_or(rect.center()), value));
    }
    if r.held {
        if let (Some(q), Some((oid, origin, start))) = (r.pointer, ui.state.drag_origin) {
            if oid == id {
                let raw_t = ((q.x - rect.left()) / rect.width()) as f64;
                let v = if ui.input.shift {
                    // Fine drag: move a tenth of the pointer travel from where
                    // the drag began, so precision does not need a second control.
                    let dt = ((q.x - origin.x) / rect.width()) as f64 * 0.1;
                    scale.from_t(scale.to_t(start, lo, hi) + dt, lo, hi)
                } else {
                    scale.from_t(raw_t, lo, hi)
                };
                out = Some(v.clamp(lo.min(hi), hi.max(lo)));
            }
        }
    }

    let shown = out.unwrap_or(value);
    let t = scale.to_t(shown, lo, hi) as f32;
    let mid = rect.center().y;
    hairline(ui.p, Pos2::new(rect.left(), mid), Pos2::new(rect.right(), mid), theme::RULE);
    let x = rect.left() + t * rect.width();
    hairline(ui.p, Pos2::new(rect.left(), mid), Pos2::new(x, mid), theme::mix(theme::RULE, theme::ACCENT, 0.7));
    let c = if r.held || r.hovered { theme::ACCENT } else { theme::INK };
    ui.p.circle_filled(Pos2::new(x, mid), 3.5, c);
    out
}

/// A numeric readout that is also a control: drag vertically to change it,
/// click to type into it. The typed form is owned by the caller so the field
/// stays a pure function of state.
pub struct FieldState {
    pub editing: Option<String>,
    pub cursor: usize,
}

impl Default for FieldState {
    fn default() -> Self {
        Self {
            editing: None,
            cursor: 0,
        }
    }
}

pub struct FieldStyle {
    pub label: &'static str,
    pub unit: &'static str,
    pub decimals: usize,
    pub step: f64,
    pub lo: f64,
    pub hi: f64,
    pub scale: Scale,
}

/// Drag-and-type numeric field. Returns a new value when it changes.
#[allow(clippy::too_many_arguments)]
pub fn number_field(
    ui: &mut Ui,
    id: Id,
    rect: Rect,
    value: f64,
    style: &FieldStyle,
    st: &mut FieldState,
    valid: bool,
) -> Option<f64> {
    let r = ui.region(id, rect);
    let focused = ui.focused(id);
    let mut out = None;

    // Enter edit mode on a double click; a single drag stays a drag.
    if r.double_clicked {
        st.editing = Some(format!("{:.*}", style.decimals, value));
        st.cursor = st.editing.as_ref().map(|s| s.len()).unwrap_or(0);
    }

    if st.editing.is_some() && focused {
        let mut buf = st.editing.take().unwrap_or_default();
        for ch in ui.input.text.chars() {
            if ch.is_ascii_digit() || ch == '.' || ch == '-' || ch == 'e' {
                buf.insert(st.cursor.min(buf.len()), ch);
                st.cursor += 1;
            }
        }
        if ui.input.key(eframe::egui::Key::Backspace) && st.cursor > 0 {
            st.cursor -= 1;
            buf.remove(st.cursor);
        }
        if ui.input.key(eframe::egui::Key::ArrowLeft) {
            st.cursor = st.cursor.saturating_sub(1);
        }
        if ui.input.key(eframe::egui::Key::ArrowRight) {
            st.cursor = (st.cursor + 1).min(buf.len());
        }
        if ui.input.key(eframe::egui::Key::Enter) {
            if let Ok(v) = buf.trim().parse::<f64>() {
                out = Some(v.clamp(style.lo, style.hi));
            }
            st.editing = None;
        } else if ui.input.key(eframe::egui::Key::Escape) {
            st.editing = None;
        } else {
            st.editing = Some(buf);
        }
    } else if st.editing.is_some() && !focused {
        st.editing = None;
    }

    // Vertical drag, scaled so a full field height covers the useful span.
    if r.pressed {
        ui.state.drag_origin = Some((id, r.pointer.unwrap_or(rect.center()), value));
    }
    if r.held && st.editing.is_none() {
        if let (Some(q), Some((oid, origin, start))) = (r.pointer, ui.state.drag_origin) {
            if oid == id {
                let dy = (origin.y - q.y) as f64;
                let fine = if ui.input.shift { 0.1 } else { 1.0 };
                let v = match style.scale {
                    Scale::Linear => start + dy * style.step * fine,
                    Scale::Log => start * (2.0f64).powf(dy * style.step * fine),
                };
                out = Some(v.clamp(style.lo, style.hi));
            }
        }
    }

    let editing = st.editing.is_some();
    let bg = if editing {
        theme::mix(theme::PANEL, theme::ACCENT, 0.12)
    } else if r.hovered || r.held {
        theme::PANEL_HI
    } else {
        theme::PANEL
    };
    fill(ui.p, rect, bg);
    outline(
        ui.p,
        rect,
        if !valid {
            theme::BAD
        } else if editing {
            theme::ACCENT
        } else if r.hovered {
            theme::RULE_HI
        } else {
            theme::RULE
        },
    );
    text(
        ui.p,
        rect.min + Vec2::new(5.0, 3.0),
        style.label,
        theme::T_MICRO,
        theme::DIM,
    );
    let shown = match &st.editing {
        Some(b) => b.clone(),
        None => format!("{:.*}{}", style.decimals, out.unwrap_or(value), style.unit),
    };
    text_right(
        ui.p,
        Pos2::new(rect.right() - 5.0, rect.top() + rect.height() - 15.0),
        &shown,
        theme::T_BODY,
        if valid { theme::INK_HI } else { theme::BAD },
    );
    if editing {
        let w = paint::text_width(ui.p, &shown, theme::T_BODY);
        let cx = rect.right() - 5.0 - w
            + paint::char_width(ui.p, theme::T_BODY) * st.cursor.min(shown.len()) as f32;
        let y = rect.top() + rect.height() - 15.0;
        hairline(
            ui.p,
            Pos2::new(cx, y),
            Pos2::new(cx, y + paint::line_height(ui.p, theme::T_BODY)),
            theme::ACCENT,
        );
    }
    out
}

/// A selectable row. Returns true when chosen.
pub fn row(
    ui: &mut Ui,
    id: Id,
    rect: Rect,
    selected: bool,
    accent: Option<Color32>,
) -> Response {
    let r = ui.region(id, rect);
    let bg = if selected {
        theme::SELECT
    } else if r.hovered {
        theme::PANEL_HI
    } else {
        theme::PANEL
    };
    fill(ui.p, rect, bg);
    if let Some(c) = accent {
        fill(
            ui.p,
            Rect::from_min_size(rect.min, Vec2::new(2.0, rect.height())),
            c,
        );
    }
    if selected {
        outline(ui.p, rect, theme::mix(theme::SELECT, theme::ACCENT, 0.5));
    }
    r
}

/// A vertical scroller. Returns the offset to draw content at, and paints its
/// own bar. Content taller than the view is the only case that scrolls.
pub fn scroll_area(ui: &mut Ui, id: Id, rect: Rect, content_h: f32) -> f32 {
    let max = (content_h - rect.height()).max(0.0);
    let cur = ui.state.scroll.get(&id).copied().unwrap_or(0.0);
    let r = ui.region(id.child("scroll"), rect);
    let mut off = cur;
    if r.hovered || r.held {
        off -= r.scroll;
    }
    off = off.clamp(0.0, max);
    ui.state.scroll.insert(id, off);

    if max > 0.0 {
        let track = Rect::from_min_size(
            Pos2::new(rect.right() - 3.0, rect.top()),
            Vec2::new(3.0, rect.height()),
        );
        fill(ui.p, track, theme::mix(theme::BG, theme::RULE, 0.5));
        let frac = rect.height() / content_h;
        let h = (rect.height() * frac).max(18.0);
        let y = rect.top() + (rect.height() - h) * (off / max);
        fill(
            ui.p,
            Rect::from_min_size(Pos2::new(track.left(), y), Vec2::new(3.0, h)),
            theme::RULE_HI,
        );
    }
    off
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn log_scale_round_trips_across_the_audio_band() {
        for v in [20.0, 100.0, 1000.0, 18_000.0] {
            let t = Scale::Log.to_t(v, 20.0, 20_000.0);
            let back = Scale::Log.from_t(t, 20.0, 20_000.0);
            assert!((back - v).abs() < 1e-6, "{v} -> {t} -> {back}");
        }
    }

    #[test]
    fn linear_scale_round_trips_and_clamps() {
        assert!((Scale::Linear.from_t(0.5, 0.0, 1.0) - 0.5).abs() < 1e-12);
        assert_eq!(Scale::Linear.from_t(2.0, 0.0, 1.0), 1.0);
        assert_eq!(Scale::Linear.to_t(-5.0, 0.0, 1.0), 0.0);
    }
}
