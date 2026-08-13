//! Interpolation across declared axes, in encoded word space.
//!
//! Two rules decide everything here.
//!
//! The first is the domain. Interpolation happens on the stored words, never on
//! decoded biquad coefficients. Blending `b1`/`b2` directly drags pole
//! trajectories inward toward the origin and drops the middle of a sweep; the
//! stored words are the log-polar radius/angle pair, so moving along them keeps
//! the sweep on its arc.
//!
//! The second is the shape. Multilinear interpolation is a tensor product, so
//! it reduces one axis at a time and the axis order does not change the result
//! beyond word rounding. Reducing axis 0 first, then axis 1, and so on
//! reproduces the runtime's morph-then-Q-then-third-axis path exactly at three
//! axes, and extends to any axis count without a second rule.

use trench_core::cascade::NUM_COEFFS;
use trench_core::minifloat::lerp_u16;

use super::lane::LaneValue;

/// Interpolates one lane across every declared axis.
///
/// `corners` is indexed by corner address, axis 0 varying fastest, and must
/// hold exactly `2^coords.len()` entries. `coords` is one position per axis.
pub fn lane_at(corners: &[LaneValue], coords: &[f32]) -> LaneValue {
    debug_assert_eq!(corners.len(), 1usize << coords.len());
    let mut words = [0u16; NUM_COEFFS];
    for (wi, word) in words.iter_mut().enumerate() {
        // Reduce one axis per pass. Axis 0 is bit 0, so the pair that differs
        // only in the current axis is always adjacent.
        let mut level: Vec<u16> = corners.iter().map(|c| c.words[wi]).collect();
        for &t in coords {
            level = level
                .chunks_exact(2)
                .map(|pair| lerp_u16(pair[0], pair[1], t))
                .collect();
        }
        *word = level[0];
    }
    LaneValue::from_words(words)
}

/// How much each corner contributes at a position.
///
/// These are the multilinear weights the interpolation itself uses, so the
/// blend an operator reads is the blend the filter is made of, not a separate
/// estimate of it. Weight of a corner is the product over axes of the distance
/// to the opposite face, and they sum to one.
pub fn corner_weights(coords: &[f32]) -> Vec<f32> {
    let n = 1usize << coords.len();
    (0..n)
        .map(|ci| {
            coords
                .iter()
                .enumerate()
                .map(|(a, &t)| if (ci >> a) & 1 == 1 { t } else { 1.0 - t })
                .product()
        })
        .collect()
}

