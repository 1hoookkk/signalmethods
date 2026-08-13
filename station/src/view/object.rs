//! The object screen: the authored object at the centre, its corner beside it,
//! and the response as a compact instrument at the side.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::Station;
use crate::model::analysis;
use crate::model::object::ObjectForm;
use crate::ui::input::Ui;
use crate::ui::paint::{label, num_right, panel, reading};
use crate::ui::theme;

use super::{breakdown, cube, response, square, PAD};

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let side_w = 330.0f32.min(r.width() * 0.30);
    let stage_r = Rect::from_min_max(r.min, Pos2::new(r.right() - side_w - PAD, r.bottom()));
    let side_r = Rect::from_min_max(Pos2::new(r.right() - side_w, r.top()), r.max);

    let title = match st.project.form() {
        Some(ObjectForm::Square) => "Object · square · 4 corners",
        Some(ObjectForm::Cube) => "Object · cube · 8 corners",
        None => "Object",
    };
    // The object above, its sections broken out beneath it.
    let strip_h = 118.0f32.min(stage_r.height() * 0.30);
    let stage = panel(
        ui.p,
        Rect::from_min_max(
            stage_r.min,
            Pos2::new(stage_r.right(), stage_r.bottom() - strip_h - PAD),
        ),
        title,
    );
    match st.project.form() {
        Some(ObjectForm::Square) => square::draw(st, ui, stage),
        Some(ObjectForm::Cube) => cube::draw(st, ui, stage),
        None => {}
    }
    let corner_resp = st
        .project
        .frames()
        .get(st.selected_corner)
        .map(|f| analysis::analyse(&f.values, st.project.sample_rate()));
    if let Some(cr) = corner_resp {
        breakdown::draw(
            st,
            ui,
            Rect::from_min_max(
                Pos2::new(stage_r.left(), stage_r.bottom() - strip_h),
                stage_r.max,
            ),
            &cr,
        );
    }

    // Side column: response above, selected corner below.
    let resp_h = (side_r.height() * 0.46).clamp(180.0, 300.0);
    response::draw(
        st,
        ui,
        Rect::from_min_size(side_r.min, Vec2::new(side_r.width(), resp_h)),
        None,
    );
    corner_panel(
        st,
        ui,
        Rect::from_min_max(
            Pos2::new(side_r.left(), side_r.top() + resp_h + PAD),
            side_r.max,
        ),
    );
}

/// What the selected corner holds.
fn corner_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    let ci = st.selected_corner;
    let name = super::corner_name(st, ci);
    let inner = panel(ui.p, r, &format!("Corner {} · {name}", ci + 1));

    let Some(frame) = st.project.frames().get(ci) else {
        return;
    };
    let sr = st.project.sample_rate();
    let resp = analysis::analyse(&frame.values, sr);
    let active: Vec<usize> = (0..frame.values.len())
        .filter(|&i| !frame.values[i].is_identity())
        .collect();

    reading(
        ui.p,
        inner.min,
        "sections",
        format!("{} of {}", active.len(), frame.values.len()),
        theme::TEXT,
    );
    reading(
        ui.p,
        inner.min + Vec2::new(110.0, 0.0),
        "order",
        format!("{}", st.project.order_at(ci)),
        theme::TEXT,
    );
    reading(
        ui.p,
        inner.min + Vec2::new(200.0, 0.0),
        "peak",
        format!("{:+.1} dB", resp.total_peak_db()),
        theme::TEXT,
    );

    // The corner's own response, small.
    let plot = Rect::from_min_max(
        Pos2::new(inner.left(), inner.top() + 46.0),
        Pos2::new(inner.right(), (inner.top() + 150.0).min(inner.bottom())),
    );
    if plot.height() > 40.0 {
        response::plot_into(st, ui, plot, &resp, None);
    }

    // The sections it carries, as a list of readings rather than a wall of
    // little graphs.
    let mut y = plot.bottom() + 10.0;
    let watch = st.grammar.pole_radius_watch;
    if active.is_empty() {
        label(
            ui.p,
            Pos2::new(inner.left(), y),
            "no sections",
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
        return;
    }
    for &li in &active {
        if y + 18.0 > inner.bottom() {
            break;
        }
        let g = frame.values[li].geometry(sr);
        label(
            ui.p,
            Pos2::new(inner.left(), y),
            super::section_name(st, li),
            theme::T_SMALL,
            if li == st.selected_lane {
                theme::ACCENT
            } else {
                theme::TEXT_DIM
            },
        );
        match g.pole {
            trench_core::stage_law::RootPair::Conjugate { hz, r: rr } => {
                num_right(
                    ui.p,
                    Pos2::new(inner.right(), y),
                    format!("{hz:>7.0} Hz   r {rr:.4}"),
                    theme::T_SMALL,
                    if rr >= watch {
                        theme::WARN
                    } else {
                        theme::TEXT
                    },
                );
            }
            _ => {
                num_right(
                    ui.p,
                    Pos2::new(inner.right(), y),
                    "real",
                    theme::T_SMALL,
                    theme::TEXT_DIM,
                );
            }
        }
        y += 18.0;
    }
}
