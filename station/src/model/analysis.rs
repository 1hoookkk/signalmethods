//! Response analysis.
//!
//! Two things matter here beyond the total curve.
//!
//! The first is the cumulative response. Sections multiply, so their dB values
//! add, and a safe-looking total can hide an enormous intermediate peak where
//! one stage feeds the next. The Station reports the running product after each
//! lane — the signal so far — alongside each lane on its own.
//!
//! The second is gain. Every gain figure here is a measurement and nothing
//! else. Nothing in this file scales a lane, a frame or a cascade, and nothing
//! anchors a response to unity at DC or anywhere else. Gain anchoring is
//! authored data or a constraint the loaded format declares; inventing one
//! would silently change the filter that was authored.

use trench_core::cascade::NUM_COEFFS;
use trench_core::minifloat::{kernel_to_biquad, stage_words_to_kernel};
use trench_core::response::{biquad_stage_complex, log_frequency_grid};

use super::lane::LaneValue;

pub const GRID_POINTS: usize = 384;
pub const GRID_LO_HZ: f64 = 20.0;
pub const GRID_HI_HZ: f64 = 20_000.0;

pub fn grid() -> Vec<f64> {
    log_frequency_grid(GRID_LO_HZ, GRID_HI_HZ, GRID_POINTS)
}

pub fn biquad_of(lane: &LaneValue) -> [f64; NUM_COEFFS] {
    kernel_to_biquad(stage_words_to_kernel(lane.words))
}

/// Bandwidth and Q of a conjugate pole pair, derived from where the poles sit
/// and nothing else.
///
/// A pole pair at radius `r` and angle `theta` has a resonance whose half-power
/// width follows from the radius: as `r` approaches the unit circle the
/// resonance narrows. `bw_hz` is that width, `q` is centre over width, and
/// `bw_oct` is the same width expressed in octaves.
///
/// These are geometric readings of a pole position. They carry no filter type
/// with them: naming a lane's Q here does not make it a peaking section, and
/// nothing downstream treats it as one.
#[derive(Clone, Copy, Debug)]
pub struct PoleShape {
    pub bw_hz: f64,
    pub bw_oct: f64,
    pub q: f64,
}

pub fn pole_shape(hz: f64, r: f64, sample_rate_hz: f64) -> Option<PoleShape> {
    if !(0.0..1.0).contains(&r) || hz <= 0.0 || sample_rate_hz <= 0.0 {
        return None;
    }
    // Half-power bandwidth of a two-pole resonance, in Hz.
    let bw_hz = -(sample_rate_hz / std::f64::consts::PI) * r.ln();
    if !bw_hz.is_finite() || bw_hz <= 0.0 {
        return None;
    }
    let lo = (hz - bw_hz * 0.5).max(1e-6);
    let hi = hz + bw_hz * 0.5;
    Some(PoleShape {
        bw_hz,
        bw_oct: (hi / lo).log2(),
        q: hz / bw_hz,
    })
}

/// The complete picture of one cascade at one position.
pub struct CascadeResponse {
    pub grid: Vec<f64>,
    /// One curve per lane, that lane alone.
    pub per_lane: Vec<Vec<f64>>,
    /// One curve per lane: the running product through that lane inclusive.
    pub cumulative: Vec<Vec<f64>>,
    /// The complete cascade.
    pub total: Vec<f64>,
    pub lane_metrics: Vec<LaneMetrics>,
}

/// Measurements of one lane. Reported, never enforced.
#[derive(Clone, Copy, Debug)]
pub struct LaneMetrics {
    pub pole_hz: Option<f64>,
    pub pole_r: Option<f64>,
    pub scale: f64,
    /// Largest magnitude of this lane alone, in dB.
    pub peak_db: f64,
    /// Largest magnitude of the cascade up to and including this lane. This is
    /// the number that reveals inter-stage build-up.
    pub cumulative_peak_db: f64,
    /// This lane's magnitude at DC, in dB. A reading, not a target.
    pub dc_db: f64,
    pub is_identity: bool,
}

fn mag_db(c: (f64, f64)) -> f64 {
    let m = (c.0 * c.0 + c.1 * c.1).sqrt();
    20.0 * m.max(1e-12).log10()
}

