//! Every screen of the Station, drawn mark by mark.
//!
//! Nothing here knows how many frames, axes or lanes exist. Counts come from
//! the project's declared topology, and layout is computed from those counts,
//! so a cube and a 4D object render through the same code.

use eframe::egui::{Pos2, Rect, Vec2};

use crate::app::{PathIntent, Station, Workspace, CMD_H, HEAD_H, PAD, STATUS_H, TAB_H};
use crate::model::analysis;
use crate::model::law;
use crate::model::project::{Origin, PackedCapability};
use crate::ui::input::{Id, Ui};
use crate::ui::paint::{
    self, curve, curve_fill, db_rules, divider, fill, frequency_rules, hairline, outline, text,
    text_right,
};
use crate::ui::theme;
use crate::ui::widgets::{self, FieldStyle, Scale};

pub fn draw(st: &mut Station, ui: &mut Ui, full: Rect) {
    let head = Rect::from_min_size(full.min, Vec2::new(full.width(), HEAD_H));
    let cmd = Rect::from_min_size(
        Pos2::new(full.left(), head.bottom()),
        Vec2::new(full.width(), CMD_H),
    );
    let tabs = Rect::from_min_size(
        Pos2::new(full.left(), cmd.bottom() + 2.0),
        Vec2::new(full.width(), TAB_H),
    );
    let status = Rect::from_min_size(
        Pos2::new(full.left(), full.bottom() - STATUS_H),
        Vec2::new(full.width(), STATUS_H),
    );
    let body = Rect::from_min_max(
        Pos2::new(full.left() + PAD, tabs.bottom() + PAD),
        Pos2::new(full.right() - PAD, status.top() - PAD),
    );

    header(st, ui, head);
    commands(st, ui, cmd);
    tab_strip(st, ui, tabs);

    match st.workspace {
        Workspace::Ingestion => crate::views_ingest::draw(st, ui, body),
        Workspace::Perceptual => crate::views_perceptual::draw(st, ui, body),
        Workspace::Topology => topology_workspace(st, ui, body),
        Workspace::Runtime => runtime_workspace(st, ui, body),
    }

    // The section editor takes the whole working area when a corner is open.
    if st.sos_corner.is_some() {
        crate::views_sos::draw(st, ui, body);
    }

    status_bar(st, ui, status);
    if st.path_bar.is_some() {
        path_bar(st, ui, full);
    }
}

// ── chrome ──────────────────────────────────────────────────────────────

fn header(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    hairline(
        ui.p,
        Pos2::new(r.left(), r.bottom()),
        Pos2::new(r.right(), r.bottom()),
        theme::RULE,
    );
    text(
        ui.p,
        r.min + Vec2::new(10.0, 6.0),
        "STATION",
        theme::T_HEAD,
        theme::INK_HI,
    );

    let origin = match st.project.origin {
        Origin::Native => "native",
        Origin::PackedNative => "packed 560",
        Origin::PackedLegacy => "packed 240",
    };
    let path = st
        .path
        .as_ref()
        .map(|p| p.display().to_string())
        .unwrap_or_else(|| "unsaved".into());
    text(
        ui.p,
        r.min + Vec2::new(92.0, 8.0),
        format!(
            "{}   {origin}   {} axes   {} frames   {} lanes   {} Hz",
            st.project.name,
            st.project.topology.axis_count(),
            st.project.frames.len(),
            st.project.lane_count(),
            st.project.sample_rate() as i64,
        ),
        theme::T_SMALL,
        theme::DIM,
    );

    // Dirty state is a mark, not a word, so it reads at a glance.
    if st.dirty() {
        ui.p.circle_filled(Pos2::new(r.right() - 12.0, r.center().y), 3.5, theme::DIRTY);
    }
    text_right(
        ui.p,
        Pos2::new(r.right() - 24.0, r.top() + 8.0),
        path,
        theme::T_SMALL,
        if st.dirty() { theme::DIRTY } else { theme::DIM },
    );
}

