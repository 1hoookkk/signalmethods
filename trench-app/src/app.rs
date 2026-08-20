use trench_core::stage_law::{max_contiguous_pole_radius, RootPair, StageGeometry, P2K_DATUM_SR};

use crate::audio::Audio;
use crate::derived::Derived;
use crate::document::edit::{Gesture, Session};
use crate::document::Target;
use crate::fit::{plan::geometry_of, Fitter, Msg};
use crate::document::{Body, Section, CORNERS, SECTIONS};
use crate::gpu::Gpu;
use crate::library::{self, Library};
use crate::theme::{self, Theme};
use crate::view::response::{r_of, r_prime};
use crate::view::{nav, response};

enum Cmd {
    LoadBody(usize),
    SelectCorner(usize),
    SelectSection(usize),
    SetRide([f32; 3]),
    Begin(usize),
    DragTo { ratio: f64, ddb: f64 },
    End,
    Refused(usize),
    Undo,
    Redo,
    AssignTarget(usize),
    Fit,
    CancelFit,
    AddSection,
    ClearSection,
    ToggleHold,
    SmoothTarget,
    Drawer(Drawer),
    SetFrequency(f64),
    SetStrength(f64),
    SetWidthQ(f64),
    NudgeWidth { section: usize, notches: f64 },
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Drawer {
    None,
    Bodies,
    Targets,
}

struct Session2 {
    corner: usize,
    section: Option<usize>,
    ride: [f32; 3],
    status: String,
    drag_from: Option<StageGeometry>,
    candidate: Option<[[f64; 5]; SECTIONS]>,
    drawer: Drawer,
    burst_until: Option<std::time::Instant>,
}

pub struct Author {
    session: Session,
    derived: Derived,
    ui: Session2,
    library: Option<Library>,
    gpu: Option<Gpu>,
    theme: Theme,
    audio: Audio,
    fitter: Fitter,
    grid: Vec<f64>,
    response: response::ResponseView,
}

impl Author {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let (library, status) = match Library::open() {
            Ok(lib) => {
                let n = lib.bodies.len();
                (Some(lib), format!("{n} bodies"))
            }
            Err(e) => (None, e),
        };
        Self::seated(Body::empty(P2K_DATUM_SR), library, cc, status)
    }

    pub fn with_body(cc: &eframe::CreationContext<'_>, body: Body) -> Self {
        Self::seated(body, None, cc, String::new())
    }

    fn seated(
        body: Body,
        library: Option<Library>,
        cc: &eframe::CreationContext<'_>,
        status: String,
    ) -> Self {
        let theme = theme::theme("flat");
        theme.apply(&cc.egui_ctx);
        let ui = Session2 {
            corner: 0,
            section: None,
            ride: [0.0; 3],
            status,
            drag_from: None,
            candidate: None,
            drawer: Drawer::None,
            burst_until: None,
        };
        let session = Session::new(body);
        let derived = Derived::new(&session.doc, ui.corner, ui.ride);
        let audio = Audio::open();
        audio.publish(&derived.packed);
        audio.set_ride(ui.ride);
        Self {
            session,
            derived,
            ui,
            library,
            gpu: Gpu::from_creation_context(cc),
            theme,
            audio,
            fitter: Fitter::default(),
            grid: author::envelope::grid(),
            response: response::ResponseView::default(),
        }
    }

