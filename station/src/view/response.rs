//! The response panel: a compact instrument at the side, not a backdrop.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::Station;
use crate::model::analysis::CascadeResponse;
use crate::ui::input::Ui;
use crate::ui::paint::{curve, db_rules, fill, frequency_rules, label, num_right, panel};
use crate::ui::theme;

/// Draws the cascade at the current position, and optionally one section of it
/// picked out against the whole.
pub fn draw(st: &Station, ui: &mut Ui, r: Rect, highlight: Option<usize>) {
    let inner = panel(ui.p, r, "Response");
    if st.project.is_empty() {
        return;
    }

    let plot = Rect::from_min_max(inner.min, Pos2::new(inner.right(), inner.bottom() - 34.0));
    plot_into(st, ui, plot, &st.live, highlight);

    // Readings beneath the plot.
    let y = plot.bottom() + 8.0;
    let peak = st.live.total_peak_db();
    let worst = st.live.worst_intermediate();
    label(
        ui.p,
        Pos2::new(inner.left(), y),
        "peak",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );
    num_right(
        ui.p,
        Pos2::new(inner.left() + 110.0, y),
        format!("{peak:+.2} dB"),
        theme::T_SMALL,
        theme::TEXT,
    );
    if let Some((li, db)) = worst {
        label(
            ui.p,
            Pos2::new(inner.left() + 130.0, y),
            "worst in chain",
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
        num_right(
            ui.p,
            Pos2::new(inner.right(), y),
            format!("{db:+.2} dB @ S{}", li + 1),
            theme::T_SMALL,
            if db > 24.0 { theme::WARN } else { theme::TEXT },
        );
    }
}

/// The plot itself, reusable wherever a response has to be shown.
pub fn plot_into(
    st: &Station,
    ui: &mut Ui,
    plot: Rect,
    resp: &CascadeResponse,
    highlight: Option<usize>,
) {
    fill(ui.p, plot, theme::SURFACE);
    if resp.total.is_empty() {
        return;
    }
    let (lo_hz, hi_hz) = (st.grammar.display_lo_hz, st.grammar.display_hi_hz);
    let (lo_db, hi_db) = resp.display_span();
    frequency_rules(ui.p, plot, lo_hz, hi_hz, true);
    db_rules(ui.p, plot, lo_db, hi_db, true);

    // The cumulative chain, faint, so build-up through the cascade is visible
    // behind the finished response.
    for c in &resp.cumulative {
        curve(
            ui.p,
            plot,
            &resp.grid,
            c,
            lo_hz,
            hi_hz,
            lo_db,
            hi_db,
            theme::mix(theme::SURFACE_LO, theme::TEXT_FAINT, 0.55),
            1.0,
        );
    }
    if let Some(li) = highlight {
        if let Some(own) = resp.per_lane.get(li) {
            curve(
                ui.p,
                plot,
                &resp.grid,
                own,
                lo_hz,
                hi_hz,
                lo_db,
                hi_db,
                theme::WARN,
                1.6,
            );
        }
    }
    curve(
        ui.p,
        plot,
        &resp.grid,
        &resp.total,
        lo_hz,
        hi_hz,
        lo_db,
        hi_db,
        theme::ACCENT,
        2.0,
    );
}
