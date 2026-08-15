use eframe::egui::{Align2, Color32, CursorIcon, Id, Painter, Pos2, Rect, Sense, Stroke, Ui};

use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::max_contiguous_pole_radius;

use crate::engine::response::SR;
use crate::session::command::Command;
use crate::session::state::{FitState, Session};
use crate::ui::{paint, theme};

pub const PANEL_W: f32 = 384.0;
const CARD_H: f32 = 76.0;

const BW_MIN: f64 = 8.0;
const BW_MAX: f64 = 6000.0;

fn bw_of(r: f64) -> f64 {
    -r.ln() * SR / std::f64::consts::PI
}

fn r_of(bw: f64) -> f64 {
    (-std::f64::consts::PI * bw / SR).exp()
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

pub fn draw(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
    legend: &mut String,
) {
    match session.selection.section {
        Some(si) => {
            let card = Rect::from_min_max(
                rect.left_top(),
                Pos2::new(rect.right(), rect.top() + CARD_H),
            );
            draw_card(session, ui, painter, card, si, cmds, legend);
        }
        None => draw_cascade_card(session, ui, painter, rect, cmds, legend),
    }
    let ladder = Rect::from_min_max(
        Pos2::new(rect.left(), rect.top() + CARD_H + 14.0),
        rect.right_bottom(),
    );
    pairing_ladder(session, ui, painter, ladder, cmds, legend);
}

fn fmt_hz2(hz: f64) -> String {
    if hz >= 999.5 {
        format!("{:.1}k", hz / 1000.0)
    } else {
        format!("{hz:.0}")
    }
}

fn pairing_ladder(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
    legend: &mut String,
) {
    use trench_core::cascade::NUM_STAGES;
    paint::label(
        painter,
        Pos2::new(rect.left() + 2.0, rect.top() + 6.0),
        Align2::LEFT_CENTER,
        "pairing",
        theme::SMALL,
        theme::INK_DIM,
    );
    let row_h = 26.0;
    let top = rect.top() + 16.0;
    let rows: Vec<Rect> = (0..NUM_STAGES)
        .map(|si| {
            Rect::from_min_max(
                Pos2::new(rect.left(), top + si as f32 * row_h),
                Pos2::new(rect.right(), top + (si + 1) as f32 * row_h),
            )
        })
        .collect();
    let pointer = ui.input(|i| i.pointer.interact_pos());
    let mut dragging: Option<usize> = None;
    for si in 0..NUM_STAGES {
        let row = rows[si];
        let ink = theme::LANES[si % 7];
        let lane = session.active_lanes()[si];
        let selected = session.selection.section == Some(si);
        let cy = row.center().y;
        paint::label(
            painter,
            Pos2::new(row.left() + 6.0, cy),
            Align2::LEFT_CENTER,
            &format!("{}", si + 1),
            theme::SMALL,
            if selected { theme::INK } else { theme::INK_DIM },
        );
        let px = row.left() + 26.0;
        if lane.pole_r > 0.0 {
            painter.circle_filled(Pos2::new(px, cy), 4.0, ink);
            paint::label(
                painter,
                Pos2::new(px + 8.0, cy),
                Align2::LEFT_CENTER,
                &fmt_hz2(lane.pole_hz),
                theme::SMALL,
                theme::INK,
            );
        } else {
            painter.circle_stroke(Pos2::new(px, cy), 4.0, Stroke::new(1.0, theme::CHROME_DK));
        }
        let zx = row.right() - 66.0;
        painter.line_segment(
            [Pos2::new(px + 52.0, cy), Pos2::new(zx - 10.0, cy)],
            Stroke::new(1.0, theme::CHROME_DK),
        );
        let zrect = Rect::from_center_size(
            Pos2::new(zx, cy),
            eframe::egui::vec2(16.0, row_h - 4.0),
        );
        if lane.zero_r > 0.0 {
            painter.circle_stroke(Pos2::new(zx, cy), 4.5, Stroke::new(1.6, ink));
            paint::label(
                painter,
                Pos2::new(zx + 9.0, cy),
                Align2::LEFT_CENTER,
                &fmt_hz2(lane.zero_hz),
                theme::SMALL,
                theme::INK,
            );
        } else {
            painter.circle_stroke(Pos2::new(zx, cy), 4.5, Stroke::new(1.0, theme::CHROME_DK));
            paint::label(
                painter,
                Pos2::new(zx + 9.0, cy),
                Align2::LEFT_CENTER,
                "—",
                theme::SMALL,
                theme::INK_DIM,
            );
        }
        let rresp = ui.interact(
            Rect::from_min_max(row.left_top(), Pos2::new(zx - 12.0, row.bottom())),
            Id::new(("pair.row", si)),
            Sense::click(),
        );
        if rresp.clicked() {
            let mut sel = session.selection;
            sel.section = if selected { None } else { Some(si) };
            cmds.push(Command::Select(sel));
        }
        if rresp.hovered() {
            *legend = "L select".into();
        }
        let zresp = ui.interact(zrect, Id::new(("pair.zero", si)), Sense::click_and_drag());
        if zresp.hovered() {
            *legend = "L drag to another row — swaps zero pairs, the whole never moves".into();
        }
        if zresp.dragged() {
            dragging = Some(si);
        }
        if zresp.drag_stopped() {
            if let Some(p) = pointer {
                for (ti, r) in rows.iter().enumerate() {
                    if r.contains(p) && ti != si {
                        cmds.push(Command::BeginEdit);
                        cmds.push(Command::SwapZeros { a: si, b: ti });
                    }
                }
            }
        }
    }
    if let (Some(si), Some(p)) = (dragging, pointer) {
        painter.circle_stroke(p, 4.5, Stroke::new(1.6, theme::LANES[si % 7]));
        for (ti, r) in rows.iter().enumerate() {
            if r.contains(p) && ti != si {
                painter.rect_stroke(*r, 0.0, Stroke::new(1.0, theme::faded(theme::LANES[si % 7], 160)));
                *legend = "release: swap zero pairs".into();
            }
        }
    }
}

fn draw_cascade_card(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
    legend: &mut String,
) {
    let card = Rect::from_min_max(
        rect.left_top(),
        Pos2::new(rect.right(), rect.top() + CARD_H),
    );
    painter.rect_filled(card, 0.0, theme::CHROME);
    let inner = paint::raised(painter, card);
    let l1 = inner.top() + 14.0;
    let l2 = inner.top() + 40.0;
    paint::label(
        painter,
        Pos2::new(inner.left() + 8.0, l1),
        Align2::LEFT_CENTER,
        "cascade",
        theme::BODY,
        theme::INK,
    );
    let active = session
        .active_lanes()
        .iter()
        .filter(|l| l.pole_r > 0.0 || l.zero_r > 0.0)
        .count();
    paint::label(
        painter,
        Pos2::new(inner.left() + 8.0, l2),
        Align2::LEFT_CENTER,
        &format!("{active} / {}", NUM_STAGES),
        theme::SMALL,
        theme::INK_DIM,
    );
    if let FitState::Complete { rms_db, .. } = &session.fit {
        paint::label(
            painter,
            Pos2::new(inner.center().x, l2),
            Align2::CENTER_CENTER,
            &format!("rms {rms_db:.2}"),
            theme::SMALL,
            theme::INK,
        );
    }
    let fitting = matches!(session.fit, FitState::Running { .. });
    let fb = Rect::from_min_max(
        Pos2::new(inner.right() - 42.0, l1 - 10.0),
        Pos2::new(inner.right() - 6.0, l1 + 10.0),
    );
    let fi = if fitting {
        paint::sunken(painter, fb)
    } else {
        paint::raised(painter, fb)
    };
    paint::label(
        painter,
        Pos2::new(fi.center().x, fi.center().y),
        Align2::CENTER_CENTER,
        "fit",
        theme::SMALL,
        if fitting { theme::INK_DIM } else { theme::INK },
    );
    let resp = ui.interact(fb, Id::new("cascade.fit"), Sense::click());
    if resp.hovered() {
        *legend = "L fit the whole cascade to the target".into();
    }
    if resp.clicked() && !fitting {
        cmds.push(Command::FitSelection);
    }
    let kb = Rect::from_min_max(
        Pos2::new(fb.left() - 52.0, l1 - 10.0),
        Pos2::new(fb.left() - 4.0, l1 + 10.0),
    );
    let ki = paint::raised(painter, kb);
    paint::label(
        painter,
        Pos2::new(ki.center().x, ki.center().y),
        Align2::CENTER_CENTER,
        "keep",
        theme::SMALL,
        theme::INK,
    );
    let resp = ui.interact(kb, Id::new("cascade.keep"), Sense::click());
    if resp.hovered() {
        *legend = "L keep this response in the library".into();
    }
    if resp.clicked() {
        cmds.push(Command::Keep);
    }
}

fn draw_card(
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
    let lane = session.active_lanes()[si];
    let law = session.document.workspace.laws[si];
    let selected = session.selection.section == Some(si);
    let have_pole = lane.pole_r > 0.0;
    let have_zero = lane.zero_r > 0.0;
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

    let mut x = swatch.right() + 24.0;
    paint::label(painter, Pos2::new(x, l1), Align2::LEFT_CENTER, "range", theme::SMALL, theme::INK_DIM);
    x += 40.0;
    let f = line(l1, x, 92.0);
    let range_text = if law.has_zone() {
        format!("{}–{}", fmt_hz(law.zone[0]), fmt_hz(law.zone[1]))
    } else {
        "—".into()
    };
    let s = scrub_field(ui, painter, f, Id::new(("section.range", si)), &range_text, value_ink);
    if s.hovered {
        *legend = "L scrub · wheel width".into();
    }
    if s.dx != 0.0 || s.wheel != 0.0 {
        if s.began {
            cmds.push(Command::BeginEdit);
        }
        let mut l = law;
        if !l.has_zone() {
            let c = if have_pole { lane.pole_hz } else { 1000.0 };
            l.zone = [c / 1.3, c * 1.3];
        }
        let shift = 2f64.powf(s.dx as f64 / 220.0);
        let width = 2f64.powf(s.wheel as f64 * 0.001);
        let center = (l.zone[0] * l.zone[1]).sqrt() * shift;
        let half = (l.zone[1] / l.zone[0]).sqrt() * width;
        l.zone = [
            (center / half).max(paint::FREQ_LO * 0.5),
            (center * half).min(SR * 0.49),
        ];
        cmds.push(Command::SetLaw { section: si, law: l });
    }

    let fitting = matches!(session.fit, FitState::Running { .. });
    let fb = Rect::from_min_max(
        Pos2::new(inner.right() - 38.0, l1 - 9.0),
        Pos2::new(inner.right() - 5.0, l1 + 9.0),
    );
    let fi = if fitting {
        paint::sunken(painter, fb)
    } else {
        paint::raised(painter, fb)
    };
    paint::label(
        painter,
        Pos2::new(fi.center().x, fi.center().y),
        Align2::CENTER_CENTER,
        "fit",
        theme::SMALL,
        if fitting { theme::INK_DIM } else { theme::INK },
    );
    let resp = ui.interact(fb, Id::new(("section.fit", si)), Sense::click());
    if resp.hovered() {
        *legend = "L fit this section inside its range".into();
    }
    if resp.clicked() && !fitting {
        cmds.push(Command::FitSection(si));
    }
    let hb = Rect::from_min_max(
        Pos2::new(fb.left() - 44.0, l1 - 9.0),
        Pos2::new(fb.left() - 4.0, l1 + 9.0),
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
        &(if have_pole { fmt_hz(lane.pole_hz) } else { "—".into() }), value_ink);
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
        &(if have_pole { format!("{:.0}", bw_of(lane.pole_r)) } else { "—".into() }), value_ink);
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
        &(if have_zero { fmt_hz(lane.zero_hz) } else { "—".into() }), value_ink);
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
        &(if have_zero { format!("{:.4}", lane.zero_r) } else { "—".into() }), value_ink);
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
