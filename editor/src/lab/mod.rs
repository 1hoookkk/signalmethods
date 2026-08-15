use std::path::{Path, PathBuf};

use eframe::egui::{Align2, ColorImage, Ui};
use trench_core::stage_law::{RootPair, StageRoots};

use crate::domain::document::Target;
use crate::engine::response::{row_db, SR};
use crate::services::repository::Entry;
use crate::services::Services;
use crate::session::command::{self, Command};
use crate::session::state::Session;
use crate::ui::{paint, theme};

#[derive(Clone, Copy, PartialEq)]
pub enum Screen {
    Home,
    Fit,
}

pub struct Lab {
    pub case_name: String,
    pub screen: Screen,
    pub shot: Option<PathBuf>,
    pub frames: u32,
    pub taken: bool,
}

fn conj(p: RootPair) -> Option<(f64, f64)> {
    match p {
        RootPair::Conjugate { hz, r } => Some((hz, r)),
        RootPair::Degenerate => Some((0.0, 0.0)),
        RootPair::RealPair { .. } => None,
    }
}

pub fn session_for(services: &mut Services, fixture: &str) -> Result<Session, String> {
    let mut session = Session::new();
    let path = services
        .repository
        .root
        .join("dev")
        .join("fixtures")
        .join(format!("{fixture}.json"));
    let text =
        std::fs::read_to_string(&path).map_err(|e| format!("{}: {e}", path.display()))?;
    let fx: serde_json::Value =
        serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;

    if let Some(rel) = fx.get("body").and_then(|v| v.as_str()) {
        let bp = services.repository.root.join(rel);
        let packed = services.repository.read_body(&bp)?;
        let ci = fx.get("corner").and_then(|v| v.as_u64()).unwrap_or(0) as usize;
        let geoms: Vec<_> = packed.words[ci]
            .iter()
            .map(|&w| trench_core::stage_law::geometry_from_words_at(w, SR))
            .collect();
        let grid = author::envelope::grid();
        let rows: Vec<[f64; 5]> = geoms.iter().map(|g| g.biquad_at(SR)).collect();
        let curve: Vec<f64> = grid
            .iter()
            .map(|&hz| rows.iter().map(|r| row_db(r, hz, SR)).sum())
            .collect();
        let name = format!("{rel} c{ci}");
        session.document.target = Some(Target {
            name: name.clone(),
            curve,
        });
        for (si, g) in geoms.iter().take(7).enumerate() {
            if let (Some((ph, pr)), Some((zh, zr))) = (conj(g.pole), conj(g.zero)) {
                session.document.workspace.lanes[si] = StageRoots {
                    pole_hz: ph,
                    pole_r: pr,
                    zero_hz: zh,
                    zero_r: zr,
                    scale: g.scale,
                };
            }
        }
        session.document.workspace.seed_name = Some(name);
        for si in 0..trench_core::cascade::NUM_STAGES {
            if crate::domain::document::lane_is_empty(&session.document.workspace.lanes[si]) {
                session.document.workspace.laws[si].writable = false;
            }
        }
        if let Some(m) = fx.get("target_mouth").and_then(|v| v.as_str()) {
            let idx = services
                .repository
                .entries
                .iter()
                .position(|e| matches!(e, Entry::Mouth { name, .. } if name == m))
                .ok_or_else(|| format!("fixture target mouth {m} not in library"))?;
            command::apply(&mut session, services, Command::SetTarget(idx))?;
        }
        for ci in 0..4 {
            let geoms: Vec<_> = packed.words[ci]
                .iter()
                .map(|&w| trench_core::stage_law::geometry_from_words_at(w, SR))
                .collect();
            let mut lanes = [StageRoots::IDENTITY; 7];
            for (si, g) in geoms.iter().take(7).enumerate() {
                if let (Some((ph, pr)), Some((zh, zr))) = (conj(g.pole), conj(g.zero)) {
                    lanes[si] = StageRoots {
                        pole_hz: ph,
                        pole_r: pr,
                        zero_hz: zh,
                        zero_r: zr,
                        scale: g.scale,
                    };
                }
            }
            session.document.field.slots[ci] = Some(author::frame::Frame {
                name: format!("{rel} c{ci}"),
                sr_hz: SR,
                provenance: String::new(),
                words: String::new(),
                lanes,
                laws: [author::frame::LaneLaw::OPEN; 7],
            });
        }
        return Ok(session);
    }

    if let Some(mouth) = fx.get("mouth").and_then(|v| v.as_str()) {
        let idx = services
            .repository
            .entries
            .iter()
            .position(|e| matches!(e, Entry::Mouth { name, .. } if name == mouth))
            .ok_or_else(|| format!("fixture mouth {mouth} not in library"))?;
        let cmd = if fx.get("seed").and_then(|v| v.as_bool()).unwrap_or(false) {
            Command::SeedFromMouth(idx)
        } else {
            Command::SetTarget(idx)
        };
        command::apply(&mut session, services, cmd)?;
        if fx.get("hand_zeros").and_then(|v| v.as_bool()).unwrap_or(false) {
            let poles: Vec<f64> = session
                .document
                .pole_candidates
                .iter()
                .map(|p| p.hz)
                .collect();
            for (k, &hz) in poles.iter().enumerate() {
                command::apply(
                    &mut session,
                    services,
                    Command::AssignSection { section: k, hz },
                )?;
                let next = if k + 1 < poles.len() {
                    poles[k + 1]
                } else {
                    (hz * 3.0).min(15_000.0)
                };
                command::apply(
                    &mut session,
                    services,
                    Command::SetZero {
                        section: k,
                        hz: (hz * next).sqrt(),
                        r: 1.0,
                    },
                )?;
            }
        }
    }
    Ok(session)
}

pub fn draw(session: &Session, lab: &mut Lab, ui: &mut Ui) -> Vec<Command> {
    match lab.case_name.as_str() {
        "workstation" => match lab.screen {
            Screen::Home => {
                let (cmds, open) = crate::ui::field_home::draw(session, ui);
                if open.is_some() {
                    lab.screen = Screen::Fit;
                }
                cmds
            }
            Screen::Fit => {
                let (cmds, back) = crate::ui::response::draw(session, ui, true);
                if back {
                    lab.screen = Screen::Home;
                }
                cmds
            }
        },
        "response_only" => crate::ui::response::draw(session, ui, false).0,
        "sections" => {
            crate::ui::sections_case::draw(session, ui);
            Vec::new()
        }
        other => {
            let rect = ui.max_rect();
            let painter = ui.painter().clone();
            painter.rect_filled(rect, 0.0, theme::CHROME);
            paint::label(
                &painter,
                rect.center(),
                Align2::CENTER_CENTER,
                other,
                theme::BODY,
                theme::ALARM,
            );
            Vec::new()
        }
    }
}

pub fn save_png(path: &Path, img: &ColorImage) -> Result<(), String> {
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir).map_err(|e| e.to_string())?;
    }
    let bytes: Vec<u8> = img.pixels.iter().flat_map(|c| c.to_array()).collect();
    image::save_buffer(
        path,
        &bytes,
        img.size[0] as u32,
        img.size[1] as u32,
        image::ColorType::Rgba8,
    )
    .map_err(|e| e.to_string())
}
