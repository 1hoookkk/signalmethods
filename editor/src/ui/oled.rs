use eframe::egui::{Color32, FontId, Painter, Pos2, Rect, Stroke, Vec2};

use crate::domain::field::Field;
use crate::engine::response::row_db;
use crate::session::state::AuditionPosition;
use crate::ui::paint;

/// 128x64 Retro OLED Phosphor Palette
pub const OLED_BG: Color32 = Color32::from_rgb(10, 14, 12);
pub const OLED_PIXEL_ON: Color32 = Color32::from_rgb(0, 240, 180);      // High-contrast cyan/mint OLED
pub const OLED_PIXEL_DIM: Color32 = Color32::from_rgb(0, 100, 75);      // Graticules and inactive edges
pub const OLED_PIXEL_BRIGHT: Color32 = Color32::from_rgb(220, 255, 240); // Active puck / focus
pub const OLED_PUCK_GLOW: Color32 = Color32::from_rgb(255, 180, 40);     // Amber audition cursor

pub struct OledDisplay {
    pub rotation_rad: f32,
    pub pitch_rad: f32,
}

impl Default for OledDisplay {
    fn default() -> Self {
        Self {
            rotation_rad: 0.65_f32, // ~37 degrees isometric angle
            pitch_rad: 0.40_f32,    // ~23 degrees elevation
        }
    }
}

impl OledDisplay {
    /// Paint the 128x64 Morpheus OLED display module with 3D wireframe cube and live coordinates.
    pub fn paint(
        &mut self,
        painter: &Painter,
        rect: Rect,
        field: &Field,
        active_corner: usize,
        audition: &AuditionPosition,
        preset_name: &str,
    ) {
        // 1. Draw outer hardware bezel with sunken bevel
        let inner = paint::sunken(painter, rect);
        painter.rect_filled(inner, 1.0_f32, OLED_BG);

        let screen = inner.shrink(2.0_f32);

        // 2. Header Bar: Preset Name & Mode
        let header_h = 14.0_f32;
        let header_rect = Rect::from_min_size(screen.min, Vec2::new(screen.width(), header_h));
        painter.line_segment(
            [
                Pos2::new(header_rect.left(), header_rect.bottom()),
                Pos2::new(header_rect.right(), header_rect.bottom()),
            ],
            Stroke::new(1.0_f32, OLED_PIXEL_DIM),
        );

        let font_title = FontId::monospace(9.5_f32);
        let font_small = FontId::monospace(8.0_f32);
        let is_cube = field.completeness() == Some(true);

        painter.text(
            header_rect.left_top() + Vec2::new(4.0_f32, 1.5_f32),
            eframe::emath::Align2::LEFT_TOP,
            format!("MORPHEUS // {}", preset_name.to_uppercase()),
            font_title,
            OLED_PIXEL_ON,
        );

        painter.text(
            header_rect.right_top() + Vec2::new(-4.0_f32, 2.0_f32),
            eframe::emath::Align2::RIGHT_TOP,
            if is_cube { "3D-CUBE" } else { "2D-PLANE" },
            font_small,
            OLED_PIXEL_DIM,
        );

        // 3. Split lower screen into 3D Wireframe Cube (Left 46%) and Spectrum Trace + Readout (Right 54%)
        let body_rect = Rect::from_min_size(
            Pos2::new(screen.left(), screen.top() + header_h),
            Vec2::new(screen.width(), screen.height() - header_h),
        );

        let cube_w = body_rect.width() * 0.46_f32;
        let cube_rect = Rect::from_min_size(body_rect.min, Vec2::new(cube_w, body_rect.height()));
        let spec_rect = Rect::from_min_size(
            Pos2::new(body_rect.left() + cube_w, body_rect.top()),
            Vec2::new(body_rect.width() - cube_w, body_rect.height()),
        );

        // Vertical divider
        painter.line_segment(
            [
                Pos2::new(spec_rect.left(), body_rect.top() + 2.0_f32),
                Pos2::new(spec_rect.left(), body_rect.bottom() - 2.0_f32),
            ],
            Stroke::new(1.0_f32, OLED_PIXEL_DIM),
        );

        // 4. Render 3D Wireframe Cube with Isometric Projection
        self.draw_3d_cube(painter, cube_rect, active_corner, audition, is_cube);

        // 5. Render Live Interpolated Response Trace + Real-Time Coordinates Readout
        self.draw_spectrum_and_coords(painter, spec_rect, field, audition, is_cube);
    }