    fn apply(&mut self, cmd: Cmd) {
        match cmd {
            Cmd::LoadBody(i) => self.load(i),
            Cmd::SelectCorner(i) => {
                self.ui.corner = i;
                self.ui.ride = [(i & 1) as f32, ((i >> 1) & 1) as f32, ((i >> 2) & 1) as f32];
            }
            Cmd::SelectSection(si) => self.ui.section = Some(si),
            Cmd::SetRide(r) => self.ui.ride = r,
            Cmd::Begin(si) => {
                self.ui.section = Some(si);
                self.ui.drag_from =
                    Some(self.session.doc.body().corners[self.ui.corner].sections[si].geometry);
                self.ui.burst_until = None;
                self.session.begin_gesture(Gesture {
                    corner: self.ui.corner,
                    section: si,
                });
            }
            Cmd::DragTo { ratio, ddb } => self.drag(ratio, ddb),
            Cmd::End => {
                self.session.end_gesture();
                self.ui.drag_from = None;
                self.ui.burst_until = None;
            }
            Cmd::Refused(si) => self.ui.status = format!("S{} HELD", si + 1),
            Cmd::Undo => {
                if self.session.undo() {
                    self.ui.status = "undo".into();
                }
            }
            Cmd::Redo => {
                if self.session.redo() {
                    self.ui.status = "redo".into();
                }
            }
            Cmd::AssignTarget(i) => self.assign_target(i),
            Cmd::Drawer(d) => {
                self.ui.drawer = if self.ui.drawer == d { Drawer::None } else { d }
            }
            Cmd::AddSection => self.add_section(),
            Cmd::ClearSection => self.clear_section(),
            Cmd::ToggleHold => self.toggle_hold(),
            Cmd::SmoothTarget => self.smooth_target(),
            Cmd::SetFrequency(v) => self.set_shape(Some(v), None, None),
            Cmd::SetStrength(v) => self.set_shape(None, Some(v), None),
            Cmd::SetWidthQ(v) => self.set_shape(None, None, Some(v)),
            Cmd::NudgeWidth { section, notches } => {
                self.ui.section = Some(section);
                let q = self.shape_of(section).map(|s| s.2).unwrap_or(4.0);
                let next = (q * 1.12f64.powf(notches)).clamp(0.4, 60.0);
                self.set_shape(None, None, Some(next));
            }
            Cmd::Fit => self.start_fit(),
            Cmd::CancelFit => {
                self.fitter.cancel();
                self.ui.candidate = None;
                self.ui.status = String::from("FIT -");
            }
        }
        self.audio.set_ride(self.ui.ride);
        self.audio.moved();
    }

    fn drag(&mut self, ratio: f64, ddb: f64) {
        let Some(start) = self.ui.drag_from else { return };
        let Some(g) = self.session.gesture() else { return };
        let sr = self.session.doc.body().datum_sr_hz;
        let Some((hz, strength, q)) = shape_of_geometry(start, sr) else {
            return;
        };
        let f = (hz * ratio).clamp(40.0, 16_000.0);
        let want = section_db(start, sr, hz) + ddb;
        let mut s = strength + ddb;
        let mut next = geometry_from_shape(start, sr, f, s, q);
        for _ in 0..4 {
            let got = section_db(next, sr, f);
            if !got.is_finite() || !want.is_finite() {
                break;
            }
            s += want - got;
            next = geometry_from_shape(start, sr, f, s, q);
        }
        self.session.edit(|b| {
            b.corners[g.corner].sections[g.section].geometry = next;
        });
    }

    fn assign_target(&mut self, index: usize) {
        let Some(lib) = &self.library else { return };
        let Some(entry) = lib.mouths.get(index) else {
            return;
        };
        match library::read_mouth(&entry.path) {
            Ok(db) => {
                let name = entry.name.clone();
                let corner = self.ui.corner;
                self.session.edit(|b| {
                    b.corners[corner].target = Some(std::sync::Arc::new(Target { name, db }));
                });
            }
            Err(e) => self.ui.status = e,
        }
    }

    fn start_fit(&mut self) {
        let corner = self.ui.corner;
        let sr = self.session.doc.body().datum_sr_hz;
        let rev = self.session.doc.rev();
        let data = &self.session.doc.body().corners[corner];
        match self.fitter.start(corner, data, &self.grid, sr, rev) {
            Ok(()) => self.ui.status = String::from("FIT"),
            Err(e) => self.ui.status = e,
        }
    }

    fn on_fit(&mut self, msg: Msg) {
        match msg {
            Msg::Candidate { roots, rms_db, .. } => {
                let sr = self.session.doc.body().datum_sr_hz;
                self.ui.candidate =
                    Some(std::array::from_fn(|si| geometry_of(&roots[si]).biquad_at(sr)));
                self.ui.status = format!("FIT {rms_db:.2} dB");
            }
            Msg::Done { sections, rms_db, .. } => {
                let corner = self.ui.corner;
                self.session.edit(|b| b.corners[corner].sections = *sections);
                self.ui.candidate = None;
                self.ui.status = format!("FIT {rms_db:.2} dB");
            }
            Msg::Failed { reason, .. } => {
                self.ui.candidate = None;
                self.ui.status = reason;
            }
        }
    }