pub fn analyse(lanes: &[LaneValue], sample_rate_hz: f64) -> CascadeResponse {
    let grid = grid();
    let n = grid.len();
    let mut per_lane = Vec::with_capacity(lanes.len());
    let mut cumulative = Vec::with_capacity(lanes.len());
    let mut running = vec![0.0f64; n];

    for lane in lanes {
        let b = biquad_of(lane);
        let curve: Vec<f64> = grid
            .iter()
            .map(|&f| mag_db(biquad_stage_complex(&b, f, sample_rate_hz)))
            .collect();
        for (r, v) in running.iter_mut().zip(&curve) {
            *r += *v;
        }
        cumulative.push(running.clone());
        per_lane.push(curve);
    }

    let total = running.clone();
    let lane_metrics = lanes
        .iter()
        .enumerate()
        .map(|(i, lane)| {
            let g = lane.geometry(sample_rate_hz);
            let conj = |p: trench_core::stage_law::RootPair| match p {
                trench_core::stage_law::RootPair::Conjugate { hz, r } => (Some(hz), Some(r)),
                _ => (None, None),
            };
            let (pole_hz, pole_r) = conj(g.pole);
            let b = biquad_of(lane);
            LaneMetrics {
                pole_hz,
                pole_r,
                scale: g.scale,
                peak_db: per_lane[i].iter().cloned().fold(f64::MIN, f64::max),
                cumulative_peak_db: cumulative[i].iter().cloned().fold(f64::MIN, f64::max),
                // z = 1 is DC. Measured directly, so the reading does not
                // depend on where the display grid happens to start.
                dc_db: mag_db(biquad_stage_complex(&b, 0.0, sample_rate_hz)),
                is_identity: lane.is_identity(),
            }
        })
        .collect();

    CascadeResponse {
        grid,
        per_lane,
        cumulative,
        total,
        lane_metrics,
    }
}

impl CascadeResponse {
    pub fn total_peak_db(&self) -> f64 {
        self.total.iter().cloned().fold(f64::MIN, f64::max)
    }

    pub fn total_min_db(&self) -> f64 {
        self.total.iter().cloned().fold(f64::MAX, f64::min)
    }

    /// The largest intermediate peak anywhere in the cascade, and the lane it
    /// occurs after. This is the headroom figure: a modest total can sit behind
    /// a very loud middle.
    pub fn worst_intermediate(&self) -> Option<(usize, f64)> {
        self.lane_metrics
            .iter()
            .enumerate()
            .map(|(i, m)| (i, m.cumulative_peak_db))
            .max_by(|a, b| a.1.partial_cmp(&b.1).unwrap_or(std::cmp::Ordering::Equal))
    }

