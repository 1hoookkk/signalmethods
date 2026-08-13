//! Palette and type.
//!
//! A light plotting surface: white ground, dark ink, thin rules. Panels are
//! separated by space and a title rather than by boxes, so the traces are the
//! only strong marks on screen. Colour identifies a corner or a state and is
//! never used as a large fill.

use eframe::egui::{Color32, FontFamily, FontId};

pub const BG: Color32 = Color32::from_rgb(0xf2, 0xf2, 0xf1);
pub const SURFACE: Color32 = Color32::from_rgb(0xff, 0xff, 0xff);
pub const SURFACE_HI: Color32 = Color32::from_rgb(0xe8, 0xe8, 0xe6);
pub const SURFACE_LO: Color32 = Color32::from_rgb(0xfa, 0xfa, 0xf9);
pub const LINE: Color32 = Color32::from_rgb(0xd0, 0xd0, 0xce);
pub const LINE_HI: Color32 = Color32::from_rgb(0x9a, 0x9a, 0x98);
pub const TEXT: Color32 = Color32::from_rgb(0x1a, 0x1a, 0x1a);
pub const TEXT_DIM: Color32 = Color32::from_rgb(0x50, 0x50, 0x50);
pub const TEXT_FAINT: Color32 = Color32::from_rgb(0x8a, 0x8a, 0x88);
pub const ACCENT: Color32 = Color32::from_rgb(0x1f, 0x77, 0xb4);
pub const GOOD: Color32 = Color32::from_rgb(0x2c, 0xa0, 0x2c);
pub const WARN: Color32 = Color32::from_rgb(0xff, 0x7f, 0x0e);
pub const BAD: Color32 = Color32::from_rgb(0xd6, 0x27, 0x28);
pub const SELECT: Color32 = Color32::from_rgb(0xdc, 0xe9, 0xf5);
pub const INK: Color32 = Color32::from_rgb(0x20, 0x20, 0x20);

/// Corner identity, on the standard categorical plotting set.
pub fn corner_color(index: usize) -> Color32 {
    const SET: [Color32; 8] = [
        Color32::from_rgb(0x1f, 0x77, 0xb4),
        Color32::from_rgb(0xff, 0x7f, 0x0e),
        Color32::from_rgb(0x2c, 0xa0, 0x2c),
        Color32::from_rgb(0xd6, 0x27, 0x28),
        Color32::from_rgb(0x94, 0x67, 0xbd),
        Color32::from_rgb(0x8c, 0x56, 0x4b),
        Color32::from_rgb(0xe3, 0x77, 0xc2),
        Color32::from_rgb(0x7f, 0x7f, 0x7f),
    ];
    SET[index % SET.len()]
}

/// Interface type: labels, navigation, controls.
pub fn ui(size: f32) -> FontId {
    FontId::new(size, FontFamily::Proportional)
}

/// Numeric type: coefficients, words, addresses, readings.
pub fn mono(size: f32) -> FontId {
    FontId::new(size, FontFamily::Monospace)
}

pub const T_SMALL: f32 = 11.5;
pub const T_BODY: f32 = 13.0;
pub const T_HEAD: f32 = 15.0;

pub fn mix(a: Color32, b: Color32, t: f32) -> Color32 {
    let t = t.clamp(0.0, 1.0);
    let f = |x: u8, y: u8| (x as f32 + (y as f32 - x as f32) * t) as u8;
    Color32::from_rgb(f(a.r(), b.r()), f(a.g(), b.g()), f(a.b(), b.b()))
}

/// Installs the system interface font when one is available.
pub fn install_fonts(ctx: &eframe::egui::Context) {
    use eframe::egui::{FontData, FontDefinitions};
    let mut fonts = FontDefinitions::default();
    for path in [
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
    ] {
        if let Ok(bytes) = std::fs::read(path) {
            fonts
                .font_data
                .insert("ui".to_owned(), FontData::from_owned(bytes).into());
            fonts
                .families
                .entry(FontFamily::Proportional)
                .or_default()
                .insert(0, "ui".to_owned());
            break;
        }
    }
    if let Ok(bytes) = std::fs::read("C:/Windows/Fonts/consola.ttf") {
        fonts
            .font_data
            .insert("num".to_owned(), FontData::from_owned(bytes).into());
        fonts
            .families
            .entry(FontFamily::Monospace)
            .or_default()
            .insert(0, "num".to_owned());
    }
    ctx.set_fonts(fonts);
}
