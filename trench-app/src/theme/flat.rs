use egui::Color32;

use super::{Chrome, DataInk, Metrics, Theme, Well};

const fn rgb(r: u8, g: u8, b: u8) -> Color32 {
    Color32::from_rgb(r, g, b)
}

pub fn theme() -> Theme {
    Theme {
        name: "flat",
        chrome: Chrome {
            face: rgb(206, 210, 214),
            face_hi: rgb(220, 224, 228),
            face_lo: rgb(188, 193, 199),
            rule: rgb(128, 134, 141),
            ink: rgb(16, 20, 24),
            ink_dim: rgb(84, 92, 100),
            select_fill: rgb(176, 192, 214),
            select_ink: rgb(24, 62, 168),
        },
        well: Well {
            bg: rgb(240, 244, 250),
            rule: rgb(206, 214, 224),
            grid: rgb(150, 163, 179),
            ink: rgb(12, 14, 18),
            dim: rgb(70, 80, 92),
        },
        data: DataInk {
            stage: [
                rgb(178, 34, 34),
                rgb(170, 88, 0),
                rgb(122, 102, 0),
                rgb(30, 110, 46),
                rgb(0, 106, 118),
                rgb(28, 82, 176),
                rgb(96, 52, 152),
            ],
            target: rgb(96, 104, 112),
            target_fill: Color32::from_rgba_unmultiplied(96, 132, 196, 26),
            live: rgb(10, 12, 16),
            preview: rgb(28, 82, 176),
            candidate: rgb(176, 24, 140),
            error: rgb(196, 28, 28),
            crown: rgb(176, 40, 40),
        },
        metrics: Metrics {
            corner_radius: 0,
            border_px: 1.0,
            bar_h: 28.0,
            row_h: 20.0,
            cell_h: 24.0,
            gap: 4.0,
            well_pad: 6.0,
        },
    }
}
