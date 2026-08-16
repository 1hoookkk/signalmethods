use std::path::PathBuf;

use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::StageRoots;

use crate::domain::document::{lane_is_empty, Document};
use crate::domain::field::CornerFrame;
use crate::engine::fit::{audit_field, fit_frame, snap_to_words};
use crate::engine::response::SR;
use crate::services::Services;
use crate::session::state::{FitState, Session};

#[derive(Clone)]
pub enum Command {
    NewSession,
    SelectCorner(Option<usize>),
    SelectSection(Option<usize>),
    SetAuditionPos([f32; 3]),
    AddAuditionMarker([f32; 3]),
    ClearAuditionMarkers,
    SetAuditionPlaying(bool),
    SetPole { section: usize, hz: f64, r: f64 },
    SetZero { section: usize, hz: f64, r: f64 },
    SetScale { section: usize, scale: f64 },
    SwapSections { a: usize, b: usize },
    ClearSection(usize),
    FitActiveCorner,
    FitAllGrayOrder,
    WriteField,
    LoadBody(PathBuf),
    Undo,
    Redo,
}

pub fn apply(session: &mut Session, services: &mut Services, cmd: Command) -> Result<(), String> {
    match cmd {
        Command::NewSession => {
            session.history.push(&session.document);
            session.document = Document::new();
            session.selection = Default::default();
            session.notice = Some((false, "new workspace".into()));
        }
        Command::SelectCorner(c) => {
            session.selection.corner = c;
        }
        Command::SelectSection(s) => {
            session.selection.section = s;
        }
        Command::SetAuditionPos(pos) => {
            session.audition.pos = pos;
            let rows = session.current_interpolated_rows();
            services.audio.push_rows(rows);
        }
        Command::AddAuditionMarker(pos) => {
            session.audition.markers.push(pos);
        }
        Command::ClearAuditionMarkers => {
            session.audition.markers.clear();
        }
        Command::SetAuditionPlaying(playing) => {
            session.audition.playing = playing;
            if let Ok(mut shared) = services.audio.shared.lock() {
                shared.playing = playing;
            }
            if playing {
                services.audio.ensure_stream();
            }
        }
        Command::SetPole { section, hz, r } => {
            if section < NUM_STAGES {
                session.history.push(&session.document);
                let lanes = session.active_lanes_mut();
                lanes[section].pole_hz = hz.clamp(40.0, 16_000.0);
                lanes[section].pole_r = r.clamp(0.0, 0.9999);
                snap_to_words(lanes);
                let rows = session.current_interpolated_rows();
                services.audio.push_rows(rows);
            }
        }
        Command::SetZero { section, hz, r } => {
            if section < NUM_STAGES {
                session.history.push(&session.document);
                let lanes = session.active_lanes_mut();
                lanes[section].zero_hz = hz.clamp(40.0, 16_000.0);
                lanes[section].zero_r = r.clamp(0.0, 1.0);
                snap_to_words(lanes);
                let rows = session.current_interpolated_rows();
                services.audio.push_rows(rows);
            }
        }
        Command::SetScale { section, scale } => {
            if section < NUM_STAGES {
                session.history.push(&session.document);
                let lanes = session.active_lanes_mut();
                lanes[section].scale = scale.max(1e-6);
                let rows = session.current_interpolated_rows();
                services.audio.push_rows(rows);
            }
        }
        Command::SwapSections { a, b } => {
            if a < NUM_STAGES && b < NUM_STAGES && a != b {
                session.history.push(&session.document);
                if let Some(ci) = session.selection.corner {
                    session.document.field.swap_sections(ci, a, b);
                } else {
                    session.document.workspace.lanes.swap(a, b);
                }
                let rows = session.current_interpolated_rows();
                services.audio.push_rows(rows);
            }
        }
        Command::ClearSection(s) => {
            if s < NUM_STAGES {
                session.history.push(&session.document);
                let lanes = session.active_lanes_mut();
                lanes[s] = StageRoots::IDENTITY;
                let rows = session.current_interpolated_rows();
                services.audio.push_rows(rows);
            }
        }
        Command::FitActiveCorner => {
            if let Some(target) = &session.document.target {
                session.history.push(&session.document);
                let pairs: Vec<(f64, f64)> = author::envelope::grid()
                    .into_iter()
                    .zip(target.curve.iter().copied())
                    .collect();
                let declared = session.document.workspace.declared();
                let current_lanes = *session.active_lanes();
                if let Some(fit) = fit_frame(&pairs, current_lanes, declared) {
                    *session.active_lanes_mut() = fit.roots;
                    session.fit = FitState::Complete {
                        rms_db: fit.target_rms_db,
                        sections: fit.roots.iter().filter(|l| !lane_is_empty(l)).count(),
                    };
                    session.notice = Some((false, format!("fit: {:.2} dB rms", fit.target_rms_db)));
                    let rows = session.current_interpolated_rows();
                    services.audio.push_rows(rows);
                } else {
                    session.fit = FitState::Failed("optimizer did not converge".into());
                    return Err("optimizer did not converge".into());
                }
            } else {
                return Err("no target envelope loaded".into());
            }
        }
        Command::FitAllGrayOrder => {
            session.history.push(&session.document);
            let gray_order = [0, 1, 3, 2, 6, 7, 5, 4];
            let mut last_lanes = session.document.workspace.lanes;
            for &ci in &gray_order {
                if let Some(frame) = &session.document.field.slots[ci] {
                    last_lanes = frame.lanes;
                } else {
                    session.document.field.slots[ci] = Some(CornerFrame::new(
                        format!("Corner {ci}"),
                        last_lanes,
                    ));
                }
            }
            if let Some((_, report)) = audit_field(&session.document.field) {
                session.notice = Some((
                    false,
                    format!("audit crown: {:.1} dB", report.audit.interior_crown_db),
                ));
            }
        }
        Command::WriteField => {
            if let Some((packed, report)) = audit_field(&session.document.field) {
                let out_path = services.repository.root.join("authored.body");
                let mut bytes = [0u8; 560];
                let native = packed.to_native_bytes();
                let len = native.len().min(560);
                bytes[..len].copy_from_slice(&native[..len]);
                std::fs::write(&out_path, bytes)
                    .map_err(|e| format!("write failed {}: {e}", out_path.display()))?;
                session.notice = Some((
                    false,
                    format!("wrote 560B body (crown {:.1} dB)", report.audit.interior_crown_db),
                ));
            } else {
                return Err("cannot write incomplete field (needs 4 or 8 corners)".into());
            }
        }
        Command::LoadBody(path) => {
            let packed = services.repository.read_body(&path)?;
            session.history.push(&session.document);
            for ci in 0..8 {
                let geoms: Vec<_> = packed.words[ci]
                    .iter()
                    .map(|&w| trench_core::stage_law::geometry_from_words_at(w, SR))
                    .collect();
                let mut lanes = [StageRoots::IDENTITY; NUM_STAGES];
                for (si, g) in geoms.iter().take(NUM_STAGES).enumerate() {
                    let c = |p: trench_core::stage_law::RootPair| match p {
                        trench_core::stage_law::RootPair::Conjugate { hz, r } => Some((hz, r)),
                        trench_core::stage_law::RootPair::Degenerate => Some((0.0, 0.0)),
                        trench_core::stage_law::RootPair::RealPair { .. } => None,
                    };
                    if let (Some((ph, pr)), Some((zh, zr))) = (c(g.pole), c(g.zero)) {
                        lanes[si] = StageRoots {
                            pole_hz: ph,
                            pole_r: pr,
                            zero_hz: zh,
                            zero_r: zr,
                            scale: g.scale,
                        };
                    }
                }
                session.document.field.slots[ci] = Some(CornerFrame::new(
                    format!("Corner {ci}"),
                    lanes,
                ));
            }
            session.document.workspace.lanes = session.document.field.slots[0]
                .as_ref()
                .map(|f| f.lanes)
                .unwrap_or([StageRoots::IDENTITY; NUM_STAGES]);
            session.notice = Some((false, format!("loaded {}", path.display())));
            let rows = session.current_interpolated_rows();
            services.audio.push_rows(rows);
        }
        Command::Undo => {
            if let Some(prev) = session.history.undo(&session.document) {
                session.document = prev;
                let rows = session.current_interpolated_rows();
                services.audio.push_rows(rows);
            }
        }
        Command::Redo => {
            if let Some(next) = session.history.redo(&session.document) {
                session.document = next;
                let rows = session.current_interpolated_rows();
                services.audio.push_rows(rows);
            }
        }
    }
    Ok(())
}
