use eframe::egui::{Align2, Id, Painter, Pos2, Rect, Sense, Stroke, Ui, Vec2};

use crate::engine::response::{row_db, SR};
use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{paint, theme};

const PAD: f32 = 14.0;
const FOOT_H: f32 = 24.0;

pub fn draw(session: &Session, ui: &mut Ui) -> (Vec<Command>, Option<usize>) {
    let mut cmds = Vec::new();
    let mut open: Option<usize> = None;
    let mut legend = String::from("L —   M —   R —");
    let rect = ui.max_rect();
    let painter = ui.painter().clone();
    painter.rect_filled(rect, 0.0, theme::CHROME);

    let foot = Rect::from_min_max(
        Pos2::new(rect.left() + PAD, rect.bottom() - PAD - FOOT_H),
        rect.right_bottom() - eframe::egui::vec2(PAD, PAD),
    );

    let cube = session.document.field.slots[4..].iter().any(|s| s.is_some());
    let cell = Vec2::new(370.0, 290.0);
    let gap = 16.0;
    let grid_w = cell.x * 2.0 + gap;
    let grid_h = cell.y * 2.0 + gap;
    let total_w = if cube { grid_w * 2.0 + 70.0 } else { grid_w + 70.0 };
    let origin = Pos2::new(
        rect.center().x - total_w / 2.0,
        rect.center().y - grid_h / 2.0 - 10.0,
    );

    let mut draw_grid = |painter: &Painter,
                         ui: &mut Ui,
                         origin: Pos2,
                         t: usize,
                         cmds: &mut Vec<Command>,
                         open: &mut Option<usize>,
                         legend: &mut String| {
        for q in 0..2usize {
            for m in 0..2usize {
                let ci = m | (q << 1) | (t << 2);
                let cr = Rect::from_min_size(
                    Pos2::new(
                        origin.x + m as f32 * (cell.x + gap),
                        origin.y + (1 - q) as f32 * (cell.y + gap),
                    ),
                    cell,
                );
                let well = paint::well(painter, cr);
                let code = format!("m{} q{}{}", m * 100, q * 100, if t == 1 { " · t" } else { "" });
                paint::label(
                    painter,
                    Pos2::new(well.left() + 5.0, well.top() + 4.0),
                    Align2::LEFT_TOP,
                    &code,
                    theme::SMALL,
                    theme::WELL_DIM,
                );
                let slot = &session.document.field.slots[ci];
                if let Some(frame) = slot {
                    let rows: Vec<[f64; 5]> =
                        frame.lanes.iter().map(|l| l.biquad_at(SR)).collect();
                    let plot = Rect::from_min_max(
                        Pos2::new(well.left(), well.top() + 6.0),
                        Pos2::new(well.right(), well.bottom() - 4.0),
                    );
                    let sample = |hz: f64| -> f64 {
                        rows.iter().map(|r| row_db(r, hz, SR)).sum()
                    };
                    let mut lo = f64::INFINITY;
                    let mut hi = f64::NEG_INFINITY;
                    for i in 0..48 {
                        let f = paint::FREQ_LO
                            * (paint::FREQ_HI / paint::FREQ_LO).powf(i as f64 / 47.0);
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
                    let zero_y = paint::db_y(0.0, plot, lo, hi);
                    if zero_y > plot.top() && zero_y < plot.bottom() {
                        painter.line_segment(
                            [
                                Pos2::new(plot.left(), zero_y),
                                Pos2::new(plot.right(), zero_y),
                            ],
                            Stroke::new(1.0, theme::GRATICULE),
                        );
                    }
                    paint::x3_trace(
                        &painter.with_clip_rect(well),
                        plot,
                        lo,
                        hi,
                        theme::NOW,
                        &sample,
                    );
                } else {
                    paint::label(
                        painter,
                        well.center(),
                        Align2::CENTER_CENTER,
                        "—",
                        theme::BODY,
                        theme::WELL_DIM,
                    );
                }
                let selected = session.selection.corner == Some(ci);
                if selected {
                    painter.rect_stroke(cr, 0.0, Stroke::new(1.0, theme::HOT));
                }
                let resp = ui.interact(cr, Id::new(("field.corner", ci)), Sense::click());
                if resp.hovered() {
                    painter.rect_stroke(cr, 0.0, Stroke::new(1.0, theme::CHROME_LT));
                    *legend = if slot.is_some() {
                        "L open this corner".into()
                    } else {
                        "L open empty corner".into()
                    };
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
                    *open = Some(ci);
                }
            }
        }
        paint::label(
            painter,
            Pos2::new(origin.x + grid_w / 2.0, origin.y - 10.0),
            Align2::CENTER_BOTTOM,
            "q",
            theme::SMALL,
            theme::INK_DIM,
        );
        paint::label(
            painter,
            Pos2::new(origin.x + grid_w / 2.0, origin.y + grid_h + 10.0),
            Align2::CENTER_TOP,
            "morph 0 → 100",
            theme::SMALL,
            theme::INK_DIM,
        );
    };

    draw_grid(&painter, ui, origin, 0, &mut cmds, &mut open, &mut legend);
    if cube {
        let origin2 = Pos2::new(origin.x + grid_w + 54.0, origin.y);
        draw_grid(&painter, ui, origin2, 1, &mut cmds, &mut open, &mut legend);
    } else {
        let pb = Rect::from_min_size(
            Pos2::new(origin.x + grid_w + 20.0, origin.y + grid_h / 2.0 - 14.0),
            Vec2::new(30.0, 28.0),
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
            legend = "L add the second grid — a cube".into();
        }
        if resp.clicked() {
            cmds.push(Command::ExpandField);
        }
    }

    paint::label(
        &painter,
        Pos2::new(foot.left(), foot.center().y),
        Align2::LEFT_CENTER,
        &legend,
        theme::SMALL,
        theme::INK_DIM,
    );
    paint::label(
        &painter,
        Pos2::new(foot.right(), foot.center().y),
        Align2::RIGHT_CENTER,
        "the field",
        theme::SMALL,
        theme::INK,
    );
    (cmds, open)
}
