use eframe::egui::{Align2, Id, Pos2, Rect, Sense, Stroke, Ui};

use crate::engine::response::{row_db, SR};
use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{paint, section_row, theme};

const STRIP_H: f32 = 30.0;
const FOOT_H: f32 = 24.0;
const PAD: f32 = 14.0;

pub fn draw(session: &Session, ui: &mut Ui) -> Vec<Command> {
    let mut cmds = Vec::new();
    let mut legend = String::from("L —   M —   R —");
    let rect = ui.max_rect();
    let painter = ui.painter().clone();
    painter.rect_filled(rect, 0.0, theme::CHROME);

    let strip = Rect::from_min_max(
        rect.left_top() + eframe::egui::vec2(PAD, PAD),
        Pos2::new(rect.right() - PAD, rect.top() + PAD + STRIP_H),
    );
    let foot = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, rect.bottom() - PAD - FOOT_H),
        rect.right_bottom() - eframe::egui::vec2(PAD, PAD),
    );
    let row_rect = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, foot.top() - PAD - section_row::ROW_H),
        Pos2::new(rect.right() - PAD, foot.top() - PAD),
    );
    let well_frame = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, strip.bottom() + PAD),
        Pos2::new(rect.right() - PAD, row_rect.top() - PAD),
    );
    let well = paint::well(&painter, well_frame);
    let well_response = ui.interact(well, Id::new("response.well"), Sense::hover());

    let ghost: Vec<(f64, f64)> = session
        .document
        .target
        .as_ref()
        .map(|t| {
            author::envelope::grid()
                .into_iter()
                .zip(t.curve.iter().copied())
                .filter(|(f, _)| (paint::FREQ_LO..=paint::FREQ_HI).contains(f))
                .collect()
        })
        .unwrap_or_default();
    let rows = session.current_rows();

    let mut lo = f64::INFINITY;
    let mut hi = f64::NEG_INFINITY;
    for &(_, db) in &ghost {
        lo = lo.min(db);
        hi = hi.max(db);
    }
    if let Some(rows) = &rows {
        for i in 0..64 {
            let t = i as f64 / 63.0;
            let f = paint::FREQ_LO * (paint::FREQ_HI / paint::FREQ_LO).powf(t);
            let db: f64 = rows.iter().map(|r| row_db(r, f, SR)).sum();
            hi = hi.max(db);
        }
    }
    if !lo.is_finite() || !hi.is_finite() {
        lo = -60.0;
        hi = 24.0;
    }
    lo -= 6.0;
    hi += 6.0;
    if hi - lo < 36.0 {
        let mid = (hi + lo) * 0.5;
        lo = mid - 18.0;
        hi = mid + 18.0;
    }

    for decade in [100.0, 1_000.0, 10_000.0] {
        for mult in 1..10 {
            let hz = decade * mult as f64;
            if !(paint::FREQ_LO..=paint::FREQ_HI).contains(&hz) {
                continue;
            }
            let x = paint::log_x(hz, well);
            let major = mult == 1;
            painter.line_segment(
                [Pos2::new(x, well.top()), Pos2::new(x, well.bottom())],
                Stroke::new(
                    1.0,
                    if major {
                        theme::GRATICULE
                    } else {
                        theme::faded(theme::GRATICULE, 120)
                    },
                ),
            );
            if major {
                let mark = if hz >= 1000.0 {
                    format!("{}k", hz / 1000.0)
                } else {
                    format!("{hz:.0}")
                };
                paint::label(
                    &painter,
                    Pos2::new(x + 3.0, well.bottom() - 2.0),
                    Align2::LEFT_BOTTOM,
                    &mark,
                    theme::SMALL,
                    theme::WELL_DIM,
                );
            }
        }
    }
    let mut db = (lo / 12.0).ceil() * 12.0;
    while db < hi {
        let y = paint::db_y(db, well, lo, hi);
        painter.line_segment(
            [Pos2::new(well.left(), y), Pos2::new(well.right(), y)],
            Stroke::new(
                1.0,
                if db == 0.0 {
                    theme::faded(theme::WELL_DIM, 180)
                } else {
                    theme::GRATICULE
                },
            ),
        );
        if y > well.top() + 12.0 {
            paint::label(
                &painter,
                Pos2::new(well.left() + 3.0, y - 1.0),
                Align2::LEFT_BOTTOM,
                &format!("{db:+.0}"),
                theme::SMALL,
                theme::WELL_DIM,
            );
        }
        db += 12.0;
    }

    let focus = session.selection.section;
    if let Some(si) = focus {
        let law = session.document.workspace.laws[si];
        if law.has_zone() {
            let a = paint::log_x(law.zone[0].max(paint::FREQ_LO), well);
            let b = paint::log_x(law.zone[1].min(paint::FREQ_HI), well);
            let band = Rect::from_min_max(Pos2::new(a, well.top()), Pos2::new(b, well.bottom()));
            painter.rect_filled(band, 0.0, theme::faded(theme::LANES[si % 7], 16));
            for x in [a, b] {
                painter.line_segment(
                    [Pos2::new(x, well.top()), Pos2::new(x, well.bottom())],
                    Stroke::new(1.0, theme::faded(theme::LANES[si % 7], 90)),
                );
            }
        }
    }

    if let Some(target) = &session.document.target {
        let curve = &target.curve;
        let egrid = author::envelope::grid();
        let span = (egrid[egrid.len() - 1] / egrid[0]).ln();
        let ask_sample = move |hz: f64| -> f64 {
            let t = ((hz / egrid[0]).ln() / span).clamp(0.0, 1.0) * (curve.len() - 1) as f64;
            let k = t.floor() as usize;
            let frac = t - k as f64;
            if k + 1 < curve.len() {
                curve[k] * (1.0 - frac) + curve[k + 1] * frac
            } else {
                curve[k]
            }
        };
        paint::x3_trace(&painter.with_clip_rect(well), well, lo, hi, theme::ASK, &ask_sample);
    }
    if let Some(rows) = rows {
        let live_sample = move |hz: f64| -> f64 {
            let w = std::f64::consts::TAU * hz / SR;
            let (cw, sw) = (w.cos() as f32, w.sin() as f32);
            let c2 = cw * cw - sw * sw;
            let s2 = 2.0 * sw * cw;
            let mut power: f32 = 1.0;
            for r in &rows {
                let (b0, b1, b2) = (r[0] as f32, r[1] as f32, r[2] as f32);
                let (a1, a2) = (r[3] as f32, r[4] as f32);
                let nr = b2 * c2 + b1 * cw + b0;
                let ni = b2 * s2 + b1 * sw;
                let dr = a2 * c2 + a1 * cw + 1.0;
                let di = a2 * s2 + a1 * sw;
                power *= (nr * nr + ni * ni) / (dr * dr + di * di);
            }
            10.0 * (power as f64).log10()
        };
        paint::x3_trace(&painter.with_clip_rect(well), well, lo, hi, theme::NOW, &live_sample);
    }
    if let Some(si) = focus {
        let lane = session.active_lanes()[si];
        if lane.pole_r > 0.0 || lane.zero_r > 0.0 {
            let row = lane.biquad_at(SR);
            let stage_sample = move |hz: f64| -> f64 { row_db(&row, hz, SR) };
            paint::x3_trace(
                &painter.with_clip_rect(well),
                well,
                lo,
                hi,
                theme::faded(theme::LANES[si % 7], 200),
                &stage_sample,
            );
        }
    }

    let hover = well_response.hover_pos().filter(|p| well.contains(*p));
    if let Some(p) = hover {
        painter.line_segment(
            [Pos2::new(p.x, well.top()), Pos2::new(p.x, well.bottom())],
            Stroke::new(1.0, theme::faded(theme::CURSOR, 150)),
        );
    }

    let cursor_hz = hover.map(|p| paint::hz_at_x(p.x, well));
    let ask_db = cursor_hz.and_then(|hz| {
        session.document.target.as_ref().map(|target| {
            let g = author::envelope::grid();
            let k = g.iter().position(|&f| f >= hz).unwrap_or(g.len() - 1);
            target.curve[k]
        })
    });
    let now_db = cursor_hz.and_then(|hz| {
        session
            .current_rows()
            .map(|rows| rows.iter().map(|r| row_db(r, hz, SR)).sum::<f64>())
    });

    let fields = [
        ("hz", cursor_hz.map(|v| format!("{v:.0}")), theme::INK),
        ("target", ask_db.map(|v| format!("{v:+.1}")), theme::ASK_INK),
        ("filter", now_db.map(|v| format!("{v:+.1}")), theme::NOW_INK),
    ];
    let mut x = strip.left();
    for (name, value, tint) in fields {
        paint::label(
            &painter,
            Pos2::new(x, strip.center().y),
            Align2::LEFT_CENTER,
            name,
            theme::SMALL,
            theme::INK_DIM,
        );
        x += name.len() as f32 * 7.0 + 6.0;
        let fw = Rect::from_min_max(
            Pos2::new(x, strip.top() + 3.0),
            Pos2::new(x + 74.0, strip.bottom() - 3.0),
        );
        let inner = paint::field(&painter, fw);
        if let Some(v) = value {
            paint::label(
                &painter,
                Pos2::new(inner.right() - 4.0, inner.center().y),
                Align2::RIGHT_CENTER,
                &v,
                theme::BODY,
                tint,
            );
        }
        x = fw.right() + 16.0;
    }

    section_row::draw(
        session,
        ui,
        &painter,
        row_rect,
        focus.unwrap_or(0),
        &mut cmds,
        &mut legend,
    );

    paint::label(
        &painter,
        Pos2::new(foot.left(), foot.center().y),
        Align2::LEFT_CENTER,
        &legend,
        theme::SMALL,
        theme::INK_DIM,
    );
    if let crate::session::state::FitState::Complete { rms_db, .. } = &session.fit {
        paint::label(
            &painter,
            Pos2::new(foot.center().x, foot.center().y),
            Align2::CENTER_CENTER,
            &format!("rms {rms_db:.2}"),
            theme::SMALL,
            theme::INK,
        );
    }
    if let Some(target) = &session.document.target {
        paint::label(
            &painter,
            Pos2::new(foot.right(), foot.center().y),
            Align2::RIGHT_CENTER,
            &target.name,
            theme::SMALL,
            theme::INK,
        );
    }
    cmds
}
