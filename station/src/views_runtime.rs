//! Workspace 4 — the runtime surface.
//!
//! The packed registers exactly as they are stored, and the linters that read
//! them. Every linter here reports. None of them edits: an operator is told
//! that a pole sits close to the circle, or that the cascade is very loud in
//! the middle, and decides what to do. Nothing is rescaled or re-anchored on
//! the Station's own initiative.

use eframe::egui::{Pos2, Rect, Vec2};

use trench_core::minifloat::decode;

use crate::app::Station;
use crate::model::project::PackedCapability;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{self, divider, fill, outline, text, text_right};
use crate::ui::theme;
use crate::ui::widgets;

/// The stored words for every frame of one lane, as hex and as decoded values.
pub fn registers(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    let lane_name = if st.project.lane_count() > 0 {
        st.grammar.lane_name(
            st.selected_lane,
            &st.project.lanes[st.selected_lane].id.to_string(),
        )
    } else {
        "—".into()
    };
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        format!("PACKED REGISTERS  —  lane {lane_name}, every frame"),
        theme::T_MICRO,
        theme::DIM,
    );
    text_right(
        ui.p,
        Pos2::new(r.right() - 8.0, r.top() + 6.0),
        "5 words per lane per frame, little-endian u16",
        theme::T_MICRO,
        theme::FAINT,
    );
    divider(
        ui.p,
        Pos2::new(r.left() + 8.0, r.top() + 22.0),
        Pos2::new(r.right() - 8.0, r.top() + 22.0),
        theme::RULE,
    );

    if st.project.frames.is_empty() || st.project.lane_count() == 0 {
        return;
    }
    let li = st.selected_lane.min(st.project.lane_count() - 1);

    // Lane picker, so every lane's registers are reachable.
    let id = Id::of("regs");
    let n_l = st.project.lane_count();
    for i in 0..n_l {
        let b = Rect::from_min_size(
            Pos2::new(r.left() + 8.0 + i as f32 * 46.0, r.top() + 28.0),
            Vec2::new(42.0, 20.0),
        );
        let name = st.grammar.lane_name(i, &st.project.lanes[i].id.to_string());
        if widgets::tab(ui, id.child(("l", i)), b, "", &name, i == li) {
            st.selected_lane = i;
        }
    }

    let head_y = r.top() + 56.0;
    for (i, h) in ["FRAME", "W0", "W1", "W2", "W3", "W4", "DECODED W0..W4"]
        .iter()
        .enumerate()
    {
        let x = r.left() + 8.0 + [0.0, 74.0, 124.0, 174.0, 224.0, 274.0, 330.0][i];
        text(ui.p, Pos2::new(x, head_y), *h, theme::T_MICRO, theme::FAINT);
    }

    let n_f = st.project.frames.len();
    for fi in 0..n_f {
        let y = head_y + 16.0 + fi as f32 * 16.0;
        if y > r.bottom() - 12.0 {
            break;
        }
        let v = st.project.frames[fi].values[li];
        let c = theme::frame_color(fi, n_f);
        let selected = fi == st.selected_frame;
        if selected {
            fill(
                ui.p,
                Rect::from_min_size(
                    Pos2::new(r.left() + 4.0, y - 2.0),
                    Vec2::new(r.width() - 8.0, 15.0),
                ),
                theme::SELECT,
            );
        }
        let hit = Rect::from_min_size(
            Pos2::new(r.left() + 4.0, y - 2.0),
            Vec2::new(r.width() - 8.0, 15.0),
        );
        if ui.region(id.child(("f", fi)), hit).clicked {
            st.selected_frame = fi;
        }
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y),
            format!("C{:02}", fi + 1),
            theme::T_MICRO,
            c,
        );
        for (wi, w) in v.words.iter().enumerate() {
            text(
                ui.p,
                Pos2::new(r.left() + 82.0 + wi as f32 * 50.0, y),
                format!("{w:04X}"),
                theme::T_MICRO,
                if v.is_identity() {
                    theme::FAINT
                } else {
                    theme::INK
                },
            );
        }
        text(
            ui.p,
            Pos2::new(r.left() + 338.0, y),
            v.words
                .iter()
                .map(|w| format!("{:.4}", decode(*w)))
                .collect::<Vec<_>>()
                .join(" "),
            theme::T_MICRO,
            theme::DIM,
        );
    }
    let _ = paint::char_width(ui.p, theme::T_MICRO);
}

