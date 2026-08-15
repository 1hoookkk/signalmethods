use eframe::egui::{Align2, Id, Painter, Pos2, Rect, Sense, Stroke, Ui, Vec2};

use crate::engine::response::{row_db, SR};
use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{paint, theme, transport};

const PAD: f32 = 14.0;
const STRIP_H: f32 = 30.0;
const FOOT_H: f32 = 24.0;

pub fn draw(session: &Session, ui: &mut Ui) -> (Vec<Command>, Option<usize>) {
    let mut cmds = Vec::new();
    let mut open: Option<usize> = None;
    let mut legend = String::from("L —   M —   R —");
    let rect = ui.max_rect();
    let painter = ui.painter().clone();
    painter.rect_filled(rect, 0.0, theme::CHROME);

    let banner = Rect::from_min_max(
        rect.left_top() + Vec2::new(PAD, PAD),
        Pos2::new(rect.right() - PAD, rect.top() + PAD + paint::BANNER_H),
    );
    paint::banner(
        &painter,
        banner,
        "TRENCH Field Editing: the field",
        concat!("v", env!("CARGO_PKG_VERSION")),
    );
    let strip = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, banner.bottom() + 6.0),
        Pos2::new(rect.right() - PAD, banner.bottom() + 6.0 + STRIP_H),
    );
    let foot = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, rect.bottom() - PAD - FOOT_H),
        rect.right_bottom() - Vec2::new(PAD, PAD),
    );
    let tb = transport::draw(
        session,
        ui,
        &painter,
        Pos2::new(strip.left(), strip.top() + 3.0),
        &mut cmds,
        &mut legend,
    );

    let pos = session.audition.pos;
    let cube = session.document.field.slots[4..].iter().any(|s| s.is_some());
    let back_plane = cube && pos[2] > 0.5;
    let mut x = tb.right() + 16.0;
    for (name, value) in [
        ("m", pos[0]),
        ("q", pos[1]),
        ("t", pos[2]),
    ] {
        paint::label(
            &painter,
            Pos2::new(x, strip.center().y),
            Align2::LEFT_CENTER,
            name,
            theme::SMALL,
            theme::INK_DIM,
        );
        x += 12.0;
        let fw = Rect::from_min_max(
            Pos2::new(x, strip.top() + 3.0),
            Pos2::new(x + 52.0, strip.bottom() - 3.0),
        );
        let inner = paint::field(&painter, fw);
        paint::label(
            &painter,
            Pos2::new(inner.right() - 4.0, inner.center().y),
            Align2::RIGHT_CENTER,
            &format!("{:.0}", value * 100.0),
            theme::BODY,
            theme::INK,
        );
        x = fw.right() + 14.0;
    }
    if !cube {
        let pb = Rect::from_min_max(
            Pos2::new(x + 6.0, strip.top() + 3.0),
            Pos2::new(x + 36.0, strip.bottom() - 3.0),
        );
        let pi = paint::raised(&painter, pb);
        paint::label(
            &painter,
            Pos2::new(pi.center().x, pi.center().y),
            Align2::CENTER_CENTER,
            "+",
            theme::BODY,
            theme::INK,
        );
        let resp = ui.interact(pb, Id::new("field.expand"), Sense::click());
        if resp.hovered() {
            legend = "L add the back plane — a cube".into();
        }
        if resp.clicked() {
            cmds.push(Command::ExpandField);
        }
    }

    let well_frame = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, strip.bottom() + PAD),
        Pos2::new(rect.right() - PAD, foot.top() - PAD),
    );
    let well = paint::well(&painter, well_frame);

    for i in 0..=10 {
        let t = i as f32 / 10.0;
        let major = i % 5 == 0;
        let stroke = Stroke::new(
            1.0,
            if major {
                theme::faded(theme::WELL_DIM, 130)
            } else {
                theme::GRATICULE
            },
        );
        let x = well.left() + t * well.width();
        painter.line_segment([Pos2::new(x, well.top()), Pos2::new(x, well.bottom())], stroke);
        let y = well.top() + t * well.height();
        painter.line_segment([Pos2::new(well.left(), y), Pos2::new(well.right(), y)], stroke);
    }

    let cell = Vec2::new(well.width() * 0.27, well.height() * 0.30);
    let inset = 10.0;
    let plane = usize::from(back_plane) << 2;
    let cell_rects: Vec<(usize, Rect)> = [(0usize, 0usize), (1, 0), (0, 1), (1, 1)]
        .iter()
        .map(|&(m, q)| {
            let ci = m | (q << 1) | plane;
            let cx = if m == 0 {
                well.left() + inset
            } else {
                well.right() - inset - cell.x
            };
            let cy = if q == 1 {
                well.top() + inset
            } else {
                well.bottom() - inset - cell.y
            };
            (ci, Rect::from_min_size(Pos2::new(cx, cy), cell))
        })
        .collect();

    let ride_resp = ui.interact(well, Id::new("field.ride"), Sense::click_and_drag());
    if ride_resp.hovered() {
        legend = if cube {
            "L ride · wheel t".into()
        } else {
            "L ride".into()
        };
        let wheel = ui.input(|i| i.raw_scroll_delta.y);
        if cube && wheel != 0.0 {
            let t = (pos[2] - wheel * 0.001).clamp(0.0, 1.0);
            cmds.push(Command::Ride([pos[0], pos[1], t]));
        }
    }
    if ride_resp.dragged() || ride_resp.clicked() {
        if let Some(p) = ride_resp.interact_pointer_pos() {
            let over_cell = cell_rects.iter().any(|(_, r)| r.contains(p));
            if !over_cell || ride_resp.dragged() {
                let m = ((p.x - well.left()) / well.width()).clamp(0.0, 1.0);
                let q = (1.0 - (p.y - well.top()) / well.height()).clamp(0.0, 1.0);
                cmds.push(Command::Ride([m, q, pos[2]]));
            }
        }
    }

    for (ci, cr) in &cell_rects {
        let ci = *ci;
        let cr = *cr;
        painter.rect_filled(cr, 0.0, theme::WELL);
        painter.rect_stroke(cr, 0.0, Stroke::new(1.0, theme::faded(theme::WELL_DIM, 110)));
        let code = format!(
            "m{} q{}{}",
            (ci & 1) * 100,
            ((ci >> 1) & 1) * 100,
            if ci & 4 != 0 { " · t" } else { "" }
        );
        paint::label(
            &painter,
            Pos2::new(cr.left() + 5.0, cr.top() + 4.0),
            Align2::LEFT_TOP,
            &code,
            theme::SMALL,
            theme::WELL_DIM,
        );
        let slot = &session.document.field.slots[ci];
        if let Some(frame) = slot {
            let rows: Vec<[f64; 5]> = frame.lanes.iter().map(|l| l.biquad_at(SR)).collect();
            let plot = Rect::from_min_max(
                Pos2::new(cr.left(), cr.top() + 6.0),
                Pos2::new(cr.right(), cr.bottom() - 4.0),
            );
            let sample = |hz: f64| -> f64 { rows.iter().map(|r| row_db(r, hz, SR)).sum() };
            let mut lo = f64::INFINITY;
            let mut hi = f64::NEG_INFINITY;
            for i in 0..48 {
                let f = paint::FREQ_LO * (paint::FREQ_HI / paint::FREQ_LO).powf(i as f64 / 47.0);
                let db = sample(f);
                lo = lo.min(db);
                hi = hi.max(db);
            }
            lo -= 5.0;
            hi += 5.0;
            if hi - lo < 24.0 {
                let mid = (hi + lo) / 2.0;
                lo = mid - 12.0;
                hi = mid + 12.0;
            }
            paint::x3_trace(&painter.with_clip_rect(cr), plot, lo, hi, theme::NOW, &sample);
        } else {
            paint::label(
                &painter,
                cr.center(),
                Align2::CENTER_CENTER,
                "—",
                theme::BODY,
                theme::WELL_DIM,
            );
        }
        let resp = ui.interact(cr, Id::new(("field.corner", ci)), Sense::click());
        if resp.hovered() {
            painter.rect_stroke(cr, 0.0, Stroke::new(1.0, theme::CHROME_LT));
            legend = "L open this corner".into();
        }
        if resp.clicked() {
            if slot.is_some() {
                cmds.push(Command::TargetCorner(ci));
            } else {
                let mut sel = session.selection;
                sel.corner = Some(ci);
                sel.section = None;
                cmds.push(Command::Select(sel));
            }
            open = Some(ci);
        }
    }

    let px = well.left() + pos[0] * well.width();
    let py = well.top() + (1.0 - pos[1]) * well.height();
    let puck = Pos2::new(px, py);
    painter.circle_stroke(puck, 8.0, Stroke::new(1.6, theme::HOT));
    painter.line_segment(
        [Pos2::new(px - 13.0, py), Pos2::new(px + 13.0, py)],
        Stroke::new(1.0, theme::faded(theme::HOT, 170)),
    );
    painter.line_segment(
        [Pos2::new(px, py - 13.0), Pos2::new(px, py + 13.0)],
        Stroke::new(1.0, theme::faded(theme::HOT, 170)),
    );

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
    paint::label(
        &painter,
        Pos2::new(fi.right() - 4.0, fi.center().y),
        Align2::RIGHT_CENTER,
        "the field",
        theme::SMALL,
        theme::INK,
    );
    (cmds, open)
}
