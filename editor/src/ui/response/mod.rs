use eframe::egui::{Align2, Id, Pos2, Rect, Sense, Stroke, Ui};

use crate::engine::response::{row_db, SR};
use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{fkeys, inspector, navigator, paint, theme, transport};

const STRIP_H: f32 = 30.0;
const FOOT_H: f32 = 24.0;
const PAD: f32 = 14.0;

pub fn draw(session: &Session, ui: &mut Ui, home: bool) -> (Vec<Command>, bool) {
    let mut cmds = Vec::new();
    let mut back = false;
    let mut legend = String::from("L —   M —   R —");
    let rect = ui.max_rect();
    let painter = ui.painter().clone();
    painter.rect_filled(rect, 0.0, theme::CHROME);

    let banner = Rect::from_min_max(
        rect.left_top() + eframe::egui::vec2(PAD, PAD),
        Pos2::new(rect.right() - PAD, rect.top() + PAD + paint::BANNER_H),
    );
    let title = format!(
        "TRENCH Response Editing: {}",
        session
            .document
            .target
            .as_ref()
            .map(|t| t.name.as_str())
            .unwrap_or("untitled")
    );
    paint::banner(&painter, banner, &title, concat!("v", env!("CARGO_PKG_VERSION")));
    let strip = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, banner.bottom() + 6.0),
        Pos2::new(rect.right() - PAD, banner.bottom() + 6.0 + STRIP_H),
    );
    let foot = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, rect.bottom() - PAD - FOOT_H),
        rect.right_bottom() - eframe::egui::vec2(PAD, PAD),
    );
    let fkeys_rect = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, foot.top() - 8.0 - fkeys::FKEY_H),
        Pos2::new(rect.right() - PAD, foot.top() - 8.0),
    );
    let panel_rect = Rect::from_min_max(
        Pos2::new(rect.right() - PAD - inspector::PANEL_W, strip.bottom() + PAD),
        Pos2::new(rect.right() - PAD, fkeys_rect.top() - PAD),
    );
    let nav_rect = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, fkeys_rect.top() - PAD - navigator::NAV_H),
        Pos2::new(panel_rect.left() - PAD, fkeys_rect.top() - PAD),
    );
    let well_frame = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, strip.bottom() + PAD),
        Pos2::new(panel_rect.left() - PAD, nav_rect.top() - PAD),
    );
    let well = paint::well(&painter, well_frame);
    let well_response = ui.interact(well, Id::new("response.well"), Sense::click());

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
    for si in 0..trench_core::cascade::NUM_STAGES {
        let law = session.document.workspace.laws[si];
        if !law.has_zone() {
            continue;
        }
        let strong = focus == Some(si);
        let a = paint::log_x(law.zone[0].max(paint::FREQ_LO), well);
        let b = paint::log_x(law.zone[1].min(paint::FREQ_HI), well);
        if strong {
            let band = Rect::from_min_max(Pos2::new(a, well.top()), Pos2::new(b, well.bottom()));
            painter.rect_filled(band, 0.0, theme::faded(theme::LANES[si % 7], 10));
        }
        for x in [a, b] {
            painter.line_segment(
                [Pos2::new(x, well.top()), Pos2::new(x, well.bottom())],
                Stroke::new(1.0, theme::faded(theme::LANES[si % 7], if strong { 100 } else { 34 })),
            );
        }
        painter.line_segment(
            [
                Pos2::new(a, well.bottom() - 3.0),
                Pos2::new(b, well.bottom() - 3.0),
            ],
            Stroke::new(2.0, theme::faded(theme::LANES[si % 7], if strong { 200 } else { 90 })),
        );
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
        let egrid2 = author::envelope::grid();
        let peaks = author::formants::peaks(&egrid2, curve);
        for p in &peaks {
            let x = paint::log_x(p.hz, well);
            let y = paint::db_y(ask_sample(p.hz), well, lo, hi);
            painter.line_segment(
                [Pos2::new(x, y - 5.0), Pos2::new(x, y - 14.0)],
                Stroke::new(1.0, theme::faded(theme::ASK, 170)),
            );
        }
        let inverted: Vec<f64> = curve.iter().map(|v| -v).collect();
        for n in &author::formants::peaks(&egrid2, &inverted) {
            let x = paint::log_x(n.hz, well);
            let y = paint::db_y(ask_sample(n.hz), well, lo, hi);
            if y < well.bottom() - 16.0 {
                painter.line_segment(
                    [Pos2::new(x, y + 5.0), Pos2::new(x, y + 14.0)],
                    Stroke::new(1.0, theme::faded(theme::ASK, 110)),
                );
            }
        }
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

    let (preview_lanes, preview_mask) = session.preview();
    let mut drag_delta: Option<(f64, f64)> = None;
    if let Some(rows) = session.current_rows() {
        let comp_at = |hz: f64| -> f64 { rows.iter().map(|r| row_db(r, hz, SR)).sum() };
        for si in 0..trench_core::cascade::NUM_STAGES {
            let lane = preview_lanes[si];
            let provisional = preview_mask[si];
            let ink = if provisional {
                theme::faded(theme::CURSOR, 140)
            } else {
                theme::LANES[si % 7]
            };
            if lane.pole_r > 0.0 {
                let hx = paint::log_x(lane.pole_hz, well);
                let hy = paint::db_y(
                    comp_at(lane.pole_hz).clamp(lo - 6.0, hi + 6.0),
                    well,
                    lo,
                    hi,
                )
                .clamp(well.top() + 4.0, well.bottom() - 4.0);
                let hrect = Rect::from_center_size(Pos2::new(hx, hy), eframe::egui::vec2(14.0, 14.0));
                painter.circle_filled(Pos2::new(hx, hy), 4.5, ink);
                painter.circle_stroke(Pos2::new(hx, hy), 4.5, Stroke::new(1.0, theme::CURSOR));
                if lane.pole_r >= trench_core::stage_law::max_contiguous_pole_radius() - 1e-6 {
                    painter.circle_stroke(Pos2::new(hx, hy), 7.0, Stroke::new(1.0, theme::ALARM));
                }
                if provisional {
                    continue;
                }
                let resp = ui.interact(hrect, Id::new(("well.pole", si)), Sense::click_and_drag());
                if resp.hovered() {
                    legend = "L drag · wheel width · R remove".into();
                    let wheel = ui.input(|i| i.raw_scroll_delta.y);
                    if wheel != 0.0 {
                        cmds.push(Command::SetPole {
                            section: si,
                            hz: lane.pole_hz,
                            r: (lane.pole_r - wheel as f64 * 0.0002)
                                .clamp(0.05, trench_core::stage_law::max_contiguous_pole_radius()),
                        });
                    }
                }
                if resp.secondary_clicked() {
                    cmds.push(Command::BeginEdit);
                    cmds.push(Command::SetPole { section: si, hz: 0.0, r: 0.0 });
                }
                if resp.drag_started() {
                    cmds.push(Command::BeginEdit);
                    let mut sel = session.selection;
                    sel.section = Some(si);
                    cmds.push(Command::Select(sel));
                    ui.memory_mut(|m| {
                        m.data.insert_temp(Id::new("drag.origin"), (lane.pole_hz, lane.pole_r))
                    });
                }
                if resp.dragged() {
                    if let Some(p) = resp.interact_pointer_pos() {
                        let hz = paint::hz_at_x(p.x, well);
                        let r = (lane.pole_r + resp.drag_delta().y as f64 * -0.0015)
                            .clamp(0.05, trench_core::stage_law::max_contiguous_pole_radius());
                        let (oh, or) = ui
                            .memory(|m| m.data.get_temp(Id::new("drag.origin")))
                            .unwrap_or((hz, r));
                        drag_delta = Some((12.0 * (hz / oh).log2(), r - or));
                        cmds.push(Command::SetPole { section: si, hz, r });
                    }
                }
            }
            if lane.zero_r > 0.0 {
                let hx = paint::log_x(lane.zero_hz, well);
                let hy = paint::db_y(
                    comp_at(lane.zero_hz).clamp(lo - 6.0, hi + 6.0),
                    well,
                    lo,
                    hi,
                )
                .clamp(well.top() + 4.0, well.bottom() - 4.0);
                let hrect = Rect::from_center_size(Pos2::new(hx, hy), eframe::egui::vec2(14.0, 14.0));
                painter.circle_stroke(Pos2::new(hx, hy), 4.5, Stroke::new(1.6, ink));
                if lane.zero_r >= 0.9999 {
                    painter.circle_filled(Pos2::new(hx, hy), 1.8, ink);
                }
                let resp = ui.interact(hrect, Id::new(("well.zero", si)), Sense::click_and_drag());
                if resp.hovered() {
                    legend = "L drag · wheel depth · R remove".into();
                    let wheel = ui.input(|i| i.raw_scroll_delta.y);
                    if wheel != 0.0 {
                        cmds.push(Command::SetZero {
                            section: si,
                            hz: lane.zero_hz,
                            r: (lane.zero_r - wheel as f64 * 0.0002).clamp(0.05, 1.0),
                        });
                    }
                }
                if resp.secondary_clicked() {
                    cmds.push(Command::BeginEdit);
                    cmds.push(Command::SetZero { section: si, hz: 0.0, r: 0.0 });
                }
                if resp.drag_started() {
                    cmds.push(Command::BeginEdit);
                    let mut sel = session.selection;
                    sel.section = Some(si);
                    cmds.push(Command::Select(sel));
                    ui.memory_mut(|m| {
                        m.data.insert_temp(Id::new("drag.origin"), (lane.zero_hz, lane.zero_r))
                    });
                }
                if resp.dragged() {
                    if let Some(p) = resp.interact_pointer_pos() {
                        let hz = paint::hz_at_x(p.x, well);
                        let r = (lane.zero_r + resp.drag_delta().y as f64 * -0.0015)
                            .clamp(0.05, 1.0);
                        let (oh, or) = ui
                            .memory(|m| m.data.get_temp(Id::new("drag.origin")))
                            .unwrap_or((hz, r));
                        drag_delta = Some((12.0 * (hz / oh).log2(), r - or));
                        cmds.push(Command::SetZero { section: si, hz, r });
                    }
                }
            }
        }
    }

    let hover = well_response.hover_pos().filter(|p| well.contains(*p));
    if well_response.double_clicked() {
        if let Some(p) = hover {
            let si = focus.or_else(|| {
                (0..trench_core::cascade::NUM_STAGES)
                    .find(|&si| crate::domain::document::lane_is_empty(&session.active_lanes()[si]))
            });
            if let Some(si) = si {
                cmds.push(Command::BeginEdit);
                cmds.push(Command::SetZero {
                    section: si,
                    hz: paint::hz_at_x(p.x, well),
                    r: 1.0,
                });
                if focus.is_none() {
                    let mut sel = session.selection;
                    sel.section = Some(si);
                    cmds.push(Command::Select(sel));
                }
            }
        }
    }
    if let Some(p) = hover {
        if legend.starts_with("L —") {
            legend = if focus.is_some() {
                "2×L place a zero on the selected section".into()
            } else {
                "2×L place a zero — the next free section takes it".into()
            };
        }
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
        ("Δst", drag_delta.map(|(st, _)| format!("{st:+.2}")), theme::INK),
        ("Δr", drag_delta.map(|(_, dr)| format!("{dr:+.4}")), theme::INK),
    ];
    let mut x = strip.left();
    let tb = transport::draw(
        session,
        ui,
        &painter,
        Pos2::new(x, strip.top() + 3.0),
        &mut cmds,
        &mut legend,
    );
    x = tb.right() + 12.0;
    if home {
        let bb = Rect::from_min_max(
            Pos2::new(x, strip.top() + 3.0),
            Pos2::new(x + 52.0, strip.bottom() - 3.0),
        );
        let bi = paint::raised(&painter, bb);
        paint::label(
            &painter,
            Pos2::new(bi.center().x, bi.center().y),
            Align2::CENTER_CENTER,
            "field",
            theme::SMALL,
            theme::INK,
        );
        let resp = ui.interact(bb, Id::new("fit.back"), Sense::click());
        if resp.hovered() {
            legend = "L back to the field".into();
        }
        if resp.clicked() || (ui.input(|i| i.key_pressed(eframe::egui::Key::Escape)) && focus.is_none()) {
            back = true;
        }
        if let Some(ci) = session.selection.corner {
            paint::label(
                &painter,
                Pos2::new(bb.right() + 10.0, strip.center().y),
                Align2::LEFT_CENTER,
                &format!("m{} q{}{}", (ci & 1) * 100, ((ci >> 1) & 1) * 100,
                    if ci & 4 != 0 { " t" } else { "" }),
                theme::SMALL,
                theme::INK,
            );
        }
        x = bb.right() + 70.0;
    }
    for (name, value, tint) in fields {
        paint::label(
            &painter,
            Pos2::new(x, strip.center().y),
            Align2::LEFT_CENTER,
            &format!("{name}:"),
            theme::SMALL,
            theme::INK_DIM,
        );
        x += name.len() as f32 * 7.0 + 10.0;
        paint::label(
            &painter,
            Pos2::new(x, strip.center().y),
            Align2::LEFT_CENTER,
            &value.unwrap_or_else(|| "—".into()),
            theme::BODY,
            tint,
        );
        x += 82.0;
    }
    let fitting = matches!(session.fit, crate::session::state::FitState::Running { .. });
    let keys = [
        (
            eframe::egui::Key::F1,
            "F1",
            if session.audition.playing { "pause" } else { "play" },
        ),
        (eframe::egui::Key::F2, "F2", "fit"),
        (eframe::egui::Key::F3, "F3", "fit poles"),
        (eframe::egui::Key::F4, "F4", "keep"),
        (eframe::egui::Key::F5, "F5", "write"),
        (eframe::egui::Key::F9, "F9", "undo"),
        (eframe::egui::Key::F10, "F10", "redo"),
    ];
    match fkeys::draw(ui, &painter, fkeys_rect, &keys) {
        Some(0) => cmds.push(Command::TogglePlay),
        Some(1) if !fitting => cmds.push(Command::FitSelection),
        Some(2) if !fitting => cmds.push(Command::FitPoles),
        Some(3) => cmds.push(Command::Keep),
        Some(4) => cmds.push(Command::WriteStatic),
        Some(5) => cmds.push(Command::Undo),
        Some(6) => cmds.push(Command::Redo),
        _ => {}
    }

    if ui.input(|i| i.key_pressed(eframe::egui::Key::Escape)) && focus.is_some() {
        let mut sel = session.selection;
        sel.section = None;
        cmds.push(Command::Select(sel));
    }

    paint::group(&painter, nav_rect.expand(5.0), "sections");
    navigator::draw(session, ui, &painter, nav_rect, well, &mut cmds, &mut legend);
    paint::group(&painter, panel_rect.expand(5.0), "inspector");
    inspector::draw(session, ui, &painter, panel_rect, &mut cmds, &mut legend);

    let fi = paint::sunken(&painter, foot);
    painter.rect_filled(fi, 0.0, theme::CHROME);
    paint::label(
        &painter,
        Pos2::new(fi.left() + 4.0, fi.center().y),
        Align2::LEFT_CENTER,
        &format!("mouse  {legend}"),
        theme::SMALL,
        theme::ECHO,
    );
    if let Some((err, text)) = &session.notice {
        paint::label(
            &painter,
            Pos2::new(fi.center().x, fi.center().y),
            Align2::CENTER_CENTER,
            text,
            theme::SMALL,
            if *err { theme::ALARM } else { theme::INK },
        );
    }
    let mut right = String::new();
    if let crate::session::state::FitState::Complete { rms_db, .. } = &session.fit {
        right.push_str(&format!("rms {rms_db:.2}  ·  "));
    }
    if let Some(target) = &session.document.target {
        right.push_str(&target.name);
    }
    paint::label(
        &painter,
        Pos2::new(fi.right() - 4.0, fi.center().y),
        Align2::RIGHT_CENTER,
        &right,
        theme::SMALL,
        theme::INK,
    );
    (cmds, back)
}
