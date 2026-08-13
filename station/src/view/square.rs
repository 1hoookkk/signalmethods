//! The square: four corners, morph across, Q up.
//!
//! The square is the control. Its area is the morph × Q plane, its four
//! corners are the authored frames, and the puck inside it is the
//! interpolation position — so the picture of the object and the pad used to
//! move through it are the same surface.

use eframe::egui::{Pos2, Rect, Stroke, Vec2};

use crate::app::Station;
use crate::model::analysis;
use crate::model::interp::corner_weights;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{chip, fill, hairline, label, label_center, num, spark};
use crate::ui::theme;

/// Screen position of a `(morph, q)` coordinate inside the pad.
fn at(pad: Rect, morph: f32, q: f32) -> Pos2 {
    Pos2::new(
        pad.left() + morph.clamp(0.0, 1.0) * pad.width(),
        pad.bottom() - q.clamp(0.0, 1.0) * pad.height(),
    )
}

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    // A square pad, centred, with room for the axis labels around it.
    let side = (r.width() - 96.0).min(r.height() - 78.0).max(120.0);
    let pad = Rect::from_center_size(
        Pos2::new(r.center().x, r.center().y - 6.0),
        Vec2::splat(side),
    );

    fill(ui.p, pad, theme::SURFACE);

    // Quarter rules, so position is readable without a numeric readout.
    for i in 1..4 {
        let t = i as f32 / 4.0;
        let c = theme::mix(
            theme::SURFACE_LO,
            theme::LINE,
            if i == 2 { 0.9 } else { 0.45 },
        );
        hairline(
            ui.p,
            Pos2::new(pad.left() + t * pad.width(), pad.top()),
            Pos2::new(pad.left() + t * pad.width(), pad.bottom()),
            c,
        );
        hairline(
            ui.p,
            Pos2::new(pad.left(), pad.bottom() - t * pad.height()),
            Pos2::new(pad.right(), pad.bottom() - t * pad.height()),
            c,
        );
    }

    // Axis labels.
    label_center(
        ui.p,
        Pos2::new(pad.center().x, pad.bottom() + 20.0),
        "MORPH",
        theme::T_SMALL,
        theme::TEXT_DIM,
    );
    let qx = pad.left() - 30.0;
    label_center(
        ui.p,
        Pos2::new(qx, pad.center().y),
        "Q",
        theme::T_SMALL,
        theme::TEXT_DIM,
    );
    for (x, s) in [(pad.left(), "0"), (pad.right(), "100")] {
        num(
            ui.p,
            Pos2::new(x - 6.0, pad.bottom() + 4.0),
            s,
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }
    for (y, s) in [(pad.bottom(), "0"), (pad.top(), "100")] {
        num(
            ui.p,
            Pos2::new(pad.left() - 26.0, y - 7.0),
            s,
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }

    let weights = corner_weights(&st.coords);
    let id = Id::of("square");

    // The pad drags the position.
    let resp = ui.region(id.child("pad"), pad);
    if resp.held {
        if let Some(q) = resp.pointer {
            if st.coords.len() >= 2 {
                st.coords[0] = ((q.x - pad.left()) / pad.width()).clamp(0.0, 1.0);
                st.coords[1] = ((pad.bottom() - q.y) / pad.height()).clamp(0.0, 1.0);
                st.touch();
            }
        }
    }

    // Corners, drawn at the four corners of the pad they address.
    for i in 0..st.project.frames().len().min(4) {
        let a = &st.project.frames()[i].address;
        let (m, q) = (a[0] as f32, a[1] as f32);
        let p = at(pad, m, q);
        let c = theme::corner_color(i);
        let selected = i == st.selected_corner;
        let w = weights.get(i).copied().unwrap_or(0.0);

        // The label sits inside the pad, tucked against its corner.
        let dx = if m > 0.5 { -112.0 } else { 10.0 };
        let dy = if q > 0.5 { 10.0 } else { -62.0 };
        let card = Rect::from_min_size(p + Vec2::new(dx, dy), Vec2::new(102.0, 54.0));
        let cr = ui.region(id.child(i), card);
        fill(
            ui.p,
            card,
            if selected || cr.hovered {
                theme::SURFACE_HI
            } else {
                theme::SURFACE
            },
        );
        chip(
            ui.p,
            card.min + Vec2::new(6.0, 5.0),
            &format!("C{}", i + 1),
            c,
        );
        label(
            ui.p,
            card.min + Vec2::new(6.0, 16.0),
            super::corner_name(st, i),
            theme::T_SMALL,
            theme::TEXT_DIM,
        );
        if w > 0.005 {
            num(
                ui.p,
                Pos2::new(card.right() - 34.0, card.top() + 6.0),
                format!("{:>3.0}%", w * 100.0),
                theme::T_SMALL,
                c,
            );
        }
        // Each corner shows what it holds.
        let resp = analysis::analyse(&st.project.frames()[i].values, st.project.sample_rate());
        spark(
            ui.p,
            Rect::from_min_max(
                Pos2::new(card.left() + 4.0, card.top() + 30.0),
                Pos2::new(card.right() - 4.0, card.bottom() - 3.0),
            ),
            &resp.grid,
            &resp.total,
            st.settings.display_lo_hz,
            st.settings.display_hi_hz,
            c,
        );

        if cr.clicked {
            st.selected_corner = i;
            st.coords[0] = m;
            st.coords[1] = q;
            st.touch();
        }

        ui.p.circle_filled(p, if selected { 6.0 } else { 4.5 }, c);
    }

    // The position.
    if st.coords.len() >= 2 {
        let p = at(pad, st.coords[0], st.coords[1]);
        hairline(
            ui.p,
            Pos2::new(pad.left(), p.y),
            Pos2::new(pad.right(), p.y),
            theme::mix(theme::SURFACE_LO, theme::ACCENT, 0.35),
        );
        hairline(
            ui.p,
            Pos2::new(p.x, pad.top()),
            Pos2::new(p.x, pad.bottom()),
            theme::mix(theme::SURFACE_LO, theme::ACCENT, 0.35),
        );
        ui.p.circle_filled(p, 5.0, theme::BG);
        ui.p.circle_stroke(p, 6.5, Stroke::new(2.0, theme::ACCENT));

        num(
            ui.p,
            Pos2::new(pad.left(), pad.top() - 20.0),
            format!("morph {:.3}    q {:.3}", st.coords[0], st.coords[1]),
            theme::T_SMALL,
            theme::TEXT_DIM,
        );
    }
}
