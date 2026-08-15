use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::StageRoots;

#[derive(Clone, Copy)]
struct C {
    re: f64,
    im: f64,
}

fn mul(a: C, b: C) -> C {
    C {
        re: a.re * b.re - a.im * b.im,
        im: a.re * b.im + a.im * b.re,
    }
}

fn div(a: C, b: C) -> C {
    let d = b.re * b.re + b.im * b.im;
    C {
        re: (a.re * b.re + a.im * b.im) / d,
        im: (a.im * b.re - a.re * b.im) / d,
    }
}

fn sub(a: C, b: C) -> C {
    C {
        re: a.re - b.re,
        im: a.im - b.im,
    }
}

fn conj(a: C) -> C {
    C { re: a.re, im: -a.im }
}

const ONE: C = C { re: 1.0, im: 0.0 };

pub struct Mode {
    pub section: usize,
    pub hz: f64,
    p: C,
    a: C,
}

pub fn modes(lanes: &[StageRoots; NUM_STAGES], sr: f64) -> Vec<Mode> {
    let tau = std::f64::consts::TAU;
    let mut poles: Vec<(usize, C, f64)> = Vec::new();
    let mut zeros: Vec<C> = Vec::new();
    let mut gain = 1.0;
    for (si, l) in lanes.iter().enumerate() {
        if l.pole_r > 0.0 {
            let th = tau * l.pole_hz / sr;
            poles.push((
                si,
                C {
                    re: l.pole_r * th.cos(),
                    im: l.pole_r * th.sin(),
                },
                l.pole_hz,
            ));
        }
        if l.zero_r > 0.0 {
            let th = tau * l.zero_hz / sr;
            zeros.push(C {
                re: l.zero_r * th.cos(),
                im: l.zero_r * th.sin(),
            });
        }
        gain *= l.scale;
    }
    poles
        .iter()
        .map(|&(section, p, hz)| {
            let w = div(ONE, p);
            let mut num = C { re: gain, im: 0.0 };
            for &z in &zeros {
                num = mul(num, sub(ONE, mul(z, w)));
                num = mul(num, sub(ONE, mul(conj(z), w)));
            }
            let mut den = ONE;
            for &(_, q, _) in &poles {
                if (q.re - p.re).abs() > 1e-12 || (q.im - p.im).abs() > 1e-12 {
                    den = mul(den, sub(ONE, mul(q, w)));
                }
                den = mul(den, sub(ONE, mul(conj(q), w)));
            }
            Mode {
                section,
                hz,
                p,
                a: div(num, den),
            }
        })
        .collect()
}

impl Mode {
    pub fn db_at(&self, hz: f64, sr: f64) -> f64 {
        let w = std::f64::consts::TAU * hz / sr;
        let e = C {
            re: w.cos(),
            im: -w.sin(),
        };
        let t = div(self.a, sub(ONE, mul(self.p, e)));
        20.0 * (2.0 * t.re).abs().max(1e-9).log10()
    }
}
