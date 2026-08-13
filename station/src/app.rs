//! The Station application: state, commands and frame dispatch.

use std::collections::HashMap;
use std::path::PathBuf;

use eframe::egui::{self, Key};

use crate::model::analysis::{self, CascadeResponse};
use crate::model::history::History;
use crate::model::law::{self, Grammar, LawFile};
use crate::model::project::{Origin, Project};
use crate::model::store;
use crate::model::topology::{Axis, Topology};
use crate::ui::editor::{Located, TextEditor};
use crate::ui::input::{Id, InputFrame, Ui, UiState};
use crate::ui::widgets::FieldState;
use crate::ui::{paint, theme};
use crate::views;

/// The four workspaces, one per abstraction layer. Measurement, perceptual
/// intent, structure and runtime are kept apart on purpose: the boundary
/// between acoustic intent and packed registers is the whole point of the
/// architecture, and one crowded screen destroys it.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Workspace {
    /// Layer 1 — measurement, import and root classification.
    Ingestion,
    /// Layer 2 — perceptual intent and the psychophysical warp.
    Perceptual,
    /// Layer 3 — topology, lane discipline and the cumulative cascade.
    Topology,
    /// Layer 4 — packed registers, linters and export.
    Runtime,
}

impl Workspace {
    pub const ALL: [Workspace; 4] = [
        Workspace::Ingestion,
        Workspace::Perceptual,
        Workspace::Topology,
        Workspace::Runtime,
    ];
    pub fn label(self) -> &'static str {
        match self {
            Workspace::Ingestion => "INGESTION",
            Workspace::Perceptual => "PERCEPTUAL",
            Workspace::Topology => "TOPOLOGY & LANES",
            Workspace::Runtime => "RUNTIME & EXPORT",
        }
    }
}

/// What the operator is being asked to type, when the path bar is open.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum PathIntent {
    Open,
    SaveAs,
    ExportPacked,
}

pub struct Station {
    pub project: Project,
    pub history: History,
    pub path: Option<PathBuf>,

    pub laws_editor: TextEditor,
    pub grammar_editor: TextEditor,
    pub laws: LawFile,
    pub grammar: Grammar,
    pub laws_path: PathBuf,

    pub selected_frame: usize,
    pub selected_lane: usize,
    /// One position per declared axis. Not a fixed triple.
    pub coords: Vec<f32>,
    /// Which two axes the working projection shows.
    pub display_axes: (usize, usize),
    pub workspace: Workspace,

    /// The corner whose sections are open for surgery, if any.
    pub sos_corner: Option<usize>,

    /// Viewing angles for the topology object.
    pub cube_yaw: f32,
    pub cube_pitch: f32,

    /// Psychophysical warp: where the perceived midpoint of a sweep sits.
    /// 0.5 is no warp. Applied to the first axis when `warp_on` is set.
    pub warp_mid: f32,
    pub warp_on: bool,

    pub ui: UiState,
    pub fields: HashMap<u64, FieldState>,
    pub note: String,
    pub note_bad: bool,

    pub path_bar: Option<PathIntent>,
    pub path_entry: TextEditor,

    /// Recomputed whenever the project or the working position changes.
    pub live: CascadeResponse,
    dirty_response: bool,
}

impl Station {
    pub fn new(project: Project, laws_path: PathBuf) -> Self {
        let history = History::new(&project);
        let coords = vec![0.0; project.topology.axis_count()];
        let live = analysis::analyse(
            &project.cascade_at(&coords).unwrap_or_default(),
            project.sample_rate(),
        );
        let display_axes = (0, 1.min(project.topology.axis_count().saturating_sub(1)));

        let laws_text = std::fs::read_to_string(&laws_path)
            .unwrap_or_else(|_| "{\n  \"laws\": []\n}\n".to_string());
        let grammar_path = laws_path.with_file_name("grammar.json");
        let grammar_text =
            std::fs::read_to_string(&grammar_path).unwrap_or_else(|_| default_grammar_text());

        let mut s = Self {
            project,
            history,
            path: None,
            laws: LawFile::default(),
            grammar: Grammar::default(),
            laws_editor: TextEditor::new(laws_text),
            grammar_editor: TextEditor::new(grammar_text),
            laws_path,
            selected_frame: 0,
            selected_lane: 0,
            coords,
            display_axes,
            workspace: Workspace::Topology,
            sos_corner: None,
            cube_yaw: 0.62,
            cube_pitch: 0.42,
            warp_mid: 0.5,
            warp_on: false,
            ui: UiState::default(),
            fields: HashMap::new(),
            note: String::new(),
            note_bad: false,
            path_bar: None,
            path_entry: TextEditor::new(""),
            live,
            dirty_response: true,
        };
        s.apply_laws();
        s.apply_grammar();
        s
    }

