//! The empty state. No object exists, so nothing about one is drawn.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::{PathIntent, Station};
use crate::model::object::ObjectForm;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{label, label_center};
use crate::ui::theme;

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let id = Id::of("start");
    let card_w = 220.0;
    let card_h = 96.0;
    let gap = 12.0;
    let total = card_w * 3.0 + gap * 2.0;
    let origin = Pos2::new(
        r.center().x - total * 0.5,
        r.center().y - card_h * 0.5 - 20.0,
    );

    let choices: [(&str, &str); 3] = [
        ("New .4d", "4 corners · morph × Q"),
        ("New cube", "8 corners · morph × Q × T"),
        ("Open", "project or packed body"),
    ];

    for (i, (title, detail)) in choices.iter().enumerate() {
        let b = Rect::from_min_size(
            Pos2::new(origin.x + i as f32 * (card_w + gap), origin.y),
            Vec2::new(card_w, card_h),
        );
        let resp = ui.region(id.child(i), b);
        crate::ui::paint::fill(
            ui.p,
            b,
            if resp.hovered {
                theme::SURFACE_HI
            } else {
                theme::SURFACE
            },
        );
        label(
            ui.p,
            b.min + Vec2::new(16.0, 26.0),
            *title,
            theme::T_HEAD,
            theme::TEXT,
        );
        label(
            ui.p,
            b.min + Vec2::new(16.0, 50.0),
            *detail,
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
        if resp.clicked {
            match i {
                0 => st.new_object(ObjectForm::Square),
                1 => st.new_object(ObjectForm::Cube),
                _ => st.open_path_bar(PathIntent::Open),
            }
        }
    }

    label_center(
        ui.p,
        Pos2::new(r.center().x, origin.y + card_h + 34.0),
        "drop a file to open",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );
}