/// The safety readings, and what export can produce.
pub fn linters(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        "LINTERS",
        theme::T_MICRO,
        theme::DIM,
    );
    divider(
        ui.p,
        Pos2::new(r.left() + 8.0, r.top() + 22.0),
        Pos2::new(r.right() - 8.0, r.top() + 22.0),
        theme::RULE,
    );

    let sr = st.project.sample_rate();
    let watch = st.grammar.pole_radius_watch;

    // Every lane at every frame, so a hot pole anywhere in the object is found
    // rather than only one at the working position.
    let mut hottest: Option<(usize, usize, f64)> = None;
    let mut real_pairs = 0usize;
    for (fi, f) in st.project.frames.iter().enumerate() {
        for (li, v) in f.values.iter().enumerate() {
            match v.geometry(sr).pole {
                trench_core::stage_law::RootPair::Conjugate { r: rr, .. } => {
                    if hottest.map(|h| rr > h.2).unwrap_or(true) {
                        hottest = Some((fi, li, rr));
                    }
                }
                trench_core::stage_law::RootPair::RealPair { .. } => real_pairs += 1,
                trench_core::stage_law::RootPair::Degenerate => {}
            }
        }
    }

    let worst = st.live.worst_intermediate();
    let dc_sum: f64 = st.live.lane_metrics.iter().map(|m| m.dc_db).sum();

    let mut rows: Vec<(String, String, eframe::egui::Color32)> = Vec::new();

    rows.push(match hottest {
        Some((fi, li, rr)) => (
            "pole radius, worst".into(),
            format!("{rr:.5}  at C{:02} lane {}", fi + 1, li + 1),
            if rr >= 1.0 {
                theme::BAD
            } else if rr >= watch {
                theme::WARN
            } else {
                theme::GOOD
            },
        ),
        None => (
            "pole radius, worst".into(),
            "no conjugate poles".into(),
            theme::DIM,
        ),
    });

    rows.push((
        "real-axis root pairs".into(),
        format!("{real_pairs}"),
        if real_pairs > 0 {
            theme::WARN
        } else {
            theme::DIM
        },
    ));

    rows.push(match worst {
        Some((li, db)) => (
            "cascade peak, worst point".into(),
            format!("{db:+.2} dB after lane {}", li + 1),
            if db > 36.0 {
                theme::BAD
            } else if db > 18.0 {
                theme::WARN
            } else {
                theme::GOOD
            },
        ),
        None => ("cascade peak, worst point".into(), "—".into(), theme::DIM),
    });

    rows.push((
        "cascade at DC".into(),
        format!("{dc_sum:+.2} dB"),
        theme::INK,
    ));

    rows.push((
        "total peak".into(),
        format!("{:+.2} dB", st.live.total_peak_db()),
        theme::INK,
    ));

    let cap = st.project.packed_capability();
    rows.push((
        "packed export".into(),
        st.project.packed_capability_text(),
        if matches!(cap, PackedCapability::Refused(_)) {
            theme::BAD
        } else {
            theme::GOOD
        },
    ));

    let mut y = r.top() + 30.0;
    for (k, v, c) in rows {
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y),
            &k,
            theme::T_MICRO,
            theme::DIM,
        );
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y + 12.0),
            &v,
            theme::T_SMALL,
            c,
        );
        y += 32.0;
    }

    text(
        ui.p,
        Pos2::new(r.left() + 8.0, r.bottom() - 40.0),
        "Gain readings are measurements.",
        theme::T_MICRO,
        theme::FAINT,
    );
    text(
        ui.p,
        Pos2::new(r.left() + 8.0, r.bottom() - 28.0),
        "The Station does not rescale or re-anchor an authored response.",
        theme::T_MICRO,
        theme::FAINT,
    );
    text(
        ui.p,
        Pos2::new(r.left() + 8.0, r.bottom() - 16.0),
        "No 4D packed contract exists; a 4D project saves as a Station project only.",
        theme::T_MICRO,
        theme::FAINT,
    );
}
