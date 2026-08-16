use eframe::egui::Color32;

// ============================================================================
// UNIX CDE / MOTIF / EDA WORKSTATION THEME SYSTEM (Synopsys / Mentor Style)
// ============================================================================

// 1. Motif Window Chrome & Framing
pub const CHROME: Color32 = Color32::from_rgb(192, 196, 200);      // Motif neutral gray
pub const BEVEL_HI: Color32 = Color32::from_rgb(240, 244, 248);    // Bevel top/left highlight
pub const BEVEL_LO: Color32 = Color32::from_rgb(100, 106, 112);    // Bevel bottom/right shadow
pub const TITLEBAR: Color32 = Color32::from_rgb(38, 92, 90);       // Motif teal title bar
pub const TITLEBAR_INK: Color32 = Color32::from_rgb(240, 255, 250);// White/mint title text
pub const PANEL_BG: Color32 = Color32::from_rgb(208, 212, 216);    // Inset panel background

// 2. High-Contrast Engineering Canvas Wells (Obsidian Deep)
pub const WELL_BG: Color32 = Color32::from_rgb(10, 14, 12);        // Deep obsidian canvas
pub const WELL_BORDER: Color32 = Color32::from_rgb(40, 48, 44);    // Canvas inner frame
pub const GRATICULE_DIM: Color32 = Color32::from_rgb(24, 34, 30);  // Minor grid lines (10 dB / oct)
pub const GRATICULE_HI: Color32 = Color32::from_rgb(48, 64, 58);   // Major datum lines (0 dB, 1 kHz)
pub const TEXT_DIM: Color32 = Color32::from_rgb(120, 140, 130);    // Grid axis labels
pub const TEXT_HI: Color32 = Color32::from_rgb(220, 235, 225);     // High-signal readout text

// 3. Signal & Acoustic Traces (Phosphor Palette)
pub const TRACE_LIVE: Color32 = Color32::from_rgb(0, 240, 180);     // Active synthesized cascade (Mint)
pub const TRACE_TARGET: Color32 = Color32::from_rgb(255, 180, 40);  // Target ghost curve (Gold)
pub const TRACE_SO_FAR: Color32 = Color32::from_rgb(80, 200, 255);  // Cumulative signal-so-far (Cyan)
pub const TRACE_ISOLATED: Color32 = Color32::from_rgb(255, 140, 60);// Isolated stage curve (Orange)

// 4. 7 Serial Cascade Stage Colors (S1..S7)
pub const STAGE_COLORS: [Color32; 7] = [
    Color32::from_rgb(0, 190, 255),   // S1: Cyan
    Color32::from_rgb(255, 160, 50),  // S2: Orange
    Color32::from_rgb(100, 230, 80),  // S3: Green
    Color32::from_rgb(255, 70, 70),   // S4: Coral red
    Color32::from_rgb(180, 110, 255), // S5: Violet
    Color32::from_rgb(255, 220, 50),  // S6: Yellow
    Color32::from_rgb(160, 175, 185), // S7: Silver slate
];

// 5. Safety & Status Telemetry
pub const SAFETY_CEILING: Color32 = Color32::from_rgb(255, 50, 50); // +36dB safety crown limit
pub const HEADROOM_OK: Color32 = Color32::from_rgb(40, 200, 120);   // Green (< +24dB)
pub const HEADROOM_WARN: Color32 = Color32::from_rgb(255, 190, 40); // Amber (+24..+36dB)
pub const HEADROOM_CLIP: Color32 = Color32::from_rgb(255, 60, 60);  // Red (> +36dB)
pub const PUCK_GLOW: Color32 = Color32::from_rgb(255, 180, 40);     // Active audition cursor

pub fn faded(c: Color32, a: u8) -> Color32 {
    Color32::from_rgba_unmultiplied(c.r(), c.g(), c.b(), a)
}
