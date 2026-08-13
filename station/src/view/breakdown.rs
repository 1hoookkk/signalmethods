//! Per-section breakdown: one small plot per section, in a row.
//!
//! Small multiples. Each section is drawn on its own baseline so the shapes can
//! be compared down the row, with the running cascade behind it in grey so the
//! contribution of each section to the chain is visible in the same glance.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::Station;
use crate::model::analysis::CascadeResponse;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{fill, label, num, spark};
use crate::ui::theme;

/// Draws the strip for one corner's cascade. Returns nothing; selection is
/// written straight back into the station.
pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect, resp: &CascadeResponse) {
    let inner = crate::ui::paint::panel(ui.p, r, "Sections");
    let active = st.project.active_lanes();
    if active.is_empty() {
        label(ui.p, inner.min, "none", theme::T_SMALL, theme::TEXT_FAINT);
        return;
    }

    let (lo_hz, hi_hz) = (st.grammar.display_lo_hz, st.grammar.display_hi_hz);
    let n = active.len();
    let gap = 8.0;
    let w = ((inner.width() - gap * (n - 1) as f32) / n as f32).max(40.0);
    let id = Id::of("breakdown");

    for (k, &li) in active.iter().enumerate() {
        let cell = Rect::from_min_size(
            Pos2::new(inner.left() + k as f32 * (w + gap), inner.top()),
            Vec2::new(w, inner.height()),
        );
        let selected = li == st.selected_lane;
        let resp_r = ui.region(id.child(li), cell);
        if selected {
            fill(ui.p, cell, theme::SELECT);
        } else if resp_r.hovered {
            fill(ui.p, cell, theme::SURFACE_HI);
        }
        if resp_r.clicked {
            st.selected_lane = li;
        }

        let head = 16.0;
        label(
            ui.p,
            cell.min + Vec2::new(4.0, 0.0),
            super::section_name(st, li),
            theme::T_SMALL,
            if selected {
                theme::TEXT
            } else {
                theme::TEXT_DIM
            },
        );
        let plot = Rect::from_min_max(
            Pos2::new(cell.left() + 4.0, cell.top() + head),
            Pos2::new(cell.right() - 4.0, cell.bottom() - 14.0),
        );
        if plot.height() < 12.0 {
            continue;
        }
        // The chain so far, behind, then this section on its own.
        if let Some(c) = resp.cumulative.get(li) {
            spark(ui.p, plot, &resp.grid, c, lo_hz, hi_hz, theme::LINE_HI);
        }
        if let Some(own) = resp.per_lane.get(li) {
            spark(
                ui.p,
                plot,
                &resp.grid,
                own,
                lo_hz,
                hi_hz,
                theme::corner_color(li),
            );
        }
        if let Some(m) = resp.lane_metrics.get(li) {
            if let Some(hz) = m.pole_hz {
                num(
                    ui.p,
                    Pos2::new(cell.left() + 4.0, cell.bottom() - 13.0),
                    format!("{hz:.0} Hz"),
                    theme::T_SMALL,
                    theme::TEXT_FAINT,
                );
            }
        }
    }
}
