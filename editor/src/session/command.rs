use author::frame::{Frame, LaneLaw};
use trench_core::cascade::NUM_STAGES;
use trench_core::praat_endpoint;
use trench_core::stage_law::StageRoots;

use crate::domain::document::{lane_is_empty, Target, Workspace};
use crate::engine::response::{row_db, SR};
use crate::services::Services;
use crate::session::state::{FitState, Selection, Session};

pub enum Command {
    BeginEdit,
    Select(Selection),
    SetTarget(usize),
    SeedFromMouth(usize),
    ApplyCage(usize),
    SetPole { section: usize, hz: f64, r: f64 },
    SetZero { section: usize, hz: f64, r: f64 },
    SetLaw { section: usize, law: LaneLaw },
    SetScale { section: usize, scale: f64 },
    AssignCorner { corner: usize, entry: usize },
    ClearCorner { corner: usize },
    SwapSections { a: usize, b: usize },
    ClearWorkspace,
    FitSelection,
    FitSection(usize),
    Keep,
    Undo,
    Redo,
}

pub fn apply(session: &mut Session, services: &mut Services, cmd: Command) -> Result<(), String> {
    match cmd {
        Command::BeginEdit => {
            session.history.push(&session.document);
            Ok(())
        }
        Command::Select(sel) => {
            session.selection = sel;
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::SetTarget(index) => {
            let (name, curve) = services.repository.load_mouth(index)?;
            session.document.target = Some(Target { name, curve });
            session.selection.entry = Some(index);
            Ok(())
        }
        Command::SeedFromMouth(index) => {
            let (name, curve) = services.repository.load_mouth(index)?;
            session.history.push(&session.document);
            session.selection.corner = None;
            let grid = author::envelope::grid();
            let peaks = author::formants::peaks(&grid, &curve);
            if peaks.is_empty() {
                return Err(format!("{name}: no formant peaks found"));
            }
            session.document.target = Some(Target {
                name: name.clone(),
                curve: curve.clone(),
            });
            session.selection.entry = Some(index);
            let mut ws = Workspace::empty();
            let mut placed = 0;
            for (fi, peak) in peaks.iter().take(NUM_STAGES).enumerate() {
                if let Some((_, r)) = praat_endpoint::pole_from_frequency_bandwidth(
                    peak.hz,
                    peak.bandwidth_hz,
                    SR,
                ) {
                    ws.lanes[placed] = StageRoots {
                        pole_hz: peak.hz,
                        pole_r: r,
                        zero_hz: 0.0,
                        zero_r: 0.0,
                        scale: 1.0,
                    };
                    ws.lane_jobs[placed] = Some((fi + 1, peak.hz));
                    placed += 1;
                }
            }
            if placed > 0 {
                let rows: Vec<_> = ws
                    .lanes
                    .iter()
                    .take(placed)
                    .map(|l| l.biquad_at(SR))
                    .collect();
                let mut diff = 0.0;
                for (k, &hz) in grid.iter().enumerate() {
                    let sum: f64 = rows.iter().map(|r| row_db(r, hz, SR)).sum();
                    diff += curve[k] - sum;
                }
                diff /= grid.len() as f64;
                let per = 10f64.powf(diff / (20.0 * placed as f64));
                for lane in ws.lanes.iter_mut().take(placed) {
                    lane.scale = per;
                }
            }
            ws.seed_name = Some(name);
            session.document.workspace = ws;
            session.fit = FitState::Idle;
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::ApplyCage(index) => {
            let cage = services.repository.load_cage(index)?;
            session.history.push(&session.document);
            session.document.workspace.laws = cage.laws;
            Ok(())
        }
        Command::SetPole { section, hz, r } => {
            let lane = &mut session.active_lanes_mut()[section];
            lane.pole_hz = hz;
            lane.pole_r = r;
            session.fit = FitState::Idle;
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::SetZero { section, hz, r } => {
            let lane = &mut session.active_lanes_mut()[section];
            lane.zero_hz = hz;
            lane.zero_r = r;
            session.fit = FitState::Idle;
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::SetScale { section, scale } => {
            session.active_lanes_mut()[section].scale = scale;
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::SetLaw { section, law } => {
            session.document.workspace.laws[section] = law;
            Ok(())
        }
        Command::AssignCorner { corner, entry } => {
            let frame = services.repository.load_frame(entry)?;
            session.history.push(&session.document);
            session.document.field.slots[corner] = Some(frame);
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::ClearCorner { corner } => {
            session.history.push(&session.document);
            session.document.field.slots[corner] = None;
            if session.selection.corner == Some(corner) {
                session.selection.corner = None;
            }
            services.jobs.invalidate_field_audio();
            Ok(())
        }
        Command::SwapSections { a, b } => {
            session.history.push(&session.document);
            if let Some(ci) = session.selection.corner {
                session.document.field.swap_sections(ci, a, b);
            } else {
                let ws = &mut session.document.workspace;
                ws.lanes.swap(a, b);
                ws.laws.swap(a, b);
                ws.lane_jobs.swap(a, b);
            }
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::ClearWorkspace => {
            session.history.push(&session.document);
            session.document.workspace = Workspace::empty();
            session.fit = FitState::Idle;
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::FitSelection => {
            if let Some(si) = session.selection.section {
                services.jobs.fit_lane(session, si);
            } else {
                services.jobs.fit_frame(session);
            }
            Ok(())
        }
        Command::FitSection(si) => {
            services.jobs.fit_lane(session, si);
            Ok(())
        }
        Command::Keep => {
            let lanes = *session.active_lanes();
            if lanes.iter().all(lane_is_empty) {
                return Err("nothing to keep".into());
            }
            let laws = session.document.workspace.laws;
            let provenance = match (&session.document.target, &session.fit) {
                (Some(t), FitState::Complete { rms_db, .. }) => {
                    format!("fit of {} · rms {rms_db:.2} dB", t.name)
                }
                (Some(t), _) => format!("hand lanes against {}", t.name),
                _ => "hand lanes".into(),
            };
            let mut frame = Frame {
                name: String::new(),
                sr_hz: SR,
                provenance,
                words: String::new(),
                lanes,
                laws,
            };
            services.repository.write_frame(&mut frame)?;
            Ok(())
        }
        Command::Undo => {
            if let Some(doc) = session.history.undo(&session.document) {
                session.document = doc;
                session.fit = FitState::Idle;
                services.jobs.invalidate_field_audio();
                services.jobs.push_audio(session, &mut services.audio);
            }
            Ok(())
        }
        Command::Redo => {
            if let Some(doc) = session.history.redo(&session.document) {
                session.document = doc;
                session.fit = FitState::Idle;
                services.jobs.invalidate_field_audio();
                services.jobs.push_audio(session, &mut services.audio);
            }
            Ok(())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::engine::fit::{audit_field, fit_frame};
    use crate::services::repository::Entry;

    fn session_and_services() -> (Session, Services) {
        (Session::new(), Services::new())
    }

    #[test]
    fn four_frames_assemble_into_an_audited_square_field() {
        let (mut session, mut services) = session_and_services();
        let mouths: Vec<usize> = services
            .repository
            .entries
            .iter()
            .enumerate()
            .filter(|(_, e)| matches!(e, Entry::Mouth { .. }))
            .map(|(i, _)| i)
            .take(2)
            .collect();
        assert_eq!(mouths.len(), 2, "need two mouths");
        let mut frames = Vec::new();
        for &i in &mouths {
            apply(&mut session, &mut services, Command::SetTarget(i)).unwrap();
            let pairs = session.target_pairs().unwrap();
            let fit = fit_frame(&pairs, session.document.workspace.lanes,
                session.document.workspace.laws, false)
                .expect("cold fit converges");
            frames.push(Frame {
                name: session.document.target.as_ref().unwrap().name.clone(),
                sr_hz: SR,
                provenance: String::new(),
                words: String::new(),
                lanes: fit.roots,
                laws: [LaneLaw::OPEN; NUM_STAGES],
            });
        }

        for ci in 0..4 {
            session.document.field.slots[ci] = Some(frames[0].clone());
        }
        let (packed, report) = audit_field(&session.document.field).expect("assembly incomplete");
        assert!(report.is_square);
        assert!(
            report.audit.pass() && report.relative_ok,
            "a field of one frame at four corners must pass: {:?}",
            report.audit.failures
        );
        let single_crown = report.frames_crown_db;
        let bytes: Vec<u8> = match packed.to_legacy_bytes() {
            Some(b) if report.legacy => b.to_vec(),
            _ => packed.to_native_bytes().to_vec(),
        };
        assert!(bytes.len() == 240 || bytes.len() == 560);
        let artifact = services
            .repository
            .root
            .join("recipes")
            .join("hero")
            .join("m3-artifact.body");
        std::fs::create_dir_all(artifact.parent().unwrap()).unwrap();
        std::fs::write(&artifact, &bytes).unwrap();

        session.document.field.slots[1] = Some(frames[1].clone());
        session.document.field.slots[3] = Some(frames[1].clone());
        let (_, mixed) = audit_field(&session.document.field).expect("mixed assembly incomplete");
        assert!(mixed.audit.finite && mixed.audit.stable);
        assert!(
            mixed.frames_crown_db >= single_crown - 1e-9,
            "the frames ceiling must cover both frames"
        );
    }

    #[test]
    fn selecting_a_corner_scopes_edits_to_that_corner_without_a_mode() {
        let (mut session, mut services) = session_and_services();
        let frame = Frame {
            name: "surgery-subject".into(),
            sr_hz: SR,
            provenance: String::new(),
            words: String::new(),
            lanes: {
                let mut lanes = [StageRoots::IDENTITY; NUM_STAGES];
                lanes[0] = StageRoots {
                    pole_hz: 700.0,
                    pole_r: 0.98,
                    zero_hz: 0.0,
                    zero_r: 0.0,
                    scale: 1.0,
                };
                lanes
            },
            laws: [LaneLaw::OPEN; NUM_STAGES],
        };
        for ci in 0..4 {
            session.document.field.slots[ci] = Some(frame.clone());
        }

        apply(
            &mut session,
            &mut services,
            Command::Select(Selection {
                corner: Some(2),
                section: None,
                entry: None,
            }),
        )
        .unwrap();
        assert_eq!(session.active_lanes()[0].pole_hz, 700.0);

        apply(&mut session, &mut services, Command::BeginEdit).unwrap();
        apply(
            &mut session,
            &mut services,
            Command::SetPole {
                section: 0,
                hz: 900.0,
                r: 0.98,
            },
        )
        .unwrap();
        assert_eq!(
            session.document.field.slots[2].as_ref().unwrap().lanes[0].pole_hz,
            900.0
        );
        assert_eq!(
            session.document.field.slots[0].as_ref().unwrap().lanes[0].pole_hz,
            700.0,
            "editing corner 2 must not touch corner 0"
        );

        apply(&mut session, &mut services, Command::Undo).unwrap();
        assert_eq!(
            session.document.field.slots[2].as_ref().unwrap().lanes[0].pole_hz,
            700.0,
            "undo restores the document"
        );
    }
}
