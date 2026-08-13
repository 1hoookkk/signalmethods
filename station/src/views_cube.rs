//! The topology view: the declared object drawn as an n-cube.
//!
//! Each corner is an authored frame, each edge is a move along exactly one
//! axis, and the puck is the working position inside the object. Three axes
//! draw a cube, two a square, four a tesseract — the projection is the same
//! code because an n-cube is just one direction vector per declared axis.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::Station;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{self, fill, hairline, line, outline, text, text_right};
use crate::ui::theme;

/// A point in the Station's viewing space.
#[derive(Clone, Copy)]
struct P3 {
    x: f32,
    y: f32,
    z: f32,
}

/// Direction each declared axis pushes a corner. The first three are the cube's
/// own edges; further axes fold outward along a diagonal at a shrinking
/// magnitude, which is how an n-cube is drawn without inventing a new rule per
/// dimension.
fn axis_direction(i: usize) -> P3 {
    match i {
        0 => P3 { x: 1.0, y: 0.0, z: 0.0 },
        1 => P3 { x: 0.0, y: 0.0, z: 1.0 },
        2 => P3 { x: 0.0, y: 1.0, z: 0.0 },
        n => {
            let s = 0.62_f32.powi(n as i32 - 2);
            P3 { x: 0.72 * s, y: 0.52 * s, z: 0.72 * s }
        }
    }
}

/// Position of a coordinate vector in viewing space, centred on the origin.
fn position(coords: &[f32]) -> P3 {
    let mut p = P3 { x: 0.0, y: 0.0, z: 0.0 };
    for (i, &c) in coords.iter().enumerate() {
        let d = axis_direction(i);
        let t = c * 2.0 - 1.0;
        p.x += d.x * t;
        p.y += d.y * t;
        p.z += d.z * t;
    }
    p
}

/// Yaw then pitch, then a mild perspective divide.
fn project(p: P3, yaw: f32, pitch: f32, r: Rect, scale: f32) -> Pos2 {
    let (sy, cy) = yaw.sin_cos();
    let (sp, cp) = pitch.sin_cos();
    let x1 = p.x * cy + p.z * sy;
    let z1 = -p.x * sy + p.z * cy;
    let y1 = p.y * cp - z1 * sp;
    let z2 = p.y * sp + z1 * cp;
    let d = 5.0;
    let k = d / (d + z2);
    Pos2::new(
        r.center().x + x1 * scale * k,
        r.center().y - y1 * scale * k,
    )
}

fn depth(p: P3, yaw: f32, pitch: f32) -> f32 {
    let (sy, cy) = yaw.sin_cos();
    let (sp, cp) = pitch.sin_cos();
    let z1 = -p.x * sy + p.z * cy;
    p.y * sp + z1 * cp
}