fn commands(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::BG);
    let id = Id::of("cmd");
    let labels: [(&str, bool); 9] = [
        ("NEW", true),
        ("NEW 4D", true),
        ("OPEN", true),
        ("SAVE", true),
        ("SAVE AS", true),
        ("EXPORT", true),
        ("UNDO", st.history.can_undo()),
        ("REDO", st.history.can_redo()),
        ("RELOAD LAWS", true),
    ];
    let w = 78.0;
    for (i, (label, enabled)) in labels.iter().enumerate() {
        let b = Rect::from_min_size(
            Pos2::new(r.left() + PAD + i as f32 * (w + 4.0), r.top() + 2.0),
            Vec2::new(w, r.height() - 6.0),
        );
        if widgets::button(ui, id.child(i), b, label, *enabled) {
            match i {
                0 => st.new_project(),
                1 => st.new_4d_project(),
                2 => st.open_path_bar(PathIntent::Open),
                3 => st.save(),
                4 => st.open_path_bar(PathIntent::SaveAs),
                5 => st.open_path_bar(PathIntent::ExportPacked),
                6 => st.undo(),
                7 => st.redo(),
                _ => {
                    let p = st.laws_path.clone();
                    match std::fs::read_to_string(&p) {
                        Ok(t) => {
                            st.laws_editor.set(t);
                            st.apply_laws();
                        }
                        Err(e) => st.say(format!("{}: {e}", p.display()), true),
                    }
                }
            }
        }
    }

    // Export capability is stated here, next to the button that would do it,
    // rather than discovered after a failure.
    let cap = st.project.packed_capability_text();
    let refused = matches!(st.project.packed_capability(), PackedCapability::Refused(_));
    text_right(
        ui.p,
        Pos2::new(r.right() - PAD, r.top() + 7.0),
        format!("EXPORT  {cap}"),
        theme::T_SMALL,
        if refused { theme::BAD } else { theme::GOOD },
    );
}

fn tab_strip(st: &mut Station, ui: &mut Ui, r: Rect) {
    let id = Id::of("tabs");
    let w = 178.0;
    for (i, t) in Workspace::ALL.iter().enumerate() {
        let b = Rect::from_min_size(
            Pos2::new(r.left() + PAD + i as f32 * (w + 4.0), r.top()),
            Vec2::new(w, r.height()),
        );
        if widgets::tab(
            ui,
            id.child(i),
            b,
            &format!("{:02}", i + 1),
            t.label(),
            st.workspace == *t,
        ) {
            st.workspace = *t;
        }
    }
}

fn status_bar(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    hairline(
        ui.p,
        Pos2::new(r.left(), r.top()),
        Pos2::new(r.right(), r.top()),
        theme::RULE,
    );
    let readings = law::read_all(&st.project, &st.laws.laws, st.selected_frame);
    let pass = readings.iter().filter(|x| x.ok).count();
    let total = readings.len();
    text(
        ui.p,
        r.min + Vec2::new(10.0, 6.0),
        if total == 0 {
            "no laws".to_string()
        } else {
            format!("{pass}/{total} laws hold")
        },
        theme::T_SMALL,
        if total == 0 {
            theme::DIM
        } else if pass == total {
            theme::GOOD
        } else {
            theme::BAD
        },
    );
    text(
        ui.p,
        r.min + Vec2::new(140.0, 6.0),
        &st.note,
        theme::T_SMALL,
        if st.note_bad { theme::BAD } else { theme::DIM },
    );
    text_right(
        ui.p,
        Pos2::new(r.right() - 10.0, r.top() + 6.0),
        "ctrl S save   ctrl O open   ctrl E export   ctrl Z undo   drop a file to import",
        theme::T_MICRO,
        theme::FAINT,
    );
}

/// The custom path entry: the Station never opens a system dialog.
fn path_bar(st: &mut Station, ui: &mut Ui, full: Rect) {
    let r = Rect::from_center_size(full.center(), Vec2::new(680.0, 96.0));
    fill(ui.p, r, theme::PANEL_HI);
    outline(ui.p, r, theme::ACCENT);
    let title = match st.path_bar {
        Some(PathIntent::Open) => "OPEN  —  station project or packed body",
        Some(PathIntent::SaveAs) => "SAVE AS  —  station project path",
        Some(PathIntent::ExportPacked) => "EXPORT PACKED  —  destination for the runtime body",
        None => "",
    };
    text(
        ui.p,
        r.min + Vec2::new(10.0, 8.0),
        title,
        theme::T_SMALL,
        theme::ACCENT,
    );
    let field = Rect::from_min_size(
        r.min + Vec2::new(10.0, 28.0),
        Vec2::new(r.width() - 20.0, 26.0),
    );
    let id = Id::of("path-entry");
    ui.set_focus(id);
    st.path_entry.show(ui, id, field);

    let ok = Rect::from_min_size(
        Pos2::new(r.right() - 170.0, r.bottom() - 30.0),
        Vec2::new(74.0, 22.0),
    );
    let cancel = Rect::from_min_size(
        Pos2::new(r.right() - 90.0, r.bottom() - 30.0),
        Vec2::new(74.0, 22.0),
    );
    let enter = ui.input.key(eframe::egui::Key::Enter);
    let esc = ui.input.key(eframe::egui::Key::Escape);
    if widgets::button(ui, id.child("ok"), ok, "CONFIRM", true) || enter {
        st.commit_path_bar();
    }
    if widgets::button(ui, id.child("cancel"), cancel, "CANCEL", true) || esc {
        st.path_bar = None;
    }
}

