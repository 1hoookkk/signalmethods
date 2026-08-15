use eframe::egui::{Align2, Id, Painter, Pos2, Rect, Sense, Stroke, Ui};

use trench_core::cascade::NUM_STAGES;

use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::{paint, theme};

pub const NAV_H: f32 = 76.0;

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
    let gain_w = 96.0;
    let gw = Rect::from_min_max(
        Pos2::new(rect.right() - cap - gain_w, rect.center().y - 10.0),
        Pos2::new(rect.right() - cap - 4.0, rect.center().y + 10.0),
    );
    {
        let committed = &session.document.workspace.lanes;
        let active: Vec<_> = committed
            .iter()
            .filter(|l| l.pole_r > 0.0 || l.zero_r > 0.0)
            .collect();
        let total_db: f64 = active
            .iter()
            .map(|l| 20.0 * l.scale.max(1e-9).log10())
            .sum();
        paint::label(
            painter,
            Pos2::new(gw.left() - 32.0, gw.center().y),
            Align2::LEFT_CENTER,
            "gain",
            theme::SMALL,
            theme::INK_DIM,
        );
        let inner_f = crate::ui::paint::field(painter, gw);
        let text = if active.is_empty() {
            "—".to_string()
        } else {
            format!("{total_db:+.1}")
        };
        paint::label(
            painter,
            Pos2::new(inner_f.right() - 4.0, inner_f.center().y),
            Align2::RIGHT_CENTER,
            &text,
            theme::BODY,
            if active.is_empty() { theme::INK_DIM } else { theme::INK },
        );
        let resp = ui
            .interact(gw, Id::new("cascade.gain"), eframe::egui::Sense::click_and_drag())
            .on_hover_cursor(eframe::egui::CursorIcon::ResizeHorizontal);
        if resp.hovered() {
            *legend = "L scrub cascade gain — one figure, spread across sections".into();
        }
        if !active.is_empty() {
            if resp.drag_started() {
                cmds.push(Command::BeginEdit);
            }
            if resp.dragged() {
                let d = resp.drag_delta().x as f64 * 0.1;
                cmds.push(Command::SetGain { db: total_db + d });
            }
        }
    }
    let inner = Rect::from_min_max(
        Pos2::new(rect.left() + cap, rect.top()),
        Pos2::new(rect.right() - cap - gain_w - 40.0, rect.bottom()),
    );
    let gap = 5.0;
    let w = (inner.width() - gap * (NUM_STAGES as f32 - 1.0)) / NUM_STAGES as f32;
    let chip_rect = |si: usize| {
        Rect::from_min_max(
            Pos2::new(inner.left() + si as f32 * (w + gap), inner.top()),
            Pos2::new(inner.left() + si as f32 * (w + gap) + w, inner.bottom()),
        )
    };
    let chip_at = |p: Pos2| (0..NUM_STAGES).find(|&ti| chip_rect(ti).contains(p));
    let (preview, mask) = session.preview();
    let mut pnum = 0;
    for si in 0..NUM_STAGES {
        let chip = chip_rect(si);
        let ink = theme::LANES[si % 7];
        let lane = preview[si];
        let provisional = mask[si];
        if provisional {
            pnum += 1;
        }
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
        let swatch = Rect::from_min_size(
            Pos2::new(ci.left() + 3.0, ci.top() + 3.0),
            eframe::egui::vec2(8.0, 8.0),
        );
        painter.rect_filled(
            swatch,
            0.0,
            if provisional {
                theme::CHROME_DK
            } else if idle {
                theme::faded(ink, 60)
            } else {
                ink
            },
        );
        painter.rect_stroke(swatch, 0.0, Stroke::new(1.0, theme::CHROME_DEEP));
        paint::label(
            painter,
            Pos2::new(swatch.right() + 5.0, ci.top() + 7.0),
            Align2::LEFT_CENTER,
            &(if provisional {
                format!("p{pnum}")
            } else {
                format!("{}", si + 1)
            }),
            theme::SMALL,
            if provisional {
                theme::INK_DIM
            } else if selected {
                theme::CHROME_LT
            } else {
                theme::INK
            },
        );
        if !idle {
            let spark = Rect::from_min_max(
                Pos2::new(ci.left() + 4.0, ci.top() + 15.0),
                Pos2::new(ci.right() - 4.0, ci.bottom() - 3.0),
            );
            if spark.width() > 24.0 {
                let row = lane.biquad_at(crate::engine::response::SR);
                let spark_alpha: u8 = if provisional { 90 } else if selected { 255 } else { 205 };
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
                    Stroke::new(1.3, theme::faded(ink, spark_alpha)),
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
            Pos2::new(ci.right() - 4.0, ci.top() + 7.0),
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
            *legend = "L select · drag onto a neighbour — swap sections · drag onto the glass to assign".into();
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
                } else if let Some(ti) = chip_at(p) {
                    if ti != si {
                        painter.rect_stroke(
                            chip_rect(ti),
                            0.0,
                            Stroke::new(1.4, theme::faded(ink, 220)),
                        );
                        *legend = "release: swap sections — order in the cascade".into();
                    }
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
                } else if let Some(ti) = chip_at(p) {
                    if ti != si {
                        cmds.push(Command::SwapSections { a: si, b: ti });
                    }
                }
            }
        }
    }
}
