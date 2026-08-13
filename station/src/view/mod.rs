//! Screens. Each module draws one thing; this file only routes.

pub mod breakdown;
pub mod chrome;
pub mod cube;
pub mod export;
pub mod laws;
pub mod object;
pub mod response;
pub mod sections;
pub mod square;
pub mod start;
pub mod zplane;

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::{Screen, Station};
use crate::ui::input::Ui;

pub const PAD: f32 = 10.0;
pub const HEAD_H: f32 = 34.0;
pub const NAV_H: f32 = 32.0;
pub const STATUS_H: f32 = 26.0;

pub fn draw(st: &mut Station, ui: &mut Ui, full: Rect) {
    let head = Rect::from_min_size(full.min, Vec2::new(full.width(), HEAD_H));
    let nav = Rect::from_min_size(
        Pos2::new(full.left(), head.bottom()),
        Vec2::new(full.width(), NAV_H),
    );
    let status = Rect::from_min_size(
        Pos2::new(full.left(), full.bottom() - STATUS_H),
        Vec2::new(full.width(), STATUS_H),
    );
    let body = Rect::from_min_max(
        Pos2::new(full.left() + PAD, nav.bottom() + PAD),
        Pos2::new(full.right() - PAD, status.top() - PAD),
    );

    chrome::header(st, ui, head);
    chrome::nav(st, ui, nav);

    if st.project.is_empty() {
        start::draw(st, ui, body);
    } else {
        match st.screen {
            Screen::Object => object::draw(st, ui, body),
            Screen::Sections => sections::draw(st, ui, body),
            Screen::Laws => laws::draw(st, ui, body),
            Screen::Export => export::draw(st, ui, body),
        }
    }

    chrome::status(st, ui, status);
    if st.path_bar.is_some() {
        chrome::path_bar(st, ui, full);
    }
}

/// The corner's name, from grammar when one is declared.
pub fn corner_name(st: &Station, i: usize) -> String {
    let fallback = st
        .project
        .frames()
        .get(i)
        .map(|f| f.label.clone())
        .unwrap_or_default();
    st.grammar.corner_name(i, &fallback)
}

/// The section's name, from grammar when one is declared.
pub fn section_name(st: &Station, i: usize) -> String {
    st.grammar.section_name(i, &format!("S{}", i + 1))
}
