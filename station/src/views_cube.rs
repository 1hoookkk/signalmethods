//! The navigator: where authoring starts.
//!
//! The response is the ground, not a panel beside one. It fills the surface,
//! and the object's topology floats on top of it — corners as named nodes with
//! their own response, edges where a single axis moves, and a puck the operator
//! drags through the space between them. What is heard and what is being
//! steered occupy the same rectangle, because they are the same thing.
//!
//! Dragging moves the two displayed axes. Every other axis keeps the explicit
//! coordinate it was given, so a 4D object is navigated two axes at a time
//! without any axis being quietly dropped.

use eframe::egui::{Pos2, Rect, Stroke, Vec2};

use crate::app::Station;
use crate::model::analysis;
use crate::model::interp::corner_weights;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{
    self, curve, curve_fill, db_rules, fill, frequency_rules, hairline, line, outline, text,
    text_right,
};
use crate::ui::theme;

/// A point in the navigator's viewing space.
#[derive(Clone, Copy)]
struct P3 {
    x: f32,
    y: f32,
    z: f32,
}

/// Direction each declared axis pushes a corner. The first three are the
/// object's own edges; further axes fold outward along a diagonal at a
/// shrinking magnitude, which is how an n-cube is drawn without inventing a
/// new rule per dimension.
fn axis_direction(i: usize) -> P3 {
    match i {
        0 => P3 {
            x: 1.0,
            y: 0.0,
            z: 0.0,
        },
        1 => P3 {
            x: 0.0,
            y: 1.0,
            z: 0.0,
        },
        2 => P3 {
            x: 0.0,
            y: 0.0,
            z: 1.0,
        },
        n => {
            let s = 0.62_f32.powi(n as i32 - 2);
            P3 {
                x: 0.70 * s,
                y: 0.50 * s,
                z: 0.70 * s,
            }
        }
    }
}

fn position(coords: &[f32]) -> P3 {
    let mut p = P3 {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    };
    for (i, &c) in coords.iter().enumerate() {
        let d = axis_direction(i);
        let t = c * 2.0 - 1.0;
        p.x += d.x * t;
        p.y += d.y * t;
        p.z += d.z * t;
    }
    p
}

fn project(p: P3, yaw: f32, pitch: f32, r: Rect, scale: f32) -> Pos2 {
    let (sy, cy) = yaw.sin_cos();
    let (sp, cp) = pitch.sin_cos();
    let x1 = p.x * cy + p.z * sy;
    let z1 = -p.x * sy + p.z * cy;
    let y1 = p.y * cp - z1 * sp;
    let z2 = p.y * sp + z1 * cp;
    let d = 5.0;
    let k = d / (d + z2);
    Pos2::new(r.center().x + x1 * scale * k, r.center().y - y1 * scale * k)
}

fn depth(p: P3, yaw: f32, pitch: f32) -> f32 {
    let (sy, cy) = yaw.sin_cos();
    let (sp, cp) = pitch.sin_cos();
    let z1 = -p.x * sy + p.z * cy;
    p.y * sp + z1 * cp
}

/// Screen direction that one unit of `axis` travel moves a point, used to turn
/// a pointer drag back into axis coordinates.
fn axis_screen_vector(axis: usize, yaw: f32, pitch: f32, r: Rect, scale: f32) -> Vec2 {
    let d = axis_direction(axis);
    let a = project(
        P3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        yaw,
        pitch,
        r,
        scale,
    );
    let b = project(
        P3 {
            x: d.x * 2.0,
            y: d.y * 2.0,
            z: d.z * 2.0,
        },
        yaw,
        pitch,
        r,
        scale,
    );
    b - a
}