    fn add_section(&mut self) {
        let corner = self.ui.corner;
        let free = (0..SECTIONS)
            .find(|&si| self.session.doc.body().corners[corner].sections[si].is_empty());
        let Some(si) = free else {
            self.ui.status = String::from("all sections in use");
            return;
        };
        let seed = RootPair::Conjugate {
            hz: 1000.0,
            r: r_of(18.0),
        };
        self.session.edit(|b| {
            let g = &mut b.corners[corner].sections[si].geometry;
            g.pole = seed;
            g.zero = seed;
            g.scale = 1.0;
        });
        self.ui.section = Some(si);
    }

    fn clear_section(&mut self) {
        let (Some(si), corner) = (self.ui.section, self.ui.corner) else {
            return;
        };
        self.session.edit(|b| {
            b.corners[corner].sections[si] = Section::EMPTY;
        });
    }

    fn toggle_hold(&mut self) {
        let (Some(si), corner) = (self.ui.section, self.ui.corner) else {
            return;
        };
        let held = self.session.doc.body().corners[corner].sections[si].hold.any();
        self.session.edit(|b| {
            let h = &mut b.corners[corner].sections[si].hold;
            h.pole = !held;
            h.zero = !held;
        });
    }

    fn smooth_target(&mut self) {
        let corner = self.ui.corner;
        let Some(t) = self.session.doc.body().corners[corner].target.clone() else {
            self.ui.status = String::from("no target");
            return;
        };
        let n = t.db.len();
        let span = (n as f64 / (16_000f64 / 40.0).log2() / 3.0).round() as usize;
        let half = span.max(1) / 2;
        let mut out = vec![0.0f64; n];
        for i in 0..n {
            let lo = i.saturating_sub(half);
            let hi = (i + half + 1).min(n);
            out[i] = t.db[lo..hi].iter().sum::<f64>() / (hi - lo) as f64;
        }
        let name = t.name.clone();
        self.ui.status = String::from("1/3 oct");
        self.session.edit(|b| {
            b.corners[corner].target = Some(std::sync::Arc::new(Target { name, db: out }));
        });
    }

    fn touch(&mut self, section: usize) {
        if self.ui.drag_from.is_some() {
            return;
        }
        let now = std::time::Instant::now();
        let fresh = self.ui.burst_until.map(|t| now < t).unwrap_or(false);
        let same = self
            .session
            .gesture()
            .map(|g| g.section == section && g.corner == self.ui.corner)
            .unwrap_or(false);
        if !(fresh && same) {
            self.session.end_gesture();
            self.session.begin_gesture(Gesture {
                corner: self.ui.corner,
                section,
            });
        }
        self.ui.burst_until = Some(now + std::time::Duration::from_millis(350));
    }

    fn expire_burst(&mut self) {
        if let Some(t) = self.ui.burst_until {
            if std::time::Instant::now() >= t {
                self.session.end_gesture();
                self.ui.burst_until = None;
            }
        }
    }

    fn shape_of(&self, si: usize) -> Option<(f64, f64, f64)> {
        let body = self.session.doc.body();
        shape_of_geometry(
            body.corners[self.ui.corner].sections[si].geometry,
            body.datum_sr_hz,
        )
    }

    fn set_shape(&mut self, hz: Option<f64>, strength: Option<f64>, q: Option<f64>) {
        let Some(si) = self.ui.section else { return };
        let corner = self.ui.corner;
        let sr = self.session.doc.body().datum_sr_hz;
        let start = self.session.doc.body().corners[corner].sections[si].geometry;
        let Some((cur_hz, cur_s, cur_q)) = shape_of_geometry(start, sr) else {
            return;
        };
        let next = geometry_from_shape(
            start,
            sr,
            hz.unwrap_or(cur_hz),
            strength.unwrap_or(cur_s),
            q.unwrap_or(cur_q),
        );
        self.touch(si);
        self.session.edit(|body| {
            body.corners[corner].sections[si].geometry = next;
        });
    }

    fn load(&mut self, index: usize) {
        let Some(lib) = &self.library else { return };
        let Some(entry) = lib.bodies.get(index) else {
            return;
        };
        match library::read_body(&entry.path, entry.datum_sr_hz, Some(entry.name.clone())) {
            Ok(body) => {
                self.ui.status.clear();
                self.session.seat(body);
                self.ui.corner = 0;
                self.ui.ride = [0.0; 3];
            }
            Err(e) => self.ui.status = e,
        }
    }