// ── 01 FRAMES ───────────────────────────────────────────────────────────

/// Every authored frame as its own response card, laid out from the declared
/// corner count. The grid is computed, never a fixed two-by-four.
pub fn frame_cards(st: &mut Station, ui: &mut Ui, r: Rect) {
    let grid_r = r;

    let n = st.project.frames.len();
    if n == 0 {
        return;
    }
    // Column count follows the object: the second display axis splits the grid
    // into planes, so a cube reads as floor and ceiling and a 4D object reads
    // as four planes.
    let cols = ((n as f32).sqrt().ceil() as usize).clamp(1, 4);
    let rows = n.div_ceil(cols);
    let cw = (grid_r.width() - PAD * (cols - 1) as f32) / cols as f32;
    let ch = (grid_r.height() - 14.0 - PAD * (rows.saturating_sub(1)) as f32) / rows as f32;

    let sr = st.project.sample_rate();
    let (lo_hz, hi_hz) = (st.grammar.display_lo_hz, st.grammar.display_hi_hz);
    let id = Id::of("frames");

    for i in 0..n {
        let (cx, cy) = (i % cols, i / cols);
        let card = Rect::from_min_size(
            Pos2::new(
                grid_r.left() + cx as f32 * (cw + PAD),
                grid_r.top() + 14.0 + cy as f32 * (ch + PAD),
            ),
            Vec2::new(cw, ch),
        );
        let color = theme::frame_color(i, n);
        let selected = i == st.selected_frame;
        let resp = widgets::row(ui, id.child(i), card, selected, Some(color));
        if resp.clicked {
            st.selected_frame = i;
            // Selecting a frame parks the working position on it, so the live
            // response and the card agree.
            let addr = st.project.frames[i].address.clone();
            st.coords = addr.iter().map(|&c| c as f32).collect();
            st.touch();
        }
        outline(ui.p, card, if selected { color } else { theme::RULE });

        let plot = Rect::from_min_max(
            Pos2::new(card.left() + 8.0, card.top() + 20.0),
            Pos2::new(card.right() - 8.0, card.bottom() - 6.0),
        );
        let resp_curve = analysis::analyse(&st.project.frames[i].values, sr);
        let (lo_db, hi_db) = resp_curve.display_span();
        frequency_rules(ui.p, plot, lo_hz, hi_hz, false);
        db_rules(ui.p, plot, lo_db, hi_db, false);
        curve_fill(
            ui.p,
            plot,
            &resp_curve.grid,
            &resp_curve.total,
            lo_hz,
            hi_hz,
            lo_db,
            hi_db,
            color,
        );
        curve(
            ui.p,
            plot,
            &resp_curve.grid,
            &resp_curve.total,
            lo_hz,
            hi_hz,
            lo_db,
            hi_db,
            color,
            1.6,
        );

        paint::chip(
            ui.p,
            card.min + Vec2::new(6.0, 4.0),
            &format!("C{:02}", i + 1),
            color,
        );
        text(
            ui.p,
            card.min + Vec2::new(46.0, 5.0),
            frame_label(st, i),
            theme::T_SMALL,
            if selected { theme::INK_HI } else { theme::INK },
        );
        text_right(
            ui.p,
            Pos2::new(card.right() - 6.0, card.top() + 5.0),
            format!("{:+.1} dB", resp_curve.total_peak_db()),
            theme::T_MICRO,
            theme::DIM,
        );
    }
}

pub fn frame_label(st: &Station, i: usize) -> String {
    let f = &st.project.frames[i];
    st.project
        .topology
        .axes
        .iter()
        .enumerate()
        .zip(&f.address)
        .map(|((ai, ax), &c)| {
            format!(
                "{}{}",
                st.grammar.axis_name(ai, &ax.name),
                if c != 0 { "100" } else { "0" }
            )
        })
        .collect::<Vec<_>>()
        .join(" ")
}

