pub const OVERSAMPLE: usize = 4;
pub const HZ_LO: f64 = 40.0;
pub const HZ_HI: f64 = 16_000.0;

const EPS: f64 = 1.0e-30;

pub struct ColumnGrid {
    pub columns: usize,
    pub ppp: f32,
    pub width_pt: f32,
    pub sr_hz: f64,
    hz: Vec<f64>,
    cos1: Vec<f64>,
    sin1: Vec<f64>,
    cos2: Vec<f64>,
    sin2: Vec<f64>,
}

fn log_position(hz: f64) -> f64 {
    (hz.clamp(HZ_LO, HZ_HI) / HZ_LO).ln() / (HZ_HI / HZ_LO).ln()
}

impl ColumnGrid {
    pub fn new(width_pt: f32, ppp: f32, sr_hz: f64) -> Self {
        let columns = ((width_pt * ppp).round().max(1.0)) as usize;
        let samples = columns * OVERSAMPLE;
        let ratio = HZ_HI / HZ_LO;
        let sr = sr_hz.max(1.0);
        let mut hz = Vec::with_capacity(samples);
        let mut cos1 = Vec::with_capacity(samples);
        let mut sin1 = Vec::with_capacity(samples);
        let mut cos2 = Vec::with_capacity(samples);
        let mut sin2 = Vec::with_capacity(samples);
        for i in 0..samples {
            let t = (i as f64 + 0.5) / samples as f64;
            let f = HZ_LO * ratio.powf(t);
            let w = std::f64::consts::TAU * f / sr;
            hz.push(f);
            cos1.push(w.cos());
            sin1.push(w.sin());
            cos2.push((2.0 * w).cos());
            sin2.push((2.0 * w).sin());
        }
        Self {
            columns,
            ppp,
            width_pt,
            sr_hz,
            hz,
            cos1,
            sin1,
            cos2,
            sin2,
        }
    }

    pub fn ensure<'a>(
        slot: &'a mut Option<Self>,
        width_pt: f32,
        ppp: f32,
        sr_hz: f64,
    ) -> &'a Self {
        let stale = match slot {
            Some(g) => g.width_pt != width_pt || g.ppp != ppp || g.sr_hz != sr_hz,
            None => true,
        };
        if stale {
            *slot = Some(Self::new(width_pt, ppp, sr_hz));
        }
        slot.as_ref().expect("just built")
    }

    pub fn samples(&self) -> usize {
        self.hz.len()
    }

    pub fn hz_at(&self, i: usize) -> f64 {
        self.hz[i]
    }

    pub fn x_of_column(&self, left_pt: f32, col: usize) -> f32 {
        let px0 = (left_pt * self.ppp).round();
        (px0 + col as f32 + 0.5) / self.ppp
    }

    pub fn x_of_hz(&self, left_pt: f32, hz: f64) -> f32 {
        left_pt + log_position(hz) as f32 * self.width_pt
    }

    pub fn hz_of_x(&self, left_pt: f32, x_pt: f32) -> f64 {
        let t = (((x_pt - left_pt) / self.width_pt) as f64).clamp(0.0, 1.0);
        HZ_LO * (HZ_HI / HZ_LO).powf(t)
    }

    pub fn column_of_hz(&self, hz: f64) -> usize {
        let t = log_position(hz) * self.columns as f64;
        (t as usize).min(self.columns - 1)
    }
}

pub fn peak_hold_into(grid: &ColumnGrid, sample: impl Fn(usize) -> f64, out: &mut Vec<f32>) {
    out.clear();
    out.reserve(grid.columns);
    for col in 0..grid.columns {
        let mut best = f64::NAN;
        for k in 0..OVERSAMPLE {
            let v = sample(col * OVERSAMPLE + k);
            if v.is_nan() {
                continue;
            }
            if best.is_nan() || v.abs() > best.abs() {
                best = v;
            }
        }
        out.push(best as f32);
    }
}

pub fn cascade_db<'a>(
    grid: &'a ColumnGrid,
    biquads: &'a [[f64; 5]],
) -> impl Fn(usize) -> f64 + 'a {
    move |i| {
        let (c1, s1, c2, s2) = (grid.cos1[i], grid.sin1[i], grid.cos2[i], grid.sin2[i]);
        let mut power = 1.0f64;
        for b in biquads {
            let nr = b[0] + b[1] * c1 + b[2] * c2;
            let ni = -b[1] * s1 - b[2] * s2;
            let dr = 1.0 + b[3] * c1 + b[4] * c2;
            let di = -b[3] * s1 - b[4] * s2;
            power *= (nr * nr + ni * ni + EPS) / (dr * dr + di * di + EPS);
        }
        10.0 * power.log10()
    }
}

pub fn stage_db<'a>(grid: &'a ColumnGrid, biquad: &'a [f64; 5]) -> impl Fn(usize) -> f64 + 'a {
    let one = std::slice::from_ref(biquad);
    cascade_db(grid, one)
}

pub fn sampled_db<'a>(grid: &'a ColumnGrid, curve: &'a [f64]) -> impl Fn(usize) -> f64 + 'a {
    move |i| {
        if curve.is_empty() {
            return f64::NAN;
        }
        let span = (curve.len() - 1) as f64;
        let t = log_position(grid.hz[i]) * span;
        let lo = (t.floor() as usize).min(curve.len() - 1);
        let hi = (lo + 1).min(curve.len() - 1);
        let f = t - lo as f64;
        curve[lo] * (1.0 - f) + curve[hi] * f
    }
}

pub fn difference<'a>(
    a: impl Fn(usize) -> f64 + 'a,
    b: impl Fn(usize) -> f64 + 'a,
) -> impl Fn(usize) -> f64 + 'a {
    move |i| a(i) - b(i)
}