    fn shortcuts(&mut self, ui: &egui::Ui, out: &mut Vec<Cmd>) {
        ui.input_mut(|i| {
            if i.consume_key(egui::Modifiers::NONE, egui::Key::Escape) {
                out.push(Cmd::CancelFit);
            }
            if i.consume_key(
                egui::Modifiers::CTRL | egui::Modifiers::SHIFT,
                egui::Key::Z,
            ) {
                out.push(Cmd::Redo);
            }
            if i.consume_key(egui::Modifiers::CTRL, egui::Key::Z) {
                out.push(Cmd::Undo);
            }
        });
    }
}

impl eframe::App for Author {
    fn logic(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.expire_burst();
        for msg in self.fitter.poll(self.ui.corner, self.session.doc.rev()) {
            self.on_fit(msg);
        }
        self.derived
            .sync(&self.session.doc, self.ui.corner, self.ui.ride);
        if self.derived.took_repack() {
            self.audio.publish(&self.derived.packed);
        }
        ctx.request_repaint_after(std::time::Duration::from_millis(33));
    }

    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        let mut out: Vec<Cmd> = Vec::new();
        self.shortcuts(ui, &mut out);

        let theme = &self.theme;
        let body = self.session.doc.body();
        let corner = self.ui.corner;
        let sr = body.datum_sr_hz;
        let sections: &[Section; SECTIONS] = &body.corners[corner].sections;
        let seated: [bool; CORNERS] = std::array::from_fn(|i| body.seated(i));
        let planes = if (4..CORNERS).any(|i| seated[i]) { 2 } else { 1 };
        let has_target = body.corners[corner].target.is_some();
        let selected = self.ui.section;
        let shape = selected.and_then(|si| self.shape_of(si));
        let pinned = selected.map(|si| sections[si].hold.any()).unwrap_or(false);
        let judged: &[[f64; 5]] = match &self.ui.candidate {
            Some(c) => c.as_slice(),
            None => &self.derived.biquads,
        };
        let rms = body.corners[corner].target.as_ref().map(|t| {
            let mut sum = 0.0f64;
            for (i, &hz) in self.grid.iter().enumerate() {
                let got = trench_core::response::biquad_cascade_mag_db(judged, hz, sr);
                let d = t.db[i] - got;
                sum += d * d;
            }
            (sum / self.grid.len() as f64).sqrt()
        });
        let at_corner = self.ui.ride
            == [
                (corner & 1) as f32,
                ((corner >> 1) & 1) as f32,
                ((corner >> 2) & 1) as f32,
            ];

