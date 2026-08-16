use eframe::egui::{Align2, Color32, Id, Painter, Pos2, Rect, Sense, Stroke, Ui, Vec2};
use trench_core::cascade::NUM_STAGES;

use crate::domain::document::lane_is_empty;
use crate::engine::response::{row_db, SR};
use crate::session::command::Command;
use crate::session::state::Session;
use crate::ui::oled::OledDisplay;
use crate::ui::{paint, theme};

pub fn draw(session: &Session, ui: &mut Ui) -> Vec<Command> {
    let mut cmds = Vec::new();
    let rect = ui.max_rect();
    let painter = ui.painter().clone();
    painter.rect_filled(rect, 0.0, theme::CHROME);

    let pad = 8.0_f32;
    let top_bar_h = 28.0_f32;
    let deck_h = (rect.height() - top_bar_h - pad * 4.0) * 0.28;
    let mid_h = (rect.height() - top_bar_h - pad * 4.0) * 0.28;
    let bottom_h = rect.height() - top_bar_h - deck_h - mid_h - pad * 5.0;

    // 1. TOP HEADER BAR
    let bar_rect = Rect::from_min_size(
        Pos2::new(rect.left() + pad, rect.top() + pad),
        Vec2::new(rect.width() - pad * 2.0, top_bar_h),
    );
    draw_top_bar(session, ui, &painter, bar_rect, &mut cmds);

    // 2. TOP DECK (Left: PCA Score Field / Center: Morpheus 128x64 OLED / Right: 3D Audition Ride Square)
    let deck_rect = Rect::from_min_size(
        Pos2::new(rect.left() + pad, bar_rect.bottom() + pad),
        Vec2::new(rect.width() - pad * 2.0, deck_h),
    );
    let oled_w = 260.0_f32.min(deck_rect.width() * 0.30);
    let rem_w = (deck_rect.width() - oled_w - pad * 2.0) * 0.5;

    let pca_rect = Rect::from_min_size(deck_rect.left_top(), Vec2::new(rem_w, deck_h));
    let oled_rect = Rect::from_min_size(Pos2::new(pca_rect.right() + pad, deck_rect.top()), Vec2::new(oled_w, deck_h));
    let ride_rect = Rect::from_min_size(Pos2::new(oled_rect.right() + pad, deck_rect.top()), Vec2::new(rem_w, deck_h));

    draw_pca_field(session, ui, &painter, pca_rect, &mut cmds);
    draw_oled_screen(session, &painter, oled_rect);
    draw_ride_square(session, ui, &painter, ride_rect, &mut cmds);

    // 3. MID DECK: 7-Stage Cascade Flow Inspector
    let mid_rect = Rect::from_min_size(
        Pos2::new(rect.left() + pad, deck_rect.bottom() + pad),
        Vec2::new(rect.width() - pad * 2.0, mid_h),
    );
    draw_cascade_inspector(session, ui, &painter, mid_rect, &mut cmds);

    // 4. BOTTOM DECK: Whole Response & Log-Polar Roots Canvas
    let bottom_rect = Rect::from_min_size(
        Pos2::new(rect.left() + pad, mid_rect.bottom() + pad),
        Vec2::new(rect.width() - pad * 2.0, bottom_h),
    );
    draw_bottom_deck(session, ui, &painter, bottom_rect, &mut cmds);

    cmds
}

fn draw_top_bar(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    bar: Rect,
    cmds: &mut Vec<Command>,
) {
    let bar_inner = paint::raised(painter, bar);
    painter.rect_filled(bar_inner, 0.0, theme::TITLEBAR);

    paint::label(
        painter,
        Pos2::new(bar_inner.left() + 8.0, bar_inner.center().y),
        Align2::LEFT_CENTER,
        "TRENCH WORKSTATION // 7-STAGE Z-PLANE CASCADE",
        theme::SMALL,
        theme::CURSOR,
    );

    let btn_w = 60.0_f32;
    let btn_h = bar_inner.height() - 4.0;
    let mut bx = bar_inner.right() - btn_w - 4.0;

    let undo_rect = Rect::from_min_size(Pos2::new(bx, bar_inner.top() + 2.0), Vec2::new(btn_w, btn_h));
    if paint::button(ui, painter, undo_rect, Id::new("top.undo"), "UNDO", false) {
        cmds.push(Command::Undo);
    }
    bx -= btn_w + 4.0;

    let redo_rect = Rect::from_min_size(Pos2::new(bx, bar_inner.top() + 2.0), Vec2::new(btn_w, btn_h));
    if paint::button(ui, painter, redo_rect, Id::new("top.redo"), "REDO", false) {
        cmds.push(Command::Redo);
    }
    bx -= btn_w + 4.0;

    let write_rect = Rect::from_min_size(Pos2::new(bx, bar_inner.top() + 2.0), Vec2::new(btn_w + 10.0, btn_h));
    if paint::button(ui, painter, write_rect, Id::new("top.write"), "WRITE 560B", false) {
        cmds.push(Command::WriteField);
    }
    bx -= btn_w + 14.0;

    let fit_rect = Rect::from_min_size(Pos2::new(bx, bar_inner.top() + 2.0), Vec2::new(btn_w, btn_h));
    if paint::button(ui, painter, fit_rect, Id::new("top.fit"), "FIT ALL", false) {
        cmds.push(Command::FitAllGrayOrder);
    }
    bx -= btn_w + 4.0;

    let new_rect = Rect::from_min_size(Pos2::new(bx, bar_inner.top() + 2.0), Vec2::new(btn_w, btn_h));
    if paint::button(ui, painter, new_rect, Id::new("top.new"), "NEW", false) {
        cmds.push(Command::NewSession);
    }

    if let Some((_, text)) = &session.notice {
        paint::label(
            painter,
            Pos2::new(bar_inner.left() + 380.0, bar_inner.center().y),
            Align2::LEFT_CENTER,
            text,
            theme::SMALL,
            theme::NOW,
        );
    }
}

