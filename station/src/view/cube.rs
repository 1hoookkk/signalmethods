//! The cube: eight corners, morph × Q × T.
//!
//! Drawn as a literal cube — eight vertices, twelve edges — with the
//! interpolation position marked inside it. The morph/Q pad and the T slider
//! beneath it are the controls; the cube is the structure they move through.

use eframe::egui::{Pos2, Rect, Stroke, Vec2};

use crate::app::Station;
use crate::model::analysis;
use crate::model::interp::corner_weights;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{chip, fill, hairline, label, label_center, line, num, spark};
use crate::ui::theme;
use crate::ui::widgets::{self, Scale};

/// Cube coordinates in view space: morph on x, Q on y, T into the screen.
fn vertex(m: f32, q: f32, t: f32) -> (f32, f32, f32) {
    (m * 2.0 - 1.0, q * 2.0 - 1.0, t * 2.0 - 1.0)
}

/// Fixed viewing angle, tilted so all three axes read distinctly and no edge
/// hides behind another.
fn project(v: (f32, f32, f32), c: Pos2, s: f32, yaw: f32, pitch: f32) -> Pos2 {
    let (x, y, z) = v;
    let (sy, cy) = yaw.sin_cos();
    let (sp, cp) = pitch.sin_cos();
    let x1 = x * cy + z * sy;
    let z1 = -x * sy + z * cy;
    let y1 = y * cp - z1 * sp;
    let z2 = y * sp + z1 * cp;
    let k = 4.2 / (4.2 + z2);
    Pos2::new(c.x + x1 * s * k, c.y - y1 * s * k)
}

fn depth(v: (f32, f32, f32), yaw: f32, pitch: f32) -> f32 {
    let (x, y, z) = v;
    let (sy, cy) = yaw.sin_cos();
    let (sp, cp) = pitch.sin_cos();
    let z1 = -x * sy + z * cy;
    y * sp + z1 * cp
}

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let controls_h = 96.0;
    let stage = Rect::from_min_max(
        r.min,
        Pos2::new(
            r.right(),
            (r.bottom() - controls_h - 12.0).max(r.top() + 80.0),
        ),
    );

    let id = Id::of("cube");
    let scale = stage.height().min(stage.width()) * 0.27;
    let centre = Pos2::new(stage.center().x, stage.center().y);

    // Orbiting is available but the default angle already reads as a cube.
    let bg = ui.region(id.child("stage"), stage);
    if bg.held {
        st.cube_yaw += bg.drag_delta.x * 0.007;
        st.cube_pitch = (st.cube_pitch + bg.drag_delta.y * 0.007).clamp(-1.1, 1.1);
    }
    let (yaw, pitch) = (st.cube_yaw, st.cube_pitch);

    let n = st.project.frames().len().min(8);
    let mut pts = Vec::with_capacity(n);
    for i in 0..n {
        let a = &st.project.frames()[i].address;
        let v = vertex(a[0] as f32, a[1] as f32, *a.get(2).unwrap_or(&0) as f32);
        pts.push((project(v, centre, scale, yaw, pitch), depth(v, yaw, pitch)));
    }

    // Twelve edges: every pair of corners differing in exactly one axis.
    for a in 0..n {
        for b in (a + 1)..n {
            if (a ^ b).count_ones() != 1 {
                continue;
            }
            let far = (pts[a].1 + pts[b].1) * 0.5;
            let fade = ((far + 1.4) / 2.8).clamp(0.0, 1.0);
            line(
                ui.p,
                pts[a].0,
                pts[b].0,
                theme::mix(theme::LINE_HI, theme::SURFACE_LO, fade * 0.7),
                if fade < 0.5 { 1.6 } else { 1.0 },
            );
        }
    }

    // The interpolation position inside the cube.
    if st.coords.len() >= 3 {
        let v = vertex(st.coords[0], st.coords[1], st.coords[2]);
        let p = project(v, centre, scale, yaw, pitch);
        ui.p.circle_filled(p, 5.0, theme::BG);
        ui.p.circle_stroke(p, 6.5, Stroke::new(2.0, theme::ACCENT));
    }

    let weights = corner_weights(&st.coords);
    let mut order: Vec<usize> = (0..n).collect();
    order.sort_by(|&a, &b| {
        pts[b]
            .1
            .partial_cmp(&pts[a].1)
            .unwrap_or(std::cmp::Ordering::Equal)
    });

    for &i in &order {
        let (p, _) = pts[i];
        let c = theme::corner_color(i);
        let selected = i == st.selected_corner;
        let w = weights.get(i).copied().unwrap_or(0.0);

        let resp = ui.region(id.child(i), Rect::from_center_size(p, Vec2::splat(20.0)));

        ui.p.circle_filled(p, if selected { 6.0 } else { 4.0 }, c);

        // Every corner carries a crude plot of what it holds, pushed clear of
        // the cube so the wireframe stays readable.
        let away = (p - centre).normalized();
        let size = Vec2::new(74.0, 40.0);
        let card = Rect::from_center_size(p + away * 44.0, size);
        let hit2 = ui.region(id.child(("card", i)), card);
        if hit2.clicked || resp.clicked {
            st.selected_corner = i;
            let a = st.project.frames()[i].address.clone();
            for (k, v) in a.iter().enumerate() {
                if k < st.coords.len() {
                    st.coords[k] = *v as f32;
                }
            }
            st.touch();
        }
        if selected || hit2.hovered {
            fill(ui.p, card, theme::SELECT);
        }
        hairline(ui.p, p, card.center(), theme::mix(theme::BG, c, 0.5));
        let ch = chip(
            ui.p,
            card.min + Vec2::new(2.0, 0.0),
            &format!("C{}", i + 1),
            c,
        );
        if w > 0.005 {
            num(
                ui.p,
                Pos2::new(ch.right() + 3.0, ch.top() + 1.0),
                format!("{:.0}%", w * 100.0),
                theme::T_SMALL,
                theme::TEXT_DIM,
            );
        }
        let cr = analysis::analyse(&st.project.frames()[i].values, st.project.sample_rate());
        spark(
            ui.p,
            Rect::from_min_max(
                Pos2::new(card.left() + 3.0, card.top() + 18.0),
                Pos2::new(card.right() - 3.0, card.bottom() - 2.0),
            ),
            &cr.grid,
            &cr.total,
            st.settings.display_lo_hz,
            st.settings.display_hi_hz,
            c,
        );
    }

    controls(
        st,
        ui,
        Rect::from_min_max(Pos2::new(r.left(), r.bottom() - controls_h), r.max),
    );
}

