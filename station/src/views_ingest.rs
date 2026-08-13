//! Workspace 1 — ingestion and measurement.
//!
//! What arrives here is an authored object and the facts that can be read off
//! it: how it is addressed, what each lane's roots actually are, and which rate
//! its stored words belong to. Root classification is read from the geometry,
//! not guessed: a lane is a conjugate pair, a real pair, or degenerate, and no
//! filter role is inferred from any of those.

use eframe::egui::{Pos2, Rect, Vec2};

use trench_core::stage_law::RootPair;

use crate::app::{PathIntent, Station, PAD};
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{self, divider, fill, outline, text, text_right};
use crate::ui::theme;
use crate::ui::widgets;
use crate::views::frame_cards;

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let left_w = r.width() * 0.40;
    let left = Rect::from_min_size(r.min, Vec2::new(left_w, r.height()));
    let right = Rect::from_min_max(Pos2::new(left.right() + PAD, r.top()), r.max);

    source_panel(
        st,
        ui,
        Rect::from_min_size(left.min, Vec2::new(left.width(), 232.0)),
    );
    classify_panel(
        st,
        ui,
        Rect::from_min_max(Pos2::new(left.left(), left.top() + 232.0 + PAD), left.max),
    );

    // The measured object itself, frame by frame.
    text(
        ui.p,
        right.min + Vec2::new(2.0, 0.0),
        "AUTHORED FRAMES  —  measured response of every corner as stored",
        theme::T_MICRO,
        theme::DIM,
    );
    frame_cards(
        st,
        ui,
        Rect::from_min_max(Pos2::new(right.left(), right.top() + 14.0), right.max),
    );
}

fn source_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        "SOURCE",
        theme::T_MICRO,
        theme::DIM,
    );
    divider(
        ui.p,
        Pos2::new(r.left() + 8.0, r.top() + 22.0),
        Pos2::new(r.right() - 8.0, r.top() + 22.0),
        theme::RULE,
    );

    let id = Id::of("ingest");
    let b = Rect::from_min_size(r.min + Vec2::new(8.0, 30.0), Vec2::new(120.0, 24.0));
    if widgets::button(ui, id.child("open"), b, "OPEN / IMPORT", true) {
        st.open_path_bar(PathIntent::Open);
    }
    text(
        ui.p,
        Pos2::new(r.left() + 138.0, r.top() + 36.0),
        "or drop a file on the window",
        theme::T_MICRO,
        theme::FAINT,
    );

    let facts = [
        ("name", st.project.name.clone()),
        (
            "origin",
            match st.project.origin {
                crate::model::project::Origin::Native => "authored in Station".into(),
                crate::model::project::Origin::PackedNative => "packed body, 560 bytes".into(),
                crate::model::project::Origin::PackedLegacy => {
                    "packed body, 240 bytes legacy".into()
                }
            },
        ),
        (
            "addressing",
            format!(
                "{} axes, {} corners, axis 0 fastest",
                st.project.topology.axis_count(),
                st.project.frames.len()
            ),
        ),
        ("lanes", format!("{} SOS lanes", st.project.lane_count())),
        ("word rate", format!("{:.4} Hz", st.project.sample_rate())),
    ];
    let mut y = r.top() + 64.0;
    for (k, v) in facts {
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y),
            k,
            theme::T_MICRO,
            theme::DIM,
        );
        text_right(
            ui.p,
            Pos2::new(r.right() - 8.0, y),
            v,
            theme::T_MICRO,
            theme::INK,
        );
        y += 14.0;
    }

    // Rate translation. Stored words belong to the rate they were authored at;
    // moving them to another rate is a re-discretisation, not a relabel.
    divider(
        ui.p,
        Pos2::new(r.left() + 8.0, y + 4.0),
        Pos2::new(r.right() - 8.0, y + 4.0),
        theme::RULE,
    );
    text(
        ui.p,
        Pos2::new(r.left() + 8.0, y + 10.0),
        "RE-DISCRETISE  —  preserves continuous root frequencies",
        theme::T_MICRO,
        theme::DIM,
    );
    let rates = [39_062.5f64, 44_100.0, 48_000.0, 96_000.0];
    for (i, rate) in rates.iter().enumerate() {
        let b = Rect::from_min_size(
            Pos2::new(r.left() + 8.0 + i as f32 * 74.0, y + 26.0),
            Vec2::new(70.0, 22.0),
        );
        let current = (st.project.sample_rate() - rate).abs() < 1e-6;
        let label = if *rate >= 1000.0 {
            format!("{:.4}k", rate / 1000.0)
        } else {
            format!("{rate:.0}")
        };
        if widgets::button(
            ui,
            id.child(("rate", i)),
            b,
            label.trim_end_matches('0').trim_end_matches('.'),
            !current,
        ) {
            st.checkpoint();
            st.project.retune(*rate);
            st.touch();
            st.say(format!("re-discretised to {rate} Hz"), false);
        }
    }
}