fn draw_pca_field(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
) {
    let inner = paint::well(painter, rect);
    paint::label(
        painter,
        Pos2::new(inner.left() + 6.0, inner.top() + 4.0),
        Align2::LEFT_TOP,
        "PCA CORNER SCORE MANIFOLD (8 CORNERS)",
        theme::SMALL,
        theme::WELL_DIM,
    );

    // Draw coordinate axes
    let center = inner.center();
    painter.line_segment(
        [Pos2::new(inner.left() + 20.0, center.y), Pos2::new(inner.right() - 20.0, center.y)],
        Stroke::new(1.0_f32, theme::GRATICULE),
    );
    painter.line_segment(
        [Pos2::new(center.x, inner.top() + 20.0), Pos2::new(center.x, inner.bottom() - 20.0)],
        Stroke::new(1.0_f32, theme::GRATICULE),
    );

    // Draw 8 corners
    for ci in 0..8 {
        let m = (ci & 1) != 0;
        let q = (ci & 2) != 0;
        let t = (ci & 4) != 0;

        let ox = if m { 0.35 } else { -0.35 } + (if t { 0.1 } else { -0.1 });
        let oy = if q { -0.3 } else { 0.3 } + (if t { -0.1 } else { 0.1 });

        let px = center.x + (ox as f32) * (inner.width() * 0.45);
        let py = center.y + (oy as f32) * (inner.height() * 0.45);
        let p_pos = Pos2::new(px, py);
        let p_rect = Rect::from_center_size(p_pos, Vec2::splat(16.0));

        let selected = session.selection.corner == Some(ci);
        let color = if selected { theme::HOT } else { theme::LANES[ci % 7] };

        painter.circle_filled(p_pos, 7.0, color);
        painter.circle_stroke(p_pos, 7.0, Stroke::new(1.5_f32, theme::CURSOR));

        paint::label(
            painter,
            Pos2::new(px + 10.0, py),
            Align2::LEFT_CENTER,
            &format!("C{ci}"),
            theme::SMALL,
            theme::CURSOR,
        );

        let resp = ui.interact(p_rect, Id::new(("pca.corner", ci)), Sense::click_and_drag());
        if resp.clicked() {
            cmds.push(Command::SelectCorner(Some(ci)));
        }
    }
}

fn draw_oled_screen(session: &Session, painter: &Painter, rect: Rect) {
    let mut oled = OledDisplay::default();
    let active_c = session.selection.corner.unwrap_or(0);
    let preset_name = session.document.target.as_ref().map(|t| t.name.as_str()).unwrap_or("INIT CUBE");
    oled.paint(
        painter,
        rect,
        &session.document.field,
        active_c,
        &session.audition,
        preset_name,
    );
}

