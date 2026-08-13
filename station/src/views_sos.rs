//! The surgical SOS editor for one corner.
//!
//! Every section of the corner is on screen at the same time, one strip each,
//! because the reason to open a corner is to see what the sections are doing to
//! each other. A strip carries the section's own response, the response of the
//! cascade up to and including it, and the values that produced both.
//!
//! Everything is edited through the stored representation. Pole and zero
//! frequency and radius and the section scale are written with the crate's own
//! validator; bandwidth and Q beside them are readings derived from where the
//! poles sit, not controls, and not a claim about what kind of filter the
//! section is.

use eframe::egui::{Pos2, Rect, Vec2};

use trench_core::stage_law::{authoring_limits_at, RootPair, StageRoots};

use crate::app::Station;
use crate::model::analysis::{self, pole_shape};
use crate::model::lane::{refusal_text, LaneValue};
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{
    self, curve, db_rules, fill, frequency_rules, hairline, outline, text, text_right,
};
use crate::ui::theme;
use crate::ui::widgets::{self, FieldStyle, Scale};

/// Which field of a section a strip edits.
const FIELDS: [(&str, usize); 5] = [
    ("POLE Hz", 0),
    ("POLE r", 1),
    ("ZERO Hz", 2),
    ("ZERO r", 3),
    ("SCALE", 4),
];

