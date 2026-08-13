use eframe::egui::{Color32, FontFamily, FontId};

pub const BG: Color32 = Color32::from_rgb(0x0d, 0x0d, 0x0f);
pub const PANEL: Color32 = Color32::from_rgb(0x13, 0x14, 0x17);
pub const RULE: Color32 = Color32::from_rgb(0x27, 0x2a, 0x30);
pub const INK: Color32 = Color32::from_rgb(0xd6, 0xd8, 0xdc);
pub const DIM: Color32 = Color32::from_rgb(0x6b, 0x70, 0x79);
pub const GOOD: Color32 = Color32::from_rgb(0x6f, 0xc2, 0x76);
pub const BAD: Color32 = Color32::from_rgb(0xe0, 0x53, 0x3d);
pub const LIVE: Color32 = Color32::from_rgb(0x7d, 0xe0, 0x8d);

/// One colour per corner, floor cool and ceiling hot, so the two planes of the
/// cube read apart at a glance.
pub const CORNER: [Color32; 8] = [
    Color32::from_rgb(0xb0, 0x7d, 0x46),
    Color32::from_rgb(0xd7, 0xb1, 0x3a),
    Color32::from_rgb(0x45, 0x9d, 0xd8),
    Color32::from_rgb(0x74, 0xc2, 0x4a),
    Color32::from_rgb(0xd8, 0x7d, 0x2e),
    Color32::from_rgb(0xd9, 0x4f, 0x4f),
    Color32::from_rgb(0x9c, 0x5c, 0xd8),
    Color32::from_rgb(0xe0, 0x3b, 0x3b),
];

pub fn mono(size: f32) -> FontId {
    FontId::new(size, FontFamily::Monospace)
}
