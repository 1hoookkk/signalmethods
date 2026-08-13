//! The sections screen: a compact list, one editor for the selected section,
//! and the response beside it.

use eframe::egui::{Pos2, Rect, Vec2};

use trench_core::stage_law::authoring_limits_at;

use crate::app::{NoteKind, Station};
use crate::model::analysis::pole_shape;
use crate::model::lane::refusal_text;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{fill, label, num, num_right, panel, reading};
use crate::ui::theme;
use crate::ui::widgets::{self, FieldStyle, Scale};

use super::{response, zplane, PAD};

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let list_w = 216.0;
    let side_w = 330.0f32.min(r.width() * 0.30);

    let list_r = Rect::from_min_size(r.min, Vec2::new(list_w, r.height()));
    let edit_r = Rect::from_min_max(
        Pos2::new(list_r.right() + PAD, r.top()),
        Pos2::new(r.right() - side_w - PAD, r.bottom()),
    );
    let side_r = Rect::from_min_max(Pos2::new(r.right() - side_w, r.top()), r.max);

    lane_list(st, ui, list_r);
    editor(st, ui, edit_r);
    response::draw(st, ui, side_r, Some(st.selected_lane));
}

/// Only the sections that carry something, plus one row for the spare
/// capacity, collapsed.
fn lane_list(st: &mut Station, ui: &mut Ui, r: Rect) {
    let corner = st.selected_corner;
    let name = super::corner_name(st, corner);
    let inner = panel(ui.p, r, &format!("Sections · C{} {name}", corner + 1));

    let active = st.project.active_lanes();
    let capacity = st.project.lane_capacity();
    let sr = st.project.sample_rate();
    let id = Id::of("lanes");
    let row_h = 30.0;
    let mut y = inner.top();

    for &li in &active {
        let row = Rect::from_min_size(
            Pos2::new(inner.left(), y),
            Vec2::new(inner.width(), row_h - 3.0),
        );
        let selected = li == st.selected_lane;
        let resp = ui.region(id.child(li), row);
        if selected {
            fill(ui.p, row, theme::SELECT);
        } else if resp.hovered {
            fill(ui.p, row, theme::SURFACE_HI);
        }
        fill(
            ui.p,
            Rect::from_min_size(row.min, Vec2::new(3.0, row.height())),
            theme::corner_color(li),
        );
        label(
            ui.p,
            row.min + Vec2::new(12.0, 3.0),
            super::section_name(st, li),
            theme::T_BODY,
            if selected {
                theme::TEXT
            } else {
                theme::TEXT_DIM
            },
        );
        if let Some(v) = st.project.frames().get(corner).map(|f| f.values[li]) {
            if let trench_core::stage_law::RootPair::Conjugate { hz, .. } = v.geometry(sr).pole {
                num_right(
                    ui.p,
                    Pos2::new(row.right() - 8.0, row.top() + 5.0),
                    format!("{hz:>7.0} Hz"),
                    theme::T_SMALL,
                    theme::TEXT_DIM,
                );
            }
        }
        if resp.clicked {
            st.selected_lane = li;
        }
        y += row_h;
    }

    // Spare capacity, collapsed to one line that adds a section when used.
    let spare = capacity.saturating_sub(active.len());
    if spare > 0 {
        let row = Rect::from_min_size(
            Pos2::new(inner.left(), y + 4.0),
            Vec2::new(inner.width(), 26.0),
        );
        let next = (0..capacity).find(|i| !active.contains(i));
        if widgets::button(
            ui,
            id.child("add"),
            row,
            &format!("+ section    {spare} free"),
            next.is_some(),
        ) {
            if let Some(li) = next {
                st.selected_lane = li;
            }
        }
    }

    if active.is_empty() {
        label(
            ui.p,
            Pos2::new(inner.left(), y + 40.0),
            "none",
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }
}

/// One section: its pole and zero on the surface, its numbers beside them.
fn editor(st: &mut Station, ui: &mut Ui, r: Rect) {
    let li = st.selected_lane;
    let ci = st.selected_corner;
    let title = format!("{} · C{}", super::section_name(st, li), ci + 1);
    let inner = panel(ui.p, r, &title);

    let surface_w = (inner.width() * 0.52).clamp(200.0, 420.0);
    let surface = Rect::from_min_size(inner.min, Vec2::new(surface_w, inner.height() - 42.0));
    zplane::draw(st, ui, surface, ci, li);

    let fx = surface.right() + PAD * 2.0;
    let sr = st.project.sample_rate();
    let lim = authoring_limits_at(sr);
    let Some(roots) = zplane::roots_of(st, ci, li) else {
        return;
    };

    // Numbers, for placement that has to be exact.
    let id = Id::of(("sec", ci, li));
    let fw = ((inner.right() - fx - PAD) / 2.0).clamp(90.0, 150.0);
    let specs: [(&'static str, f64); 5] = [
        ("Pole Hz", roots.pole_hz),
        ("Pole r", roots.pole_r),
        ("Zero Hz", roots.zero_hz),
        ("Zero r", roots.zero_r),
        ("Gain", roots.scale),
    ];
    let mut edited: Option<(usize, f64)> = None;
    for (k, (name, value)) in specs.iter().enumerate() {
        let b = Rect::from_min_size(
            Pos2::new(
                fx + (k % 2) as f32 * (fw + 8.0),
                inner.top() + (k / 2) as f32 * 44.0,
            ),
            Vec2::new(fw, 38.0),
        );
        let style = style_for(name, &lim);
        let fid = id.child(k);
        let mut fs = st.fields.remove(&fid.0).unwrap_or_default();
        let out = widgets::number_field(ui, fid, b, *value, &style, &mut fs, true);
        st.fields.insert(fid.0, fs);
        if let Some(v) = out {
            edited = Some((k, v));
        }
    }

    // Derived readings from the pole's position.
    let ry = inner.top() + 3.0 * 44.0 + 6.0;
    if let Some(shape) = pole_shape(roots.pole_hz, roots.pole_r, sr) {
        reading(
            ui.p,
            Pos2::new(fx, ry),
            "bandwidth",
            format!("{:.1} Hz", shape.bw_hz),
            theme::TEXT,
        );
        reading(
            ui.p,
            Pos2::new(fx + fw + 8.0, ry),
            "Q",
            format!("{:.2}", shape.q),
            theme::TEXT,
        );
    }

    // Ordering, available but not dominant.
    let n = st.project.lane_capacity();
    let by = inner.bottom() - 26.0;
    let mut swap = None;
    if widgets::button(
        ui,
        id.child("up"),
        Rect::from_min_size(Pos2::new(inner.left(), by), Vec2::new(56.0, 24.0)),
        "Move up",
        li > 0,
    ) {
        swap = Some((li, li - 1));
    }
    if widgets::button(
        ui,
        id.child("down"),
        Rect::from_min_size(Pos2::new(inner.left() + 62.0, by), Vec2::new(64.0, 24.0)),
        "Move down",
        li + 1 < n,
    ) {
        swap = Some((li, li + 1));
    }
    let diag = Rect::from_min_size(Pos2::new(inner.left() + 136.0, by), Vec2::new(96.0, 24.0));
    if widgets::button(
        ui,
        id.child("diag"),
        diag,
        if st.show_diagnostics {
            "Hide raw"
        } else {
            "Show raw"
        },
        true,
    ) {
        st.show_diagnostics = !st.show_diagnostics;
    }

    if st.show_diagnostics {
        if let Some(v) = st.project.frames().get(ci).map(|f| f.values[li]) {
            let words = v
                .words
                .iter()
                .map(|w| format!("{w:04X}"))
                .collect::<Vec<_>>()
                .join(" ");
            let b = crate::model::analysis::biquad_of(&v);
            let coeffs = b
                .iter()
                .map(|x| format!("{x:+.5}"))
                .collect::<Vec<_>>()
                .join("  ");
            num(
                ui.p,
                Pos2::new(inner.left(), by - 34.0),
                format!("words {words}"),
                theme::T_SMALL,
                theme::TEXT_FAINT,
            );
            num(
                ui.p,
                Pos2::new(inner.left(), by - 19.0),
                coeffs,
                theme::T_SMALL,
                theme::TEXT_FAINT,
            );
        }
    }

    if let Some((a, b)) = swap {
        st.checkpoint();
        if st.project.swap_lanes_at_frame(ci, a, b) {
            st.selected_lane = b;
            st.touch();
        }
    }

    if let Some((which, v)) = edited {
        let mut next = roots;
        match which {
            0 => next.pole_hz = v,
            1 => next.pole_r = v,
            2 => next.zero_hz = v,
            3 => next.zero_r = v,
            _ => next.scale = v,
        }
        let sr = st.project.sample_rate();
        if let Some(o) = st.project.object.as_ref() {
            let mut lv = o.frames[ci].values[li];
            match lv.set_roots(&next, sr) {
                Ok(()) => {
                    st.checkpoint();
                    if let Some(o) = st.project.object.as_mut() {
                        o.frames[ci].values[li] = lv;
                    }
                    st.touch();
                }
                Err(e) => st.say(refusal_text(e), NoteKind::Warn),
            }
        }
    }
}

fn style_for(name: &'static str, lim: &trench_core::stage_law::AuthoringLimits) -> FieldStyle {
    match name {
        "Pole Hz" | "Zero Hz" => FieldStyle {
            label: name,
            unit: "",
            decimals: 1,
            step: 0.004,
            lo: lim.display_freq_min_hz,
            hi: lim.authoring_freq_max_hz,
            scale: Scale::Log,
        },
        "Pole r" => FieldStyle {
            label: name,
            unit: "",
            decimals: 4,
            step: 0.0008,
            lo: 0.0,
            hi: lim.pole_radius_max,
            scale: Scale::Linear,
        },
        "Zero r" => FieldStyle {
            label: name,
            unit: "",
            decimals: 4,
            step: 0.0008,
            lo: 0.0,
            hi: lim.zero_radius_max,
            scale: Scale::Linear,
        },
        _ => FieldStyle {
            label: name,
            unit: "",
            decimals: 4,
            step: 0.002,
            lo: lim.scale_min,
            hi: lim.scale_max,
            scale: Scale::Linear,
        },
    }
}
