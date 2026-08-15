use eframe::egui::Color32;

pub const CHROME: Color32 = Color32::from_rgb(198, 194, 188);
pub const CHROME_LT: Color32 = Color32::from_rgb(236, 233, 228);
pub const CHROME_DK: Color32 = Color32::from_rgb(128, 124, 118);
pub const CHROME_DEEP: Color32 = Color32::from_rgb(74, 71, 67);
pub const INK: Color32 = Color32::from_rgb(38, 35, 31);
pub const INK_DIM: Color32 = Color32::from_rgb(104, 100, 94);
pub const FIELD: Color32 = Color32::from_rgb(233, 231, 226);
pub const WELL: Color32 = Color32::from_rgb(6, 8, 9);
pub const GRATICULE: Color32 = Color32::from_rgb(27, 35, 39);
pub const WELL_DIM: Color32 = Color32::from_rgb(80, 98, 104);
pub const CURSOR: Color32 = Color32::from_rgb(238, 236, 229);
pub const ASK: Color32 = Color32::from_rgb(88, 172, 216);
pub const NOW: Color32 = Color32::from_rgb(79, 217, 124);
pub const ASK_INK: Color32 = Color32::from_rgb(38, 100, 140);
pub const NOW_INK: Color32 = Color32::from_rgb(26, 122, 66);
pub const HOT: Color32 = Color32::from_rgb(255, 122, 60);
pub const ALARM: Color32 = Color32::from_rgb(232, 80, 64);

pub const LANES: [Color32; 7] = [
    Color32::from_rgb(79, 195, 247),
    Color32::from_rgb(255, 179, 84),
    Color32::from_rgb(124, 227, 139),
    Color32::from_rgb(255, 110, 110),
    Color32::from_rgb(181, 140, 255),
    Color32::from_rgb(217, 160, 102),
    Color32::from_rgb(255, 143, 208),
];

pub const TITLE: f32 = 11.5;
pub const BODY: f32 = 12.5;
pub const SMALL: f32 = 10.0;

pub fn faded(c: Color32, alpha: u8) -> Color32 {
    Color32::from_rgba_unmultiplied(c.r(), c.g(), c.b(), alpha)
}
