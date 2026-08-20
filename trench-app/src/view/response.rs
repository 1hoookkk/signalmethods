use egui::{pos2, Align2, Color32, Painter, Pos2, Rect, Response, Sense, Stroke, StrokeKind, Ui};
use trench_core::stage_law::RootPair;

use crate::document::{Section, SECTIONS};
use crate::plot::axes::{hz_label, DbAxis, Split, ERROR_DB_SPAN, HZ_TICKS, MAIN_DB_HI, MAIN_DB_LO};
use crate::plot::columns::{
    cascade_db, difference, peak_hold_into, sampled_db, stage_db, ColumnGrid,
};
use crate::theme::Theme;

pub const HIT_RADIUS: f32 = 18.0;
pub const RP_MAX: f64 = 84.0;

pub fn r_prime(r: f64) -> f64 {
    if r >= 1.0 {
        RP_MAX
    } else if r <= 0.0 {
        0.0
    } else {
        (20.0 * (1.0 / (1.0 - r)).log10()).min(RP_MAX)
    }
}

pub fn r_of(rp: f64) -> f64 {
    1.0 - 10f64.powf(-rp.max(0.0) / 20.0)
}

pub enum Action {
    Select(usize),
    Begin(usize),
    DragTo { ratio: f64, ddb: f64 },
    End,
    Refused(usize),
    Width { section: usize, notches: f64 },
}

pub struct Model<'a> {
    pub biquads: &'a [[f64; 5]],
    pub sections: &'a [Section; SECTIONS],
    pub selected: Option<usize>,
    pub target: Option<&'a [f64]>,
    pub candidate: Option<&'a [[f64; 5]]>,
    pub judged: Option<&'a [[f64; 5]]>,
    pub rms_db: Option<f64>,
    pub sr_hz: f64,
    pub editable: bool,
}

pub struct Shown {
    pub action: Option<Action>,
    pub anchor: Option<Pos2>,
    pub main: Rect,
}

struct Handle {
    section: usize,
    pos: Pos2,
    draggable: bool,
    offscale: i8,
}

#[derive(Default)]
pub struct ResponseView {
    grid: Option<ColumnGrid>,
    cols: Vec<f32>,
    sum: Vec<f32>,
    points: Vec<Pos2>,
    origin: Option<Pos2>,
}

fn peak_hz(grid: &ColumnGrid, b: &[f64; 5]) -> f64 {
    let at = stage_db(grid, b);
    let mut best = (0usize, -1.0f64);
    for i in 0..grid.samples() {
        let v = at(i).abs();
        if v > best.1 {
            best = (i, v);
        }
    }
    grid.hz_at(best.0)
}

fn handles(
    sections: &[Section; SECTIONS],
    biquads: &[[f64; 5]],
    grid: &ColumnGrid,
    rect: Rect,
    axis: &DbAxis,
    sr_hz: f64,
) -> Vec<Handle> {
    let mut out = Vec::new();
    for (si, section) in sections.iter().enumerate() {
        if section.is_empty() {
            continue;
        }
        let Some(b) = biquads.get(si) else { continue };
        let hz = match section.geometry.pole {
            RootPair::Conjugate { hz, r } if r > 0.0 => hz,
            _ => peak_hz(grid, b),
        };
        let db = trench_core::response::biquad_cascade_mag_db(std::slice::from_ref(b), hz, sr_hz);
        let y = axis.y_of(db).clamp(rect.top() + 12.0, rect.bottom() - 12.0);
        let offscale = if db > axis.hi() {
            1
        } else if db < axis.lo() {
            -1
        } else {
            0
        };
        out.push(Handle {
            section: si,
            pos: pos2(grid.x_of_hz(rect.left(), hz), y),
            draggable: section.is_conjugate_expressible(),
            offscale,
        });
    }
    out
}

