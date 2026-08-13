//! Workspace 2 — perceptual intent.
//!
//! This workspace holds the operator's motion, not the filter's registers. No
//! coefficient, word or radius appears here by design: the boundary between
//! what is intended and how it is stored is the reason the workspaces are
//! separate at all.
//!
//! What it does is real. The bisection control re-times travel along the first
//! axis so the middle of a sweep lands where the middle of the transition
//! actually happens rather than at the arithmetic midpoint, and the response
//! beneath it is recomputed through that warp. It moves where the object is
//! sampled and never edits an authored frame.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::{warp, Station, PAD};
use crate::model::analysis;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{
    curve, db_rules, divider, fill, frequency_rules, hairline, line, outline, text, text_right,
};
use crate::ui::theme;
use crate::ui::widgets::{self, Scale};

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let left_w = r.width() * 0.44;
    let left = Rect::from_min_size(r.min, Vec2::new(left_w, r.height()));
    let right = Rect::from_min_max(Pos2::new(left.right() + PAD, r.top()), r.max);

    bisection_panel(st, ui, left);
    sweep_panel(st, ui, right);
}

/// The bisection curve: travel in, perceived position out.
fn bisection_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    let axis_name = st
        .project
        .topology
        .axes
        .first()
        .map(|a| st.grammar.axis_name(0, &a.name))
        .unwrap_or_else(|| "AXIS".into());
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        format!("BISECTION  —  re-times travel along {axis_name}"),
        theme::T_MICRO,
        theme::DIM,
    );

    let id = Id::of("bisect");
    let toggle_r = Rect::from_min_size(r.min + Vec2::new(8.0, 24.0), Vec2::new(150.0, 22.0));
    if widgets::toggle(ui, id.child("on"), toggle_r, "WARP ACTIVE", st.warp_on) {
        st.warp_on = !st.warp_on;
        st.touch();
    }
    text_right(
        ui.p,
        Pos2::new(r.right() - 8.0, r.top() + 28.0),
        if st.warp_on {
            format!("midpoint at {:.0}", st.warp_mid * 100.0)
        } else {
            "identity".into()
        },
        theme::T_SMALL,
        if st.warp_on {
            theme::ACCENT
        } else {
            theme::DIM
        },
    );

    // The curve itself, drawn on a square field: x is the operator's travel,
    // y is where the object is sampled.
    let side = (r.width() - 32.0).min(r.height() - 150.0).max(80.0);
    let field = Rect::from_min_size(
        Pos2::new(r.left() + 16.0, r.top() + 58.0),
        Vec2::splat(side),
    );
    fill(ui.p, field, theme::BG);
    outline(ui.p, field, theme::RULE);
    for i in 1..4 {
        let t = i as f32 / 4.0;
        hairline(
            ui.p,
            Pos2::new(field.left() + t * field.width(), field.top()),
            Pos2::new(field.left() + t * field.width(), field.bottom()),
            theme::mix(theme::BG, theme::RULE, 0.6),
        );
        hairline(
            ui.p,
            Pos2::new(field.left(), field.bottom() - t * field.height()),
            Pos2::new(field.right(), field.bottom() - t * field.height()),
            theme::mix(theme::BG, theme::RULE, 0.6),
        );
    }
    // The identity, for comparison.
    line(
        ui.p,
        field.left_bottom(),
        field.right_top(),
        theme::FAINT,
        1.0,
    );

    let mid = st.warp_mid;
    let mut prev: Option<Pos2> = None;
    for i in 0..=64 {
        let t = i as f32 / 64.0;
        let v = if st.warp_on { warp(t, mid) } else { t };
        let pt = Pos2::new(
            field.left() + t * field.width(),
            field.bottom() - v * field.height(),
        );
        if let Some(q) = prev {
            line(ui.p, q, pt, theme::ACCENT, 2.0);
        }
        prev = Some(pt);
    }

    // The anchor is dragged directly on the field.
    let anchor = Pos2::new(field.center().x, field.bottom() - mid * field.height());
    let hit = Rect::from_center_size(anchor, Vec2::splat(22.0));
    let resp = ui.region(id.child("anchor"), hit);
    if resp.held {
        if let Some(q) = resp.pointer {
            st.warp_mid = ((field.bottom() - q.y) / field.height()).clamp(0.02, 0.98);
            st.warp_on = true;
            st.touch();
        }
    }
    ui.p.circle_filled(anchor, 5.0, theme::ACCENT);
    ui.p.circle_stroke(
        anchor,
        8.0,
        eframe::egui::Stroke::new(1.0, theme::mix(theme::PANEL, theme::ACCENT, 0.6)),
    );
    text(
        ui.p,
        Pos2::new(field.left() + 4.0, field.bottom() - 14.0),
        "travel",
        theme::T_MICRO,
        theme::FAINT,
    );
    text(
        ui.p,
        Pos2::new(field.left() + 4.0, field.top() + 3.0),
        "sampled position",
        theme::T_MICRO,
        theme::FAINT,
    );

    // Position along the axis, as an intent control rather than a coefficient.
    let track = Rect::from_min_size(
        Pos2::new(r.left() + 16.0, field.bottom() + 22.0),
        Vec2::new(field.width(), 14.0),
    );
    text(
        ui.p,
        Pos2::new(track.left(), track.top() - 13.0),
        format!("{axis_name} POSITION"),
        theme::T_MICRO,
        theme::DIM,
    );
    if !st.coords.is_empty() {
        if let Some(v) = widgets::slider(
            ui,
            id.child("pos"),
            track,
            st.coords[0] as f64,
            0.0,
            1.0,
            Scale::Linear,
        ) {
            st.coords[0] = v as f32;
            st.touch();
        }
        text_right(
            ui.p,
            Pos2::new(track.right(), track.bottom() + 4.0),
            format!(
                "travel {:.0}   sampled {:.0}",
                st.coords[0] * 100.0,
                st.warped_coords()[0] * 100.0
            ),
            theme::T_MICRO,
            theme::INK,
        );
    }

    text(
        ui.p,
        Pos2::new(r.left() + 8.0, r.bottom() - 26.0),
        "The warp moves where the object is sampled. Authored frames are untouched.",
        theme::T_MICRO,
        theme::FAINT,
    );
    text(
        ui.p,
        Pos2::new(r.left() + 8.0, r.bottom() - 14.0),
        "Live audition needs an output device and is not built into this Station.",
        theme::T_MICRO,
        theme::FAINT,
    );
}