    fn draw_3d_cube(
        &self,
        painter: &Painter,
        rect: Rect,
        active_corner: usize,
        audition: &AuditionPosition,
        is_cube: bool,
    ) {
        let center = rect.center() + Vec2::new(0.0_f32, 2.0_f32);
        let scale = rect.height().min(rect.width()) * 0.38_f32;

        // 3D Rotation matrices
        let cos_r = self.rotation_rad.cos();
        let sin_r = self.rotation_rad.sin();
        let cos_p = self.pitch_rad.cos();
        let sin_p = self.pitch_rad.sin();

        // 3D -> 2D projection closure: (x, y, z) in [-1, 1] -> screen Pos2
        let project = |x: f32, y: f32, z: f32| -> Pos2 {
            let x1 = x * cos_r - z * sin_r;
            let z1 = x * sin_r + z * cos_r;
            let y2 = y * cos_p - z1 * sin_p;
            Pos2::new(center.x + x1 * scale, center.y - y2 * scale)
        };

        let z_far = if is_cube { 1.0_f32 } else { -1.0_f32 };
        let verts_3d = [
            (-1.0_f32, -1.0_f32, -1.0_f32),
            ( 1.0_f32, -1.0_f32, -1.0_f32),
            (-1.0_f32,  1.0_f32, -1.0_f32),
            ( 1.0_f32,  1.0_f32, -1.0_f32),
            (-1.0_f32, -1.0_f32, z_far),
            ( 1.0_f32, -1.0_f32, z_far),
            (-1.0_f32,  1.0_f32, z_far),
            ( 1.0_f32,  1.0_f32, z_far),
        ];

        let pts: Vec<Pos2> = verts_3d.iter().map(|&(x, y, z)| project(x, y, z)).collect();

        // 12 Cube Edges
        let edges = [
            (0, 1), (1, 3), (3, 2), (2, 0),
            (4, 5), (5, 7), (7, 6), (6, 4),
            (0, 4), (1, 5), (2, 6), (3, 7),
        ];

        for &(i, j) in &edges {
            if !is_cube && (i >= 4 || j >= 4) {
                continue;
            }
            painter.line_segment([pts[i], pts[j]], Stroke::new(1.0_f32, OLED_PIXEL_DIM));
        }

        // Draw 8 Vertex Dots with Active Highlight
        for i in 0..8 {
            if !is_cube && i >= 4 {
                continue;
            }
            let is_sel = i == active_corner;
            let dot_col = if is_sel { OLED_PIXEL_BRIGHT } else { OLED_PIXEL_ON };
            let radius = if is_sel { 3.0_f32 } else { 1.8_f32 };

            painter.circle_filled(pts[i], radius, dot_col);
            if is_sel {
                painter.circle_stroke(pts[i], 4.5_f32, Stroke::new(1.0_f32, OLED_PIXEL_ON));
            }

            let font = FontId::monospace(6.5_f32);
            painter.text(
                pts[i] + Vec2::new(3.0_f32, -4.0_f32),
                eframe::emath::Align2::LEFT_BOTTOM,
                format!("{}", i),
                font,
                if is_sel { OLED_PIXEL_BRIGHT } else { OLED_PIXEL_DIM },
            );
        }

        // Audition Cursor / Current Position (pos[0]=M, pos[1]=F, pos[2]=T)
        let ax = audition.pos[0] * 2.0_f32 - 1.0_f32;
        let ay = audition.pos[1] * 2.0_f32 - 1.0_f32;
        let az = if is_cube { audition.pos[2] * 2.0_f32 - 1.0_f32 } else { -1.0_f32 };

        let puck_pt = project(ax, ay, az);
        let floor_pt = project(ax, ay, -1.0_f32);
        painter.line_segment([floor_pt, puck_pt], Stroke::new(1.0_f32, OLED_PIXEL_DIM));

        painter.circle_filled(puck_pt, 2.5_f32, OLED_PUCK_GLOW);
        painter.circle_stroke(puck_pt, 4.5_f32, Stroke::new(1.0_f32, OLED_PUCK_GLOW));
    }

