pub mod plan;

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{channel, Receiver};
use std::sync::Arc;
use std::time::{Duration, Instant};

use trench_core::arma_endpoint::fit_arma_planned_watched;
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::StageRoots;

use crate::document::{Corner, Section, SECTIONS};
use plan::{install, plan, Plan};

const CANDIDATE_MS: u64 = 120;

pub enum Msg {
    Candidate {
        token: u64,
        roots: Box<[StageRoots; NUM_STAGES]>,
        rms_db: f64,
    },
    Done {
        token: u64,
        sections: Box<[Section; SECTIONS]>,
        rms_db: f64,
    },
    Failed {
        token: u64,
        reason: String,
    },
}

struct Live {
    token: u64,
    corner: usize,
    rev: u64,
    quiet: Arc<AtomicBool>,
}

#[derive(Default)]
pub struct Fitter {
    rx: Option<Receiver<Msg>>,
    live: Option<Live>,
    next: u64,
}

impl Fitter {
    pub fn running(&self) -> bool {
        self.live.is_some()
    }

    pub fn start(
        &mut self,
        corner_index: usize,
        corner: &Corner,
        grid: &[f64],
        sr: f64,
        rev: u64,
    ) -> Result<(), String> {
        let built = plan(corner, grid, sr).map_err(|e| e.to_string())?;
        self.cancel();
        self.next = self.next.wrapping_add(1);
        let token = self.next;
        let quiet = Arc::new(AtomicBool::new(false));
        let (tx, rx) = channel();
        let sections = corner.sections;
        let flag = quiet.clone();
        std::thread::spawn(move || {
            let Plan {
                target,
                seed,
                freedom,
                writable,
                grow,
                zones,
                fixed,
            } = built;
            let mut last = Instant::now();
            let sent = tx.clone();
            let mut watch = |roots: &[StageRoots; NUM_STAGES], rms: f64| {
                if flag.load(Ordering::Relaxed) {
                    return;
                }
                if last.elapsed() < Duration::from_millis(CANDIDATE_MS) {
                    return;
                }
                last = Instant::now();
                let _ = sent.send(Msg::Candidate {
                    token,
                    roots: Box::new(*roots),
                    rms_db: rms,
                });
            };
            let outcome = fit_arma_planned_watched(
                &target, sr, &seed, &freedom, &writable, &grow, &zones, &[], &mut watch,
            );
            let msg = match outcome {
                Some(fit) => {
                    let mut next = sections;
                    let replan = Plan {
                        target,
                        seed,
                        freedom,
                        writable,
                        grow,
                        zones,
                        fixed,
                    };
                    install(&replan, &fit, &mut next);
                    Msg::Done {
                        token,
                        sections: Box::new(next),
                        rms_db: fit.target_rms_db,
                    }
                }
                None => Msg::Failed {
                    token,
                    reason: "no solution".into(),
                },
            };
            let _ = tx.send(msg);
        });
        self.rx = Some(rx);
        self.live = Some(Live {
            token,
            corner: corner_index,
            rev,
            quiet,
        });
        Ok(())
    }

    pub fn cancel(&mut self) {
        if let Some(live) = self.live.take() {
            live.quiet.store(true, Ordering::Relaxed);
        }
        self.rx = None;
    }

    pub fn poll(&mut self, corner: usize, rev: u64) -> Vec<Msg> {
        let mut out = Vec::new();
        let Some(rx) = &self.rx else { return out };
        let stale = match &self.live {
            Some(l) => l.corner != corner || l.rev != rev,
            None => true,
        };
        while let Ok(msg) = rx.try_recv() {
            let token = match &msg {
                Msg::Candidate { token, .. } => *token,
                Msg::Done { token, .. } => *token,
                Msg::Failed { token, .. } => *token,
            };
            let live_token = self.live.as_ref().map(|l| l.token);
            if Some(token) != live_token || stale {
                continue;
            }
            let terminal = !matches!(msg, Msg::Candidate { .. });
            out.push(msg);
            if terminal {
                self.live = None;
                self.rx = None;
                break;
            }
        }
        if stale {
            self.cancel();
        }
        out
    }
}
