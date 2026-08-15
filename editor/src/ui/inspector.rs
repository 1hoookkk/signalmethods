use eframe::egui::{Align2, Color32, CursorIcon, Id, Painter, Pos2, Rect, Sense, Stroke, Ui};

use trench_core::stage_law::max_contiguous_pole_radius;

use crate::engine::response::SR;
use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{paint, theme};

pub const CARD_H: f32 = 76.0;

const BW_MIN: f64 = 8.0;
const BW_MAX: f64 = 6000.0;

fn bw_of(r: f64) -> f64 {
    -r.ln() * SR / std::f64::consts::PI
}

fn r_of(bw: f64) -> f64 {
    (-std::f64::consts::PI * bw / SR).exp()
}

fn log_frac(hz: f64) -> f32 {
    (((hz.max(paint::FREQ_LO) / paint::FREQ_LO).ln()
        / (paint::FREQ_HI / paint::FREQ_LO).ln()) as f32)
        .clamp(0.0, 1.0)
}

fn bw_frac(bw: f64) -> f32 {
    (((bw.max(BW_MIN) / BW_MIN).ln() / (BW_MAX / BW_MIN).ln()) as f32).clamp(0.0, 1.0)
}

fn fmt_hz(hz: f64) -> String {
    if hz >= 9999.5 {
        format!("{:.1}k", hz / 1000.0)
    } else if hz >= 999.5 {
        format!("{:.2}k", hz / 1000.0)
    } else {
        format!("{hz:.0}")
    }
}

struct Scrub {
    began: bool,
    dx: f32,
    wheel: f32,
    hovered: bool,
}

fn scrub_field(
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    id: Id,
    text: &str,
    tint: Color32,
    frac: Option<f32>,
) -> Scrub {
    let inner = paint::field(painter, rect);
    let resp = ui
        .interact(rect, id, Sense::click_and_drag())
        .on_hover_cursor(CursorIcon::ResizeHorizontal);
    paint::label(
        painter,
        Pos2::new(inner.right() - 4.0, inner.center().y),
        Align2::RIGHT_CENTER,
        text,
        theme::SMALL,
        tint,
    );
    if let Some(t) = frac {
        let x = inner.left() + t.clamp(0.0, 1.0) * inner.width();
        painter.line_segment(
            [Pos2::new(x, inner.bottom() - 3.0), Pos2::new(x, inner.bottom())],
            Stroke::new(2.0, theme::INK_DIM),
        );
    }
    Scrub {
        began: resp.drag_started(),
        dx: if resp.dragged() { resp.drag_delta().x } else { 0.0 },
        wheel: if resp.hovered() {
            ui.input(|i| i.raw_scroll_delta.y)
        } else {
            0.0
        },
        hovered: resp.hovered(),
    }
}

fn toggle(
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    id: Id,
    mark: &str,
    on: bool,
) -> (bool, bool) {
    let inner = if on {
        let inner = paint::sunken(painter, rect);
        painter.rect_filled(inner, 0.0, theme::CHROME_DK);
        inner
    } else {
        paint::raised(painter, rect)
    };
    paint::label(
        painter,
        Pos2::new(inner.center().x, inner.center().y),
        Align2::CENTER_CENTER,
        mark,
        theme::SMALL,
        if on { theme::HOT } else { theme::INK_DIM },
    );
    let resp = ui.interact(rect, id, Sense::click());
    (resp.clicked(), resp.hovered())
}

fn fmt_hz2(hz: f64) -> String {
    if hz >= 999.5 {
        format!("{:.1}k", hz / 1000.0)
    } else {
        format!("{hz:.0}")
    }
}