pub fn draw(st: &mut Station, ui: &mut Ui, r: Rect) {
    let Some(fi) = st.sos_corner else {
        return;
    };
    if fi >= st.project.frames.len() {
        st.sos_corner = None;
        return;
    }

    fill(ui.p, r, theme::BG);
    outline(ui.p, r, theme::ACCENT);

    let n_frames = st.project.frames.len();
    let color = theme::frame_color(fi, n_frames);
    let sr = st.project.sample_rate();
    let nyquist = sr * 0.5;
    let name = st
        .grammar
        .frame_name(fi, &crate::views::frame_label(st, fi));

    // ── header ───────────────────────────────────────────────────────────
    let id = Id::of("sos");
    paint::chip(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        &format!("C{:02}", fi + 1),
        color,
    );
    text(
        ui.p,
        r.min + Vec2::new(54.0, 5.0),
        format!("SECTIONS  —  {name}"),
        theme::T_BODY,
        theme::INK_HI,
    );
    let frame_resp = analysis::analyse(&st.project.frames[fi].values, sr);
    text(
        ui.p,
        r.min + Vec2::new(54.0, 19.0),
        format!(
            "{} sections   ·   frame peak {:+.2} dB   ·   worst point in the chain {:+.2} dB",
            st.project.lane_count(),
            frame_resp.total_peak_db(),
            frame_resp
                .worst_intermediate()
                .map(|(_, d)| d)
                .unwrap_or(f64::NAN),
        ),
        theme::T_MICRO,
        theme::DIM,
    );

    let close = Rect::from_min_size(
        Pos2::new(r.right() - 84.0, r.top() + 5.0),
        Vec2::new(76.0, 22.0),
    );
    if widgets::button(ui, id.child("close"), close, "CLOSE", true) {
        st.sos_corner = None;
        return;
    }

    hairline(
        ui.p,
        Pos2::new(r.left() + 4.0, r.top() + 34.0),
        Pos2::new(r.right() - 4.0, r.top() + 34.0),
        theme::RULE,
    );

    // ── one strip per section, all of them at once ───────────────────────
    let n = st.project.lane_count();
    if n == 0 {
        return;
    }
    let top = r.top() + 38.0;
    let strip_h = ((r.bottom() - top - 4.0) / n as f32).min(96.0);
    let lim = authoring_limits_at(sr);

    let mut pending: Option<(usize, usize, f64)> = None;
    let mut dissolve: Option<usize> = None;
    let mut swap: Option<(usize, usize)> = None;

    for li in 0..n {
        let y = top + li as f32 * strip_h;
        if y + strip_h > r.bottom() {
            break;
        }
        let strip = Rect::from_min_size(
            Pos2::new(r.left() + 4.0, y),
            Vec2::new(r.width() - 8.0, strip_h - 3.0),
        );
        let selected = li == st.selected_lane;
        let lane_c = theme::frame_color(li, n);
        if widgets::row(ui, id.child(("strip", li)), strip, selected, Some(lane_c)).clicked {
            st.selected_lane = li;
        }

        let lane = st.project.frames[fi].values[li];
        let g = lane.geometry(sr);
        let lane_name = st
            .grammar
            .lane_name(li, &st.project.lanes[li].id.to_string());

        text(
            ui.p,
            strip.min + Vec2::new(8.0, 4.0),
            format!("S{}", li + 1),
            theme::T_BODY,
            lane_c,
        );
        text(
            ui.p,
            strip.min + Vec2::new(8.0, 19.0),
            &lane_name,
            theme::T_MICRO,
            theme::DIM,
        );

        // Permutation: move this section's content against its neighbour, so a
        // deliberate crossing can be authored at this corner only.
        let up = Rect::from_min_size(strip.min + Vec2::new(8.0, 34.0), Vec2::new(22.0, 18.0));
        let dn = Rect::from_min_size(strip.min + Vec2::new(33.0, 34.0), Vec2::new(22.0, 18.0));
        if widgets::button(ui, id.child(("up", li)), up, "^", li > 0) {
            swap = Some((li, li - 1));
        }
        if widgets::button(ui, id.child(("dn", li)), dn, "v", li + 1 < n) {
            swap = Some((li, li + 1));
        }

        // ── the section's own response, and the chain so far ─────────────
        let plot = Rect::from_min_max(
            Pos2::new(strip.left() + 64.0, strip.top() + 4.0),
            Pos2::new(strip.left() + 64.0 + 230.0, strip.bottom() - 4.0),
        );
        fill(ui.p, plot, theme::BG);
        outline(ui.p, plot, theme::RULE);
        let own = &frame_resp.per_lane[li];
        let so_far = &frame_resp.cumulative[li];
        let mut hi = own.iter().cloned().fold(f64::MIN, f64::max);
        let mut lo = own.iter().cloned().fold(f64::MAX, f64::min);
        hi = hi.max(so_far.iter().cloned().fold(f64::MIN, f64::max));
        lo = lo.min(so_far.iter().cloned().fold(f64::MAX, f64::min));
        if !hi.is_finite() || !lo.is_finite() {
            hi = 12.0;
            lo = -60.0;
        }
        lo = lo.max(hi - 90.0);
        let (lo, hi) = (lo - 2.0, hi + 2.0);
        // The full band the object actually runs over, out to Nyquist.
        frequency_rules(ui.p, plot, 20.0, nyquist, false);
        db_rules(ui.p, plot, lo, hi, false);
        curve(
            ui.p,
            plot,
            &frame_resp.grid,
            so_far,
            20.0,
            nyquist,
            lo,
            hi,
            theme::mix(theme::PANEL, theme::INK, 0.55),
            1.0,
        );
        curve(
            ui.p,
            plot,
            &frame_resp.grid,
            own,
            20.0,
            nyquist,
            lo,
            hi,
            lane_c,
            1.6,
        );

        // ── the values behind it ─────────────────────────────────────────
        let roots = lane.roots(sr);
        let fx = plot.right() + 8.0;
        let fw = 104.0;
        match roots {
            Some(rt) => {
                let vals = [rt.pole_hz, rt.pole_r, rt.zero_hz, rt.zero_r, rt.scale];
                for (k, (label, _)) in FIELDS.iter().enumerate() {
                    let b = Rect::from_min_size(
                        Pos2::new(fx + k as f32 * (fw + 4.0), strip.top() + 4.0),
                        Vec2::new(fw, 32.0),
                    );
                    if b.right() > strip.right() - 8.0 {
                        break;
                    }
                    let style = field_style(label, &lim);
                    let fid = id.child(("f", li, k));
                    let mut fs = st.fields.remove(&fid.0).unwrap_or_default();
                    let out = widgets::number_field(ui, fid, b, vals[k], &style, &mut fs, true);
                    st.fields.insert(fid.0, fs);
                    if let Some(v) = out {
                        pending = Some((li, k, v));
                    }
                }

                // Derived readings, stated as derived.
                let ry = strip.top() + 40.0;
                let shape = pole_shape(rt.pole_hz, rt.pole_r, sr);
                let readings = [
                    (
                        "pole width",
                        shape
                            .map(|s| format!("{:.1} Hz", s.bw_hz))
                            .unwrap_or_else(|| "—".into()),
                    ),
                    (
                        "pole Q",
                        shape
                            .map(|s| format!("{:.2}  {:.2} oct", s.q, s.bw_oct))
                            .unwrap_or_else(|| "—".into()),
                    ),
                    (
                        "zero vs pole",
                        if rt.pole_hz > 0.0 && rt.zero_hz > 0.0 && rt.zero_r > 0.0 {
                            format!("{:+.1} st", 12.0 * (rt.zero_hz / rt.pole_hz).log2())
                        } else {
                            "—".into()
                        },
                    ),
                    (
                        "section peak",
                        format!("{:+.2} dB", frame_resp.lane_metrics[li].peak_db),
                    ),
                    (
                        "chain so far",
                        format!("{:+.2} dB", frame_resp.lane_metrics[li].cumulative_peak_db),
                    ),
                ];
                for (k, (kk, vv)) in readings.iter().enumerate() {
                    let x = fx + k as f32 * (fw + 4.0);
                    if x + fw > strip.right() - 8.0 {
                        break;
                    }
                    text(ui.p, Pos2::new(x, ry), *kk, theme::T_MICRO, theme::FAINT);
                    text(
                        ui.p,
                        Pos2::new(x, ry + 11.0),
                        vv,
                        theme::T_MICRO,
                        theme::INK,
                    );
                }
            }
            None => {
                text(
                    ui.p,
                    Pos2::new(fx, strip.top() + 8.0),
                    if lane.is_identity() {
                        "pass-through"
                    } else {
                        "real-axis roots"
                    },
                    theme::T_MICRO,
                    theme::DIM,
                );
                text(
                    ui.p,
                    Pos2::new(fx, strip.top() + 22.0),
                    format!(
                        "words {}",
                        lane.words
                            .iter()
                            .map(|w| format!("{w:04X}"))
                            .collect::<Vec<_>>()
                            .join(" ")
                    ),
                    theme::T_MICRO,
                    theme::FAINT,
                );
                let b =
                    Rect::from_min_size(Pos2::new(fx, strip.top() + 38.0), Vec2::new(120.0, 20.0));
                if widgets::button(ui, id.child(("seed", li)), b, "AUTHOR A POLE", true) {
                    pending = Some((li, 5, 0.0));
                }
            }
        }

        // Zero dissolution: collapse the numerator to the pass-through
        // sentinel, turning a notch into a plain all-pole section.
        let dz = Rect::from_min_size(
            Pos2::new(strip.right() - 118.0, strip.bottom() - 24.0),
            Vec2::new(110.0, 20.0),
        );
        let zero_alive = matches!(g.zero, RootPair::Conjugate { r, .. } if r > 0.001);
        if widgets::button(ui, id.child(("dz", li)), dz, "DISSOLVE ZERO", zero_alive) {
            dissolve = Some(li);
        }
    }

    // ── apply, once, outside the drawing loop ────────────────────────────
    if let Some((li, which, v)) = pending {
        st.checkpoint();
        let lane = st.project.frames[fi].values[li];
        let mut lv = lane;
        let base = lane.roots(sr).unwrap_or(StageRoots {
            pole_hz: 1000.0,
            pole_r: 0.9,
            zero_hz: 1000.0,
            zero_r: 0.5,
            scale: 1.0,
        });
        let mut next = base;
        match which {
            0 => next.pole_hz = v,
            1 => next.pole_r = v,
            2 => next.zero_hz = v,
            3 => next.zero_r = v,
            4 => next.scale = v,
            _ => {}
        }
        match lv.set_roots(&next, sr) {
            Ok(()) => {
                st.project.frames[fi].values[li] = lv;
                st.touch();
                st.say("", false);
            }
            Err(e) => st.say(refusal_text(e), true),
        }
    }

    if let Some(li) = dissolve {
        st.checkpoint();
        let lane = st.project.frames[fi].values[li];
        match lane.roots(sr) {
            Some(rt) => {
                let mut lv = lane;
                let mut next = rt;
                next.zero_r = 0.0;
                match lv.set_roots(&next, sr) {
                    Ok(()) => {
                        st.project.frames[fi].values[li] = lv;
                        st.touch();
                        st.say("zero dissolved: section is now all-pole", false);
                    }
                    Err(e) => st.say(refusal_text(e), true),
                }
            }
            None => {
                st.project.frames[fi].values[li] = LaneValue::IDENTITY;
                st.touch();
                st.say("section set to the pass-through sentinel", false);
            }
        }
    }

    if let Some((a, b)) = swap {
        st.checkpoint();
        if st.project.swap_lanes_at_frame(fi, a, b) {
            st.touch();
            st.say(
                format!("S{} and S{} exchanged at this corner only", a + 1, b + 1),
                false,
            );
        }
    }

    text_right(
        ui.p,
        Pos2::new(r.right() - 8.0, r.top() + 19.0),
        "double-click to type  ·  drag to sweep  ·  shift for fine",
        theme::T_MICRO,
        theme::FAINT,
    );
}

fn field_style(label: &'static str, lim: &trench_core::stage_law::AuthoringLimits) -> FieldStyle {
    match label {
        "POLE Hz" | "ZERO Hz" => FieldStyle {
            label,
            unit: "",
            decimals: 1,
            step: 0.004,
            lo: lim.display_freq_min_hz,
            hi: lim.authoring_freq_max_hz,
            scale: Scale::Log,
        },
        "POLE r" => FieldStyle {
            label,
            unit: "",
            decimals: 4,
            step: 0.0008,
            lo: 0.0,
            hi: lim.pole_radius_max,
            scale: Scale::Linear,
        },
        "ZERO r" => FieldStyle {
            label,
            unit: "",
            decimals: 4,
            step: 0.0008,
            lo: 0.0,
            hi: lim.zero_radius_max,
            scale: Scale::Linear,
        },
        _ => FieldStyle {
            label,
            unit: "",
            decimals: 4,
            step: 0.002,
            lo: lim.scale_min,
            hi: lim.scale_max,
            scale: Scale::Linear,
        },
    }
}
