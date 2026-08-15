use eframe::egui::{Align2, Id, Painter, Pos2, Rect, Sense, Stroke, Ui};

use trench_core::cascade::NUM_STAGES;

use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{paint, theme};

pub const NAV_H: f32 = 26.0;

fn fmt_hz(hz: f64) -> String {
    if hz >= 999.5 {
        format!("{:.1}k", hz / 1000.0)
    } else {
        format!("{hz:.0}")
    }
}

pub fn draw(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    well: Rect,
    cmds: &mut Vec<Command>,
    legend: &mut String,
) {
    let cap = 30.0;
    paint::label(
        painter,
        Pos2::new(rect.left() + 2.0, rect.center().y),
        Align2::LEFT_CENTER,
        "in",
        theme::SMALL,
        theme::INK_DIM,
    );
    paint::label(
        painter,
        Pos2::new(rect.right() - 2.0, rect.center().y),
        Align2::RIGHT_CENTER,
        "out",
        theme::SMALL,
        theme::INK_DIM,
    );
    let inner = Rect::from_min_max(
        Pos2::new(rect.left() + cap, rect.top()),
        Pos2::new(rect.right() - cap, rect.bottom()),
    );
    let gap = 5.0;
    let w = (inner.width() - gap * (NUM_STAGES as f32 - 1.0)) / NUM_STAGES as f32;
    for si in 0..NUM_STAGES {
        let chip = Rect::from_min_max(
            Pos2::new(inner.left() + si as f32 * (w + gap), inner.top()),
            Pos2::new(inner.left() + si as f32 * (w + gap) + w, inner.bottom()),
        );
        let ink = theme::LANES[si % 7];
        let lane = session.active_lanes()[si];
        let law = session.document.workspace.laws[si];
        let selected = session.selection.section == Some(si);
        let idle = lane.pole_r <= 0.0 && lane.zero_r <= 0.0;
        let ci = if selected {
            let c = paint::sunken(painter, chip);
            painter.rect_filled(c, 0.0, theme::CHROME_DK);
            c
        } else {
            paint::raised(painter, chip)
        };
        let swatch = Rect::from_min_max(
            Pos2::new(ci.left() + 3.0, ci.top() + 3.0),
            Pos2::new(ci.left() + 9.0, ci.bottom() - 3.0),
        );
        painter.rect_filled(swatch, 0.0, if idle { theme::faded(ink, 60) } else { ink });
        painter.rect_stroke(swatch, 0.0, Stroke::new(1.0, theme::CHROME_DEEP));
        paint::label(
            painter,
            Pos2::new(swatch.right() + 5.0, ci.center().y),
            Align2::LEFT_CENTER,
            &format!("{}", si + 1),
            theme::SMALL,
            if selected { theme::CHROME_LT } else { theme::INK },
        );
        if !idle {
            let spark = Rect::from_min_max(
                Pos2::new(swatch.right() + 18.0, ci.top() + 3.0),
                Pos2::new(ci.right() - 44.0, ci.bottom() - 3.0),
            );
            if spark.width() > 24.0 {
                let row = lane.biquad_at(crate::engine::response::SR);
                let n = 48;
                let mut vals = Vec::with_capacity(n);
                let mut lo = f64::INFINITY;
                let mut hi = f64::NEG_INFINITY;
                for i in 0..n {
                    let f = crate::ui::paint::FREQ_LO
                        * (crate::ui::paint::FREQ_HI / crate::ui::paint::FREQ_LO)
                            .powf(i as f64 / (n - 1) as f64);
                    let db = crate::engine::response::row_db(&row, f, crate::engine::response::SR);
                    lo = lo.min(db);
                    hi = hi.max(db);
                    vals.push(db);
                }
                lo -= 2.0;
                hi += 2.0;
                if hi - lo < 12.0 {
                    let mid = (hi + lo) / 2.0;
                    lo = mid - 6.0;
                    hi = mid + 6.0;
                }
                let pts: Vec<Pos2> = vals
                    .iter()
                    .enumerate()
                    .map(|(i, &db)| {
                        Pos2::new(
                            spark.left() + i as f32 / (n - 1) as f32 * spark.width(),
                            spark.bottom()
                                - ((db - lo) / (hi - lo)) as f32 * spark.height(),
                        )
                    })
                    .collect();
                painter.add(eframe::egui::Shape::line(
                    pts,
                    Stroke::new(1.0, theme::faded(ink, if selected { 255 } else { 175 })),
                ));
            }
        }
        let state = if idle {
            "—".into()
        } else if !law.writable {
            "hold".into()
        } else if lane.pole_r > 0.0 {
            fmt_hz(lane.pole_hz)
        } else {
            format!("z{}", fmt_hz(lane.zero_hz))
        };
        paint::label(
            painter,
            Pos2::new(ci.right() - 4.0, ci.center().y),
            Align2::RIGHT_CENTER,
            &state,
            theme::SMALL,
            if !law.writable && !idle {
                theme::HOT
            } else if selected {
                theme::CHROME_LT
            } else {
                theme::INK_DIM
            },
        );
        let resp = ui.interact(chip, Id::new(("nav.section", si)), Sense::click_and_drag());
        if resp.hovered() {
            *legend = "L select · drag onto the glass to assign".into();
        }
        if resp.clicked() {
            let mut sel = session.selection;
            sel.section = if selected { None } else { Some(si) };
            cmds.push(Command::Select(sel));
        }
        if resp.dragged() {
            if let Some(p) = resp.interact_pointer_pos() {
                if well.contains(p) {
                    let hz = crate::ui::paint::hz_at_x(p.x, well);
                    let a = crate::ui::paint::log_x(hz / 1.3, well);
                    let b = crate::ui::paint::log_x(hz * 1.3, well);
                    painter.rect_filled(
                        Rect::from_min_max(
                            Pos2::new(a, well.top()),
                            Pos2::new(b, well.bottom()),
                        ),
                        0.0,
                        theme::faded(ink, 22),
                    );
                    painter.line_segment(
                        [Pos2::new(p.x, well.top()), Pos2::new(p.x, well.bottom())],
                        Stroke::new(1.0, theme::faded(ink, 180)),
                    );
                    *legend = "release: its pole hunts here".into();
                }
            }
        }
        if resp.drag_stopped() {
            if let Some(p) = resp.interact_pointer_pos() {
                if well.contains(p) {
                    cmds.push(Command::AssignSection {
                        section: si,
                        hz: crate::ui::paint::hz_at_x(p.x, well),
                    });
                }
            }
        }
    }
}