pub fn root_map(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
    legend: &mut String,
    only: Option<usize>,
) {
    use trench_core::cascade::NUM_STAGES;
    paint::label(
        painter,
        Pos2::new(rect.left() + 2.0, rect.top() + 6.0),
        Align2::LEFT_CENTER,
        "roots",
        theme::SMALL,
        theme::INK_DIM,
    );
    let map = Rect::from_min_max(
        Pos2::new(rect.left() + 4.0, rect.top() + 18.0),
        Pos2::new(rect.right() - 4.0, rect.bottom() - 4.0),
    );
    let well = paint::well(painter, map);
    let ceiling = max_contiguous_pole_radius();
    let rdb = |r: f64| 20.0 * (1.0 / (1.0 - r.min(0.999_999))).log10();
    let top_db = rdb(ceiling) + 6.0;
    let y_of = |r: f64| {
        (well.bottom() - (rdb(r.max(0.0)) / top_db) as f32 * well.height())
            .clamp(well.top() + 4.0, well.bottom() - 2.0)
    };
    let r_at = |y: f32| {
        let db = (((well.bottom() - y) / well.height()) as f64 * top_db).max(0.0);
        1.0 - 10f64.powf(-db / 20.0)
    };
    for f in [100.0, 1000.0, 10000.0] {
        let x = paint::log_x(f, well);
        painter.line_segment(
            [Pos2::new(x, well.top()), Pos2::new(x, well.bottom())],
            Stroke::new(1.0, theme::GRATICULE),
        );
    }
    let step = if well.height() < 90.0 {
        48.0
    } else if well.height() < 170.0 {
        24.0
    } else {
        12.0
    };
    let mut db = step;
    while db < top_db {
        let y = well.bottom() - (db / top_db) as f32 * well.height();
        painter.line_segment(
            [Pos2::new(well.left(), y), Pos2::new(well.right(), y)],
            Stroke::new(1.0, theme::GRATICULE),
        );
        if y > well.top() + 10.0 {
            paint::label(
                painter,
                Pos2::new(well.left() + 3.0, y - 1.0),
                Align2::LEFT_BOTTOM,
                &format!("{db:.0}"),
                theme::SMALL,
                theme::WELL_DIM,
            );
        }
        db += step;
    }
    let cy = well.bottom() - (rdb(ceiling) / top_db) as f32 * well.height();
    painter.line_segment(
        [Pos2::new(well.left(), cy), Pos2::new(well.right(), cy)],
        Stroke::new(1.0, theme::faded(theme::ALARM, 110)),
    );
    let (preview, mask) = session.preview();
    let pointer = ui.input(|i| i.pointer.interact_pos());
    let ctrl = ui.input(|i| i.modifiers.ctrl || i.modifiers.command);
    let mut repairing: Option<usize> = None;
    let mut zero_dots: Vec<(usize, Pos2)> = Vec::new();
    for si in 0..NUM_STAGES {
        let lane = preview[si];
        if lane.zero_r > 0.0 {
            zero_dots.push((
                si,
                Pos2::new(paint::log_x(lane.zero_hz, well), y_of(lane.zero_r)),
            ));
        }
    }
    for si in 0..NUM_STAGES {
        let lane = preview[si];
        if only.is_some_and(|o| o != si) {
            if lane.zero_r > 0.0 {
                painter.circle_stroke(
                    Pos2::new(paint::log_x(lane.zero_hz, well), y_of(lane.zero_r)),
                    3.0,
                    Stroke::new(1.0, theme::faded(theme::LANES[si % 7], 70)),
                );
            }
            continue;
        }
        let provisional = mask[si];
        let selected = session.selection.section == Some(si);
        let ink = if provisional {
            theme::faded(theme::CURSOR, 110)
        } else {
            theme::LANES[si % 7]
        };
        let has_pole = lane.pole_r > 0.0;
        let has_zero = lane.zero_r > 0.0;
        if !has_pole && !has_zero {
            continue;
        }
        let pp = Pos2::new(paint::log_x(lane.pole_hz, well), y_of(lane.pole_r));
        let zp = Pos2::new(paint::log_x(lane.zero_hz, well), y_of(lane.zero_r));
        let tag = |hz: f64, r: f64| format!("{} · {:.0}", fmt_hz2(hz), rdb(r));
        let lp = painter.with_clip_rect(well);
        let tag_at = |p: Pos2, above: bool, text: &str, color: Color32| {
            let (x, anchor) = if p.x > well.right() - 76.0 {
                (p.x - 8.0, Align2::RIGHT_CENTER)
            } else {
                (p.x + 8.0, Align2::LEFT_CENTER)
            };
            let y = (if above { p.y - 10.0 } else { p.y + 10.0 })
                .clamp(well.top() + 6.0, well.bottom() - 6.0);
            paint::label(&lp, Pos2::new(x, y), anchor, text, theme::SMALL, color);
        };
        if has_pole && has_zero {
            painter.line_segment(
                [pp, zp],
                Stroke::new(
                    if selected { 2.0 } else { 1.2 },
                    theme::faded(ink, if selected { 255 } else { 170 }),
                ),
            );
        }
        if has_pole {
            painter.circle_filled(pp, if selected { 5.0 } else { 4.0 }, ink);
            if lane.pole_r >= ceiling - 1e-6 {
                painter.circle_stroke(pp, 7.0, Stroke::new(1.0, theme::ALARM));
            }
            let prect = Rect::from_center_size(pp, eframe::egui::vec2(14.0, 14.0));
            if !provisional {
                let resp =
                    ui.interact(prect, Id::new(("map.pole", si)), Sense::click_and_drag());
                if resp.hovered() {
                    *legend = "L select · L drag — frequency and resonance".into();
                    tag_at(pp, true, &tag(lane.pole_hz, lane.pole_r), theme::CURSOR);
                }
                if resp.clicked() {
                    let mut sel = session.selection;
                    sel.section = if selected { None } else { Some(si) };
                    cmds.push(Command::Select(sel));
                }
                if resp.drag_started() {
                    cmds.push(Command::BeginEdit);
                    let mut sel = session.selection;
                    sel.section = Some(si);
                    cmds.push(Command::Select(sel));
                }
                if resp.dragged() {
                    if let Some(p) = resp.interact_pointer_pos() {
                        cmds.push(Command::SetPole {
                            section: si,
                            hz: paint::hz_at_x(p.x, well),
                            r: r_at(p.y).clamp(0.05, ceiling),
                        });
                    }
                }
            }
        }
        if has_zero {
            painter.circle_stroke(zp, if selected { 5.0 } else { 4.0 }, Stroke::new(1.6, ink));
            if lane.zero_r >= 0.9999 {
                painter.circle_filled(zp, 1.8, ink);
            }
            let zrect = Rect::from_center_size(zp, eframe::egui::vec2(14.0, 14.0));
            if !provisional {
                let resp =
                    ui.interact(zrect, Id::new(("map.zero", si)), Sense::click_and_drag());
                if resp.hovered() {
                    *legend =
                        "L drag — frequency and depth, top edge is the null · ctrl-drag re-pair"
                            .into();
                    tag_at(zp, false, &tag(lane.zero_hz, lane.zero_r), theme::CURSOR);
                }
                if resp.clicked() {
                    let mut sel = session.selection;
                    sel.section = if selected { None } else { Some(si) };
                    cmds.push(Command::Select(sel));
                }
                if resp.drag_started() && !ctrl {
                    cmds.push(Command::BeginEdit);
                    let mut sel = session.selection;
                    sel.section = Some(si);
                    cmds.push(Command::Select(sel));
                }
                if resp.dragged() {
                    if ctrl {
                        repairing = Some(si);
                        ui.memory_mut(|m| m.data.insert_temp(Id::new("map.repair"), si));
                    } else if let Some(p) = resp.interact_pointer_pos() {
                        let r = if p.y < well.top() + 8.0 {
                            1.0
                        } else {
                            r_at(p.y).clamp(0.05, 1.0)
                        };
                        cmds.push(Command::SetZero {
                            section: si,
                            hz: paint::hz_at_x(p.x, well),
                            r,
                        });
                    }
                }
                if resp.drag_stopped() {
                    let was = ui.memory(|m| m.data.get_temp::<usize>(Id::new("map.repair")));
                    ui.memory_mut(|m| m.data.remove::<usize>(Id::new("map.repair")));
                    if was == Some(si) {
                        if let Some(p) = pointer {
                            let target = zero_dots
                                .iter()
                                .filter(|(ti, _)| *ti != si)
                                .min_by(|a, b| {
                                    a.1.distance(p).total_cmp(&b.1.distance(p))
                                });
                            if let Some(&(ti, at)) = target {
                                if at.distance(p) < 40.0 {
                                    cmds.push(Command::BeginEdit);
                                    cmds.push(Command::SwapZeros { a: si, b: ti });
                                }
                            }
                        }
                    }
                }
            }
        }
        if selected {
            if has_pole {
                tag_at(pp, true, &tag(lane.pole_hz, lane.pole_r), ink);
            }
            if has_zero {
                tag_at(zp, false, &tag(lane.zero_hz, lane.zero_r), ink);
            }
        }
    }
    if let (Some(si), Some(p)) = (repairing, pointer) {
        let ink = theme::LANES[si % 7];
        painter.circle_stroke(p, 4.5, Stroke::new(1.6, ink));
        if let Some(&(_, at)) = zero_dots
            .iter()
            .filter(|(ti, _)| *ti != si)
            .min_by(|a, b| a.1.distance(p).total_cmp(&b.1.distance(p)))
        {
            if at.distance(p) < 40.0 {
                painter.circle_stroke(at, 8.0, Stroke::new(1.0, theme::faded(ink, 200)));
                *legend = "release: swap zero pairs".into();
            }
        }
    }
}


