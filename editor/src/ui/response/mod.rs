use eframe::egui::{Align2, Id, Key, Painter, Pos2, Rect, Sense, Stroke, Ui};

use crate::engine::response::{row_db, SR};
use crate::session::command::Command;
use crate::session::state::{FitState, Session};
use crate::ui::{inspector, navigator, paint, theme, transport};

const BAR_H: f32 = 30.0;
const PAD: f32 = 14.0;
const EDITOR_H: f32 = 200.0;
const THIN_NAV_H: f32 = 24.0;

fn bar_button(
    ui: &mut Ui,
    painter: &Painter,
    x: &mut f32,
    bar: Rect,
    label: &str,
    key: Option<Key>,
) -> bool {
    let w = label.len() as f32 * 6.4 + 14.0;
    let b = Rect::from_min_max(
        Pos2::new(*x, bar.top() + 3.0),
        Pos2::new(*x + w, bar.bottom() - 3.0),
    );
    *x = b.right() + 6.0;
    let inner = paint::raised(painter, b);
    paint::label(
        painter,
        inner.center(),
        Align2::CENTER_CENTER,
        label,
        theme::SMALL,
        theme::INK,
    );
    let resp = ui.interact(b, Id::new(("bar", label)), Sense::click());
    resp.clicked() || key.is_some_and(|k| ui.input(|i| i.key_pressed(k)))
}