    pub fn say(&mut self, msg: impl Into<String>, bad: bool) {
        self.note = msg.into();
        self.note_bad = bad;
    }

    pub fn touch(&mut self) {
        self.dirty_response = true;
    }

    /// Records the pre-edit state so the next mutation is undoable.
    pub fn checkpoint(&mut self) {
        let before = self.project.clone();
        self.history.record(&before);
    }

    /// The working position after the psychophysical warp.
    ///
    /// The warp is a monotone reparametrisation of travel along the first
    /// axis, so that the middle of the operator's motion lands where the
    /// middle of the transition is heard rather than where the arithmetic
    /// midpoint falls. It moves where the object is sampled; it never alters
    /// the authored frames.
    pub fn warped_coords(&self) -> Vec<f32> {
        let mut c = self.coords.clone();
        if self.warp_on {
            if let Some(first) = c.first_mut() {
                *first = warp(*first, self.warp_mid);
            }
        }
        c
    }

    pub fn recompute(&mut self) {
        if !self.dirty_response {
            return;
        }
        let lanes = self
            .project
            .cascade_at(&self.warped_coords())
            .unwrap_or_default();
        self.live = analysis::analyse(&lanes, self.project.sample_rate());
        self.dirty_response = false;
    }

    // ── laws and grammar ────────────────────────────────────────────────

    pub fn apply_laws(&mut self) {
        let text = self.laws_editor.text.clone();
        match law::parse_laws(&text) {
            Ok(f) => {
                let mut errs = law::unknown_quantities(&text, &f);
                if errs.iter().any(|e| e.fatal) {
                    self.laws_editor.errors = errs;
                    self.say("laws not applied: see marked lines", true);
                    return;
                }
                errs.clear();
                self.laws_editor.errors = errs;
                let n = f.laws.len();
                self.laws = f;
                self.laws_editor.commit();
                self.say(format!("{n} laws applied"), false);
            }
            Err(e) => {
                self.laws_editor.errors = vec![e];
                self.say("laws not applied: malformed", true);
            }
        }
    }

    pub fn apply_grammar(&mut self) {
        let text = self.grammar_editor.text.clone();
        match law::parse_grammar(&text) {
            Ok(g) => {
                let problems = g.check();
                if !problems.is_empty() {
                    self.grammar_editor.errors = problems
                        .iter()
                        .map(|m| Located {
                            line: 1,
                            column: 1,
                            message: m.clone(),
                            fatal: true,
                        })
                        .collect();
                    self.say(format!("grammar not applied: {}", problems[0]), true);
                    return;
                }
                self.grammar_editor.errors.clear();
                self.grammar = g;
                self.grammar_editor.commit();
                self.say("grammar applied", false);
                self.touch();
            }
            Err(e) => {
                self.grammar_editor.errors = vec![e];
                self.say("grammar not applied: malformed", true);
            }
        }
    }

    // ── file commands ───────────────────────────────────────────────────

    pub fn new_project(&mut self) {
        let p = Project::blank("untitled", Topology::packed_runtime(), 7);
        self.adopt(p, None);
        self.say("new project", false);
    }

    /// A 4D project the packed format cannot hold, so the export refusal is
    /// reachable from the interface rather than only from a test.
    pub fn new_4d_project(&mut self) {
        let topo = Topology::new(vec![
            Axis::new("morph", "M"),
            Axis::new("q", "Q"),
            Axis::new("t2", "T"),
            Axis::new("t3", "W"),
        ]);
        let p = Project::blank("untitled-4d", topo, 7);
        self.adopt(p, None);
        self.say("new 4D project — packed export unsupported", false);
    }

    pub fn adopt(&mut self, p: Project, path: Option<PathBuf>) {
        self.coords = vec![0.0; p.topology.axis_count()];
        self.display_axes = (0, 1.min(p.topology.axis_count().saturating_sub(1)));
        self.selected_frame = 0;
        self.selected_lane = 0;
        self.history.reset(&p);
        self.project = p;
        self.path = path;
        self.touch();
    }