        egui::Panel::top("menu")
            .exact_size(28.0)
            .resizable(false)
            .frame(theme.bar_frame())
            .show(ui, |ui| {
                ui.horizontal_centered(|ui| {
                    ui.spacing_mut().item_spacing.x = 2.0;
                    let tab = egui::vec2(82.0, 20.0);
                    for (label, which) in [("BODY", Drawer::Bodies), ("TARGET", Drawer::Targets)] {
                        let on = self.ui.drawer == which;
                        if ui
                            .add(
                                egui::Button::new(label)
                                    .min_size(tab)
                                    .fill(if on {
                                        theme.chrome.face_lo
                                    } else {
                                        theme.chrome.face
                                    })
                                    .stroke(theme.selection_stroke(on)),
                            )
                            .clicked()
                        {
                            out.push(Cmd::Drawer(which));
                        }
                    }
                    ui.add_space(12.0);
                    ui.label(
                        egui::RichText::new(body.name.clone().unwrap_or_else(|| "-".into()))
                            .font(theme.num())
                            .color(theme.chrome.ink),
                    );
                    if let Some(t) = &body.corners[corner].target {
                        ui.add_space(8.0);
                        ui.label(
                            egui::RichText::new(t.name.as_str())
                                .font(theme.num())
                                .color(theme.chrome.ink_dim),
                        );
                        if ui
                            .add(egui::Button::new("SMOOTH").min_size(egui::vec2(64.0, 20.0)))
                            .clicked()
                        {
                            out.push(Cmd::SmoothTarget);
                        }
                    }
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        ui.label(
                            egui::RichText::new(self.ui.status.as_str())
                                .font(theme.num())
                                .color(theme.chrome.ink_dim),
                        );
                    });
                });
            });

        egui::Panel::bottom("nav")
            .exact_size(44.0)
            .resizable(false)
            .frame(theme.bar_frame())
            .show(ui, |ui| {
                ui.horizontal_centered(|ui| {
                    if let Some(a) = nav::show(
                        ui,
                        theme,
                        &nav::Model {
                            corner,
                            ride: self.ui.ride,
                            seated: &seated,
                            corners_shown: planes * 4,
                        },
                    ) {
                        out.push(match a {
                            nav::Action::SelectCorner(i) => Cmd::SelectCorner(i),
                        });
                    }
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        let verb = egui::vec2(78.0, 24.0);
                        if ui
                            .add_enabled(has_target, egui::Button::new("FIT").min_size(verb))
                            .clicked()
                        {
                            out.push(Cmd::Fit);
                        }
                        ui.add_space(10.0);
                        if ui
                            .add_enabled(
                                selected.is_some(),
                                egui::Button::new("ERASE").min_size(verb),
                            )
                            .clicked()
                        {
                            out.push(Cmd::ClearSection);
                        }
                        if ui
                            .add(egui::Button::new("ADD").min_size(verb))
                            .clicked()
                        {
                            out.push(Cmd::AddSection);
                        }
                    });
                });
            });

        if self.ui.drawer != Drawer::None {
            egui::Panel::left("drawer")
                .default_size(240.0)
                .size_range(200.0..=360.0)
                .frame(theme.dock_frame())
                .show(ui, |ui| {
                    let bodies = self.ui.drawer == Drawer::Bodies;
                    ui.label(
                        egui::RichText::new(if bodies { "BODIES" } else { "TARGETS" })
                            .font(theme.label())
                            .strong(),
                    );
                    ui.separator();
                    egui::ScrollArea::vertical().show(ui, |ui| {
                        if let Some(lib) = &self.library {
                            let list = if bodies { &lib.bodies } else { &lib.mouths };
                            for (i, entry) in list.iter().enumerate() {
                                if ui.button(entry.name.as_str()).clicked() {
                                    out.push(if bodies {
                                        Cmd::LoadBody(i)
                                    } else {
                                        Cmd::AssignTarget(i)
                                    });
                                }
                            }
                        }
                    });
                });
        }

        egui::CentralPanel::no_frame()
            .frame(egui::Frame::NONE.fill(theme.chrome.face))
            .show(ui, |ui| {
                let shown = self.response.show(
                    ui,
                    theme,
                    &response::Model {
                        biquads: &self.derived.ride_biquads,
                        sections,
                        selected,
                        target: body.corners[corner].target.as_ref().map(|t| t.db.as_slice()),
                        candidate: self.ui.candidate.as_ref().map(|c| c.as_slice()),
                        judged: Some(judged),
                        rms_db: rms,
                        sr_hz: sr,
                        editable: at_corner,
                    },
                );
                if let Some(a) = shown.action {
                    out.push(match a {
                        response::Action::Select(si) => Cmd::SelectSection(si),
                        response::Action::Begin(si) => Cmd::Begin(si),
                        response::Action::DragTo { ratio, ddb } => Cmd::DragTo { ratio, ddb },
                        response::Action::End => Cmd::End,
                        response::Action::Refused(si) => Cmd::Refused(si),
                        response::Action::Width { section, notches } => {
                            Cmd::NudgeWidth { section, notches }
                        }
                    });
                }
                if let (Some(si), Some(at)) = (selected, shown.anchor) {
                    inspector(
                        ui.ctx(),
                        theme,
                        si,
                        shape,
                        pinned,
                        at,
                        shown.main,
                        &mut out,
                    );
                }
            });

        for cmd in out {
            self.apply(cmd);
        }
    }

    fn persist_egui_memory(&self) -> bool {
        false
    }

    fn clear_color(&self, _visuals: &egui::Visuals) -> [f32; 4] {
        self.theme.chrome.face.to_normalized_gamma_f32()
    }
}

