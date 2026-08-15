use author::frame::{Frame, LaneLaw};
use trench_core::cascade::NUM_STAGES;
use trench_core::praat_endpoint;
use trench_core::stage_law::StageRoots;

use crate::domain::document::{lane_is_empty, PolePair, Target, Workspace, ZeroPair};
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
    TargetCorner(usize),
    ExpandField,
    SwapSections { a: usize, b: usize },
    SwapZeros { a: usize, b: usize },
    AssignSection { section: usize, hz: f64 },
    ClearWorkspace,
    FitSelection,
    FitSection(usize),
    Keep,
    TogglePlay,
    Ride([f32; 3]),
    Undo,
    Redo,
}

fn section_exists(session: &Session, section: usize) -> Result<(), String> {
    if session.selection.corner.is_none()
        && lane_is_empty(&session.document.workspace.lanes[section])
    {
        return Err("assign the section first: candidates are not sections".into());
    }
    Ok(())
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
            session.document.target = Some(Target {
                name: name.clone(),
                curve: curve.clone(),
            });
            session.selection.entry = Some(index);
            let mut ws = Workspace::empty();
            let mut residual = curve.clone();
            let mut tilts: Vec<StageRoots> = Vec::new();
            for _ in 0..2 {
                let Some(lane) = tilt_section(&residual, &grid) else {
                    break;
                };
                let row = lane.biquad_at(SR);
                for (v, &hz) in residual.iter_mut().zip(grid.iter()) {
                    *v -= row_db(&row, hz, SR);
                }
                tilts.push(lane);
            }
            let peaks = author::formants::peaks(&grid, &residual);
            if peaks.is_empty() && tilts.is_empty() {
                return Err(format!("{name}: nothing measurable to seed"));
            }
            let mut placed = 0;
            let formant_budget = NUM_STAGES - tilts.len();
            for (fi, peak) in peaks.iter().take(formant_budget).enumerate() {
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
            let inverted: Vec<f64> = residual.iter().map(|v| -v).collect();
            let mut notches = author::formants::peaks(&grid, &inverted);
            for k in 0..placed {
                let here = ws.lanes[k].pole_hz;
                let next = if k + 1 < placed {
                    ws.lanes[k + 1].pole_hz
                } else {
                    (here * 4.0).min(16_000.0)
                };
                let found = notches
                    .iter()
                    .position(|n| n.hz > here && n.hz < next)
                    .map(|i| notches.remove(i));
                if let Some(n) = found {
                    let r = praat_endpoint::pole_from_frequency_bandwidth(n.hz, n.bandwidth_hz, SR)
                        .map(|(_, r)| r.min(1.0))
                        .unwrap_or(0.96);
                    ws.lanes[k].zero_hz = n.hz;
                    ws.lanes[k].zero_r = r;
                } else {
                    ws.lanes[k].zero_hz = (here * next).sqrt();
                    ws.lanes[k].zero_r = 0.93;
                }
                ws.laws[k].zone = [here / 1.3, here * 1.3];
            }
            for lane in tilts {
                if placed >= NUM_STAGES {
                    break;
                }
                ws.lanes[placed] = lane;
                ws.laws[placed].zone = [lane.pole_hz / 2.0, lane.pole_hz * 2.0];
                placed += 1;
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
            let _ = name;
            session.document.pole_candidates = ws
                .lanes
                .iter()
                .take(placed)
                .filter(|l| l.pole_r > 0.0)
                .map(|l| PolePair {
                    hz: l.pole_hz,
                    r: l.pole_r,
                })
                .collect();
            session.document.zero_candidates = ws
                .lanes
                .iter()
                .take(placed)
                .filter(|l| l.zero_r > 0.0)
                .map(|l| ZeroPair {
                    hz: l.zero_hz,
                    r: l.zero_r,
                })
                .collect();
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
            section_exists(session, section)?;
            let lane = &mut session.active_lanes_mut()[section];
            lane.pole_hz = hz;
            lane.pole_r = r;
            session.fit = FitState::Idle;
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::SetZero { section, hz, r } => {
            section_exists(session, section)?;
            let lane = &mut session.active_lanes_mut()[section];
            lane.zero_hz = hz;
            lane.zero_r = r;
            session.fit = FitState::Idle;
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::SetScale { section, scale } => {
            section_exists(session, section)?;
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
        Command::TargetCorner(ci) => {
            let Some(frame) = session.document.field.slots[ci].clone() else {
                return Err("corner is empty".into());
            };
            let rows: Vec<_> = frame.lanes.iter().map(|l| l.biquad_at(SR)).collect();
            let curve: Vec<f64> = author::envelope::grid()
                .iter()
                .map(|&hz| rows.iter().map(|r| row_db(r, hz, SR)).sum())
                .collect();
            let code = format!(
                "m{} q{}{}",
                ci & 1,
                (ci >> 1) & 1,
                if ci & 4 != 0 { " t1" } else { "" }
            );
            session.document.target = Some(Target {
                name: format!("{} · {code}", frame.name),
                curve,
            });
            session.selection.corner = Some(ci);
            session.selection.section = None;
            session.fit = FitState::Idle;
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::ExpandField => {
            session.history.push(&session.document);
            let front: Vec<_> = session.document.field.slots[..4].to_vec();
            for (i, f) in front.into_iter().enumerate() {
                session.document.field.slots[i + 4] = f;
            }
            services.jobs.invalidate_field_audio();
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
        Command::SwapZeros { a, b } => {
            if a == b || a >= NUM_STAGES || b >= NUM_STAGES {
                return Ok(());
            }
            session.history.push(&session.document);
            let lanes = session.active_lanes_mut();
            let (zh, zr) = (lanes[a].zero_hz, lanes[a].zero_r);
            lanes[a].zero_hz = lanes[b].zero_hz;
            lanes[a].zero_r = lanes[b].zero_r;
            lanes[b].zero_hz = zh;
            lanes[b].zero_r = zr;
            services.jobs.invalidate_field_audio();
            services.jobs.push_audio(session, &mut services.audio);
            Ok(())
        }
        Command::AssignSection { section, hz } => {
            session.history.push(&session.document);
            let zone = [hz / 1.3, hz * 1.3];
            {
                let law = &mut session.document.workspace.laws[section];
                law.zone = zone;
                law.writable = true;
            }
            let empty = lane_is_empty(&session.active_lanes()[section]);
            if empty
                && session.selection.corner.is_none()
                && !session.document.pole_candidates.is_empty()
            {
                let pi = session
                    .document
                    .pole_candidates
                    .iter()
                    .enumerate()
                    .min_by(|a, b| {
                        (a.1.hz / hz).ln().abs().total_cmp(&(b.1.hz / hz).ln().abs())
                    })
                    .map(|(i, _)| i)
                    .unwrap();
                let pp = session.document.pole_candidates.remove(pi);
                let zi = session
                    .document
                    .zero_candidates
                    .iter()
                    .enumerate()
                    .filter(|(_, z)| z.hz > pp.hz && z.hz < pp.hz * 2.5)
                    .min_by(|a, b| a.1.hz.total_cmp(&b.1.hz))
                    .map(|(i, _)| i);
                let zp = zi.map(|i| session.document.zero_candidates.remove(i));
                let lane = &mut session.document.workspace.lanes[section];
                lane.pole_hz = pp.hz;
                lane.pole_r = pp.r;
                lane.scale = 1.0;
                if let Some(zp) = zp {
                    lane.zero_hz = zp.hz;
                    lane.zero_r = zp.r;
                }
                session.document.workspace.laws[section].zone = [pp.hz / 1.3, pp.hz * 1.3];
                session.selection.section = Some(section);
                session.fit = FitState::Idle;
                services.jobs.invalidate_field_audio();
                services.jobs.push_audio(session, &mut services.audio);
                return Ok(());
            }
            if empty {
                let mut pole = (hz, 0.95);
                let mut zero: Option<(f64, f64)> = None;
                if let Some(t) = &session.document.target {
                    let grid = author::envelope::grid();
                    let peaks = author::formants::peaks(&grid, &t.curve);
                    if let Some(p) = peaks
                        .iter()
                        .filter(|p| p.hz >= zone[0] && p.hz <= zone[1])
                        .min_by(|a, b| {
                            (a.hz / hz).ln().abs().total_cmp(&(b.hz / hz).ln().abs())
                        })
                    {
                        if let Some((_, r)) = praat_endpoint::pole_from_frequency_bandwidth(
                            p.hz,
                            p.bandwidth_hz,
                            SR,
                        ) {
                            pole = (p.hz, r);
                        }
                    }
                    let inverted: Vec<f64> = t.curve.iter().map(|v| -v).collect();
                    let notches = author::formants::peaks(&grid, &inverted);
                    if let Some(n) = notches
                        .iter()
                        .filter(|n| n.hz > pole.0 && n.hz < pole.0 * 2.5)
                        .min_by(|a, b| a.hz.total_cmp(&b.hz))
                    {
                        let r = praat_endpoint::pole_from_frequency_bandwidth(
                            n.hz,
                            n.bandwidth_hz,
                            SR,
                        )
                        .map(|(_, r)| r.min(1.0))
                        .unwrap_or(0.94);
                        zero = Some((n.hz, r));
                    }
                }
                let lane = &mut session.active_lanes_mut()[section];
                lane.pole_hz = pole.0;
                lane.pole_r = pole.1;
                lane.scale = 1.0;
                if let Some((zh, zr)) = zero {
                    lane.zero_hz = zh;
                    lane.zero_r = zr;
                }
            }
            session.selection.section = Some(section);
            session.fit = FitState::Idle;
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
            section_exists(session, si)?;
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
        Command::TogglePlay => {
            services.audio.ensure_stream();
            if let Some(e) = services.audio.error.clone() {
                return Err(e);
            }
            session.audition.playing = !session.audition.playing;
            if let Ok(mut s) = services.audio.shared.lock() {
                s.playing = session.audition.playing;
            }
            if session.audition.playing {
                services.jobs.push_audio(session, &mut services.audio);
            }
            Ok(())
        }
        Command::Ride(pos) => {
            session.audition.pos = pos;
            if session.audition.playing {
                services.jobs.push_audio(session, &mut services.audio);
            }
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

fn broad(curve: &[f64], grid: &[f64]) -> Vec<f64> {
    let n = curve.len();
    let mut out = vec![0.0; n];
    for i in 0..n {
        let mut acc = 0.0;
        let mut count = 0.0;
        for j in 0..n {
            if (grid[j] / grid[i]).ln().abs() < 0.45 {
                acc += curve[j];
                count += 1.0;
            }
        }
        out[i] = acc / count;
    }
    out
}

fn tilt_section(curve: &[f64], grid: &[f64]) -> Option<StageRoots> {
    let b = broad(curve, grid);
    let (mut imax, mut imin) = (0, 0);
    for i in 0..b.len() {
        if b[i] > b[imax] {
            imax = i;
        }
        if b[i] < b[imin] {
            imin = i;
        }
    }
    if b[imax] - b[imin] < 10.0 {
        return None;
    }
    let ph = grid[imax];
    let pr = (-std::f64::consts::PI * (ph * 4.0).min(6_000.0) / SR).exp();
    Some(StageRoots {
        pole_hz: ph,
        pole_r: pr,
        zero_hz: grid[imin],
        zero_r: 0.97,
        scale: 1.0,
    })
}
