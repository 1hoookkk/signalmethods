//! Where the roots sit, on the ARMAdillo plot.
//!
//! Angle is `theta' = pi(10 + log2(theta/pi))/10` and radial distance is
//! `R' = 20 log10(1/(1-R))`, drawn linear in dB so equal radial steps are
//! equal dB. Both come from `trench_core::armadillo`; nothing is re-derived
//! here.
//!
//! A root below the floor of the plot is left off it. The transform returns a
//! negative angle for those, and the crate is explicit that they are excluded
//! rather than moved onto the rim.
//!
//! This is an inspector. Roots are placed by the fitter or typed as numbers;
//! this pane says where they landed.

use eframe::egui::{Pos2, Rect, Stroke, Vec2};

use trench_core::armadillo::{
    display_radius_from_r_prime_db, on_plot, r_prime_db_from_radius, theta_prime_from_hz, RIM_DB,
};
use trench_core::stage_law::RootPair;

use crate::app::Station;
use crate::ui::input::Ui;
use crate::ui::paint::{hairline, label, line, num};
use crate::ui::theme;

/// Screen position of a root, or `None` when it is off the plot.
fn place(centre: Pos2, rho_max: f32, hz: f64, r: f64, fs: f64) -> Option<Pos2> {
    let theta_prime = theta_prime_from_hz(hz, fs).ok()?;
    if !on_plot(theta_prime) {
        return None;
    }
    let db = r_prime_db_from_radius(r).ok()?;
    let rho = display_radius_from_r_prime_db(db.clamp(0.0, RIM_DB), rho_max as f64) as f32;
    Some(Pos2::new(
        centre.x + rho * theta_prime.cos() as f32,
        centre.y - rho * theta_prime.sin() as f32,
    ))
}

pub fn draw(st: &Station, ui: &mut Ui, r: Rect, corner: usize, selected_lane: usize) {
    let fs = st.project.sample_rate();
    let rho_max = (r.width() * 0.5).min(r.height() - 16.0).max(20.0);
    let centre = Pos2::new(r.center().x, r.bottom() - 12.0);

    // The rim, and rings at even dB so radial distance reads as resonance.
    for (db, heavy) in [(24.0, false), (48.0, false), (72.0, false), (RIM_DB, true)] {
        let rho = display_radius_from_r_prime_db(db, rho_max as f64) as f32;
        let n = 48;
        let mut prev: Option<Pos2> = None;
        for i in 0..=n {
            let a = i as f32 / n as f32 * std::f32::consts::PI;
            let p = Pos2::new(centre.x + a.cos() * rho, centre.y - a.sin() * rho);
            if let Some(q) = prev {
                line(
                    ui.p,
                    q,
                    p,
                    if heavy { theme::LINE_HI } else { theme::LINE },
                    1.0,
                );
            }
            prev = Some(p);
        }
    }
    hairline(
        ui.p,
        Pos2::new(centre.x - rho_max, centre.y),
        Pos2::new(centre.x + rho_max, centre.y),
        theme::LINE,
    );

    // Decade spokes, placed through the same transform as the roots.
    for hz in [100.0f64, 1_000.0, 10_000.0] {
        let Ok(tp) = theta_prime_from_hz(hz, fs) else {
            continue;
        };
        if !on_plot(tp) {
            continue;
        }
        let tip = Pos2::new(
            centre.x + tp.cos() as f32 * rho_max,
            centre.y - tp.sin() as f32 * rho_max,
        );
        hairline(
            ui.p,
            centre,
            tip,
            theme::mix(theme::SURFACE, theme::LINE, 0.6),
        );
        num(
            ui.p,
            tip + Vec2::new(-10.0, -14.0),
            if hz >= 1000.0 {
                format!("{:.0}k", hz / 1000.0)
            } else {
                format!("{hz:.0}")
            },
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }

    // Every section of this corner, so correspondence is visible at a glance.
    let Some(frame) = st.project.frames().get(corner) else {
        return;
    };
    let mut off_plot = 0usize;
    for (li, v) in frame.values.iter().enumerate() {
        if v.is_identity() {
            continue;
        }
        let g = v.geometry(fs);
        let c = theme::corner_color(li);
        let sel = li == selected_lane;

        if let RootPair::Conjugate { hz, r: rr } = g.zero {
            if rr > 0.0 {
                match place(centre, rho_max, hz, rr, fs) {
                    Some(p) => {
                        ui.p.circle_stroke(p, 4.0, Stroke::new(1.2, theme::TEXT_FAINT));
                    }
                    None => off_plot += 1,
                }
            }
        }
        if let RootPair::Conjugate { hz, r: rr } = g.pole {
            match place(centre, rho_max, hz, rr, fs) {
                Some(p) => {
                    let w = if sel { 2.4 } else { 1.4 };
                    let s = if sel { 6.0 } else { 4.5 };
                    line(ui.p, p + Vec2::new(-s, -s), p + Vec2::new(s, s), c, w);
                    line(ui.p, p + Vec2::new(-s, s), p + Vec2::new(s, -s), c, w);
                }
                None => off_plot += 1,
            }
        }
    }

    label(
        ui.p,
        r.min,
        "pole ×   zero ○   radial dB",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );
    if off_plot > 0 {
        // Rossum excludes roots below the floor rather than piling them on the
        // rim, so they are counted here instead of drawn somewhere wrong.
        num(
            ui.p,
            Pos2::new(r.left(), r.top() + 14.0),
            format!("{off_plot} below plot floor"),
            theme::T_SMALL,
            theme::WARN,
        );
    }
}
