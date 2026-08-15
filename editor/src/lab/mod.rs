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

pub struct Lab {
    pub case_name: String,
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
    }
    Ok(session)
}

pub fn draw(session: &Session, case: &str, ui: &mut Ui) {
    match case {
        "response_only" => crate::ui::response::draw(session, ui),
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