impl ResponseView {
    pub fn show(&mut self, ui: &mut Ui, theme: &Theme, model: &Model<'_>) -> Shown {
        let size = ui.available_size();
        let (rect, response) = ui.allocate_exact_size(size, Sense::click_and_drag());
        response
            .widget_info(|| egui::WidgetInfo::labeled(egui::WidgetType::Other, true, "RESPONSE"));
        if !ui.is_rect_visible(rect) {
            return Shown {
                action: None,
                anchor: None,
                main: rect,
            };
        }

        let ppp = ui.pixels_per_point();
        let split = Split::with_error(rect, model.target.is_some());
        let main_axis = DbAxis::main(split.main);
        let painter = ui.painter_at(rect);

        let Self {
            grid,
            cols,
            sum,
            points,
            origin,
        } = self;
        let grid = ColumnGrid::ensure(grid, split.main.width(), ppp, model.sr_hz);

        painter.rect_filled(rect, theme.radius(), theme.well.bg);
        graticule(&painter, theme, &split, &main_axis, grid, ppp);

        let clip = painter.with_clip_rect(split.main);

        peak_hold_into(grid, cascade_db(grid, model.biquads), sum);

        if let Some(target) = model.target {
            peak_hold_into(grid, sampled_db(grid, target), cols);
            stroke(
                &clip, split.main, &main_axis, grid, ppp, theme.data.target, 1.2, cols, points,
            );
        }

        if let Some(si) = model.selected {
            if let Some(b) = model.biquads.get(si) {
                peak_hold_into(grid, stage_db(grid, b), cols);
                let ink = theme.data.stage[si % theme.data.stage.len()];
                fill_to_zero(
                    &clip,
                    split.main,
                    &main_axis,
                    grid,
                    cols,
                    ink.gamma_multiply(0.16),
                );
                stroke(&clip, split.main, &main_axis, grid, ppp, ink, 1.1, cols, points);
            }
        }

        if let Some(candidate) = model.candidate {
            peak_hold_into(grid, cascade_db(grid, candidate), cols);
            stroke(
                &clip, split.main, &main_axis, grid, ppp, theme.data.candidate, 1.0, cols, points,
            );
        }

        stroke(
            &clip, split.main, &main_axis, grid, ppp, theme.data.live, 1.6, sum, points,
        );

        let marks = if model.editable {
            handles(
                model.sections,
                model.biquads,
                grid,
                split.main,
                &main_axis,
                model.sr_hz,
            )
        } else {
            Vec::new()
        };
        draw_handles(&clip, theme, &marks, model.selected);

        let anchor = marks
            .iter()
            .find(|h| Some(h.section) == model.selected)
            .map(|h| h.pos);
        let action = gestures(ui, &response, &marks, grid, split.main, &main_axis, origin);

        error_strip(ui, &painter, theme, &split, grid, model, ppp, cols, points);
        Shown {
            action,
            anchor,
            main: split.main,
        }
    }
}

fn nearest(marks: &[Handle], at: Pos2) -> Option<&Handle> {
    marks
        .iter()
        .map(|h| (h, (h.pos.x - at.x).hypot(h.pos.y - at.y)))
        .filter(|(_, d)| *d < HIT_RADIUS)
        .min_by(|a, b| a.1.total_cmp(&b.1))
        .map(|(h, _)| h)
}

fn gestures(
    ui: &Ui,
    response: &Response,
    marks: &[Handle],
    grid: &ColumnGrid,
    rect: Rect,
    axis: &DbAxis,
    origin: &mut Option<Pos2>,
) -> Option<Action> {
    if marks.is_empty() {
        return None;
    }
    if response.drag_started() || (response.is_pointer_button_down_on() && origin.is_none()) {
        let at = ui
            .input(|i| i.pointer.press_origin())
            .or_else(|| response.interact_pointer_pos())?;
        let hit = nearest(marks, at)?;
        if !hit.draggable {
            return Some(Action::Refused(hit.section));
        }
        *origin = Some(at);
        return Some(Action::Begin(hit.section));
    }
    if response.dragged() {
        let (Some(start), Some(now)) = (*origin, response.interact_pointer_pos()) else {
            return None;
        };
        let from = grid.hz_of_x(rect.left(), start.x).max(1e-6);
        let to = grid.hz_of_x(rect.left(), now.x);
        let ddb = axis.db_of(now.y) - axis.db_of(start.y);
        return Some(Action::DragTo {
            ratio: to / from,
            ddb,
        });
    }
    if response.drag_stopped() || (origin.is_some() && !response.is_pointer_button_down_on()) {
        *origin = None;
        return Some(Action::End);
    }
    if response.hovered() {
        let scroll = ui.input(|i| i.smooth_scroll_delta.y) as f64;
        if scroll.abs() > 0.5 {
            if let Some(hit) = response.hover_pos().and_then(|p| nearest(marks, p)) {
                if !hit.draggable {
                    return Some(Action::Refused(hit.section));
                }
                return Some(Action::Width {
                    section: hit.section,
                    notches: scroll / 50.0,
                });
            }
        }
    }
    if response.clicked() {
        let at = response.interact_pointer_pos()?;
        let hit = nearest(marks, at)?;
        return Some(Action::Select(hit.section));
    }
    None
}