fn draw_ride_square(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
) {
    let inner = paint::well(painter, rect);
    paint::label(
        painter,
        Pos2::new(inner.left() + 6.0, inner.top() + 4.0),
        Align2::LEFT_TOP,
        "3D AUDITION RIDE SQUARE (HOLD TO HEAR, WHEEL FOR Z)",
        theme::SMALL,
        theme::WELL_DIM,
    );

    let resp = ui.interact(inner, Id::new("ride.surface"), Sense::click_and_drag());

    // Mouse wheel controls Z / Transform
    if resp.hovered() {
        let wheel = ui.input(|i| i.raw_scroll_delta.y);
        if wheel != 0.0 {
            let mut p = session.audition.pos;
            p[2] = (p[2] + wheel * 0.002).clamp(0.0, 1.0);
            cmds.push(Command::SetAuditionPos(p));
        }
    }

    if resp.dragged() || resp.is_pointer_button_down_on() {
        if let Some(ptr) = resp.interact_pointer_pos() {
            let u_x = ((ptr.x - inner.left()) / inner.width()).clamp(0.0, 1.0);
            let u_y = (1.0 - (ptr.y - inner.top()) / inner.height()).clamp(0.0, 1.0);
            let mut p = session.audition.pos;
            p[0] = u_x;
            p[1] = u_y;
            cmds.push(Command::SetAuditionPos(p));
            cmds.push(Command::SetAuditionPlaying(true));
        }
    } else if resp.drag_stopped() {
        cmds.push(Command::SetAuditionPlaying(false));
    }

    if resp.secondary_clicked() {
        cmds.push(Command::AddAuditionMarker(session.audition.pos));
    }

    if resp.middle_clicked() {
        cmds.push(Command::WriteField);
    }

    // Draw markers
    for (mi, &m_pos) in session.audition.markers.iter().enumerate() {
        let mx = inner.left() + m_pos[0] * inner.width();
        let my = inner.bottom() - m_pos[1] * inner.height();
        painter.circle_stroke(Pos2::new(mx, my), 5.0, Stroke::new(1.0_f32, theme::ECHO));
        paint::label(
            painter,
            Pos2::new(mx + 6.0, my - 6.0),
            Align2::LEFT_BOTTOM,
            &format!("M{mi}"),
            theme::SMALL * 0.8,
            theme::ECHO,
        );
    }

    // Draw live audition puck
    let live_x = inner.left() + session.audition.pos[0] * inner.width();
    let live_y = inner.bottom() - session.audition.pos[1] * inner.height();
    let live_pos = Pos2::new(live_x, live_y);

    let puck_color = if session.audition.playing { theme::NOW } else { theme::ASK };
    painter.circle_filled(live_pos, 6.0, puck_color);
    painter.circle_stroke(live_pos, 6.0, Stroke::new(1.5_f32, theme::CURSOR));

    // Live coordinate HUD
    let hud = format!(
        "M: {:.2}  Q: {:.2}  T: {:.2}",
        session.audition.pos[0], session.audition.pos[1], session.audition.pos[2]
    );
    paint::label(
        painter,
        Pos2::new(inner.right() - 6.0, inner.bottom() - 4.0),
        Align2::RIGHT_BOTTOM,
        &hud,
        theme::SMALL,
        theme::CURSOR,
    );
}

fn draw_cascade_inspector(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
) {
    let inner = paint::sunken(painter, rect);
    let gap = 4.0_f32;
    let col_w = (inner.width() - gap * (NUM_STAGES as f32 - 1.0)) / (NUM_STAGES as f32);
    let rows = session.current_interpolated_rows();

    for si in 0..NUM_STAGES {
        let cx = inner.left() + (si as f32) * (col_w + gap);
        let col_rect = Rect::from_min_size(Pos2::new(cx, inner.top()), Vec2::new(col_w, inner.height()));
        let cell_inner = paint::well(painter, col_rect);

        let selected = session.selection.section == Some(si);
        if selected {
            painter.rect_stroke(col_rect, 0.0, Stroke::new(1.5_f32, theme::HOT));
        }

        let stage_name = format!("STAGE S{}", si + 1);
        paint::label(
            painter,
            Pos2::new(cell_inner.left() + 4.0, cell_inner.top() + 3.0),
            Align2::LEFT_TOP,
            &stage_name,
            theme::SMALL * 0.85,
            theme::LANES[si % 7],
        );

        let row = rows[si];
        if row != [1.0, 0.0, 0.0, 0.0, 0.0] {
            paint::x3_trace(
                painter,
                cell_inner,
                -30.0,
                30.0,
                theme::LANES[si % 7],
                &move |hz| row_db(&row, hz, SR),
            );
        } else {
            paint::label(
                painter,
                cell_inner.center(),
                Align2::CENTER_CENTER,
                "—",
                theme::SMALL,
                theme::WELL_DIM,
            );
        }

        let resp = ui.interact(col_rect, Id::new(("cascade.stage", si)), Sense::click());
        if resp.clicked() {
            cmds.push(Command::SelectSection(if selected { None } else { Some(si) }));
        }
    }
}

fn draw_bottom_deck(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
) {
    let half_w = (rect.width() - 8.0) * 0.5;
    let resp_rect = Rect::from_min_size(rect.left_top(), Vec2::new(half_w, rect.height()));
    let roots_rect = Rect::from_min_size(Pos2::new(resp_rect.right() + 8.0, rect.top()), Vec2::new(half_w, rect.height()));

    draw_whole_response(session, painter, resp_rect);
    draw_roots_canvas(session, ui, painter, roots_rect, cmds);
}

