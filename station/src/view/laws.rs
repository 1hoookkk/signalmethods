//! Laws and grammar: editable text with diagnostics beside it.
//!
//! Nothing here blocks authoring. A law that does not hold, or one naming a
//! quantity the Station cannot measure, is reported and the work continues.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::Station;
use crate::model::law;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{fill, label, num_right, panel};
use crate::ui::theme;
use crate::ui::widgets;

use super::PAD;

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let col = (r.width() - PAD * 2.0) / 3.0;
    editor(
        st,
        ui,
        Rect::from_min_size(r.min, Vec2::new(col, r.height())),
        true,
    );
    editor(
        st,
        ui,
        Rect::from_min_size(
            Pos2::new(r.left() + col + PAD, r.top()),
            Vec2::new(col, r.height()),
        ),
        false,
    );
    readings(
        st,
        ui,
        Rect::from_min_max(Pos2::new(r.left() + (col + PAD) * 2.0, r.top()), r.max),
    );
}

fn editor(st: &mut Station, ui: &mut Ui, r: Rect, is_laws: bool) {
    let dirty = if is_laws {
        st.laws_editor.dirty()
    } else {
        st.grammar_editor.dirty()
    };
    let title = if is_laws { "Laws" } else { "Grammar" };
    let inner = panel(ui.p, r, title);
    if dirty {
        ui.p.circle_filled(
            Pos2::new(r.right() - 14.0, r.top() + 13.0),
            3.5,
            theme::WARN,
        );
    }

    let bar_h = 30.0;
    let ed = Rect::from_min_max(
        inner.min,
        Pos2::new(inner.right(), inner.bottom() - bar_h - 20.0),
    );
    let id = Id::of(if is_laws { "laws-ed" } else { "gram-ed" });
    if is_laws {
        st.laws_editor.show(ui, id, ed);
    } else {
        st.grammar_editor.show(ui, id, ed);
    }

    let by = ed.bottom() + 6.0;
    if widgets::button(
        ui,
        id.child("apply"),
        Rect::from_min_size(Pos2::new(inner.left(), by), Vec2::new(72.0, 26.0)),
        "Apply",
        dirty,
    ) {
        if is_laws {
            st.apply_laws();
        } else {
            st.apply_grammar();
        }
    }
    if widgets::button(
        ui,
        id.child("revert"),
        Rect::from_min_size(Pos2::new(inner.left() + 78.0, by), Vec2::new(72.0, 26.0)),
        "Revert",
        dirty,
    ) {
        if is_laws {
            st.laws_editor.revert();
        } else {
            st.grammar_editor.revert();
        }
    }
    if is_laws
        && widgets::button(
            ui,
            id.child("reload"),
            Rect::from_min_size(Pos2::new(inner.left() + 156.0, by), Vec2::new(72.0, 26.0)),
            "Reload",
            true,
        )
    {
        let p = st.laws_path.clone();
        if let Ok(t) = std::fs::read_to_string(&p) {
            st.laws_editor.set(t);
            st.apply_laws();
        }
    }

    let errs = if is_laws {
        st.laws_editor.errors.clone()
    } else {
        st.grammar_editor.errors.clone()
    };
    if let Some(e) = errs.first() {
        label(
            ui.p,
            Pos2::new(inner.left(), by + 30.0),
            format!("line {}  {}", e.line, first_line(&e.message)),
            theme::T_SMALL,
            if e.fatal { theme::BAD } else { theme::WARN },
        );
    }
}

fn first_line(s: &str) -> String {
    let one = s.lines().next().unwrap_or(s);
    if one.len() > 58 {
        format!("{}…", &one[..58])
    } else {
        one.to_string()
    }
}

fn readings(st: &mut Station, ui: &mut Ui, r: Rect) {
    let inner = panel(ui.p, r, "Diagnostics");
    let readings = law::read_all(&st.project, &st.laws.laws, st.selected_corner);
    if readings.is_empty() {
        label(ui.p, inner.min, "none", theme::T_SMALL, theme::TEXT_FAINT);
        return;
    }
    let row_h = 40.0;
    for (i, rd) in readings.iter().enumerate() {
        let y = inner.top() + i as f32 * row_h;
        if y + row_h > inner.bottom() {
            break;
        }
        let c = if rd.ok { theme::GOOD } else { theme::WARN };
        ui.p.circle_filled(Pos2::new(inner.left() + 4.0, y + 8.0), 3.5, c);
        label(
            ui.p,
            Pos2::new(inner.left() + 14.0, y),
            &rd.law.q,
            theme::T_BODY,
            theme::TEXT,
        );
        num_right(
            ui.p,
            Pos2::new(inner.right(), y + 1.0),
            format!("{:.2}", rd.value),
            theme::T_BODY,
            c,
        );
        label(
            ui.p,
            Pos2::new(inner.left() + 14.0, y + 17.0),
            format!("{:.2} to {:.2}", rd.law.min, rd.law.max),
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );

        // The span, with the reading on it.
        let g = Rect::from_min_size(
            Pos2::new(inner.right() - 110.0, y + 22.0),
            Vec2::new(104.0, 6.0),
        );
        fill(ui.p, g, theme::SURFACE_LO);
        let pad = (rd.law.max - rd.law.min).abs().max(1e-9) * 0.35;
        let (a, b) = (rd.law.min - pad, rd.law.max + pad);
        let at = |x: f64| g.left() + (((x - a) / (b - a)).clamp(0.0, 1.0) as f32) * g.width();
        fill(
            ui.p,
            Rect::from_min_max(
                Pos2::new(at(rd.law.min), g.top()),
                Pos2::new(at(rd.law.max), g.bottom()),
            ),
            theme::mix(theme::SURFACE_LO, theme::LINE_HI, 0.8),
        );
        ui.p.circle_filled(Pos2::new(at(rd.value), g.center().y), 3.5, c);
    }
}