/// One slider per declared axis, plus the choice of which two axes the working
/// projection shows. Axes that are not displayed keep an explicit coordinate.
pub fn axis_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        "WORKING POSITION",
        theme::T_MICRO,
        theme::DIM,
    );
    divider(
        ui.p,
        Pos2::new(r.left() + 8.0, r.top() + 22.0),
        Pos2::new(r.right() - 8.0, r.top() + 22.0),
        theme::RULE,
    );

    let id = Id::of("axes");
    let n = st.project.topology.axis_count();
    let mut y = r.top() + 32.0;
    for ai in 0..n {
        let name = st
            .grammar
            .axis_name(ai, &st.project.topology.axes[ai].name.clone());
        let displayed = ai == st.display_axes.0 || ai == st.display_axes.1;
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y),
            &name,
            theme::T_MICRO,
            if displayed { theme::ACCENT } else { theme::DIM },
        );
        text_right(
            ui.p,
            Pos2::new(r.right() - 8.0, y),
            format!("{:.0}", st.coords[ai] * 100.0),
            theme::T_SMALL,
            theme::INK_HI,
        );
        let track = Rect::from_min_size(
            Pos2::new(r.left() + 8.0, y + 14.0),
            Vec2::new(r.width() - 16.0, 12.0),
        );
        if let Some(v) = widgets::slider(
            ui,
            id.child(ai),
            track,
            st.coords[ai] as f64,
            0.0,
            1.0,
            Scale::Linear,
        ) {
            st.coords[ai] = v as f32;
            st.touch();
        }
        // Choosing displayed axes: click the name row.
        let pick = Rect::from_min_size(Pos2::new(r.left() + 8.0, y - 2.0), Vec2::new(90.0, 14.0));
        if ui.region(id.child(("pick", ai)), pick).clicked {
            st.display_axes = (ai, st.display_axes.0);
        }
        y += 36.0;
    }

    // The live response at the working position.
    let plot = Rect::from_min_max(
        Pos2::new(r.left() + 8.0, y + 6.0),
        Pos2::new(r.right() - 8.0, r.bottom() - 8.0),
    );
    if plot.height() > 40.0 {
        text(
            ui.p,
            Pos2::new(plot.left(), plot.top() - 12.0),
            "LIVE",
            theme::T_MICRO,
            theme::DIM,
        );
        let (lo_db, hi_db) = st.live.display_span();
        fill(ui.p, plot, theme::BG);
        outline(ui.p, plot, theme::RULE);
        frequency_rules(
            ui.p,
            plot,
            st.grammar.display_lo_hz,
            st.grammar.display_hi_hz,
            true,
        );
        db_rules(ui.p, plot, lo_db, hi_db, true);
        curve(
            ui.p,
            plot,
            &st.live.grid,
            &st.live.total,
            st.grammar.display_lo_hz,
            st.grammar.display_hi_hz,
            lo_db,
            hi_db,
            theme::ACCENT,
            1.8,
        );
    }
}

// ── 02 CASCADE ──────────────────────────────────────────────────────────

/// The signal so far. Sections multiply, so a safe total can hide a very loud
/// middle; the running product after each lane is what shows it.
pub fn cumulative_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    let plot_r = r;

    fill(ui.p, plot_r, theme::PANEL);
    outline(ui.p, plot_r, theme::RULE);
    text(
        ui.p,
        plot_r.min + Vec2::new(8.0, 6.0),
        "CUMULATIVE CASCADE  —  running product through each lane",
        theme::T_MICRO,
        theme::DIM,
    );

    let plot = Rect::from_min_max(
        Pos2::new(plot_r.left() + 8.0, plot_r.top() + 24.0),
        Pos2::new(plot_r.right() - 8.0, plot_r.bottom() - 8.0),
    );
    let (lo_hz, hi_hz) = (st.grammar.display_lo_hz, st.grammar.display_hi_hz);

    // The span covers every intermediate curve, not just the total, so an
    // intermediate peak cannot be cropped out of view.
    let mut hi_db = st.live.total_peak_db();
    let mut lo_db = st.live.total_min_db();
    for c in &st.live.cumulative {
        hi_db = hi_db.max(c.iter().cloned().fold(f64::MIN, f64::max));
        lo_db = lo_db.min(c.iter().cloned().fold(f64::MAX, f64::min));
    }
    if !hi_db.is_finite() || !lo_db.is_finite() {
        hi_db = 12.0;
        lo_db = -60.0;
    }
    lo_db = lo_db.max(hi_db - 120.0);
    let (lo_db, hi_db) = (lo_db - 3.0, hi_db + 3.0);

    frequency_rules(ui.p, plot, lo_hz, hi_hz, true);
    db_rules(ui.p, plot, lo_db, hi_db, true);

    let lanes = st.project.lane_count();
    for (i, c) in st.live.cumulative.iter().enumerate() {
        let col = theme::frame_color(i, lanes.max(1));
        let selected = i == st.selected_lane;
        curve(
            ui.p,
            plot,
            &st.live.grid,
            c,
            lo_hz,
            hi_hz,
            lo_db,
            hi_db,
            if selected {
                col
            } else {
                theme::mix(theme::PANEL, col, 0.55)
            },
            if selected { 2.0 } else { 1.0 },
        );
    }
    // The selected lane alone, so its own contribution can be told apart from
    // the running product it sits inside.
    if let Some(own) = st.live.per_lane.get(st.selected_lane) {
        curve(
            ui.p,
            plot,
            &st.live.grid,
            own,
            lo_hz,
            hi_hz,
            lo_db,
            hi_db,
            theme::WARN,
            1.2,
        );
    }
    curve(
        ui.p,
        plot,
        &st.live.grid,
        &st.live.total,
        lo_hz,
        hi_hz,
        lo_db,
        hi_db,
        theme::INK_HI,
        2.0,
    );
    text_right(
        ui.p,
        Pos2::new(plot.right() - 4.0, plot_r.top() + 6.0),
        "white total  ·  amber selected lane  ·  tinted running product",
        theme::T_MICRO,
        theme::FAINT,
    );
}