    pub fn open_path(&mut self, path: &std::path::Path) {
        let ext_is_project = path
            .to_string_lossy()
            .to_ascii_lowercase()
            .ends_with(".station.json");
        let result = if ext_is_project {
            store::load(path).map(|p| (p, Some(path.to_path_buf())))
        } else {
            std::fs::read(path)
                .map_err(|e| format!("{}: {e}", path.display()))
                .and_then(|raw| {
                    let name = path
                        .file_stem()
                        .map(|s| s.to_string_lossy().to_string())
                        .unwrap_or_else(|| "imported".into());
                    Project::from_packed(name, &raw, 44_100.0).map(|p| (p, None))
                })
        };
        match result {
            Ok((p, saved_as)) => {
                let what = match p.origin {
                    Origin::Native => "project",
                    Origin::PackedNative => "native body",
                    Origin::PackedLegacy => "legacy body",
                };
                let frames = p.frames.len();
                let lanes = p.lane_count();
                self.adopt(p, saved_as);
                self.say(
                    format!("opened {what}: {frames} frames, {lanes} lanes"),
                    false,
                );
            }
            Err(e) => self.say(e, true),
        }
    }

    pub fn save(&mut self) {
        match self.path.clone() {
            Some(p) => self.save_to(&p),
            None => self.open_path_bar(PathIntent::SaveAs),
        }
    }

    pub fn save_to(&mut self, path: &std::path::Path) {
        match store::save(&self.project, path) {
            Ok(()) => {
                self.history.mark_saved(&self.project);
                self.path = Some(path.to_path_buf());
                self.say(format!("saved {}", path.display()), false);
            }
            Err(e) => self.say(e, true),
        }
    }

    pub fn export_packed(&mut self, path: &std::path::Path) {
        match self.project.to_packed() {
            Ok(packed) => {
                let bytes = packed.to_native_bytes();
                match std::fs::write(path, bytes) {
                    Ok(()) => self.say(
                        format!("exported {} bytes to {}", bytes.len(), path.display()),
                        false,
                    ),
                    Err(e) => self.say(format!("{}: {e}", path.display()), true),
                }
            }
            Err(r) => self.say(format!("export refused — {}", r.text()), true),
        }
    }

    pub fn open_path_bar(&mut self, intent: PathIntent) {
        let seed = self
            .path
            .as_ref()
            .map(|p| p.display().to_string())
            .unwrap_or_default();
        self.path_entry.set(seed);
        self.path_bar = Some(intent);
        self.ui.focus = Some(Id::of("path-entry"));
    }

    pub fn commit_path_bar(&mut self) {
        let raw = self.path_entry.text.trim().trim_matches('"').to_string();
        let intent = self.path_bar.take();
        if raw.is_empty() {
            return;
        }
        let path = PathBuf::from(raw);
        match intent {
            Some(PathIntent::Open) => self.open_path(&path),
            Some(PathIntent::SaveAs) => {
                let path = if path.to_string_lossy().contains('.') {
                    path
                } else {
                    path.with_extension(store::PROJECT_EXTENSION)
                };
                self.save_to(&path)
            }
            Some(PathIntent::ExportPacked) => self.export_packed(&path),
            None => {}
        }
    }

    pub fn undo(&mut self) {
        let mut p = std::mem::replace(
            &mut self.project,
            Project::blank("", Topology::new(vec![]), 0),
        );
        let ok = self.history.undo(&mut p);
        self.project = p;
        self.clamp_selection();
        self.touch();
        self.say(if ok { "undo" } else { "nothing to undo" }, !ok);
    }

    pub fn redo(&mut self) {
        let mut p = std::mem::replace(
            &mut self.project,
            Project::blank("", Topology::new(vec![]), 0),
        );
        let ok = self.history.redo(&mut p);
        self.project = p;
        self.clamp_selection();
        self.touch();
        self.say(if ok { "redo" } else { "nothing to redo" }, !ok);
    }

    fn clamp_selection(&mut self) {
        if self.coords.len() != self.project.topology.axis_count() {
            self.coords = vec![0.0; self.project.topology.axis_count()];
        }
        self.selected_frame = self
            .selected_frame
            .min(self.project.frames.len().saturating_sub(1));
        self.selected_lane = self
            .selected_lane
            .min(self.project.lane_count().saturating_sub(1));
        let n = self.project.topology.axis_count();
        if n > 0 {
            self.display_axes.0 = self.display_axes.0.min(n - 1);
            self.display_axes.1 = self.display_axes.1.min(n - 1);
        }
    }

    pub fn dirty(&self) -> bool {
        self.history.dirty(&self.project)
    }

