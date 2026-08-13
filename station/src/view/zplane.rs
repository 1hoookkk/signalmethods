//! The pole/zero surface.
//!
//! The upper half of the unit disc: angle is frequency, distance from the
//! origin is radius. A section's pole and zero are dragged here directly, and
//! the writes go through the crate's validator, so a placement the encoder
//! could not store is refused rather than rounded into something else.

use eframe::egui::{Pos2, Rect, Stroke, Vec2};

use trench_core::stage_law::{RootPair, StageRoots};

use crate::app::{NoteKind, Station};
use crate::model::lane::{refusal_text, LaneValue};
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{hairline, label, num};
use crate::ui::theme;

/// What the pointer is holding.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Grab {
    Pole,
    Zero,
}

fn to_screen(disc: Rect, radius_px: f32, hz: f64, r: f64, sr: f64) -> Pos2 {
    let theta = std::f64::consts::PI * (hz / (sr * 0.5)).clamp(0.0, 1.0);
    let origin = Pos2::new(disc.center().x, disc.bottom());
    Pos2::new(
        origin.x + (r * theta.cos()) as f32 * radius_px,
        origin.y - (r * theta.sin()) as f32 * radius_px,
    )
}

fn from_screen(disc: Rect, radius_px: f32, p: Pos2, sr: f64) -> (f64, f64) {
    let origin = Pos2::new(disc.center().x, disc.bottom());
    let dx = ((p.x - origin.x) / radius_px) as f64;
    let dy = ((origin.y - p.y) / radius_px).max(0.0) as f64;
    let r = (dx * dx + dy * dy).sqrt().clamp(0.0, 0.999_9);
    let theta = dy.atan2(dx).clamp(0.0, std::f64::consts::PI);
    let hz = theta / std::f64::consts::PI * (sr * 0.5);
    (hz, r)
}