// ── 03 LANES ────────────────────────────────────────────────────────────

/// The lane matrix and the root editor. Editing writes through
/// `LaneValue::set_roots`, so the crate's own validator decides what may be
/// stored and a refusal leaves the lane untouched.
pub fn lane_matrix_panel(st: &mut Station, ui: &mut Ui, r: Rect) {
    let matrix_r = r;

    fill(ui.p, matrix_r, theme::PANEL);
    outline(ui.p, matrix_r, theme::RULE);
    text(
        ui.p,
        matrix_r.min + Vec2::new(8.0, 6.0),
        "LANE MATRIX      pole Hz / radius",
        theme::T_MICRO,
        theme::DIM,
    );

    let n_f = st.project.frames.len();
    let n_l = st.project.lane_count();
    if n_f == 0 || n_l == 0 {
        return;
    }
    let head_w = 108.0;
    let grid = Rect::from_min_max(
        Pos2::new(matrix_r.left() + head_w, matrix_r.top() + 40.0),
        Pos2::new(matrix_r.right() - 8.0, matrix_r.bottom() - 8.0),
    );
    let cw = grid.width() / n_f as f32;
    let rh = (grid.height() / n_l as f32).min(34.0);

    // Column heads: one per frame, in its identity colour.
    for fi in 0..n_f {
        let x = grid.left() + fi as f32 * cw;
        let c = theme::frame_color(fi, n_f);
        paint::chip(
            ui.p,
            Pos2::new(x + 2.0, matrix_r.top() + 22.0),
            &format!("C{:02}", fi + 1),
            c,
        );
    }

    let id = Id::of("matrix");
    let sr = st.project.sample_rate();
    let watch = st.grammar.pole_radius_watch;
    for li in 0..n_l {
        let y = grid.top() + li as f32 * rh;
        let name = st
            .grammar
            .lane_name(li, &st.project.lanes[li].id.to_string());
        text(
            ui.p,
            Pos2::new(matrix_r.left() + 8.0, y + 4.0),
            &name,
            theme::T_SMALL,
            if li == st.selected_lane {
                theme::ACCENT
            } else {
                theme::INK
            },
        );
        for fi in 0..n_f {
            let cell = Rect::from_min_size(
                Pos2::new(grid.left() + fi as f32 * cw, y),
                Vec2::new(cw - 2.0, rh - 2.0),
            );
            let selected = fi == st.selected_frame && li == st.selected_lane;
            let resp = widgets::row(ui, id.child((fi, li)), cell, selected, None);
            if resp.clicked {
                st.selected_frame = fi;
                st.selected_lane = li;
                let addr = st.project.frames[fi].address.clone();
                st.coords = addr.iter().map(|&c| c as f32).collect();
                st.touch();
            }
            let v = st.project.frames[fi].values[li];
            let g = v.geometry(sr);
            let (label, col) = match g.pole {
                trench_core::stage_law::RootPair::Conjugate { hz, r: rr } => (
                    format!("{hz:>6.0}  {rr:.3}"),
                    if rr >= watch { theme::BAD } else { theme::INK },
                ),
                _ if v.is_identity() => ("pass".to_string(), theme::FAINT),
                _ => ("real".to_string(), theme::WARN),
            };
            text(
                ui.p,
                cell.min + Vec2::new(5.0, 4.0),
                label,
                theme::T_MICRO,
                col,
            );
        }
    }
}