fn draw_handles(p: &Painter, theme: &Theme, marks: &[Handle], selected: Option<usize>) {
    for h in marks {
        let current = selected == Some(h.section);
        let base = theme.data.stage[h.section % theme.data.stage.len()];
        let ink = if current { base } else { base.gamma_multiply(0.55) };
        let r = if current { 11.0 } else { 8.5 };
        let pegged = h.offscale != 0;
        p.circle_filled(h.pos, r, if pegged { ink } else { theme.well.bg });
        p.circle_stroke(h.pos, r, Stroke::new(if current { 2.5 } else { 1.4 }, ink));
        p.text(
            h.pos,
            Align2::CENTER_CENTER,
            format!("{}", h.section + 1),
            theme.num(),
            if pegged { theme.well.bg } else { ink },
        );
        if !h.draggable {
            let d = r * 0.72;
            p.line_segment(
                [
                    pos2(h.pos.x - d, h.pos.y - d),
                    pos2(h.pos.x + d, h.pos.y + d),
                ],
                Stroke::new(1.2, ink),
            );
        }
    }
}

fn graticule(p: &Painter, theme: &Theme, split: &Split, axis: &DbAxis, grid: &ColumnGrid, ppp: f32) {
    let hair = Stroke::new(1.0 / ppp, theme.well.rule);
    let major = Stroke::new(1.0 / ppp, theme.well.grid);
    let mut db = (MAIN_DB_LO / 6.0).ceil() * 6.0;
    while db <= MAIN_DB_HI {
        let y = axis.y_of(db);
        p.hline(split.main.x_range(), y, if db == 0.0 { major } else { hair });
        if db != 0.0 {
            p.text(
                pos2(split.main.right() - 4.0, y - 1.0),
                Align2::RIGHT_BOTTOM,
                format!("{}{}", if db > 0.0 { "+" } else { "" }, db as i64),
                theme.num(),
                theme.well.dim,
            );
        }
        db += 6.0;
    }
    for hz in HZ_TICKS {
        let x = grid.x_of_hz(split.main.left(), hz);
        p.vline(
            x,
            split.main.y_range(),
            if hz == 1000.0 { major } else { hair },
        );
        let lx = x.clamp(split.main.left() + 14.0, split.main.right() - 14.0);
        p.text(
            pos2(lx, split.axis.center().y),
            Align2::CENTER_CENTER,
            hz_label(hz),
            theme.num(),
            theme.well.dim,
        );
    }
}

fn fill_to_zero(
    p: &Painter,
    rect: Rect,
    axis: &DbAxis,
    grid: &ColumnGrid,
    cols: &[f32],
    color: Color32,
) {
    if cols.len() < 2 {
        return;
    }
    let base = axis.y_of(0.0);
    let mut mesh = egui::Mesh::default();
    for (col, &v) in cols.iter().enumerate() {
        if v.is_nan() {
            continue;
        }
        let x = grid.x_of_column(rect.left(), col);
        let y = axis.y_of(v as f64).clamp(rect.top(), rect.bottom());
        mesh.colored_vertex(pos2(x, base), color);
        mesh.colored_vertex(pos2(x, y), color);
    }
    let quads = mesh.vertices.len() / 2;
    for q in 0..quads.saturating_sub(1) {
        let i = (q * 2) as u32;
        mesh.add_triangle(i, i + 1, i + 2);
        mesh.add_triangle(i + 1, i + 2, i + 3);
    }
    if quads > 1 {
        p.add(egui::Shape::mesh(mesh));
    }
}