    /// A display span with a little air, never a normalisation.
    pub fn display_span(&self) -> (f64, f64) {
        let hi = self.total_peak_db();
        let lo = self.total_min_db();
        if !hi.is_finite() || !lo.is_finite() {
            return (-60.0, 12.0);
        }
        let lo = lo.max(hi - 96.0);
        ((lo - 3.0).min(hi - 6.0), hi + 3.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use trench_core::stage_law::StageRoots;

    const SR: f64 = 44_100.0;

    #[test]
    fn an_all_identity_cascade_is_flat_at_unity() {
        let lanes = vec![LaneValue::IDENTITY; 7];
        let r = analyse(&lanes, SR);
        for v in &r.total {
            assert!(v.abs() < 1e-9, "identity cascade is not flat: {v}");
        }
        assert!(r.lane_metrics.iter().all(|m| m.is_identity));
    }

    #[test]
    fn cumulative_is_the_running_sum_of_the_lanes_in_db() {
        let mut a = LaneValue::IDENTITY;
        a.set_roots(
            &StageRoots {
                pole_hz: 400.0,
                pole_r: 0.95,
                zero_hz: 900.0,
                zero_r: 0.7,
                scale: 1.0,
            },
            SR,
        )
        .unwrap();
        let mut b = LaneValue::IDENTITY;
        b.set_roots(
            &StageRoots {
                pole_hz: 2000.0,
                pole_r: 0.93,
                zero_hz: 5000.0,
                zero_r: 0.6,
                scale: 1.0,
            },
            SR,
        )
        .unwrap();
        let r = analyse(&[a, b], SR);
        for i in 0..r.grid.len() {
            let want = r.per_lane[0][i] + r.per_lane[1][i];
            assert!(
                (r.cumulative[1][i] - want).abs() < 1e-9,
                "cumulative diverged from the running product at {i}"
            );
        }
        // And the total is the last cumulative curve.
        assert_eq!(r.total, r.cumulative[1]);
    }

    /// The reason cumulative response is shown at all: a stage can be far
    /// louder in the middle of the chain than the finished cascade is.
    #[test]
    fn intermediate_peak_is_reported_even_when_the_total_is_tame() {
        let mut boost = LaneValue::IDENTITY;
        boost
            .set_roots(
                &StageRoots {
                    pole_hz: 1000.0,
                    pole_r: 0.995,
                    zero_hz: 1000.0,
                    zero_r: 0.0,
                    scale: 1.0,
                },
                SR,
            )
            .unwrap();
        let mut cut = LaneValue::IDENTITY;
        cut.set_roots(
            &StageRoots {
                pole_hz: 1000.0,
                pole_r: 0.0,
                zero_hz: 1000.0,
                zero_r: 0.995,
                scale: 1.0,
            },
            SR,
        )
        .unwrap();
        let r = analyse(&[boost, cut], SR);
        let (idx, worst) = r.worst_intermediate().unwrap();
        assert_eq!(idx, 0, "the loud point is after the first lane");
        assert!(
            worst > r.total_peak_db() + 20.0,
            "intermediate {worst:.1} dB should tower over the total {:.1} dB",
            r.total_peak_db()
        );
    }

    /// The reading has to track the geometry it is read from: a pole nearer the
    /// circle is a narrower, higher-Q resonance, and a pole at the origin has
    /// no resonance to measure.
    #[test]
    fn pole_shape_follows_the_pole_radius() {
        let a = pole_shape(1000.0, 0.90, SR).expect("0.90 has a width");
        let b = pole_shape(1000.0, 0.99, SR).expect("0.99 has a width");
        assert!(b.bw_hz < a.bw_hz, "nearer the circle must be narrower");
        assert!(b.q > a.q, "narrower must read as higher Q");
        assert!(b.bw_oct < a.bw_oct);
        // A pole on or outside the circle is not a resonance this can measure.
        assert!(pole_shape(1000.0, 1.0, SR).is_none());
        assert!(pole_shape(1000.0, -0.1, SR).is_none());
        assert!(pole_shape(0.0, 0.9, SR).is_none());
    }

    /// The width is the actual half-power width of the response, not a label.
    #[test]
    fn pole_shape_matches_the_measured_minus_three_db_width() {
        let hz = 1000.0;
        let r = 0.98;
        let mut lane = LaneValue::IDENTITY;
        lane.set_roots(
            &StageRoots {
                pole_hz: hz,
                pole_r: r,
                // Zero at the origin, so the shape measured is the pole's.
                zero_hz: hz,
                zero_r: 0.0,
                scale: 1.0,
            },
            SR,
        )
        .unwrap();
        let shape = pole_shape(hz, r, SR).unwrap();

        let b = biquad_of(&lane);
        let at = |f: f64| mag_db(trench_core::response::biquad_stage_complex(&b, f, SR));
        let peak = at(hz);
        // Walk out to where the response has fallen 3 dB and compare widths.
        let mut lo = hz;
        while lo > 1.0 && at(lo) > peak - 3.0 {
            lo -= 0.5;
        }
        let mut hi = hz;
        while hi < SR * 0.49 && at(hi) > peak - 3.0 {
            hi += 0.5;
        }
        let measured = hi - lo;
        let ratio = measured / shape.bw_hz;
        assert!(
            (0.8..1.25).contains(&ratio),
            "derived width {:.1} Hz vs measured {:.1} Hz",
            shape.bw_hz,
            measured
        );
    }

    /// Nothing here may quietly re-level a cascade.
    #[test]
    fn analysis_never_normalises_the_response() {
        let mut loud = LaneValue::IDENTITY;
        loud.set_roots(
            &StageRoots {
                pole_hz: 800.0,
                pole_r: 0.99,
                zero_hz: 8000.0,
                zero_r: 0.5,
                scale: 3.0,
            },
            SR,
        )
        .unwrap();
        let r = analyse(&[loud], SR);
        // The authored scale of 3 is a real +9.5 dB and must survive as one.
        assert!(
            r.total_peak_db() > 20.0,
            "authored gain was flattened: peak {:.2} dB",
            r.total_peak_db()
        );
        assert!(
            (r.lane_metrics[0].scale - 3.0).abs() < 0.2,
            "authored scale was altered: {}",
            r.lane_metrics[0].scale
        );
    }
}