/// Pole, zero and scale for the selected lane at the selected frame, in the
/// representation the format actually stores.
pub fn root_editor(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);

    let fi = st
        .selected_frame
        .min(st.project.frames.len().saturating_sub(1));
    let li = st
        .selected_lane
        .min(st.project.lane_count().saturating_sub(1));
    if st.project.frames.is_empty() || st.project.lane_count() == 0 {
        return;
    }
    let sr = st.project.sample_rate();
    let lane = st.project.frames[fi].values[li];
    let name = st
        .grammar
        .lane_name(li, &st.project.lanes[li].id.to_string());
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        format!("EDIT  {}  ·  C{:02} {}", name, fi + 1, frame_label(st, fi)),
        theme::T_MICRO,
        theme::DIM,
    );
    text_right(
        ui.p,
        Pos2::new(r.right() - 8.0, r.top() + 6.0),
        format!(
            "words  {}",
            lane.words
                .iter()
                .map(|w| format!("{w:04X}"))
                .collect::<Vec<_>>()
                .join(" ")
        ),
        theme::T_MICRO,
        theme::FAINT,
    );

    let Some(roots) = lane.roots(sr) else {
        text(
            ui.p,
            r.min + Vec2::new(8.0, 40.0),
            if lane.is_identity() {
                "pass-through"
            } else {
                "real-axis roots — not editable as a conjugate pair"
            },
            theme::T_SMALL,
            theme::DIM,
        );
        // Authoring a pass-through lane into a real one, without inventing a
        // filter role for it.
        let b = Rect::from_min_size(
            Pos2::new(r.left() + 8.0, r.bottom() - 32.0),
            Vec2::new(150.0, 22.0),
        );
        if widgets::button(ui, Id::of("seed-lane"), b, "AUTHOR A POLE", true) {
            st.checkpoint();
            let seed = trench_core::stage_law::StageRoots {
                pole_hz: 1000.0,
                pole_r: 0.9,
                zero_hz: 1000.0,
                zero_r: 0.5,
                scale: 1.0,
            };
            let mut v = lane;
            match v.set_roots(&seed, sr) {
                Ok(()) => {
                    st.project.frames[fi].values[li] = v;
                    st.touch();
                    st.say("lane authored", false);
                }
                Err(e) => st.say(crate::model::lane::refusal_text(e), true),
            }
        }
        return;
    };

    let lim = trench_core::stage_law::authoring_limits_at(sr);
    let specs: [(FieldStyle, f64); 5] = [
        (
            FieldStyle {
                label: "POLE Hz",
                unit: "",
                decimals: 1,
                step: 0.004,
                lo: lim.display_freq_min_hz,
                hi: lim.authoring_freq_max_hz,
                scale: Scale::Log,
            },
            roots.pole_hz,
        ),
        (
            FieldStyle {
                label: "POLE r",
                unit: "",
                decimals: 4,
                step: 0.0008,
                lo: 0.0,
                hi: lim.pole_radius_max,
                scale: Scale::Linear,
            },
            roots.pole_r,
        ),
        (
            FieldStyle {
                label: "ZERO Hz",
                unit: "",
                decimals: 1,
                step: 0.004,
                lo: lim.display_freq_min_hz,
                hi: lim.authoring_freq_max_hz,
                scale: Scale::Log,
            },
            roots.zero_hz,
        ),
        (
            FieldStyle {
                label: "ZERO r",
                unit: "",
                decimals: 4,
                step: 0.0008,
                lo: 0.0,
                hi: lim.zero_radius_max,
                scale: Scale::Linear,
            },
            roots.zero_r,
        ),
        (
            FieldStyle {
                label: "SCALE",
                unit: "",
                decimals: 4,
                step: 0.002,
                lo: lim.scale_min,
                hi: lim.scale_max,
                scale: Scale::Linear,
            },
            roots.scale,
        ),
    ];

    let id = Id::of("roots");
    let fw = 120.0;
    let mut edited: Option<(usize, f64)> = None;
    for (i, (style, value)) in specs.iter().enumerate() {
        let b = Rect::from_min_size(
            Pos2::new(r.left() + 8.0 + i as f32 * (fw + 6.0), r.top() + 28.0),
            Vec2::new(fw, 34.0),
        );
        let fid = id.child(i);
        let mut fs = st.fields.remove(&fid.0).unwrap_or_default();
        let out = widgets::number_field(ui, fid, b, *value, style, &mut fs, true);
        st.fields.insert(fid.0, fs);
        if let Some(v) = out {
            edited = Some((i, v));
        }
    }

    if let Some((which, v)) = edited {
        let mut next = roots;
        match which {
            0 => next.pole_hz = v,
            1 => next.pole_r = v,
            2 => next.zero_hz = v,
            3 => next.zero_r = v,
            _ => next.scale = v,
        }
        st.checkpoint();
        let mut lv = lane;
        match lv.set_roots(&next, sr) {
            Ok(()) => {
                st.project.frames[fi].values[li] = lv;
                st.touch();
                st.say("", false);
            }
            Err(e) => st.say(crate::model::lane::refusal_text(e), true),
        }
    }

    // Readings for this lane, so the consequence of an edit is visible next to
    // the control that caused it.
    if let Some(m) = st.live.lane_metrics.get(li) {
        let y = r.top() + 72.0;
        let readings = [
            ("lane peak", format!("{:+.2} dB", m.peak_db)),
            ("cascade so far", format!("{:+.2} dB", m.cumulative_peak_db)),
            ("lane at DC", format!("{:+.2} dB", m.dc_db)),
        ];
        for (i, (k, v)) in readings.iter().enumerate() {
            let x = r.left() + 8.0 + i as f32 * 150.0;
            text(ui.p, Pos2::new(x, y), *k, theme::T_MICRO, theme::DIM);
            text(
                ui.p,
                Pos2::new(x, y + 12.0),
                v,
                theme::T_SMALL,
                theme::INK_HI,
            );
        }
    }
}