#[allow(clippy::too_many_arguments)]
fn inspector(
    ctx: &egui::Context,
    theme: &Theme,
    si: usize,
    shape: Option<(f64, f64, f64)>,
    pinned: bool,
    at: egui::Pos2,
    main: egui::Rect,
    out: &mut Vec<Cmd>,
) {
    let w = 348.0;
    let left = main.left() + 8.0;
    let right = (main.right() - w - 8.0).max(left);
    let x = (at.x - w * 0.5).clamp(left, right);
    egui::Area::new(egui::Id::new("section.inspector"))
        .order(egui::Order::Foreground)
        .fade_in(false)
        .fixed_pos(egui::pos2(x, main.top() + 8.0))
        .show(ctx, |ui| {
            egui::Frame::NONE
                .fill(theme.chrome.face)
                .stroke(egui::Stroke::new(1.0, theme.chrome.rule))
                .inner_margin(egui::Margin::symmetric(8, 5))
                .show(ui, |ui| {
                    ui.set_width(w - 16.0);
                    ui.horizontal(|ui| {
                        ui.spacing_mut().item_spacing.x = 8.0;
                        ui.label(
                            egui::RichText::new(format!("S{}", si + 1))
                                .font(theme.head())
                                .strong()
                                .color(theme.data.stage[si % theme.data.stage.len()]),
                        );
                        match shape {
                            Some((hz, strength, q)) => {
                                let mut f = hz;
                                if ui
                                    .add(
                                        egui::DragValue::new(&mut f)
                                            .speed(4.0)
                                            .range(40.0..=16_000.0)
                                            .suffix(" Hz")
                                            .fixed_decimals(0),
                                    )
                                    .changed()
                                {
                                    out.push(Cmd::SetFrequency(f));
                                }
                                let mut s = strength;
                                if ui
                                    .add(
                                        egui::DragValue::new(&mut s)
                                            .speed(0.2)
                                            .range(0.0..=84.0)
                                            .suffix(" dB")
                                            .fixed_decimals(1),
                                    )
                                    .changed()
                                {
                                    out.push(Cmd::SetStrength(s));
                                }
                                let mut qq = q;
                                if ui
                                    .add(
                                        egui::DragValue::new(&mut qq)
                                            .speed(0.08)
                                            .range(0.4..=60.0)
                                            .prefix("Q ")
                                            .fixed_decimals(2),
                                    )
                                    .changed()
                                {
                                    out.push(Cmd::SetWidthQ(qq));
                                }
                            }
                            None => {
                                ui.label(
                                    egui::RichText::new("real pair")
                                        .font(theme.num())
                                        .color(theme.chrome.ink_dim),
                                );
                            }
                        }
                        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                            if ui
                                .add(
                                    egui::Button::new(if pinned { "PINNED" } else { "FREE" })
                                        .min_size(egui::vec2(66.0, 22.0))
                                        .fill(if pinned {
                                            theme.chrome.face_lo
                                        } else {
                                            theme.chrome.face
                                        })
                                        .stroke(theme.selection_stroke(pinned)),
                                )
                                .clicked()
                            {
                                out.push(Cmd::ToggleHold);
                            }
                        });
                    });
                });
        });
}

fn shape_of_geometry(g: StageGeometry, sr: f64) -> Option<(f64, f64, f64)> {
    let RootPair::Conjugate { hz, r } = g.pole else {
        return None;
    };
    let b = if r > 0.0 && r < 1.0 {
        -(r.ln()) * sr / std::f64::consts::PI
    } else {
        0.0
    };
    let q = if b > 0.0 { hz / b } else { 0.0 };
    let strength = match g.zero {
        RootPair::Conjugate { r: zr, .. } if zr > 0.0 => r_prime(r) - r_prime(zr),
        _ => r_prime(r),
    };
    Some((hz, strength, q))
}

fn geometry_from_shape(g: StageGeometry, sr: f64, hz: f64, strength: f64, q: f64) -> StageGeometry {
    let f = hz.clamp(40.0, 16_000.0);
    let s = strength.clamp(0.0, 84.0);
    let qq = q.clamp(0.4, 60.0);
    let b = (f / qq).max(0.5);
    let pole_r = (-(b * std::f64::consts::PI / sr))
        .exp()
        .min(max_contiguous_pole_radius());
    let zero_r = r_of((r_prime(pole_r) - s).max(0.0)).min(1.0);
    let ratio = match g.pole {
        RootPair::Conjugate { hz, .. } if hz > 0.0 => f / hz,
        _ => 1.0,
    };
    let zero_hz = match g.zero {
        RootPair::Conjugate { hz, .. } if hz > 0.0 => (hz * ratio).clamp(40.0, 16_000.0),
        _ => f,
    };
    let mut out = g;
    out.pole = RootPair::Conjugate { hz: f, r: pole_r };
    out.zero = if zero_r > 0.0 {
        RootPair::Conjugate {
            hz: zero_hz,
            r: zero_r,
        }
    } else {
        RootPair::Degenerate
    };
    out
}

fn section_db(g: StageGeometry, sr: f64, hz: f64) -> f64 {
    trench_core::response::biquad_cascade_mag_db(&[g.biquad_at(sr)], hz, sr)
}