/// Root classification, read from geometry. This is the only place the Station
/// says anything about what a lane *is*, and it says only what the roots are.
fn classify_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        "ROOT CLASSIFICATION  —  selected frame",
        theme::T_MICRO,
        theme::DIM,
    );
    text_right(
        ui.p,
        Pos2::new(r.right() - 8.0, r.top() + 6.0),
        format!("C{:02}", st.selected_frame + 1),
        theme::T_MICRO,
        theme::frame_color(st.selected_frame, st.project.frames.len().max(1)),
    );

    let sr = st.project.sample_rate();
    let fi = st
        .selected_frame
        .min(st.project.frames.len().saturating_sub(1));
    if st.project.frames.is_empty() {
        return;
    }
    let n = st.project.lane_count();
    let head = r.top() + 26.0;
    for (i, h) in ["LANE", "POLE", "ZERO", "SCALE"].iter().enumerate() {
        let x = r.left() + 8.0 + [0.0, 62.0, 190.0, 318.0][i];
        text(ui.p, Pos2::new(x, head), *h, theme::T_MICRO, theme::FAINT);
    }

    let describe = |p: RootPair| -> (String, eframe::egui::Color32) {
        match p {
            RootPair::Conjugate { hz, r } => {
                (format!("conjugate {hz:>7.0} Hz r{r:.4}"), theme::INK)
            }
            RootPair::RealPair { root_a, root_b } => {
                (format!("real pair  {root_a:+.4} {root_b:+.4}"), theme::WARN)
            }
            RootPair::Degenerate => ("degenerate".to_string(), theme::FAINT),
        }
    };

    for li in 0..n {
        let y = head + 16.0 + li as f32 * 17.0;
        if y > r.bottom() - 14.0 {
            break;
        }
        let v = st.project.frames[fi].values[li];
        let g = v.geometry(sr);
        let name = st
            .grammar
            .lane_name(li, &st.project.lanes[li].id.to_string());
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y),
            &name,
            theme::T_MICRO,
            if li == st.selected_lane {
                theme::ACCENT
            } else {
                theme::INK
            },
        );
        if v.is_identity() {
            text(
                ui.p,
                Pos2::new(r.left() + 70.0, y),
                "pass-through identity",
                theme::T_MICRO,
                theme::FAINT,
            );
            continue;
        }
        let (pt, pc) = describe(g.pole);
        let (zt, zc) = describe(g.zero);
        text(ui.p, Pos2::new(r.left() + 70.0, y), pt, theme::T_MICRO, pc);
        text(ui.p, Pos2::new(r.left() + 198.0, y), zt, theme::T_MICRO, zc);
        text(
            ui.p,
            Pos2::new(r.left() + 326.0, y),
            format!("{:.4}", g.scale),
            theme::T_MICRO,
            theme::DIM,
        );
    }

    let _ = paint::char_width(ui.p, theme::T_MICRO);
}