// ── 04 LAWS ─────────────────────────────────────────────────────────────

/// Layer 3. The object, its lanes, and what the cascade is doing by the time
/// the signal has been through them.
fn topology_workspace(st: &mut Station, ui: &mut Ui, r: Rect) {
    let left_w = r.width() * 0.62;
    let left = Rect::from_min_size(r.min, Vec2::new(left_w, r.height()));
    let right = Rect::from_min_max(Pos2::new(left.right() + PAD, r.top()), r.max);

    // The navigator is the ground of this workspace and takes the room to be
    // one. The cumulative cascade sits beneath it, because it answers a
    // different question about the same position.
    let nav_h = left.height() * 0.66;
    crate::views_cube::draw(
        st,
        ui,
        Rect::from_min_size(left.min, Vec2::new(left.width(), nav_h)),
    );
    cumulative_panel(
        st,
        ui,
        Rect::from_min_max(Pos2::new(left.left(), left.top() + nav_h + PAD), left.max),
    );

    let axis_h = 132.0;
    let edit_h = 168.0;
    axis_panel(
        st,
        ui,
        Rect::from_min_size(right.min, Vec2::new(right.width(), axis_h)),
    );
    lane_matrix_panel(
        st,
        ui,
        Rect::from_min_max(
            Pos2::new(right.left(), right.top() + axis_h + PAD),
            Pos2::new(right.right(), right.bottom() - edit_h - PAD),
        ),
    );
    root_editor(
        st,
        ui,
        Rect::from_min_max(Pos2::new(right.left(), right.bottom() - edit_h), right.max),
    );
}

/// Layer 4. Packed registers, the linters, the declared laws and grammar, and
/// what export can honestly produce.
fn runtime_workspace(st: &mut Station, ui: &mut Ui, r: Rect) {
    let top_h = r.height() * 0.46;
    let top = Rect::from_min_size(r.min, Vec2::new(r.width(), top_h));
    let bottom = Rect::from_min_max(Pos2::new(r.left(), top.bottom() + PAD), r.max);

    let reg_w = top.width() * 0.62;
    crate::views_runtime::registers(
        st,
        ui,
        Rect::from_min_size(top.min, Vec2::new(reg_w, top.height())),
    );
    crate::views_runtime::linters(
        st,
        ui,
        Rect::from_min_max(Pos2::new(top.left() + reg_w + PAD, top.top()), top.max),
    );

    let col = (bottom.width() - PAD * 2.0) / 3.0;
    let laws_r = Rect::from_min_size(bottom.min, Vec2::new(col, bottom.height()));
    let gram_r = Rect::from_min_size(
        Pos2::new(laws_r.right() + PAD, bottom.top()),
        Vec2::new(col, bottom.height()),
    );
    let read_r = Rect::from_min_max(Pos2::new(gram_r.right() + PAD, bottom.top()), bottom.max);
    editor_pane(st, ui, laws_r, "LAWS", Id::of("laws-editor"), true);
    editor_pane(st, ui, gram_r, "GRAMMAR", Id::of("grammar-editor"), false);
    readings_pane(st, ui, read_r);
}