fn draw_whole_response(
    session: &Session,
    painter: &Painter,
    rect: Rect,
) {
    let inner = paint::well(painter, rect);
    paint::label(
        painter,
        Pos2::new(inner.left() + 6.0, inner.top() + 4.0),
        Align2::LEFT_TOP,
        "WHOLE CASCADE SPECTRUM & SAFETY AUDIT GATES",
        theme::SMALL,
        theme::WELL_DIM,
    );

    let lo = -30.0;
    let hi = 45.0;
    paint::draw_graticule(painter, inner, lo, hi);

    // Draw safety crown bound lines
    let crown_top_y = paint::db_y(36.0, inner, lo, hi);
    painter.line_segment(
        [Pos2::new(inner.left(), crown_top_y), Pos2::new(inner.right(), crown_top_y)],
        Stroke::new(1.0_f32, theme::ALARM),
    );
    paint::label(
        painter,
        Pos2::new(inner.right() - 4.0, crown_top_y - 2.0),
        Align2::RIGHT_BOTTOM,
        "+36dB CROWN CEILING",
        theme::SMALL * 0.8,
        theme::ALARM,
    );

    let rows = session.current_interpolated_rows();
    paint::x3_trace(
        painter,
        inner,
        lo,
        hi,
        theme::NOW,
        &move |hz| rows.iter().map(|r| row_db(r, hz, SR)).sum(),
    );
}

fn draw_roots_canvas(
    session: &Session,
    ui: &mut Ui,
    painter: &Painter,
    rect: Rect,
    cmds: &mut Vec<Command>,
) {
    let inner = paint::well(painter, rect);
    paint::label(
        painter,
        Pos2::new(inner.left() + 6.0, inner.top() + 4.0),
        Align2::LEFT_TOP,
        "LOG-POLAR ROOTS CANVAS (R' dB vs LOG f)",
        theme::SMALL,
        theme::WELL_DIM,
    );

    // Draw encoder limit ceiling
    let ceil_y = inner.top() + 18.0;
    painter.line_segment(
        [Pos2::new(inner.left(), ceil_y), Pos2::new(inner.right(), ceil_y)],
        Stroke::new(1.0_f32, theme::HOT),
    );
    paint::label(
        painter,
        Pos2::new(inner.right() - 4.0, ceil_y - 2.0),
        Align2::RIGHT_BOTTOM,
        "ENCODER CEILING (R < 1.0)",
        theme::SMALL * 0.8,
        theme::HOT,
    );

    let lanes = session.active_lanes();

    for (si, l) in lanes.iter().enumerate() {
        if lane_is_empty(l) {
            continue;
        }
        let color = theme::LANES[si % 7];
        let px = paint::log_x(l.pole_hz, inner);
        let pr_db = 20.0 * (1.0 / (1.0 - l.pole_r.clamp(0.0, 0.9999))).log10();
        let py = paint::db_y(pr_db, inner, 0.0, 60.0);
        let p_pos = Pos2::new(px, py.clamp(ceil_y, inner.bottom() - 4.0));

        let zx = paint::log_x(l.zero_hz, inner);
        let zr_db = 20.0 * (1.0 / (1.0 - l.zero_r.clamp(0.0, 0.9999))).log10();
        let zy = paint::db_y(zr_db, inner, 0.0, 60.0);
        let z_pos = Pos2::new(zx, zy.clamp(inner.top() + 4.0, inner.bottom() - 4.0));

        // Draw cord linking pole and zero
        painter.line_segment([p_pos, z_pos], Stroke::new(1.0_f32, theme::faded(color, 120)));

        // Draw Pole puck (filled)
        painter.circle_filled(p_pos, 5.0, color);
        painter.circle_stroke(p_pos, 5.0, Stroke::new(1.0_f32, theme::CURSOR));

        // Draw Zero puck (open ring)
        painter.circle_stroke(z_pos, 5.0, Stroke::new(1.5_f32, color));

        let p_rect = Rect::from_center_size(p_pos, Vec2::splat(12.0));
        let p_resp = ui.interact(p_rect, Id::new(("pole.puck", si)), Sense::drag());
        if p_resp.dragged() {
            if let Some(ptr) = p_resp.interact_pointer_pos() {
                let new_hz = paint::hz_at_x(ptr.x, inner);
                let t_y = ((inner.bottom() - ptr.y) / inner.height()).clamp(0.0, 1.0);
                let new_r_db = (t_y as f64) * 60.0;
                let new_r = 1.0 - 10f64.powf(-new_r_db / 20.0);
                cmds.push(Command::SetPole { section: si, hz: new_hz, r: new_r });
            }
        }
    }
}
