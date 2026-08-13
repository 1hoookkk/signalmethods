//! Export, and the raw words behind it.

use eframe::egui::{Pos2, Rect, Vec2};

use trench_core::minifloat::decode;

use crate::app::{PathIntent, Station};
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{fill, label, num, num_right, panel, reading};
use crate::ui::theme;
use crate::ui::widgets;

use super::PAD;

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let left_w = 340.0f32.min(r.width() * 0.34);
    summary(
        st,
        ui,
        Rect::from_min_size(r.min, Vec2::new(left_w, r.height())),
    );
    words(
        st,
        ui,
        Rect::from_min_max(Pos2::new(r.left() + left_w + PAD, r.top()), r.max),
    );
}

fn summary(st: &mut Station, ui: &mut Ui, r: Rect) {
    let inner = panel(ui.p, r, "Export");
    let Some(form) = st.project.form() else {
        return;
    };

    reading(ui.p, inner.min, "form", form.name(), theme::TEXT);
    reading(
        ui.p,
        inner.min + Vec2::new(110.0, 0.0),
        "body",
        format!("{} bytes", form.body_bytes()),
        theme::TEXT,
    );
    reading(
        ui.p,
        inner.min + Vec2::new(220.0, 0.0),
        "rate",
        format!("{:.0} Hz", st.project.sample_rate()),
        theme::TEXT,
    );

    // Whether it can be written, and if not exactly why.
    let check = st.project.to_body_bytes();
    let y = inner.top() + 52.0;
    match &check {
        Ok(bytes) => {
            ui.p.circle_filled(Pos2::new(inner.left() + 4.0, y + 7.0), 3.5, theme::GOOD);
            label(
                ui.p,
                Pos2::new(inner.left() + 14.0, y),
                format!("writes {} bytes", bytes.len()),
                theme::T_BODY,
                theme::TEXT,
            );
        }
        Err(e) => {
            ui.p.circle_filled(Pos2::new(inner.left() + 4.0, y + 7.0), 3.5, theme::BAD);
            label(
                ui.p,
                Pos2::new(inner.left() + 14.0, y),
                e,
                theme::T_SMALL,
                theme::BAD,
            );
        }
    }

    let id = Id::of("export");
    let b = Rect::from_min_size(Pos2::new(inner.left(), y + 30.0), Vec2::new(140.0, 28.0));
    if widgets::button(ui, id.child("go"), b, "Export body", check.is_ok()) {
        st.open_path_bar(PathIntent::ExportBody);
    }

    // Section and order census, per corner.
    let mut ly = y + 74.0;
    label(
        ui.p,
        Pos2::new(inner.left(), ly),
        "corners",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );
    ly += 18.0;
    for i in 0..st.project.frames().len() {
        if ly + 18.0 > inner.bottom() {
            break;
        }
        let c = theme::corner_color(i);
        ui.p.circle_filled(Pos2::new(inner.left() + 4.0, ly + 7.0), 3.0, c);
        label(
            ui.p,
            Pos2::new(inner.left() + 14.0, ly),
            super::corner_name(st, i),
            theme::T_SMALL,
            theme::TEXT_DIM,
        );
        num_right(
            ui.p,
            Pos2::new(inner.right(), ly),
            format!("order {}", st.project.order_at(i)),
            theme::T_SMALL,
            theme::TEXT_DIM,
        );
        ly += 18.0;
    }
}

/// The stored words, as they will be written.
fn words(st: &mut Station, ui: &mut Ui, r: Rect) {
    let inner = panel(ui.p, r, "Stored words");
    let Some(obj) = st.project.object.as_ref() else {
        return;
    };
    let id = Id::of("words");

    // Corner selector.
    let mut x = inner.left();
    for i in 0..obj.frames.len() {
        let b = Rect::from_min_size(Pos2::new(x, inner.top()), Vec2::new(46.0, 24.0));
        let active = i == st.selected_corner;
        let resp = ui.region(id.child(i), b);
        fill(
            ui.p,
            b,
            if active {
                theme::SELECT
            } else if resp.hovered {
                theme::SURFACE_HI
            } else {
                theme::SURFACE_LO
            },
        );
        num(
            ui.p,
            b.min + Vec2::new(10.0, 4.0),
            format!("C{}", i + 1),
            theme::T_SMALL,
            if active { theme::TEXT } else { theme::TEXT_DIM },
        );
        if resp.clicked {
            st.selected_corner = i;
        }
        x += 50.0;
    }

    let frame = &obj.frames[st.selected_corner.min(obj.frames.len() - 1)];
    let head = inner.top() + 34.0;
    for (i, h) in ["", "w0", "w1", "w2", "w3", "w4", "decoded"]
        .iter()
        .enumerate()
    {
        let cx = inner.left() + [0.0, 44.0, 92.0, 140.0, 188.0, 236.0, 292.0][i];
        label(
            ui.p,
            Pos2::new(cx, head),
            *h,
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }

    for (li, v) in frame.values.iter().enumerate() {
        let y = head + 20.0 + li as f32 * 19.0;
        if y + 19.0 > inner.bottom() {
            break;
        }
        let idle = v.is_identity();
        label(
            ui.p,
            Pos2::new(inner.left(), y),
            super::section_name(st, li),
            theme::T_SMALL,
            if idle {
                theme::TEXT_FAINT
            } else {
                theme::TEXT_DIM
            },
        );
        for (wi, w) in v.words.iter().enumerate() {
            num(
                ui.p,
                Pos2::new(inner.left() + 44.0 + wi as f32 * 48.0, y),
                format!("{w:04X}"),
                theme::T_SMALL,
                if idle { theme::TEXT_FAINT } else { theme::TEXT },
            );
        }
        num(
            ui.p,
            Pos2::new(inner.left() + 292.0, y),
            v.words
                .iter()
                .map(|w| format!("{:.3}", decode(*w)))
                .collect::<Vec<_>>()
                .join(" "),
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }
}