pub fn draw_card(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    si: usize,
    cmds: &mut Vec<Command>,
    legend: &mut String,
) {
    let ink = theme::LANES[si % 7];
    painter.rect_filled(rect, 0.0, theme::CHROME);
    let inner = paint::raised(painter, rect);
    let (preview, mask) = session.preview();
    let lane = preview[si];
    let provisional = mask[si];
    let law = session.document.workspace.laws[si];
    let selected = session.selection.section == Some(si);
    let have_pole = !provisional && lane.pole_r > 0.0;
    let have_zero = !provisional && lane.zero_r > 0.0;
    let idle = !have_pole && !have_zero;
    let ceiling = max_contiguous_pole_radius();
    let value_ink = if idle { theme::INK_DIM } else { theme::INK };

    let l1 = inner.top() + 12.0;
    let l2 = inner.top() + 35.0;
    let l3 = inner.top() + 58.0;
    let line = |y: f32, x: f32, w: f32| {
        Rect::from_min_max(Pos2::new(x, y - 9.0), Pos2::new(x + w, y + 9.0))
    };

    let swatch = Rect::from_min_max(
        Pos2::new(inner.left() + 6.0, l1 - 7.0),
        Pos2::new(inner.left() + 15.0, l1 + 7.0),
    );
    painter.rect_filled(
        swatch,
        0.0,
        if selected { ink } else { theme::faded(ink, if idle { 60 } else { 130 }) },
    );
    painter.rect_stroke(swatch, 0.0, Stroke::new(1.0, theme::CHROME_DEEP));
    paint::label(
        painter,
        Pos2::new(swatch.right() + 6.0, l1),
        Align2::LEFT_CENTER,
        &format!("{}", si + 1),
        theme::BODY,
        if selected { theme::INK } else { theme::INK_DIM },
    );
    let sel_resp = ui.interact(
        Rect::from_min_max(inner.left_top(), Pos2::new(swatch.right() + 20.0, inner.bottom())),
        Id::new(("section.select", si)),
        Sense::click(),
    );
    if sel_resp.clicked() {
        let mut sel = session.selection;
        sel.section = if selected { None } else { Some(si) };
        cmds.push(Command::Select(sel));
    }
    if sel_resp.hovered() {
        *legend = "L select".into();
    }
    if selected {
        painter.line_segment(
            [
                Pos2::new(inner.left() + 1.0, inner.top() + 1.0),
                Pos2::new(inner.left() + 1.0, inner.bottom() - 1.0),
            ],
            Stroke::new(2.0, ink),
        );
    }

    let hb = Rect::from_min_max(
        Pos2::new(inner.right() - 45.0, l1 - 9.0),
        Pos2::new(inner.right() - 5.0, l1 + 9.0),
    );
    let (clicked, hov) = toggle(ui, painter, hb, Id::new(("section.hold", si)), "hold", !law.writable);
    if hov {
        *legend = "L hold — fitter may not rewrite".into();
    }
    if clicked {
        let mut l = law;
        l.writable = !l.writable;
        cmds.push(Command::BeginEdit);
        cmds.push(Command::SetLaw { section: si, law: l });
    }

    let mut x = inner.left() + 10.0;
    paint::label(painter, Pos2::new(x, l2), Align2::LEFT_CENTER, "pole", theme::SMALL, theme::INK_DIM);
    x += 34.0;
    let f = line(l2, x, 62.0);
    let s = scrub_field(ui, painter, f, Id::new(("section.pole.hz", si)),
        &(if have_pole { fmt_hz(lane.pole_hz) } else { "—".into() }), value_ink,
        have_pole.then(|| log_frac(lane.pole_hz)));
    if s.hovered {
        *legend = "L scrub · wheel fine".into();
    }
    if have_pole && law.freedom[0] && (s.dx != 0.0 || s.wheel != 0.0) {
        if s.began {
            cmds.push(Command::BeginEdit);
        }
        let factor = 2f64.powf(s.dx as f64 / 220.0 + s.wheel as f64 * 0.0004);
        cmds.push(Command::SetPole {
            section: si,
            hz: (lane.pole_hz * factor).clamp(paint::FREQ_LO, SR * 0.49),
            r: lane.pole_r,
        });
    }
    x = f.right() + 6.0;
    let f = line(l2, x, 52.0);
    let s = scrub_field(ui, painter, f, Id::new(("section.pole.bw", si)),
        &(if have_pole { format!("{:.0}", bw_of(lane.pole_r)) } else { "—".into() }), value_ink,
        have_pole.then(|| bw_frac(bw_of(lane.pole_r))));
    if s.hovered {
        *legend = "L scrub · wheel fine".into();
    }
    if have_pole && law.freedom[1] && (s.dx != 0.0 || s.wheel != 0.0) {
        if s.began {
            cmds.push(Command::BeginEdit);
        }
        let factor = 2f64.powf(s.dx as f64 / 220.0 + s.wheel as f64 * 0.0004);
        let bw = (bw_of(lane.pole_r) * factor).clamp(BW_MIN, BW_MAX);
        cmds.push(Command::SetPole {
            section: si,
            hz: lane.pole_hz,
            r: r_of(bw).min(ceiling),
        });
    }
    x = f.right() + 5.0;
    let t = line(l2, x, 24.0);
    let (clicked, hov) = toggle(ui, painter, t, Id::new(("section.pole.lock_at", si)), "at", !law.freedom[0]);
    if hov {
        *legend = "L lock frequency".into();
    }
    if clicked {
        let mut l = law;
        l.freedom[0] = !l.freedom[0];
        cmds.push(Command::BeginEdit);
        cmds.push(Command::SetLaw { section: si, law: l });
    }
    let x2 = t.right() + 3.0;
    let t = line(l2, x2, 24.0);
    let (clicked, hov) = toggle(ui, painter, t, Id::new(("section.pole.lock_bw", si)), "bw", !law.freedom[1]);
    if hov {
        *legend = "L lock width".into();
    }
    if clicked {
        let mut l = law;
        l.freedom[1] = !l.freedom[1];
        cmds.push(Command::BeginEdit);
        cmds.push(Command::SetLaw { section: si, law: l });
    }

    let mut x = inner.left() + 10.0;
    paint::label(painter, Pos2::new(x, l3), Align2::LEFT_CENTER, "zero", theme::SMALL, theme::INK_DIM);
    x += 34.0;
    let f = line(l3, x, 62.0);
    let s = scrub_field(ui, painter, f, Id::new(("section.zero.hz", si)),
        &(if have_zero { fmt_hz(lane.zero_hz) } else { "—".into() }), value_ink,
        have_zero.then(|| log_frac(lane.zero_hz)));
    if s.hovered {
        *legend = "L scrub · wheel fine".into();
    }
    if have_zero && law.freedom[2] && (s.dx != 0.0 || s.wheel != 0.0) {
        if s.began {
            cmds.push(Command::BeginEdit);
        }
        let factor = 2f64.powf(s.dx as f64 / 220.0 + s.wheel as f64 * 0.0004);
        cmds.push(Command::SetZero {
            section: si,
            hz: (lane.zero_hz * factor).clamp(paint::FREQ_LO, SR * 0.49),
            r: lane.zero_r,
        });
    }
    x = f.right() + 6.0;
    let f = line(l3, x, 52.0);
    let s = scrub_field(ui, painter, f, Id::new(("section.zero.r", si)),
        &(if have_zero { format!("{:.4}", lane.zero_r) } else { "—".into() }), value_ink,
        have_zero.then(|| lane.zero_r as f32));
    if s.hovered {
        *legend = "L scrub · wheel fine".into();
    }
    if have_zero && law.freedom[3] && (s.dx != 0.0 || s.wheel != 0.0) {
        if s.began {
            cmds.push(Command::BeginEdit);
        }
        let r = (lane.zero_r + s.dx as f64 * 0.0015 + s.wheel as f64 * 0.0002).clamp(0.05, 1.0);
        cmds.push(Command::SetZero {
            section: si,
            hz: lane.zero_hz,
            r,
        });
    }
    x = f.right() + 5.0;
    let t = line(l3, x, 24.0);
    let (clicked, hov) = toggle(ui, painter, t, Id::new(("section.zero.lock_at", si)), "at", !law.freedom[2]);
    if hov {
        *legend = "L lock frequency".into();
    }
    if clicked {
        let mut l = law;
        l.freedom[2] = !l.freedom[2];
        cmds.push(Command::BeginEdit);
        cmds.push(Command::SetLaw { section: si, law: l });
    }
    let x2 = t.right() + 3.0;
    let t = line(l3, x2, 24.0);
    let (clicked, hov) = toggle(ui, painter, t, Id::new(("section.zero.lock_r", si)), "r", !law.freedom[3]);
    if hov {
        *legend = "L lock depth".into();
    }
    if clicked {
        let mut l = law;
        l.freedom[3] = !l.freedom[3];
        cmds.push(Command::BeginEdit);
        cmds.push(Command::SetLaw { section: si, law: l });
    }
}
