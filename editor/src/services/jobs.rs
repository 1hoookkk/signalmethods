use std::sync::mpsc::{channel, Receiver};
use std::time::Instant;

use trench_core::arma_endpoint::ArmaFit;
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::{
    geometry_from_words_at, words_from_geometry_at, words_from_roots_at,
};

use crate::engine::fit;
use crate::engine::response::SR;
use crate::services::audio::Audio;
use crate::session::state::{FitError, FitState, Session};

enum FitMsg {
    Done(Box<ArmaFit>),
    Failed(FitError),
}

pub struct Jobs {
    rx: Option<Receiver<FitMsg>>,
    field_audio: Option<(f64, PackedCorners)>,
}

impl Jobs {
    pub fn new() -> Self {
        Self {
            rx: None,
            field_audio: None,
        }
    }

    pub fn running(&self) -> bool {
        self.rx.is_some()
    }

    pub fn fit_frame(&mut self, session: &mut Session) {
        if self.running() {
            return;
        }
        let Some(pairs) = session.target_pairs() else {
            session.fit = FitState::Failed(FitError::NoTarget);
            return;
        };
        session.history.push(&session.document);
        let lanes = *session.active_lanes();
        let laws = session.document.workspace.laws;
        let declared = session.document.workspace.declared();
        let (tx, rx) = channel();
        self.rx = Some(rx);
        session.fit = FitState::Running {
            started: Instant::now(),
            corners_done: 0,
        };
        std::thread::spawn(move || {
            let _ = tx.send(match fit::fit_frame(&pairs, lanes, laws, declared) {
                Some(f) => FitMsg::Done(Box::new(f)),
                None => FitMsg::Failed(FitError::DidNotConverge),
            });
        });
    }

    pub fn fit_lane(&mut self, session: &mut Session, lane: usize) {
        if self.running() {
            return;
        }
        let Some(pairs) = session.target_pairs() else {
            session.fit = FitState::Failed(FitError::NoTarget);
            return;
        };
        let law = session.document.workspace.laws[lane];
        if !law.writable {
            session.fit = FitState::Failed(FitError::SectionHeld(lane));
            return;
        }
        session.history.push(&session.document);
        let lanes = *session.active_lanes();
        let (tx, rx) = channel();
        self.rx = Some(rx);
        session.fit = FitState::Running {
            started: Instant::now(),
            corners_done: 0,
        };
        std::thread::spawn(move || {
            let _ = tx.send(match fit::fit_lane(&pairs, lanes, lane, law) {
                Some(f) => FitMsg::Done(Box::new(f)),
                None => FitMsg::Failed(FitError::SectionFoundNothing(lane)),
            });
        });
    }

    pub fn poll(&mut self, session: &mut Session, audio: &mut Audio) {
        let Some(rx) = &self.rx else { return };
        let mut done = None;
        let mut failed = None;
        for msg in rx.try_iter() {
            match msg {
                FitMsg::Done(f) => done = Some(f),
                FitMsg::Failed(e) => failed = Some(e),
            }
        }
        if let Some(f) = done {
            *session.active_lanes_mut() = f.roots;
            let held = session
                .active_lanes()
                .iter()
                .filter(|l| !crate::domain::document::lane_is_empty(l))
                .count();
            session.fit = FitState::Complete {
                rms_db: f.target_rms_db,
                sections: held,
            };
            self.rx = None;
            self.invalidate_field_audio();
            self.push_audio(session, audio);
        } else if let Some(e) = failed {
            session.fit = FitState::Failed(e);
            self.rx = None;
        }
    }

    pub fn push_audio(&mut self, session: &Session, audio: &mut Audio) {
        let rate = audio.rate;
        {
            if let Some(packed) = session.document.field.words_at(SR) {
                let rebuild = match &self.field_audio {
                    Some((r, _)) => (*r - rate).abs() > 1e-9,
                    None => true,
                };
                if rebuild {
                    let repacked = session.document.field.words_at(rate).unwrap_or_else(|| {
                        let mut words = packed.words;
                        for corner in words.iter_mut() {
                            for stage in corner.iter_mut() {
                                let g = geometry_from_words_at(*stage, SR);
                                *stage = words_from_geometry_at(&g, rate);
                            }
                        }
                        PackedCorners { words }
                    });
                    self.field_audio = Some((rate, repacked));
                }
                if let Some((_, packed)) = &self.field_audio {
                    let p = session.audition.pos;
                    audio.push_rows(packed.interpolate_biquad(p[0], p[1], p[2]));
                    return;
                }
            }
        }
        let mut rows = [[1.0, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
        for (row, lane) in rows.iter_mut().zip(session.active_lanes()) {
            *row = trench_core::minifloat::stage_words_to_biquad(words_from_roots_at(lane, rate));
        }
        audio.push_rows(rows);
    }

    pub fn invalidate_field_audio(&mut self) {
        self.field_audio = None;
    }
}

pub mod pca {
    use super::*;
    use author::{body, envelope, pca};
    use std::path::Path;

    pub fn subjects(root: &Path) -> Vec<String> {
        let dvtd = root.join("recipes").join("vocal").join("dvtd");
        let mut subjects: Vec<String> = std::fs::read_dir(&dvtd)
            .map(|rd| {
                rd.flatten()
                    .filter(|e| e.path().is_dir())
                    .map(|e| e.file_name().to_string_lossy().into_owned())
                    .filter(|n| n.starts_with("subject"))
                    .collect()
            })
            .unwrap_or_default();
        subjects.sort();
        subjects
    }

    pub fn load_basis(root: &Path, subject: &str) -> Result<(pca::Basis, Vec<[f64; 2]>), String> {
        let dir = root
            .join("recipes")
            .join("vocal")
            .join("dvtd")
            .join(subject);
        let files = envelope::scan(&dir);
        let mut rows = Vec::new();
        for (_, path) in &files {
            rows.push(envelope::on_grid(&envelope::read_vvtf(path)?));
        }
        let basis = pca::fit(&rows, 3)?;
        let dots = rows
            .iter()
            .map(|row| {
                let s = basis.project(row);
                [s[0], s[1]]
            })
            .collect();
        Ok((basis, dots))
    }

    pub fn corner_scores(basis: &pca::Basis) -> [[f64; 3]; NUM_CORNERS] {
        std::array::from_fn(|ci| {
            std::array::from_fn(|axis| {
                let sigma = basis.sigma.get(axis).copied().unwrap_or(0.0);
                if ci >> axis & 1 == 1 {
                    sigma
                } else {
                    -sigma
                }
            })
        })
    }

    pub fn build(
        basis: &pca::Basis,
        scores: &[[f64; 3]; NUM_CORNERS],
        on_corner: &mut dyn FnMut(usize, f64),
    ) -> Result<(body::Build, Vec<Vec<[f64; 2]>>), String> {
        let curves: [Vec<f64>; NUM_CORNERS] =
            std::array::from_fn(|ci| basis.reconstruct(&scores[ci]));
        let grid = envelope::grid();
        let build = body::build_progress(&curves, on_corner)?;
        let targets: Vec<Vec<[f64; 2]>> = curves
            .iter()
            .map(|c| grid.iter().zip(c).map(|(&f, &db)| [f, db]).collect())
            .collect();
        Ok((build, targets))
    }

    pub fn _coeff_dims() -> (usize, usize) {
        (NUM_STAGES, NUM_COEFFS)
    }
}