    fn shortcuts(&mut self, input: &InputFrame) {
        if self.ui.focus.is_some() && self.path_bar.is_some() {
            return;
        }
        // Text focus owns the keyboard; only the path bar and editors take it.
        let editing = matches!(self.workspace, Workspace::Runtime) && self.ui.focus.is_some();
        if editing {
            return;
        }
        if input.ctrl {
            if input.key(Key::Z) {
                if input.shift {
                    self.redo()
                } else {
                    self.undo()
                }
            }
            if input.key(Key::Y) {
                self.redo();
            }
            if input.key(Key::S) {
                self.save();
            }
            if input.key(Key::O) {
                self.open_path_bar(PathIntent::Open);
            }
            if input.key(Key::N) {
                self.new_project();
            }
            if input.key(Key::E) {
                self.open_path_bar(PathIntent::ExportPacked);
            }
        }
    }
}

fn default_grammar_text() -> String {
    "{\n  \"axis_names\": [],\n  \"lane_names\": [],\n  \"display_lo_hz\": 20.0,\n  \"display_hi_hz\": 20000.0,\n  \"pole_radius_watch\": 0.995\n}\n".into()
}

impl eframe::App for Station {
    fn update(&mut self, ctx: &egui::Context, _f: &mut eframe::Frame) {
        let input = InputFrame::read(ctx);

        // Drag and drop is a first-class way in.
        let dropped: Vec<PathBuf> = ctx.input(|i| {
            i.raw
                .dropped_files
                .iter()
                .filter_map(|f| f.path.clone())
                .collect()
        });
        if let Some(p) = dropped.first() {
            let p = p.clone();
            self.open_path(&p);
        }

        self.shortcuts(&input);
        self.recompute();

        let frame = egui::Frame {
            fill: theme::BG,
            ..Default::default()
        };
        egui::CentralPanel::default().frame(frame).show(ctx, |cui| {
            let (resp, painter) =
                cui.allocate_painter(cui.available_size(), egui::Sense::click_and_drag());
            let full = resp.rect;
            paint::fill(&painter, full, theme::BG);

            let mut state = std::mem::take(&mut self.ui);
            state.begin_frame();
            {
                let mut ui = Ui::new(&painter, &input, &mut state, full);
                ui.background(full);
                views::draw(self, &mut ui, full);
            }
            self.ui = state;
        });

        // The response view must follow an edit within the same interaction,
        // so recompute again after the views have had their say.
        self.recompute();
        ctx.request_repaint();
    }
}

/// Layout constants shared by the views.
pub const HEAD_H: f32 = 26.0;
pub const CMD_H: f32 = 26.0;
pub const TAB_H: f32 = 26.0;
pub const STATUS_H: f32 = 22.0;
pub const PAD: f32 = 8.0;

/// A monotone warp of travel through a chosen midpoint.
///
/// `mid` is where the perceived middle of the sweep sits. At 0.5 this is the
/// identity. The curve is two straight segments joined at the anchor, which is
/// monotone by construction, so a sweep can be re-timed without it ever
/// doubling back on itself.
pub fn warp(t: f32, mid: f32) -> f32 {
    let t = t.clamp(0.0, 1.0);
    let mid = mid.clamp(0.02, 0.98);
    if t <= 0.5 {
        t / 0.5 * mid
    } else {
        mid + (t - 0.5) / 0.5 * (1.0 - mid)
    }
}

#[cfg(test)]
mod tests {
    use super::warp;

    #[test]
    fn warp_is_the_identity_at_the_arithmetic_midpoint() {
        for i in 0..=10 {
            let t = i as f32 / 10.0;
            assert!((warp(t, 0.5) - t).abs() < 1e-6, "warp({t}) moved");
        }
    }

    #[test]
    fn warp_moves_the_midpoint_and_keeps_the_ends() {
        assert!((warp(0.0, 0.25) - 0.0).abs() < 1e-6);
        assert!((warp(1.0, 0.25) - 1.0).abs() < 1e-6);
        assert!((warp(0.5, 0.25) - 0.25).abs() < 1e-6);
    }

    #[test]
    fn warp_never_doubles_back() {
        for mid in [0.05, 0.25, 0.5, 0.75, 0.95] {
            let mut prev = -1.0;
            for i in 0..=100 {
                let v = warp(i as f32 / 100.0, mid);
                assert!(v >= prev - 1e-6, "warp went backwards at mid {mid}");
                prev = v;
            }
        }
    }
}