/// Draws the surface for one section and applies any drag to it.
pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect, corner: usize, lane: usize) {
    let sr = st.project.sample_rate();
    let Some(frame) = st.project.frames().get(corner) else {
        return;
    };
    let value = frame.values[lane];

    let radius_px = (r.width() * 0.46).min(r.height() - 26.0).max(30.0);
    let disc = Rect::from_min_max(
        Pos2::new(r.center().x - radius_px, r.bottom() - radius_px - 18.0),
        Pos2::new(r.center().x + radius_px, r.bottom() - 18.0),
    );
    let origin = Pos2::new(disc.center().x, disc.bottom());

    // The unit circle, as an arc, plus radius rings and frequency spokes.
    for ring in [0.25f32, 0.5, 0.75, 1.0] {
        let n = 64;
        let mut prev: Option<Pos2> = None;
        for i in 0..=n {
            let t = i as f32 / n as f32 * std::f32::consts::PI;
            let p = Pos2::new(
                origin.x + t.cos() * radius_px * ring,
                origin.y - t.sin() * radius_px * ring,
            );
            if let Some(q) = prev {
                crate::ui::paint::line(
                    ui.p,
                    q,
                    p,
                    if ring >= 1.0 {
                        theme::LINE_HI
                    } else {
                        theme::mix(theme::SURFACE, theme::LINE, 0.7)
                    },
                    if ring >= 1.0 { 1.4 } else { 1.0 },
                );
            }
            prev = Some(p);
        }
    }
    hairline(
        ui.p,
        Pos2::new(origin.x - radius_px, origin.y),
        Pos2::new(origin.x + radius_px, origin.y),
        theme::LINE,
    );
    // Frequency spokes at decade-ish landmarks.
    for hz in [100.0f64, 1_000.0, 10_000.0] {
        if hz >= sr * 0.5 {
            continue;
        }
        let theta = std::f64::consts::PI * hz / (sr * 0.5);
        let p = Pos2::new(
            origin.x + theta.cos() as f32 * radius_px,
            origin.y - theta.sin() as f32 * radius_px,
        );
        hairline(
            ui.p,
            origin,
            p,
            theme::mix(theme::SURFACE, theme::LINE, 0.5),
        );
        num(
            ui.p,
            p + Vec2::new(-8.0, -14.0),
            if hz >= 1000.0 {
                format!("{:.0}k", hz / 1000.0)
            } else {
                format!("{hz:.0}")
            },
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
    }

    let Some(roots) = value.roots(sr) else {
        // Nothing conjugate to place. A click puts a root on the surface where
        // the pointer is, which is how a section starts carrying signal.
        let id = Id::of(("zplane-empty", corner, lane));
        let resp = ui.region(id, disc);
        if let (true, Some(p)) = (resp.clicked, resp.pointer) {
            let (hz, rr) = from_screen(disc, radius_px, p, sr);
            let seed = StageRoots {
                pole_hz: hz.max(20.0),
                pole_r: rr.clamp(0.0, 0.98),
                zero_hz: hz.max(20.0),
                zero_r: 0.0,
                scale: 1.0,
            };
            apply(st, corner, lane, seed);
        }
        label(
            ui.p,
            Pos2::new(disc.left(), disc.top() - 18.0),
            "empty",
            theme::T_SMALL,
            theme::TEXT_FAINT,
        );
        return;
    };

    let pole_at = to_screen(disc, radius_px, roots.pole_hz, roots.pole_r, sr);
    let zero_at = to_screen(disc, radius_px, roots.zero_hz, roots.zero_r, sr);

    // Drag targets. The pole is on top, since it is the one usually moved.
    let id = Id::of(("zplane", corner, lane));
    let zr = ui.region(
        id.child("zero"),
        Rect::from_center_size(zero_at, Vec2::splat(20.0)),
    );
    let pr = ui.region(
        id.child("pole"),
        Rect::from_center_size(pole_at, Vec2::splat(20.0)),
    );

    let mut next = roots;
    let mut changed = false;
    if pr.held {
        if let Some(p) = pr.pointer {
            let (hz, rr) = from_screen(disc, radius_px, p, sr);
            next.pole_hz = hz.max(1.0);
            next.pole_r = rr;
            changed = true;
        }
    } else if zr.held {
        if let Some(p) = zr.pointer {
            let (hz, rr) = from_screen(disc, radius_px, p, sr);
            next.zero_hz = hz.max(1.0);
            next.zero_r = rr;
            changed = true;
        }
    }

    // Zero, hollow.
    ui.p.circle_stroke(zero_at, 6.0, Stroke::new(1.8, theme::TEXT_DIM));
    // Pole, a filled cross.
    let c = if pr.held || pr.hovered {
        theme::ACCENT
    } else {
        theme::corner_color(corner)
    };
    crate::ui::paint::line(
        ui.p,
        pole_at + Vec2::new(-6.0, -6.0),
        pole_at + Vec2::new(6.0, 6.0),
        c,
        2.2,
    );
    crate::ui::paint::line(
        ui.p,
        pole_at + Vec2::new(-6.0, 6.0),
        pole_at + Vec2::new(6.0, -6.0),
        c,
        2.2,
    );

    label(
        ui.p,
        Pos2::new(disc.left(), disc.top() - 18.0),
        "pole ×    zero ○",
        theme::T_SMALL,
        theme::TEXT_FAINT,
    );

    if changed {
        apply(st, corner, lane, next);
    }
}

fn apply(st: &mut Station, corner: usize, lane: usize, roots: StageRoots) {
    let sr = st.project.sample_rate();
    let Some(obj) = st.project.object.as_ref() else {
        return;
    };
    let mut v: LaneValue = obj.frames[corner].values[lane];
    // Only checkpoint when the write will actually land.
    match v.set_roots(&roots, sr) {
        Ok(()) => {
            st.checkpoint();
            if let Some(o) = st.project.object.as_mut() {
                o.frames[corner].values[lane] = v;
            }
            st.touch();
            st.say("", NoteKind::Plain);
        }
        Err(e) => st.say(refusal_text(e), NoteKind::Warn),
    }
}

/// Reads a section's roots, when it has them.
pub fn roots_of(st: &Station, corner: usize, lane: usize) -> Option<StageRoots> {
    let sr = st.project.sample_rate();
    st.project.frames().get(corner)?.values.get(lane)?.roots(sr)
}

/// True when the section holds a conjugate pole.
pub fn has_pole(st: &Station, corner: usize, lane: usize) -> bool {
    let sr = st.project.sample_rate();
    st.project
        .frames()
        .get(corner)
        .and_then(|f| f.values.get(lane))
        .map(|v| matches!(v.geometry(sr).pole, RootPair::Conjugate { r, .. } if r > 0.0))
        .unwrap_or(false)
}