/// Places one card per node around the edge of the field.
///
/// A card is pushed from the centre through its node out to the boundary, so it
/// ends up on the same side as the corner it belongs to. Overlaps are then
/// relaxed apart, because two corners can project to nearly the same direction
/// and a card that covers another card is worse than one slightly off its ray.
fn perimeter_cards(pts: &[(Pos2, f32)], field: Rect, size: Vec2) -> Vec<Rect> {
    let inset = Rect::from_min_max(
        Pos2::new(field.left() + 4.0, field.top() + 4.0),
        Pos2::new(field.right() - 4.0, field.bottom() - 22.0),
    );
    let c = inset.center();
    let half = size * 0.5;
    // The rectangle the card *centres* may occupy, so no card leaves the field.
    let (lx, rx) = (inset.left() + half.x, inset.right() - half.x);
    let (ty, by) = (inset.top() + half.y, inset.bottom() - half.y);

    let mut centres: Vec<Pos2> = pts
        .iter()
        .map(|(at, _)| {
            let mut d = *at - c;
            if d.length() < 1e-3 {
                d = Vec2::new(0.0, -1.0);
            }
            // Scale the ray until it meets whichever edge it reaches first.
            let sx = if d.x.abs() > 1e-6 {
                ((if d.x > 0.0 { rx } else { lx }) - c.x) / d.x
            } else {
                f32::MAX
            };
            let sy = if d.y.abs() > 1e-6 {
                ((if d.y > 0.0 { by } else { ty }) - c.y) / d.y
            } else {
                f32::MAX
            };
            let s = sx.min(sy).max(0.0);
            Pos2::new((c.x + d.x * s).clamp(lx, rx), (c.y + d.y * s).clamp(ty, by))
        })
        .collect();

    // Relax overlaps: push pairs apart along the line joining them.
    for _ in 0..24 {
        let mut moved = false;
        for i in 0..centres.len() {
            for j in (i + 1)..centres.len() {
                let d = centres[j] - centres[i];
                let overlap_x = size.x + 4.0 - d.x.abs();
                let overlap_y = size.y + 3.0 - d.y.abs();
                if overlap_x > 0.0 && overlap_y > 0.0 {
                    // Separate along whichever axis needs the least motion.
                    if overlap_x / size.x < overlap_y / size.y {
                        let push = overlap_x * 0.5 * if d.x >= 0.0 { 1.0 } else { -1.0 };
                        centres[i].x -= push;
                        centres[j].x += push;
                    } else {
                        let push = overlap_y * 0.5 * if d.y >= 0.0 { 1.0 } else { -1.0 };
                        centres[i].y -= push;
                        centres[j].y += push;
                    }
                    moved = true;
                }
            }
        }
        for p in centres.iter_mut() {
            p.x = p.x.clamp(lx, rx);
            p.y = p.y.clamp(ty, by);
        }
        if !moved {
            break;
        }
    }

    centres
        .into_iter()
        .map(|p| Rect::from_center_size(p, size))
        .collect()
}

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::BG);
    outline(ui.p, r, theme::RULE);

    let n_frames = st.project.frames.len();
    let n_axes = st.project.topology.axis_count();
    let (lo_hz, hi_hz) = (st.grammar.display_lo_hz, st.grammar.display_hi_hz);
    let _ = hi_hz;

    // ── the ground: the live response, full bleed ────────────────────────
    let plot = Rect::from_min_max(
        Pos2::new(r.left() + 1.0, r.top() + 20.0),
        Pos2::new(r.right() - 1.0, r.bottom() - 1.0),
    );
    let (lo_db, hi_db) = st.live.display_span();
    frequency_rules(ui.p, plot, lo_hz, hi_hz, true);
    db_rules(ui.p, plot, lo_db, hi_db, true);
    curve_fill(
        ui.p,
        plot,
        &st.live.grid,
        &st.live.total,
        lo_hz,
        hi_hz,
        lo_db,
        hi_db,
        theme::ACCENT,
    );
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
        r.min + Vec2::new(8.0, 5.0),
        format!(
            "NAVIGATOR  —  {}",
            match n_axes {
                0 => "single frame".to_string(),
                1 => "line, 2 frames".to_string(),
                2 => "square, 4 frames".to_string(),
                3 => "cube, 8 frames".to_string(),
                n => format!("{n}-cube, {n_frames} frames"),
            }
        ),
        theme::T_MICRO,
        theme::DIM,
    );
    text_right(
        ui.p,
        Pos2::new(r.right() - 8.0, r.top() + 5.0),
        "drag the puck to morph   ·   click a node to travel there   ·   click a card to open its sections",
        theme::T_MICRO,
        theme::FAINT,
    );

    if n_frames == 0 {
        return;
    }

    // ── the overlay: the object ──────────────────────────────────────────
    let id = Id::of("navigator");
    let field = plot;
    let scale = field.height().min(field.width()) * 0.26;
    let (yaw, pitch) = (st.cube_yaw, st.cube_pitch);

    let pts: Vec<(Pos2, f32)> = (0..n_frames)
        .map(|i| {
            let coords: Vec<f32> = st.project.frames[i]
                .address
                .iter()
                .map(|&c| c as f32)
                .collect();
            let p = position(&coords);
            (project(p, yaw, pitch, field, scale), depth(p, yaw, pitch))
        })
        .collect();

    let weights = corner_weights(&st.coords);
    let puck = project(position(&st.coords), yaw, pitch, field, scale);

    // Edges, drawn behind everything, brightened by how much the two corners
    // they join are actually contributing.
    for a in 0..n_frames {
        for b in (a + 1)..n_frames {
            if (a ^ b).count_ones() != 1 {
                continue;
            }
            let axis = (a ^ b).trailing_zeros() as usize;
            let live = (weights[a] + weights[b]).clamp(0.0, 1.0);
            let displayed = axis == st.display_axes.0 || axis == st.display_axes.1;
            let base = if displayed {
                theme::mix(theme::RULE, theme::ACCENT, 0.35)
            } else {
                theme::RULE
            };
            line(
                ui.p,
                pts[a].0,
                pts[b].0,
                theme::mix(base, theme::INK, live * 0.55),
                if displayed { 1.4 } else { 1.0 },
            );
        }
    }

    // Corners, far first. Each carries its own response, so a corner is
    // recognised by what it sounds like rather than by its address.
    let mut order: Vec<usize> = (0..n_frames).collect();
    order.sort_by(|&a, &b| {
        pts[b]
            .1
            .partial_cmp(&pts[a].1)
            .unwrap_or(std::cmp::Ordering::Equal)
    });

    // Cards live on the perimeter, not over the curve. Each is pushed out along
    // the ray from the centre through its node until it meets the edge, then
    // nudged apart from its neighbours, so the response underneath stays
    // readable and a card always sits on the side its corner is on.
    let cards = perimeter_cards(&pts, field, Vec2::new(96.0, 42.0));

    let sr = st.project.sample_rate();
    let mut clicked = None;
    let mut open_sections: Option<usize> = None;
    for &i in &order {
        let (at, _) = pts[i];
        let c = theme::frame_color(i, n_frames);
        let selected = i == st.selected_frame;
        let contribution = weights.get(i).copied().unwrap_or(0.0);

        let card = cards[i];

        // Two separate targets, because they do different things: the node is
        // where the corner sits in the space, and clicking it travels there.
        // The card is the corner's own response, and clicking it opens the
        // sections that produce it.
        let node = ui.region(
            id.child(("node", i)),
            Rect::from_center_size(at, Vec2::splat(22.0)),
        );
        if node.clicked {
            clicked = Some(i);
        }
        let resp = ui.region(id.child(("card", i)), card);
        if resp.clicked {
            open_sections = Some(i);
        }

        // A corner that is carrying the sound is drawn as carrying it.
        let lit = theme::mix(theme::PANEL, c, 0.10 + contribution * 0.30);
        fill(ui.p, card, lit);
        outline(
            ui.p,
            card,
            if selected || resp.hovered || node.hovered {
                c
            } else {
                theme::mix(theme::RULE, c, 0.35 + contribution * 0.5)
            },
        );

        let thumb = Rect::from_min_max(
            Pos2::new(card.left() + 3.0, card.top() + 13.0),
            Pos2::new(card.right() - 3.0, card.bottom() - 2.0),
        );
        // 1:1 with the corner's own transfer function, out to Nyquist rather
        // than cropped at the top of the display span.
        let resp_curve = analysis::analyse(&st.project.frames[i].values, sr);
        let (clo, chi) = resp_curve.display_span();
        curve(
            ui.p,
            thumb,
            &resp_curve.grid,
            &resp_curve.total,
            lo_hz,
            sr * 0.5,
            clo,
            chi,
            c,
            1.2,
        );
        text(
            ui.p,
            card.min + Vec2::new(4.0, 1.0),
            st.grammar.frame_name(i, &crate::views::frame_label(st, i)),
            theme::T_MICRO,
            if contribution > 0.02 {
                theme::INK_HI
            } else {
                theme::DIM
            },
        );
        if contribution > 0.02 {
            text_right(
                ui.p,
                Pos2::new(card.right() - 4.0, card.top() + 1.0),
                format!("{:.0}%", contribution * 100.0),
                theme::T_MICRO,
                c,
            );
        }

        // The node itself, and its tie to the card.
        hairline(ui.p, at, card.center(), theme::mix(theme::BG, c, 0.35));
        ui.p.circle_filled(at, 5.0 + contribution * 4.0, c);
        if selected {
            ui.p.circle_stroke(at, 9.0, Stroke::new(1.0, c));
        }
    }

    // ── the puck: drag it, and the object follows ────────────────────────
    let puck_hit = Rect::from_center_size(puck, Vec2::splat(26.0));
    let presp = ui.region(id.child("puck"), puck_hit);
    if presp.held {
        if let Some(q) = presp.pointer {
            // Turn the pointer offset into travel on the two displayed axes by
            // projecting it onto each axis's screen direction. Axes that are
            // not displayed keep their coordinate.
            let (ax, ay) = st.display_axes;
            let dv = q - puck;
            for axis in [ax, ay] {
                if axis >= st.coords.len() {
                    continue;
                }
                let v = axis_screen_vector(axis, yaw, pitch, field, scale);
                let len2 = v.length_sq();
                if len2 > 1e-6 {
                    let t = (dv.x * v.x + dv.y * v.y) / len2;
                    st.coords[axis] = (st.coords[axis] + t).clamp(0.0, 1.0);
                }
            }
            st.touch();
        }
    }

    // Orbiting happens on empty space, and must not steal the puck's drag.
    let bg = ui.region(id.child("field"), field);
    if bg.held && !presp.held {
        st.cube_yaw += bg.drag_delta.x * 0.008;
        st.cube_pitch = (st.cube_pitch + bg.drag_delta.y * 0.008).clamp(-1.2, 1.2);
    }

    let pc = if presp.held || presp.hovered {
        theme::INK_HI
    } else {
        theme::ACCENT
    };
    hairline(
        ui.p,
        Pos2::new(puck.x - 11.0, puck.y),
        Pos2::new(puck.x + 11.0, puck.y),
        pc,
    );
    hairline(
        ui.p,
        Pos2::new(puck.x, puck.y - 11.0),
        Pos2::new(puck.x, puck.y + 11.0),
        pc,
    );
    ui.p.circle_filled(puck, 4.0, theme::BG);
    ui.p.circle_stroke(puck, 6.5, Stroke::new(2.0, pc));

    if let Some(i) = open_sections {
        st.selected_frame = i;
        st.sos_corner = Some(i);
    }
    if let Some(i) = clicked {
        st.selected_frame = i;
        let addr = st.project.frames[i].address.clone();
        st.coords = addr.iter().map(|&c| c as f32).collect();
        st.touch();
    }

    blend_readout(st, ui, &weights, field);
}

