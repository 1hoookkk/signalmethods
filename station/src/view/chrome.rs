//! Title bar, file commands, navigation, status line and the path bar.

use eframe::egui::{Key, Pos2, Rect, Vec2};

use crate::app::{NoteKind, PathIntent, Screen, Station};
use crate::model::object::ObjectForm;
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{fill, hairline, label, label_right, num_right, outline};
use crate::ui::theme;
use crate::ui::widgets;

use super::PAD;

pub fn header(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::SURFACE);
    hairline(
        ui.p,
        Pos2::new(r.left(), r.bottom()),
        Pos2::new(r.right(), r.bottom()),
        theme::LINE,
    );

    label(
        ui.p,
        r.min + Vec2::new(PAD, 8.0),
        "Station",
        theme::T_HEAD,
        theme::TEXT,
    );

    // File commands. Every one of these does something on every frame it is
    // enabled, and is disabled when it cannot.
    let has_object = !st.project.is_empty();
    let cmds: [(&str, bool); 7] = [
        ("New .4d", true),
        ("New cube", true),
        ("Open", true),
        ("Save", has_object),
        ("Save As", has_object),
        ("Undo", st.history.can_undo()),
        ("Redo", st.history.can_redo()),
    ];
    let id = Id::of("cmd");
    let mut x = r.left() + 96.0;
    for (i, (text, enabled)) in cmds.iter().enumerate() {
        let w = crate::ui::paint::label_width(ui.p, text, theme::T_BODY) + 20.0;
        let b = Rect::from_min_size(Pos2::new(x, r.top() + 5.0), Vec2::new(w, r.height() - 11.0));
        if widgets::button(ui, id.child(i), b, text, *enabled) {
            match i {
                0 => st.new_object(ObjectForm::Square),
                1 => st.new_object(ObjectForm::Cube),
                2 => st.open_path_bar(PathIntent::Open),
                3 => st.save(),
                4 => st.open_path_bar(PathIntent::SaveAs),
                5 => st.undo(),
                _ => st.redo(),
            }
        }
        x += w + 4.0;
    }

    // Right side: what is open.
    let title = if st.project.is_empty() {
        "no object".to_string()
    } else {
        let form = st.project.form().map(|f| f.name()).unwrap_or("");
        format!("{}  ·  {form}", st.project.name)
    };
    let mark = if st.dirty() && !st.project.is_empty() {
        ui.p.circle_filled(
            Pos2::new(r.right() - PAD - 4.0, r.center().y),
            3.5,
            theme::WARN,
        );
        16.0
    } else {
        0.0
    };
    label_right(
        ui.p,
        Pos2::new(r.right() - PAD - mark, r.top() + 9.0),
        title,
        theme::T_BODY,
        theme::TEXT_DIM,
    );
}

pub fn nav(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::SURFACE_LO);
    hairline(
        ui.p,
        Pos2::new(r.left(), r.bottom()),
        Pos2::new(r.right(), r.bottom()),
        theme::LINE,
    );
    if st.project.is_empty() {
        return;
    }
    let id = Id::of("nav");
    let mut x = r.left() + PAD;
    for (i, s) in Screen::ALL.iter().enumerate() {
        let w = crate::ui::paint::label_width(ui.p, s.label(), theme::T_BODY) + 30.0;
        let b = Rect::from_min_size(Pos2::new(x, r.top()), Vec2::new(w, r.height()));
        let active = st.screen == *s;
        let resp = ui.region(id.child(i), b);
        if active {
            fill(ui.p, b, theme::SURFACE);
            fill(
                ui.p,
                Rect::from_min_size(
                    Pos2::new(b.left(), b.bottom() - 2.0),
                    Vec2::new(b.width(), 2.0),
                ),
                theme::ACCENT,
            );
        } else if resp.hovered {
            fill(ui.p, b, theme::mix(theme::SURFACE_LO, theme::SURFACE, 0.6));
        }
        crate::ui::paint::label_center(
            ui.p,
            b.center(),
            s.label(),
            theme::T_BODY,
            if active { theme::TEXT } else { theme::TEXT_DIM },
        );
        if resp.clicked {
            st.screen = *s;
        }
        x += w;
    }
}

pub fn status(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::SURFACE);
    hairline(
        ui.p,
        Pos2::new(r.left(), r.top()),
        Pos2::new(r.right(), r.top()),
        theme::LINE,
    );

    // Laws report; they never gate.
    if !st.project.is_empty() && !st.laws.laws.is_empty() {
        let readings = crate::model::law::read_all(&st.project, &st.laws.laws, st.selected_corner);
        let held = readings.iter().filter(|x| x.ok).count();
        let total = readings.len();
        let c = if held == total {
            theme::GOOD
        } else {
            theme::WARN
        };
        ui.p.circle_filled(Pos2::new(r.left() + PAD + 4.0, r.center().y), 3.5, c);
        label(
            ui.p,
            Pos2::new(r.left() + PAD + 14.0, r.top() + 6.0),
            format!("{held} of {total} laws hold"),
            theme::T_SMALL,
            theme::TEXT_DIM,
        );
    }

    if !st.note.is_empty() {
        let c = match st.note_kind {
            NoteKind::Plain => theme::TEXT_DIM,
            NoteKind::Warn => theme::WARN,
            NoteKind::Error => theme::BAD,
        };
        label(
            ui.p,
            Pos2::new(r.left() + 190.0, r.top() + 6.0),
            &st.note,
            theme::T_SMALL,
            c,
        );
    }

    if let Some(p) = &st.path {
        num_right(
            ui.p,
            Pos2::new(r.right() - PAD, r.top() + 6.0),
            p.display().to_string(),
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }
}

/// Path entry. The Station takes a typed path rather than a system dialog.
pub fn path_bar(st: &mut Station, ui: &mut Ui, full: Rect) {
    let r = Rect::from_center_size(full.center(), Vec2::new(720.0, 118.0));
    fill(
        ui.p,
        r.expand(4.0),
        theme::mix(theme::BG, theme::SURFACE_LO, 0.6),
    );
    fill(ui.p, r, theme::SURFACE);
    outline(ui.p, r, theme::ACCENT);

    let title = match st.path_bar {
        Some(PathIntent::Open) => "Open",
        Some(PathIntent::SaveAs) => "Save as",
        Some(PathIntent::ExportBody) => "Export body",
        None => "",
    };
    label(
        ui.p,
        r.min + Vec2::new(14.0, 10.0),
        title,
        theme::T_HEAD,
        theme::TEXT,
    );

    let field = Rect::from_min_size(
        r.min + Vec2::new(14.0, 38.0),
        Vec2::new(r.width() - 28.0, 30.0),
    );
    let id = Id::of("path-entry");
    ui.set_focus(id);
    st.path_entry.show(ui, id, field);

    let ok = Rect::from_min_size(
        Pos2::new(r.right() - 190.0, r.bottom() - 36.0),
        Vec2::new(84.0, 26.0),
    );
    let cancel = Rect::from_min_size(
        Pos2::new(r.right() - 98.0, r.bottom() - 36.0),
        Vec2::new(84.0, 26.0),
    );
    let enter = ui.input.key(Key::Enter);
    let esc = ui.input.key(Key::Escape);
    if widgets::button(ui, id.child("ok"), ok, "Confirm", true) || enter {
        st.commit_path_bar();
    }
    if widgets::button(ui, id.child("cancel"), cancel, "Cancel", true) || esc {
        st.path_bar = None;
    }
}