pub fn draw(session: &Session, ui: &mut Ui, home: bool) -> (Vec<Command>, bool) {
    let mut cmds = Vec::new();
    let mut back = false;
    let mut legend = String::new();
    let rect = ui.max_rect();
    let painter = ui.painter().clone();
    painter.rect_filled(rect, 0.0, theme::CHROME);
    let focus = session.selection.section;

    let bar = Rect::from_min_max(
        rect.left_top() + eframe::egui::vec2(PAD, PAD),
        Pos2::new(rect.right() - PAD, rect.top() + PAD + BAR_H),
    );
    let row_h = if focus.is_some() {
        THIN_NAV_H
    } else {
        navigator::NAV_H
    };
    let nav_rect = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, rect.bottom() - PAD - row_h),
        Pos2::new(rect.right() - PAD, rect.bottom() - PAD),
    );
    let editor_rect = focus.map(|_| {
        Rect::from_min_max(
            Pos2::new(rect.left() + PAD, nav_rect.top() - 8.0 - EDITOR_H),
            Pos2::new(rect.right() - PAD, nav_rect.top() - 8.0),
        )
    });
    let well_bottom = editor_rect.map(|e| e.top()).unwrap_or(nav_rect.top()) - PAD;
    let well_frame = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, bar.bottom() + PAD),
        Pos2::new(rect.right() - PAD, well_bottom),
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
        painter.line_segment(
            [Pos2::new(p.x, well.top()), Pos2::new(p.x, well.bottom())],
            Stroke::new(1.0, theme::faded(theme::CURSOR, 150)),
        );
    }

    let tb = transport::draw(
        session,
        ui,
        &painter,
        Pos2::new(bar.left(), bar.top() + 3.0),
        &mut cmds,
        &mut legend,
    );
    let mut x = tb.right() + 8.0;
    if home
        && (bar_button(ui, &painter, &mut x, bar, "field", None)
            || (ui.input(|i| i.key_pressed(Key::Escape)) && focus.is_none()))
    {
        back = true;
    }
    let list_id = Id::new("target.list.open");
    let mut list_open = ui
        .memory(|m| m.data.get_temp::<bool>(list_id))
        .unwrap_or(false);
    if bar_button(ui, &painter, &mut x, bar, "target", None) {
        list_open = !list_open;
    }
    let fitting = matches!(session.fit, FitState::Running { .. });
    let fit_label = match focus {
        Some(si) => format!("fit S{}", si + 1),
        None => "fit".to_string(),
    };
    if bar_button(ui, &painter, &mut x, bar, &fit_label, Some(Key::F2)) && !fitting {
        cmds.push(Command::FitSelection);
    }
    if bar_button(ui, &painter, &mut x, bar, "fit poles", Some(Key::F3)) && !fitting {
        cmds.push(Command::FitPoles);
    }
    if bar_button(ui, &painter, &mut x, bar, "keep", Some(Key::F4)) {
        cmds.push(Command::Keep);
    }
    if bar_button(ui, &painter, &mut x, bar, "write", Some(Key::F5)) {
        cmds.push(Command::WriteStatic);
    }
    if bar_button(ui, &painter, &mut x, bar, "undo", Some(Key::F9)) {
        cmds.push(Command::Undo);
    }
    if bar_button(ui, &painter, &mut x, bar, "redo", Some(Key::F10)) {
        cmds.push(Command::Redo);
    }
    if ui.input(|i| i.key_pressed(Key::Escape)) {
        if list_open {
            list_open = false;
        } else if focus.is_some() {
            let mut sel = session.selection;
            sel.section = None;
            cmds.push(Command::Select(sel));
        }
    }
    if list_open {
        enum Row {
            Mouth(usize, String),
            Divider,
            Scaffold(usize, String),
        }
        let mut items: Vec<Row> = session
            .mouths
            .iter()
            .map(|(e, n)| Row::Mouth(*e, n.clone()))
            .collect();
        if !session.scaffolds.is_empty() {
            items.push(Row::Divider);
            for (i, (n, g)) in session.scaffolds.iter().enumerate() {
                let label = if g.is_empty() {
                    n.clone()
                } else {
                    format!("{n} — {g}")
                };
                items.push(Row::Scaffold(i, label));
            }
        }
        if !items.is_empty() {
            let row_h = 18.0;
            let panel = Rect::from_min_max(
                Pos2::new(well.left() + 8.0, well.top() + 8.0),
                Pos2::new(
                    well.left() + 400.0,
                    (well.top() + 14.0 + row_h * items.len() as f32).min(well.bottom() - 8.0),
                ),
            );
            let inner = paint::raised(&painter, panel);
            painter.rect_filled(inner, 0.0, theme::CHROME);
            let scroll_id = Id::new("target.list.scroll");
            let mut offset = ui
                .memory(|m| m.data.get_temp::<f32>(scroll_id))
                .unwrap_or(0.0);
            let visible = ((inner.height() - 4.0) / row_h).floor() as usize;
            let max_off = items.len().saturating_sub(visible) as f32;
            if ui.rect_contains_pointer(panel) {
                let wheel = ui.input(|i| i.raw_scroll_delta.y);
                offset = (offset - wheel / row_h * 0.5).clamp(0.0, max_off);
            }
            ui.memory_mut(|m| m.data.insert_temp(scroll_id, offset));
            let first = offset.floor() as usize;
            for (k, item) in items.iter().enumerate().skip(first).take(visible) {
                let rr = Rect::from_min_max(
                    Pos2::new(inner.left() + 2.0, inner.top() + 2.0 + (k - first) as f32 * row_h),
                    Pos2::new(
                        inner.right() - 2.0,
                        inner.top() + 2.0 + (k - first + 1) as f32 * row_h,
                    ),
                );
                match item {
                    Row::Divider => {
                        painter.line_segment(
                            [
                                Pos2::new(rr.left() + 4.0, rr.center().y),
                                Pos2::new(rr.right() - 4.0, rr.center().y),
                            ],
                            Stroke::new(1.0, theme::CHROME_DK),
                        );
                    }
                    Row::Mouth(entry, name) => {
                        let resp = ui.interact(rr, Id::new(("target.row", k)), Sense::click());
                        if resp.secondary_clicked() {
                            cmds.push(Command::SetTarget(*entry));
                            cmds.push(Command::PlaceSkeleton);
                            list_open = false;
                        }
                        let current = session.selection.entry == Some(*entry);
                        if resp.hovered() {
                            painter.rect_filled(rr, 0.0, theme::CHROME_LT);
                        } else if current {
                            painter.rect_filled(rr, 0.0, theme::CHROME_DK);
                        }
                        paint::label(
                            &painter,
                            Pos2::new(rr.left() + 5.0, rr.center().y),
                            Align2::LEFT_CENTER,
                            name,
                            theme::SMALL,
                            if current && !resp.hovered() {
                                theme::CHROME_LT
                            } else {
                                theme::INK
                            },
                        );
                        if resp.clicked() {
                            cmds.push(Command::SetTarget(*entry));
                            list_open = false;
                        }
                    }
                    Row::Scaffold(i, label) => {
                        let resp = ui.interact(rr, Id::new(("target.row", k)), Sense::click());
                        if resp.hovered() {
                            painter.rect_filled(rr, 0.0, theme::CHROME_LT);
                        }
                        paint::label(
                            &painter,
                            Pos2::new(rr.left() + 5.0, rr.center().y),
                            Align2::LEFT_CENTER,
                            label,
                            theme::SMALL,
                            theme::INK,
                        );
                        if resp.clicked() {
                            cmds.push(Command::ApplyScaffold(*i));
                            list_open = false;
                        }
                    }
                }
            }
        }
    }
    ui.memory_mut(|m| m.data.insert_temp(list_id, list_open));

    let live_err = session.target_pairs().and_then(|pairs| {
        session.current_rows().map(|rows| {
            let mut acc = 0.0;
            for (hz, db) in &pairs {
                let sum: f64 = rows.iter().map(|r| row_db(r, *hz, SR)).sum();
                acc += (db - sum) * (db - sum);
            }
            (acc / pairs.len() as f64).sqrt()
        })
    });
    let mut right = String::new();
    if let Some(ci) = session.selection.corner {
        right.push_str(&format!(
            "editing corner m{} q{}{}  ·  ",
            (ci & 1) * 100,
            ((ci >> 1) & 1) * 100,
            if ci & 4 != 0 { " t" } else { "" }
        ));
    }
    if fitting {
        right.push_str("fitting…  ·  ");
    }
    if let Some(err) = live_err {
        right.push_str(&format!("err {err:.2} dB"));
    }
    if let Some(target) = &session.document.target {
        if !right.is_empty() {
            right.push_str("  ·  ");
        }
        right.push_str(&target.name);
    }
    paint::label(
        &painter,
        Pos2::new(bar.right() - 4.0, bar.center().y),
        Align2::RIGHT_CENTER,
        &right,
        theme::SMALL,
        theme::INK,
    );
    if let Some((err, text)) = &session.notice {
        paint::label(
            &painter,
            Pos2::new(x + 12.0, bar.center().y),
            Align2::LEFT_CENTER,
            text,
            theme::SMALL,
            if *err { theme::ALARM } else { theme::INK },
        );
    }

    if let (Some(si), Some(er)) = (focus, editor_rect) {
        let split = er.left() + (er.width() * 0.42).min(er.width() - 400.0).max(180.0);
        let curve_frame = Rect::from_min_max(er.left_top(), Pos2::new(split - 8.0, er.bottom()));
        let cwell = paint::well(&painter, curve_frame);
        let (preview, _) = session.preview();
        let lane = preview[si];
        if lane.pole_r > 0.0 || lane.zero_r > 0.0 {
            let row = lane.biquad_at(SR);
            let mut slo = f64::INFINITY;
            let mut shi = f64::NEG_INFINITY;
            for i in 0..64 {
                let t = i as f64 / 63.0;
                let f = paint::FREQ_LO * (paint::FREQ_HI / paint::FREQ_LO).powf(t);
                let db = row_db(&row, f, SR);
                slo = slo.min(db);
                shi = shi.max(db);
            }
            slo -= 4.0;
            shi += 4.0;
            if shi - slo < 24.0 {
                let mid = (shi + slo) * 0.5;
                slo = mid - 12.0;
                shi = mid + 12.0;
            }
            for f in [100.0, 1000.0, 10000.0] {
                let gx = paint::log_x(f, cwell);
                painter.line_segment(
                    [Pos2::new(gx, cwell.top()), Pos2::new(gx, cwell.bottom())],
                    Stroke::new(1.0, theme::GRATICULE),
                );
            }
            if (slo..shi).contains(&0.0) {
                let y = paint::db_y(0.0, cwell, slo, shi);
                painter.line_segment(
                    [Pos2::new(cwell.left(), y), Pos2::new(cwell.right(), y)],
                    Stroke::new(1.0, theme::faded(theme::WELL_DIM, 150)),
                );
            }
            let sample = move |hz: f64| -> f64 { row_db(&row, hz, SR) };
            paint::x3_trace(
                &painter.with_clip_rect(cwell),
                cwell,
                slo,
                shi,
                theme::LANES[si % 7],
                &sample,
            );
        }
        let card = Rect::from_min_max(
            Pos2::new(split, er.top()),
            Pos2::new(er.right(), er.top() + inspector::CARD_H),
        );
        inspector::draw_card(session, ui, &painter, card, si, &mut cmds, &mut legend);
        let roots = Rect::from_min_max(Pos2::new(split, card.bottom() + 2.0), er.right_bottom());
        inspector::root_map(session, ui, &painter, roots, &mut cmds, &mut legend, Some(si));
    }

    navigator::draw(session, ui, &painter, nav_rect, well, &mut cmds, &mut legend);
    if !legend.is_empty() {
        paint::label(
            &painter,
            Pos2::new(well.left() + 6.0, well.bottom() - 6.0),
            Align2::LEFT_BOTTOM,
            &legend,
            theme::SMALL,
            theme::faded(theme::CURSOR, 120),
        );
    }
    (cmds, back)
}