#[allow(clippy::too_many_arguments)]
fn stroke(
    p: &Painter,
    rect: Rect,
    axis: &DbAxis,
    grid: &ColumnGrid,
    ppp: f32,
    color: Color32,
    width_px: f32,
    cols: &[f32],
    points: &mut Vec<Pos2>,
) {
    let line = Stroke::new(width_px / ppp, color);
    let over = Stroke::new(1.0 / ppp, color.gamma_multiply(0.5));
    points.clear();
    let mut last_y = f32::NAN;
    for (col, &db) in cols.iter().enumerate() {
        if db.is_nan() {
            if points.len() > 1 {
                p.line(std::mem::take(points), line);
            }
            points.clear();
            last_y = f32::NAN;
            continue;
        }
        let x = grid.x_of_column(rect.left(), col);
        let raw = axis.y_of(db as f64);
        if raw < rect.top() || raw > rect.bottom() {
            if !last_y.is_nan() {
                let (a, b) = if raw < rect.top() {
                    (rect.top(), rect.top() + 7.0)
                } else {
                    (rect.bottom() - 7.0, rect.bottom())
                };
                p.line_segment([pos2(x, a), pos2(x, b)], over);
            }
            if points.len() > 1 {
                p.line(std::mem::take(points), line);
            }
            points.clear();
            last_y = f32::NAN;
            continue;
        }
        let mut y = raw;
        if (y - last_y).abs() * ppp < 0.15 {
            y = last_y;
        }
        last_y = y;
        points.push(pos2(x, y));
    }
    if points.len() > 1 {
        p.line(std::mem::take(points), line);
    }
}

#[allow(clippy::too_many_arguments)]
fn error_strip(
    ui: &Ui,
    p: &Painter,
    theme: &Theme,
    split: &Split,
    grid: &ColumnGrid,
    model: &Model<'_>,
    ppp: f32,
    cols: &mut Vec<f32>,
    points: &mut Vec<Pos2>,
) {
    if split.error.height() < 2.0 {
        return;
    }

    let residual = model.target.map(|target| {
        peak_hold_into(
            grid,
            difference(
                sampled_db(grid, target),
                cascade_db(grid, model.judged.unwrap_or(model.biquads)),
            ),
            cols,
        );
        let peak = cols
            .iter()
            .filter(|v| !v.is_nan())
            .fold(0.0f32, |m, v| m.max(v.abs()));
        [6.0f32, 12.0, 24.0, 48.0, 96.0]
            .into_iter()
            .find(|s| peak <= *s)
            .unwrap_or(96.0)
    });
    let span = residual.unwrap_or(ERROR_DB_SPAN as f32);
    let axis = DbAxis::new(split.error, -(span as f64), span as f64);

    p.rect_stroke(
        split.error,
        theme.radius(),
        Stroke::new(1.0 / ppp, theme.well.rule),
        StrokeKind::Inside,
    );
    p.hline(
        split.error.x_range(),
        axis.y_of(0.0),
        Stroke::new(1.0 / ppp, theme.well.grid),
    );

    let strip = ui.interact(split.error, ui.id().with("response.error"), Sense::hover());
    strip.widget_info(|| egui::WidgetInfo::labeled(egui::WidgetType::Other, true, "ERROR"));

    p.text(
        pos2(split.error.left() + 4.0, split.error.top() + 2.0),
        Align2::LEFT_TOP,
        format!("ERROR \u{b1}{} dB", span as i64),
        theme.num(),
        theme.well.dim,
    );
    if let Some(rms) = model.rms_db {
        p.text(
            pos2(split.error.right() - 4.0, split.error.top() + 2.0),
            Align2::RIGHT_TOP,
            format!("{rms:.2} dB RMS"),
            theme.num(),
            theme.well.ink,
        );
    }

    if residual.is_none() {
        return;
    }
    let clip = p.with_clip_rect(split.error);
    stroke(
        &clip, split.error, &axis, grid, ppp, theme.data.error, 1.0, cols, points,
    );
}
