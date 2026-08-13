//! The Station's palette and type scale.
//!
//! Frame identity colours are generated from the frame index rather than read
//! from a table, because the number of frames is declared by the object and a
//! table would cap it. Eight frames and sixteen frames get the same treatment.

use eframe::egui::{Color32, FontFamily, FontId};

pub const BG: Color32 = Color32::from_rgb(0x0b, 0x0c, 0x0e);
pub const PANEL: Color32 = Color32::from_rgb(0x11, 0x13, 0x16);
pub const PANEL_HI: Color32 = Color32::from_rgb(0x17, 0x1a, 0x1e);
pub const RULE: Color32 = Color32::from_rgb(0x24, 0x28, 0x2e);
pub const RULE_HI: Color32 = Color32::from_rgb(0x39, 0x3f, 0x48);
pub const INK: Color32 = Color32::from_rgb(0xd8, 0xdb, 0xe0);
pub const INK_HI: Color32 = Color32::from_rgb(0xf2, 0xf4, 0xf7);
pub const DIM: Color32 = Color32::from_rgb(0x6a, 0x71, 0x7c);
pub const FAINT: Color32 = Color32::from_rgb(0x44, 0x4a, 0x54);
pub const ACCENT: Color32 = Color32::from_rgb(0x7c, 0xd9, 0x92);
pub const GOOD: Color32 = Color32::from_rgb(0x6f, 0xc2, 0x76);
pub const WARN: Color32 = Color32::from_rgb(0xd8, 0xa8, 0x3a);
pub const BAD: Color32 = Color32::from_rgb(0xe0, 0x53, 0x3d);
pub const DIRTY: Color32 = Color32::from_rgb(0xd8, 0x7d, 0x2e);
pub const SELECT: Color32 = Color32::from_rgb(0x2a, 0x3d, 0x54);

/// Identity colour for a frame. Hues step by the golden angle so neighbouring
/// frames stay distinct at any count, and the second half of the set is warmed
/// so the far plane of an axis reads apart from the near one.
pub fn frame_color(index: usize, total: usize) -> Color32 {
    let half = (total / 2).max(1);
    let warm = index >= half && total > 1;
    let hue = (index as f32 * 0.618_034) % 1.0;
    let (s, v) = if warm { (0.62, 0.90) } else { (0.52, 0.82) };
    hsv(hue, s, v)
}

fn hsv(h: f32, s: f32, v: f32) -> Color32 {
    let i = (h * 6.0).floor();
    let f = h * 6.0 - i;
    let (p, q, t) = (v * (1.0 - s), v * (1.0 - f * s), v * (1.0 - (1.0 - f) * s));
    let (r, g, b) = match (i as i32) % 6 {
        0 => (v, t, p),
        1 => (q, v, p),
        2 => (p, v, t),
        3 => (p, q, v),
        4 => (t, p, v),
        _ => (v, p, q),
    };
    Color32::from_rgb((r * 255.0) as u8, (g * 255.0) as u8, (b * 255.0) as u8)
}

/// Everything is monospace. Alignment carries the hierarchy here, not weight.
pub fn mono(size: f32) -> FontId {
    FontId::new(size, FontFamily::Monospace)
}

pub const T_MICRO: f32 = 9.0;
pub const T_SMALL: f32 = 10.0;
pub const T_BODY: f32 = 11.0;
pub const T_HEAD: f32 = 13.0;

/// Fades a colour toward the background without an alpha layer, so overlapping
/// marks keep exact colours.
pub fn mix(a: Color32, b: Color32, t: f32) -> Color32 {
    let t = t.clamp(0.0, 1.0);
    let f = |x: u8, y: u8| (x as f32 + (y as f32 - x as f32) * t) as u8;
    Color32::from_rgb(f(a.r(), b.r()), f(a.g(), b.g()), f(a.b(), b.b()))
}
