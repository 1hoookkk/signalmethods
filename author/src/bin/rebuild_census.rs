use std::sync::Mutex;

use author::envelope;
use author::recipes::{corner_geometry, CORNERS};
use trench_core::arma_endpoint::fit_arma;
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::{RootPair, StageGeometry, StageRoots};

const SR: f64 = 39_062.5;

fn stage_db(g: &StageGeometry, hz: f64) -> f64 {
    let c = g.biquad_at(SR);
    let w = std::f64::consts::TAU * hz / SR;
    let (cw, sw) = (w.cos(), w.sin());
    let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
    let nr = c[0] + c[1] * cw + c[2] * c2;
    let ni = -(c[1] * sw + c[2] * s2);
    let dr = 1.0 + c[3] * cw + c[4] * c2;
    let di = -(c[3] * sw + c[4] * s2);
    10.0 * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30)).log10()
}

fn conjugate_view(g: &StageGeometry) -> Option<StageRoots> {
    let (pole_hz, pole_r) = match g.pole {
        RootPair::Conjugate { hz, r } => (hz, r),
        RootPair::Degenerate => (0.0, 0.0),
        RootPair::RealPair { .. } => return None,
    };
    let (zero_hz, zero_r) = match g.zero {
        RootPair::Conjugate { hz, r } => (hz, r),
        RootPair::Degenerate => (0.0, 0.0),
        RootPair::RealPair { .. } => return None,
    };
    Some(StageRoots {
        pole_hz,
        pole_r,
        zero_hz,
        zero_r,
        scale: g.scale,
    })
}

#[derive(Default, Clone)]
struct Census {
    n: usize,
    null: usize,
    strong: usize,
    soft: usize,
    faint: usize,
    seat_on: usize,
    seat_near: usize,
    seat_mid: usize,
    seat_span: usize,
    tight_pole: usize,
    pole_top: usize,
    pole_low: usize,
}

impl Census {
    fn add(&mut self, s: &StageRoots) {
        if s.pole_r <= 0.0 && s.zero_r <= 0.0 {
            return;
        }
        self.n += 1;
        if s.zero_r >= 0.995 {
            self.null += 1;
        } else if s.zero_r >= 0.9 {
            self.strong += 1;
        } else if s.zero_r >= 0.7 {
            self.soft += 1;
        } else {
            self.faint += 1;
        }
        if s.pole_r >= 0.99 {
            self.tight_pole += 1;
        }
        if s.pole_hz >= 8_000.0 {
            self.pole_top += 1;
        }
        if s.pole_hz > 0.0 && s.pole_hz < 200.0 {
            self.pole_low += 1;
        }
        if s.pole_hz > 0.0 && s.zero_hz > 0.0 {
            let sep = 12.0 * (s.zero_hz / s.pole_hz).log2().abs();
            if sep <= 3.0 {
                self.seat_on += 1;
            } else if sep <= 12.0 {
                self.seat_near += 1;
            } else if sep <= 24.0 {
                self.seat_mid += 1;
            } else {
                self.seat_span += 1;
            }
        }
    }

    fn print(&self, tag: &str) {
        let n = self.n.max(1) as f64;
        let pc = |k: usize| format!("{:.0}%", 100.0 * k as f64 / n);
        println!("== {tag} — {} sections", self.n);
        println!(
            " zero depth: null {} · strong {} · soft {} · faint {}",
            pc(self.null),
            pc(self.strong),
            pc(self.soft),
            pc(self.faint)
        );
        println!(
            " zero seat:  on {} · near {} · mid {} · spanning {}",
            pc(self.seat_on),
            pc(self.seat_near),
            pc(self.seat_mid),
            pc(self.seat_span)
        );
        println!(
            " poles: tight {} · above 8k {} · below 200 {}",
            pc(self.tight_pole),
            pc(self.pole_top),
            pc(self.pole_low)
        );
    }
}

fn corner_stages(doc: &serde_json::Value, corner: &str) -> Option<Vec<StageGeometry>> {
    let sections = doc.get("sections")?.as_array()?;
    sections
        .iter()
        .map(|s| corner_geometry(s, corner))
        .collect()
}

fn main() {
    let root = std::path::Path::new("recipes/architectures");
    let mut files: Vec<_> = std::fs::read_dir(root)
        .expect("recipes/architectures")
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x == "json"))
        .collect();
    files.sort();

    let mut jobs = Vec::new();
    for path in &files {
        let doc: serde_json::Value =
            serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
        let name = doc
            .get("name")
            .and_then(|v| v.as_str())
            .unwrap_or("unnamed")
            .to_string();
        for corner in CORNERS {
            if let Some(stages) = corner_stages(&doc, corner) {
                jobs.push((format!("{name} {corner}"), stages));
            }
        }
    }
    println!("{} corners to rebuild", jobs.len());

    let grid = envelope::grid();
    let hand = Mutex::new(Census::default());
    let machine = Mutex::new(Census::default());
    let rms_all = Mutex::new(Vec::new());
    let next = std::sync::atomic::AtomicUsize::new(0);

    std::thread::scope(|scope| {
        for _ in 0..std::thread::available_parallelism().map(|n| n.get()).unwrap_or(4) {
            scope.spawn(|| loop {
                let i = next.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                let Some((label, stages)) = jobs.get(i) else { break };
                let target: Vec<(f64, f64)> = grid
                    .iter()
                    .map(|&hz| (hz, stages.iter().map(|s| stage_db(s, hz)).sum()))
                    .collect();
                let Some(fit) = fit_arma(&target, SR) else {
                    println!("NO CONVERGENCE {label}");
                    continue;
                };
                {
                    let mut h = hand.lock().unwrap();
                    for s in stages {
                        if let Some(view) = conjugate_view(s) {
                            h.add(&view);
                        }
                    }
                    let mut m = machine.lock().unwrap();
                    for si in 0..NUM_STAGES {
                        m.add(&fit.roots[si]);
                    }
                    rms_all.lock().unwrap().push((fit.target_rms_db, label.clone()));
                }
            });
        }
    });

    let mut rms = rms_all.into_inner().unwrap();
    rms.sort_by(|a, b| a.0.total_cmp(&b.0));
    let med = rms[rms.len() / 2].0;
    let p90 = rms[rms.len() * 9 / 10].0;
    println!(
        "rebuild fidelity over {} corners: median {med:.2} dB · p90 {p90:.2} dB",
        rms.len()
    );
    println!("worst five:");
    for (v, l) in rms.iter().rev().take(5) {
        println!("  {v:.2} dB  {l}");
    }
    println!();
    hand.into_inner().unwrap().print("HAND (factory letters on these curves)");
    machine.into_inner().unwrap().print("FITTER (cold refit of the same curves)");
}
