//! Application state and per-frame dispatch.

use std::collections::HashMap;
use std::path::PathBuf;

use eframe::egui::{self, Key};

use crate::model::analysis::{self, CascadeResponse};
use crate::model::history::History;
use crate::model::object::ObjectForm;
use crate::model::project::Project;
use crate::model::settings::Settings;
use crate::model::store;
use crate::ui::editor::TextEditor;
use crate::ui::input::{Id, InputFrame, Ui, UiState};
use crate::ui::theme;
use crate::ui::widgets::FieldState;
use crate::view;

/// Task-based screens.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Screen {
    Object,
    Sections,
    Export,
}

impl Screen {
    pub const ALL: [Screen; 3] = [Screen::Object, Screen::Sections, Screen::Export];
    pub fn label(self) -> &'static str {
        match self {
            Screen::Object => "Object",
            Screen::Sections => "Sections",
            Screen::Export => "Export",
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum PathIntent {
    Open,
    SaveAs,
    ExportBody,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum NoteKind {
    Plain,
    Warn,
    Error,
}

pub struct Station {
    pub project: Project,
    pub history: History,
    pub path: Option<PathBuf>,

    pub settings: Settings,

    pub selected_corner: usize,
    pub selected_lane: usize,
    /// One position per axis of the open form.
    pub coords: Vec<f32>,
    pub screen: Screen,

    /// Viewing angle for the cube.
    pub cube_yaw: f32,
    pub cube_pitch: f32,

    /// Raw words and coefficients are a diagnostic, shown on request.
    pub show_diagnostics: bool,

    pub ui: UiState,
    pub fields: HashMap<u64, FieldState>,
    pub note: String,
    pub note_kind: NoteKind,

    pub path_bar: Option<PathIntent>,
    pub path_entry: TextEditor,

    pub live: CascadeResponse,
    dirty_response: bool,
}

impl Station {
    pub fn new(project: Project) -> Self {
        let coords = vec![0.0; project.form().map(|f| f.axis_count()).unwrap_or(0)];
        let live = analysis::analyse(
            &project.cascade_at(&coords).unwrap_or_default(),
            project.sample_rate(),
        );

        let mut s = Self {
            history: History::new(&project),
            project,
            path: None,
            settings: Settings::default(),
            selected_corner: 0,
            selected_lane: 0,
            coords,
            screen: Screen::Object,
            cube_yaw: 0.62,
            cube_pitch: 0.38,
            show_diagnostics: false,
            ui: UiState::default(),
            fields: HashMap::new(),
            note: String::new(),
            note_kind: NoteKind::Plain,
            path_bar: None,
            path_entry: TextEditor::new(""),
            live,
            dirty_response: true,
        };
        s.note.clear();
        s
    }

    pub fn say(&mut self, msg: impl Into<String>, kind: NoteKind) {
        self.note = msg.into();
        self.note_kind = kind;
    }

    pub fn touch(&mut self) {
        self.dirty_response = true;
    }

    pub fn checkpoint(&mut self) {
        let before = self.project.clone();
        self.history.record(&before);
    }

    pub fn recompute(&mut self) {
        if !self.dirty_response {
            return;
        }
        let lanes = self.project.cascade_at(&self.coords).unwrap_or_default();
        self.live = analysis::analyse(&lanes, self.project.sample_rate());
        self.dirty_response = false;
    }

    pub fn dirty(&self) -> bool {
        self.history.dirty(&self.project)
    }

    pub fn new_object(&mut self, form: ObjectForm) {
        let p = Project::new_object(format!("untitled.{}", form.extension()), form, 44_100.0);
        self.adopt(p, None);
        self.say(format!("new {}", form.name()), NoteKind::Plain);
    }

    pub fn adopt(&mut self, p: Project, path: Option<PathBuf>) {
        self.coords = vec![0.0; p.form().map(|f| f.axis_count()).unwrap_or(0)];
        self.selected_corner = 0;
        self.selected_lane = 0;
        self.history.reset(&p);
        self.project = p;
        self.path = path;
        self.touch();
    }

    pub fn open_path(&mut self, path: &std::path::Path) {
        let is_project = path
            .to_string_lossy()
            .to_ascii_lowercase()
            .ends_with(".station.json");
        let result = if is_project {
            store::load(path).map(|p| (p, Some(path.to_path_buf())))
        } else {
            std::fs::read(path)
                .map_err(|e| format!("{}: {e}", path.display()))
                .and_then(|raw| {
                    let name = path
                        .file_name()
                        .map(|s| s.to_string_lossy().to_string())
                        .unwrap_or_else(|| "imported".into());
                    Project::from_packed(name, &raw, 44_100.0).map(|p| (p, None))
                })
        };
        match result {
            Ok((p, saved)) => {
                let what = p
                    .form()
                    .map(|f| f.name().to_string())
                    .unwrap_or_else(|| "empty".into());
                let corners = p.frames().len();
                let active = p.active_lanes().len();
                self.adopt(p, saved);
                self.say(
                    format!("{what}, {corners} corners, {active} sections"),
                    NoteKind::Plain,
                );
            }
            Err(e) => self.say(e, NoteKind::Error),
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
                self.say(format!("saved {}", path.display()), NoteKind::Plain);
            }
            Err(e) => self.say(e, NoteKind::Error),
        }
    }

    pub fn export_body(&mut self, path: &std::path::Path) {
        match self.project.to_body_bytes() {
            Ok(bytes) => match std::fs::write(path, &bytes) {
                Ok(()) => self.say(
                    format!("{} bytes to {}", bytes.len(), path.display()),
                    NoteKind::Plain,
                ),
                Err(e) => self.say(format!("{}: {e}", path.display()), NoteKind::Error),
            },
            Err(e) => self.say(e, NoteKind::Error),
        }
    }

    pub fn open_path_bar(&mut self, intent: PathIntent) {
        let seed = match intent {
            PathIntent::ExportBody => self
                .project
                .form()
                .map(|f| {
                    let stem = self
                        .project
                        .name
                        .rsplit_once('.')
                        .map(|(a, _)| a.to_string())
                        .unwrap_or_else(|| self.project.name.clone());
                    format!("{stem}.{}", f.extension())
                })
                .unwrap_or_default(),
            _ => self
                .path
                .as_ref()
                .map(|p| p.display().to_string())
                .unwrap_or_default(),
        };
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
            Some(PathIntent::ExportBody) => self.export_body(&path),
            None => {}
        }
    }

    pub fn undo(&mut self) {
        let mut p = std::mem::replace(&mut self.project, Project::empty());
        let ok = self.history.undo(&mut p);
        self.project = p;
        self.clamp();
        self.touch();
        if ok {
            self.say("undo", NoteKind::Plain);
        }
    }

    pub fn redo(&mut self) {
        let mut p = std::mem::replace(&mut self.project, Project::empty());
        let ok = self.history.redo(&mut p);
        self.project = p;
        self.clamp();
        self.touch();
        if ok {
            self.say("redo", NoteKind::Plain);
        }
    }

    fn clamp(&mut self) {
        let axes = self.project.form().map(|f| f.axis_count()).unwrap_or(0);
        if self.coords.len() != axes {
            self.coords = vec![0.0; axes];
        }
        self.selected_corner = self
            .selected_corner
            .min(self.project.frames().len().saturating_sub(1));
        self.selected_lane = self
            .selected_lane
            .min(self.project.lane_capacity().saturating_sub(1));
    }

    fn shortcuts(&mut self, input: &InputFrame) {
        if self.path_bar.is_some() || self.ui.focus.is_some() {
            return;
        }
        if !input.ctrl {
            return;
        }
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
        if input.key(Key::E) {
            self.open_path_bar(PathIntent::ExportBody);
        }
    }
}

impl eframe::App for Station {
    fn update(&mut self, ctx: &egui::Context, _f: &mut eframe::Frame) {
        let input = InputFrame::read(ctx);

        let dropped: Vec<PathBuf> = ctx.input(|i| {
            i.raw
                .dropped_files
                .iter()
                .filter_map(|f| f.path.clone())
                .collect()
        });
        if let Some(p) = dropped.first().cloned() {
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
            crate::ui::paint::fill(&painter, full, theme::BG);

            let mut state = std::mem::take(&mut self.ui);
            state.begin_frame();
            {
                let mut ui = Ui::new(&painter, &input, &mut state, full);
                ui.background(full);
                view::draw(self, &mut ui, full);
            }
            self.ui = state;
        });

        self.recompute();
        ctx.request_repaint();
    }
}