/// Morph × Q as a pad, T as a track.
fn controls(st: &mut Station, ui: &mut Ui, r: Rect) {
    let id = Id::of("cube-controls");
    let pad_side = r.height().min(96.0);
    let pad = Rect::from_min_size(Pos2::new(r.left() + 34.0, r.top()), Vec2::splat(pad_side));
    fill(ui.p, pad, theme::SURFACE);
    for i in 1..4 {
        let t = i as f32 / 4.0;
        let c = theme::mix(
            theme::SURFACE_LO,
            theme::LINE,
            if i == 2 { 0.8 } else { 0.4 },
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
    let resp = ui.region(id.child("pad"), pad);
    if resp.held {
        if let Some(q) = resp.pointer {
            st.coords[0] = ((q.x - pad.left()) / pad.width()).clamp(0.0, 1.0);
            st.coords[1] = ((pad.bottom() - q.y) / pad.height()).clamp(0.0, 1.0);
            st.touch();
        }
    }
    let p = Pos2::new(
        pad.left() + st.coords[0] * pad.width(),
        pad.bottom() - st.coords[1] * pad.height(),
    );
    ui.p.circle_filled(p, 4.0, theme::BG);
    ui.p.circle_stroke(p, 5.5, Stroke::new(2.0, theme::ACCENT));
    label_center(
        ui.p,
        Pos2::new(pad.center().x, pad.bottom() + 10.0),
        "MORPH",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );
    label_center(
        ui.p,
        Pos2::new(pad.left() - 18.0, pad.center().y),
        "Q",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );

    // T on its own track.
    let tx = pad.right() + 40.0;
    let track = Rect::from_min_size(Pos2::new(tx, pad.top() + 18.0), Vec2::new(200.0, 14.0));
    label(
        ui.p,
        Pos2::new(tx, pad.top()),
        "T",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );
    if st.coords.len() >= 3 {
        if let Some(v) = widgets::slider(
            ui,
            id.child("t"),
            track,
            st.coords[2] as f64,
            0.0,
            1.0,
            Scale::Linear,
        ) {
            st.coords[2] = v as f32;
            st.touch();
        }
        num(
            ui.p,
            Pos2::new(track.right() + 10.0, track.top() - 1.0),
            format!("{:.3}", st.coords[2]),
            theme::T_SMALL,
            theme::TEXT_DIM,
        );
        num(
            ui.p,
            Pos2::new(tx, track.bottom() + 10.0),
            format!("morph {:.3}   q {:.3}", st.coords[0], st.coords[1]),
            theme::T_SMALL,
            theme::TEXT_DIM,
        );
    }
}