/// What the puck is actually made of, in the operator's words.
fn blend_readout(st: &Station, ui: &mut Ui, weights: &[f32], field: Rect) {
    let mut ranked: Vec<(usize, f32)> = weights
        .iter()
        .copied()
        .enumerate()
        .filter(|(_, w)| *w > 0.005)
        .collect();
    ranked.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap_or(std::cmp::Ordering::Equal));
    ranked.truncate(4);

    // Its own strip along the foot of the panel, clear of the frequency
    // labels sitting on the plot.
    let bar = Rect::from_min_max(
        Pos2::new(field.left() + 6.0, field.bottom() - 15.0),
        Pos2::new(field.right() - 6.0, field.bottom() - 1.0),
    );
    fill(ui.p, bar.expand(1.0), theme::BG);
    let mut x = bar.left();
    text(
        ui.p,
        Pos2::new(x, bar.top() + 2.0),
        "BLEND",
        theme::T_MICRO,
        theme::DIM,
    );
    x += 44.0;
    for (i, w) in ranked {
        let c = theme::frame_color(i, st.project.frames.len().max(1));
        let name = st.grammar.frame_name(i, &crate::views::frame_label(st, i));
        let s = format!("{name} {:.0}%", w * 100.0);
        text(ui.p, Pos2::new(x, bar.top() + 2.0), &s, theme::T_MICRO, c);
        x += paint::text_width(ui.p, &s, theme::T_MICRO) + 14.0;
    }

    let coords = st
        .coords
        .iter()
        .enumerate()
        .map(|(a, c)| {
            format!(
                "{} {:.2}",
                st.grammar
                    .axis_name(a, &st.project.topology.axes[a].name.clone()),
                c
            )
        })
        .collect::<Vec<_>>()
        .join("   ");
    text_right(
        ui.p,
        Pos2::new(bar.right(), bar.top() + 2.0),
        coords,
        theme::T_MICRO,
        theme::DIM,
    );
}