pub fn editor_pane(st: &mut Station, ui: &mut Ui, r: Rect, title: &str, id: Id, is_laws: bool) {
    let dirty = if is_laws {
        st.laws_editor.dirty()
    } else {
        st.grammar_editor.dirty()
    };
    text(
        ui.p,
        r.min + Vec2::new(2.0, 0.0),
        title,
        theme::T_MICRO,
        theme::DIM,
    );
    if dirty {
        ui.p.circle_filled(Pos2::new(r.left() + 60.0, r.top() + 5.0), 3.0, theme::DIRTY);
        text(
            ui.p,
            Pos2::new(r.left() + 68.0, r.top()),
            "modified",
            theme::T_MICRO,
            theme::DIRTY,
        );
    }

    let bar_h = 24.0;
    let ed_r = Rect::from_min_max(
        Pos2::new(r.left(), r.top() + 14.0),
        Pos2::new(r.right(), r.bottom() - bar_h - 4.0),
    );
    if is_laws {
        st.laws_editor.show(ui, id, ed_r);
    } else {
        st.grammar_editor.show(ui, id, ed_r);
    }

    let by = r.bottom() - bar_h;
    let apply = Rect::from_min_size(Pos2::new(r.left(), by), Vec2::new(78.0, bar_h - 2.0));
    let revert = Rect::from_min_size(Pos2::new(r.left() + 82.0, by), Vec2::new(78.0, bar_h - 2.0));
    if widgets::button(ui, id.child("apply"), apply, "APPLY", dirty) {
        if is_laws {
            st.apply_laws();
        } else {
            st.apply_grammar();
        }
    }
    if widgets::button(ui, id.child("revert"), revert, "REVERT", dirty) {
        if is_laws {
            st.laws_editor.revert();
        } else {
            st.grammar_editor.revert();
        }
    }

    let errs = if is_laws {
        st.laws_editor.errors.clone()
    } else {
        st.grammar_editor.errors.clone()
    };
    if let Some(e) = errs.first() {
        text(
            ui.p,
            Pos2::new(r.left() + 168.0, by + 4.0),
            format!("line {}:{}  {}", e.line, e.column, short(&e.message)),
            theme::T_MICRO,
            theme::BAD,
        );
    }
}

fn short(s: &str) -> String {
    let one = s.lines().next().unwrap_or(s);
    if one.len() > 52 {
        format!("{}…", &one[..52])
    } else {
        one.to_string()
    }
}

pub fn readings_pane(st: &mut Station, ui: &mut Ui, r: Rect) {
    fill(ui.p, r, theme::PANEL);
    outline(ui.p, r, theme::RULE);
    let readings = law::read_all(&st.project, &st.laws.laws, st.selected_frame);
    let pass = readings.iter().filter(|x| x.ok).count();
    text(
        ui.p,
        r.min + Vec2::new(8.0, 6.0),
        "READINGS",
        theme::T_MICRO,
        theme::DIM,
    );
    text_right(
        ui.p,
        Pos2::new(r.right() - 8.0, r.top() + 6.0),
        format!("{pass}/{} hold", readings.len()),
        theme::T_SMALL,
        if readings.is_empty() {
            theme::DIM
        } else if pass == readings.len() {
            theme::GOOD
        } else {
            theme::BAD
        },
    );

    let row_h = 34.0;
    for (i, rd) in readings.iter().enumerate() {
        let y = r.top() + 26.0 + i as f32 * row_h;
        if y + row_h > r.bottom() {
            break;
        }
        let c = if rd.ok { theme::GOOD } else { theme::BAD };
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y),
            &rd.law.q,
            theme::T_SMALL,
            theme::INK,
        );
        text_right(
            ui.p,
            Pos2::new(r.right() - 8.0, y),
            format!("{:.2}", rd.value),
            theme::T_SMALL,
            c,
        );
        text(
            ui.p,
            Pos2::new(r.left() + 8.0, y + 13.0),
            format!("{}   {}", rd.where_, rd.law.why),
            theme::T_MICRO,
            theme::FAINT,
        );
        // The span, drawn as a track with the reading on it.
        let g = Rect::from_min_size(
            Pos2::new(r.right() - 130.0, y + 15.0),
            Vec2::new(122.0, 8.0),
        );
        let pad = (rd.law.max - rd.law.min).abs().max(1e-9) * 0.35;
        let (a, b) = (rd.law.min - pad, rd.law.max + pad);
        let at = |x: f64| g.left() + (((x - a) / (b - a)).clamp(0.0, 1.0) as f32) * g.width();
        hairline(
            ui.p,
            Pos2::new(g.left(), g.center().y),
            Pos2::new(g.right(), g.center().y),
            theme::RULE,
        );
        for e in [rd.law.min, rd.law.max] {
            hairline(
                ui.p,
                Pos2::new(at(e), g.top()),
                Pos2::new(at(e), g.bottom()),
                theme::DIM,
            );
        }
        ui.p.circle_filled(Pos2::new(at(rd.value), g.center().y), 3.0, c);
    }

    if st.laws.laws.is_empty() {
        text(
            ui.p,
            r.min + Vec2::new(8.0, 34.0),
            "no laws declared",
            theme::T_MICRO,
            theme::DIM,
        );
    }
}