/// Draws the object and handles corner picking and rotation.
pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);

    let n_axes = st.project.topology.axis_count();
    let n_frames = st.project.frames.len();
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        format!(
            "TOPOLOGY  —  {}",
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
        Pos2::new(r.right() - 8.0, r.top() + 6.0),
        "drag to rotate   ·   click a corner to select",
        theme::T_MICRO,
        theme::FAINT,
    );

    let id = Id::of("cube");
    // Rotation: dragging the field turns the object.
    let field = Rect::from_min_max(
        Pos2::new(r.left() + 4.0, r.top() + 22.0),
        Pos2::new(r.right() - 4.0, r.bottom() - 4.0),
    );
    let bg = ui.region(id.child("field"), field);
    if bg.held {
        st.cube_yaw += bg.drag_delta.x * 0.008;
        st.cube_pitch = (st.cube_pitch + bg.drag_delta.y * 0.008).clamp(-1.2, 1.2);
    }

    let scale = field.height().min(field.width()) * 0.30;
    let (yaw, pitch) = (st.cube_yaw, st.cube_pitch);

    // Corner screen positions, once.
    let mut pts: Vec<(Pos2, f32)> = Vec::with_capacity(n_frames);
    for i in 0..n_frames {
        let coords: Vec<f32> = st.project.frames[i]
            .address
            .iter()
            .map(|&c| c as f32)
            .collect();
        let p = position(&coords);
        pts.push((project(p, yaw, pitch, field, scale), depth(p, yaw, pitch)));
    }

    // Edges join corners that differ in exactly one axis, which is precisely
    // where a single-axis morph travels.
    for a in 0..n_frames {
        for b in (a + 1)..n_frames {
            let (ia, ib) = (a, b);
            let diff = ia ^ ib;
            if diff.count_ones() != 1 {
                continue;
            }
            let axis = diff.trailing_zeros() as usize;
            let far = (pts[a].1 + pts[b].1) * 0.5;
            let fade = ((far + 1.6) / 3.2).clamp(0.0, 1.0);
            let base = if axis == st.display_axes.0 {
                theme::mix(theme::RULE, theme::ACCENT, 0.45)
            } else {
                theme::RULE_HI
            };
            line(
                ui.p,
                pts[a].0,
                pts[b].0,
                theme::mix(base, theme::PANEL, fade * 0.65),
                if axis == st.display_axes.0 { 1.6 } else { 1.0 },
            );
        }
    }

    // The working position, and its thread back to the selected corner.
    let live = project(position(&st.coords), yaw, pitch, field, scale);

    // Corners, far ones first so near ones sit on top.
    let mut order: Vec<usize> = (0..n_frames).collect();
    order.sort_by(|&a, &b| pts[b].1.partial_cmp(&pts[a].1).unwrap_or(std::cmp::Ordering::Equal));

    let mut clicked: Option<usize> = None;
    for &i in &order {
        let (at, dz) = pts[i];
        let c = theme::frame_color(i, n_frames);
        let near = ((1.6 - dz) / 3.2).clamp(0.0, 1.0);
        let radius = 5.0 + near * 3.5;
        let hit = Rect::from_center_size(at, Vec2::splat(radius * 2.0 + 10.0));
        let resp = ui.region(id.child(i), hit);
        if resp.clicked {
            clicked = Some(i);
        }
        let selected = i == st.selected_frame;

        if selected {
            ui.p.circle_filled(at, radius + 5.0, theme::mix(theme::PANEL, c, 0.30));
        }
        ui.p.circle_filled(at, radius, theme::mix(theme::PANEL, c, 0.55 + near * 0.45));
        ui.p.circle_stroke(
            at,
            radius,
            eframe::egui::Stroke::new(if selected { 2.0 } else { 1.0 }, c),
        );
        if resp.hovered || selected {
            paint::chip(
                ui.p,
                at + Vec2::new(radius + 5.0, -7.0),
                &format!("C{:02}", i + 1),
                c,
            );
            text(
                ui.p,
                at + Vec2::new(radius + 5.0, 7.0),
                crate::views::frame_label(st, i),
                theme::T_MICRO,
                theme::INK,
            );
        }
    }

    // The puck last: it is where the ear is.
    hairline(
        ui.p,
        Pos2::new(live.x - 9.0, live.y),
        Pos2::new(live.x + 9.0, live.y),
        theme::ACCENT,
    );
    hairline(
        ui.p,
        Pos2::new(live.x, live.y - 9.0),
        Pos2::new(live.x, live.y + 9.0),
        theme::ACCENT,
    );
    ui.p.circle_stroke(
        live,
        5.0,
        eframe::egui::Stroke::new(1.5, theme::ACCENT),
    );

    if let Some(i) = clicked {
        st.selected_frame = i;
        let addr = st.project.frames[i].address.clone();
        st.coords = addr.iter().map(|&c| c as f32).collect();
        st.touch();
    }

    // Axis legend, so an edge's meaning is readable rather than remembered.
    let mut y = field.bottom() - 14.0 - (n_axes as f32) * 13.0;
    for ai in 0..n_axes {
        let name = st
            .grammar
            .axis_name(ai, &st.project.topology.axes[ai].name.clone());
        let shown = ai == st.display_axes.0;
        text(
            ui.p,
            Pos2::new(field.left() + 8.0, y),
            format!("{}  {}", if shown { "─" } else { "·" }, name),
            theme::T_MICRO,
            if shown { theme::ACCENT } else { theme::DIM },
        );
        y += 13.0;
    }
}