    fn draw_spectrum_and_coords(
        &self,
        painter: &Painter,
        rect: Rect,
        field: &Field,
        audition: &AuditionPosition,
        is_cube: bool,
    ) {
        let pad = 4.0_f32;
        let plot_rect = Rect::from_min_size(
            Pos2::new(rect.left() + pad, rect.top() + pad),
            Vec2::new(rect.width() - pad * 2.0_f32, rect.height() * 0.58_f32),
        );

        let coords_rect = Rect::from_min_size(
            Pos2::new(rect.left() + pad, plot_rect.bottom() + 2.0_f32),
            Vec2::new(rect.width() - pad * 2.0_f32, rect.height() - plot_rect.height() - pad * 2.0_f32),
        );

        // 1. Mini Graticule Box
        painter.rect_filled(plot_rect, 1.0_f32, OLED_BG);
        painter.rect_stroke(plot_rect, 1.0_f32, Stroke::new(1.0_f32, OLED_PIXEL_DIM));

        let mid_y = plot_rect.center().y;
        painter.line_segment(
            [
                Pos2::new(plot_rect.left(), mid_y),
                Pos2::new(plot_rect.right(), mid_y),
            ],
            Stroke::new(1.0_f32, OLED_PIXEL_DIM),
        );

        // 2. Mini Frequency Response Curve
        let sr = crate::engine::response::SR;
        let num_pts = 48;
        let mut curve_pts = Vec::with_capacity(num_pts);

        if let Some(c0) = field.slots[0].as_ref() {
            let rows: Vec<[f64; 5]> = c0.lanes.iter().map(|s| s.biquad_at(sr)).collect();
            for i in 0..num_pts {
                let t = i as f64 / (num_pts - 1) as f64;
                let hz = 40.0 * (16000.0f64 / 40.0).powf(t);
                let sum_db: f64 = rows.iter().map(|r| row_db(r, hz, sr)).sum();

                let px = plot_rect.left() + (t as f32) * plot_rect.width();
                let norm_y = ((sum_db + 36.0) / 60.0).clamp(0.0, 1.0) as f32;
                let py = plot_rect.bottom() - norm_y * plot_rect.height();
                curve_pts.push(Pos2::new(px, py));
            }
        }

        if curve_pts.len() >= 2 {
            painter.add(eframe::egui::Shape::line(
                curve_pts,
                Stroke::new(1.2_f32, OLED_PIXEL_ON),
            ));
        }

        // 3. Real-Time Coordinates Readout (M, F, T values)
        let font_val = FontId::monospace(8.5_f32);
        let m_pct = (audition.pos[0] * 100.0_f32).round() as i32;
        let f_pct = (audition.pos[1] * 100.0_f32).round() as i32;
        let t_pct = (audition.pos[2] * 100.0_f32).round() as i32;

        let txt1 = format!("M:{:3}% F:{:3}%", m_pct, f_pct);
        let txt2 = format!("T:{:3}% C:{}", t_pct, if is_cube { "8" } else { "4" });

        painter.text(
            coords_rect.left_top() + Vec2::new(2.0_f32, 1.0_f32),
            eframe::emath::Align2::LEFT_TOP,
            txt1,
            font_val.clone(),
            OLED_PIXEL_BRIGHT,
        );

        painter.text(
            coords_rect.left_top() + Vec2::new(2.0_f32, 11.0_f32),
            eframe::emath::Align2::LEFT_TOP,
            txt2,
            font_val,
            OLED_PIXEL_ON,
        );
    }
}