/// Interpolates every lane of a frame set at one position.
pub fn cascade_at(corner_lanes: &[Vec<LaneValue>], coords: &[f32]) -> Vec<LaneValue> {
    if corner_lanes.is_empty() {
        return Vec::new();
    }
    let lane_count = corner_lanes[0].len();
    (0..lane_count)
        .map(|li| {
            let column: Vec<LaneValue> = corner_lanes.iter().map(|c| c[li]).collect();
            lane_at(&column, coords)
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use trench_core::minifloat::PackedCorners;

    fn packed_fixture() -> PackedCorners {
        // A real factory body, so the comparison runs on shipped words rather
        // than on values chosen to make the test pass.
        let raw = std::fs::read("../ref/presets/P2k_013_talking_hedz.bin")
            .expect("factory preset is present");
        PackedCorners::from_body_bytes(&raw).expect("factory preset decodes")
    }

    /// The generalised N-axis path must reproduce the runtime's trilinear
    /// interpolation bit for bit, or the Station is not showing the runtime.
    #[test]
    fn three_axes_match_the_runtime_word_for_word() {
        let packed = packed_fixture();
        let corners: Vec<Vec<LaneValue>> = (0..8)
            .map(|ci| {
                (0..trench_core::cascade::NUM_STAGES)
                    .map(|si| LaneValue::from_words(packed.words[ci][si]))
                    .collect()
            })
            .collect();

        for &(m, q, z) in &[
            (0.0f32, 0.0f32, 0.0f32),
            (1.0, 1.0, 1.0),
            (0.5, 0.5, 0.5),
            (0.25, 0.75, 0.125),
            (0.3333, 0.6667, 0.9),
            (0.07, 0.93, 0.42),
        ] {
            let want = packed.interpolate_words(m, q, z);
            let got = cascade_at(&corners, &[m, q, z]);
            for si in 0..trench_core::cascade::NUM_STAGES {
                assert_eq!(
                    got[si].words, want[si],
                    "stage {si} at ({m},{q},{z}) diverged from the runtime"
                );
            }
        }
    }

    #[test]
    fn every_corner_is_returned_exactly_at_its_own_address() {
        let packed = packed_fixture();
        let corners: Vec<Vec<LaneValue>> = (0..8)
            .map(|ci| {
                (0..trench_core::cascade::NUM_STAGES)
                    .map(|si| LaneValue::from_words(packed.words[ci][si]))
                    .collect()
            })
            .collect();
        for ci in 0..8 {
            let coords = [
                (ci & 1) as f32,
                ((ci >> 1) & 1) as f32,
                ((ci >> 2) & 1) as f32,
            ];
            let got = cascade_at(&corners, &coords);
            for (si, lane) in got.iter().enumerate() {
                assert_eq!(lane.words, packed.words[ci][si], "corner {ci} stage {si}");
            }
        }
    }

    /// Four axes are not a special case of three; they are the same reduction
    /// run one more time.
    #[test]
    fn four_axes_recover_all_sixteen_corners() {
        let corners: Vec<LaneValue> = (0..16)
            .map(|i| LaneValue::from_words([i as u16 * 1000; NUM_COEFFS]))
            .collect();
        for i in 0..16 {
            let coords = [
                (i & 1) as f32,
                ((i >> 1) & 1) as f32,
                ((i >> 2) & 1) as f32,
                ((i >> 3) & 1) as f32,
            ];
            assert_eq!(lane_at(&corners, &coords).words[0], i as u16 * 1000);
        }
    }

    #[test]
    fn corner_weights_sum_to_one_and_pick_out_corners() {
        for coords in [
            vec![0.0f32, 0.0, 0.0],
            vec![1.0, 1.0, 1.0],
            vec![0.5, 0.5, 0.5],
            vec![0.2, 0.9, 0.35],
            vec![0.3, 0.7, 0.1, 0.6],
        ] {
            let w = corner_weights(&coords);
            assert_eq!(w.len(), 1 << coords.len());
            let sum: f32 = w.iter().sum();
            assert!((sum - 1.0).abs() < 1e-5, "weights summed to {sum}");
            assert!(w.iter().all(|x| *x >= -1e-6));
        }
        // At a corner, that corner carries everything.
        let w = corner_weights(&[1.0, 0.0, 1.0]);
        assert!((w[0b101] - 1.0).abs() < 1e-6, "{w:?}");
    }

    /// The blend an operator reads has to be the blend the filter is made of.
    #[test]
    fn corner_weights_reconstruct_the_interpolated_lane() {
        let packed = packed_fixture();
        let corners: Vec<LaneValue> = (0..8)
            .map(|ci| LaneValue::from_words(packed.words[ci][0]))
            .collect();
        let coords = [0.37f32, 0.62, 0.18];
        let w = corner_weights(&coords);
        // Weighted sum of the corner words, in the same word domain the
        // interpolation works in.
        let want: f64 = (0..8)
            .map(|ci| w[ci] as f64 * corners[ci].words[0] as f64)
            .sum();
        let got = lane_at(&corners, &coords).words[0] as f64;
        assert!(
            (got - want).abs() < 4.0,
            "weights disagree with the interpolation: {got} vs {want}"
        );
    }

    #[test]
    fn a_zero_axis_object_is_its_single_corner() {
        let only = LaneValue::from_words([7; NUM_COEFFS]);
        assert_eq!(lane_at(&[only], &[]).words, [7; NUM_COEFFS]);
    }
}