/// The sweep as the operator would hear it: the response at even steps of
/// travel, warped, so the effect of the bisection is visible rather than
/// asserted.
fn sweep_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        "SWEEP  —  response at even steps of travel",
        theme::T_MICRO,
        theme::DIM,
    );
    divider(
        ui.p,
        Pos2::new(r.left() + 8.0, r.top() + 22.0),
        Pos2::new(r.right() - 8.0, r.top() + 22.0),
        theme::RULE,
    );

    let plot = Rect::from_min_max(
        Pos2::new(r.left() + 8.0, r.top() + 30.0),
        Pos2::new(r.right() - 8.0, r.bottom() - 30.0),
    );
    let (lo_hz, hi_hz) = (st.grammar.display_lo_hz, st.grammar.display_hi_hz);

    let steps = 9;
    let mut curves = Vec::with_capacity(steps);
    let mut hi_db = f64::MIN;
    let mut lo_db = f64::MAX;
    for i in 0..steps {
        let t = i as f32 / (steps - 1) as f32;
        let mut coords = st.coords.clone();
        if coords.is_empty() {
            break;
        }
        coords[0] = if st.warp_on { warp(t, st.warp_mid) } else { t };
        let lanes = st.project.cascade_at(&coords).unwrap_or_default();
        let resp = analysis::analyse(&lanes, st.project.sample_rate());
        hi_db = hi_db.max(resp.total_peak_db());
        lo_db = lo_db.min(resp.total_min_db());
        curves.push((t, resp));
    }
    if curves.is_empty() {
        return;
    }
    if !hi_db.is_finite() || !lo_db.is_finite() {
        hi_db = 12.0;
        lo_db = -60.0;
    }
    lo_db = lo_db.max(hi_db - 96.0);
    let (lo_db, hi_db) = (lo_db - 3.0, hi_db + 3.0);

    frequency_rules(ui.p, plot, lo_hz, hi_hz, true);
    db_rules(ui.p, plot, lo_db, hi_db, true);
    for (t, resp) in &curves {
        // Steps are shaded from the start of the sweep to its end, so the
        // direction of travel is legible without a legend.
        let c = theme::mix(theme::frame_color(0, 2), theme::frame_color(1, 2), *t);
        curve(
            ui.p,
            plot,
            &resp.grid,
            &resp.total,
            lo_hz,
            hi_hz,
            lo_db,
            hi_db,
            theme::mix(theme::PANEL, c, 0.75),
            1.3,
        );
    }
    // The current position, drawn on top.
    curve(
        ui.p,
        plot,
        &st.live.grid,
        &st.live.total,
        lo_hz,
        hi_hz,
        lo_db,
        hi_db,
        theme::ACCENT,
        2.0,
    );
    text(
        ui.p,
        Pos2::new(plot.left(), r.bottom() - 24.0),
        format!(
            "{steps} steps   ·   accent is the current position   ·   warp {}",
            if st.warp_on { "active" } else { "off" }
        ),
        theme::T_MICRO,
        theme::FAINT,
    );
}
