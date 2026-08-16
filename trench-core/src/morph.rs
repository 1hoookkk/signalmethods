use crate::minifloat::lerp_u16;
use crate::stage_law::{geometry_from_words_at, words_from_geometry_at, StageGeometry};

pub fn section_at(
    corners: &[StageGeometry],
    coords: &[f32],
    sample_rate_hz: f64,
) -> StageGeometry {
    debug_assert_eq!(corners.len(), 1usize << coords.len());
    let encoded: Vec<[u16; 5]> = corners
        .iter()
        .map(|g| words_from_geometry_at(g, sample_rate_hz))
        .collect();
    let mut out = [0u16; 5];
    for (wi, word) in out.iter_mut().enumerate() {
        let mut level: Vec<u16> = encoded.iter().map(|w| w[wi]).collect();
        for &t in coords {
            level = level
                .chunks_exact(2)
                .map(|pair| lerp_u16(pair[0], pair[1], t))
                .collect();
        }
        *word = level[0];
    }
    geometry_from_words_at(out, sample_rate_hz)
}

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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::minifloat::PackedCorners;
    use crate::stage_law::DEFAULT_AUTHORING_SR;

    fn fixture() -> PackedCorners {
        let raw = std::fs::read("../ref/presets/P2k_013_talking_hedz.bin")
            .expect("factory preset is present");
        PackedCorners::from_body_bytes(&raw).expect("factory preset decodes")
    }

    #[test]
    fn geometry_is_an_exact_fixed_point_of_the_encoder_on_every_factory_preset() {
        let mut stages = 0usize;
        for entry in std::fs::read_dir("../ref/presets").expect("preset directory") {
            let path = entry.unwrap().path();
            let raw = std::fs::read(&path).unwrap();
            if raw.len() != crate::minifloat::LEGACY_BODY_BYTES {
                continue;
            }
            let packed = PackedCorners::from_body_bytes(&raw).unwrap();
            for ci in 0..crate::minifloat::LEGACY_CORNERS {
                for si in 0..crate::minifloat::LEGACY_STAGES {
                    let w = packed.words[ci][si];
                    for sr in [DEFAULT_AUTHORING_SR, 44_100.0, 48_000.0] {
                        let back =
                            words_from_geometry_at(&geometry_from_words_at(w, sr), sr);
                        assert_eq!(
                            back,
                            w,
                            "{} corner {ci} section {si} at {sr} Hz did not survive geometry",
                            path.display()
                        );
                    }
                    stages += 1;
                }
            }
        }
        assert!(stages > 700, "only {stages} stages were checked");
    }

    #[test]
    fn three_axes_match_the_runtime_word_for_word() {
        let packed = fixture();
        let sr = DEFAULT_AUTHORING_SR;
        for &(m, q, z) in &[
            (0.0f32, 0.0f32, 0.0f32),
            (1.0, 1.0, 1.0),
            (0.5, 0.5, 0.5),
            (0.25, 0.75, 0.125),
            (0.3333, 0.6667, 0.9),
            (0.07, 0.93, 0.42),
        ] {
            let want = packed.interpolate_words(m, q, z);
            for si in 0..crate::cascade::NUM_STAGES {
                let corners: Vec<StageGeometry> = (0..8)
                    .map(|ci| geometry_from_words_at(packed.words[ci][si], sr))
                    .collect();
                let got = section_at(&corners, &[m, q, z], sr);
                assert_eq!(
                    words_from_geometry_at(&got, sr),
                    want[si],
                    "section {si} at ({m},{q},{z}) diverged from the runtime"
                );
            }
        }
    }

    #[test]
    fn every_corner_is_returned_exactly_at_its_own_address() {
        let packed = fixture();
        let sr = DEFAULT_AUTHORING_SR;
        for si in 0..crate::cascade::NUM_STAGES {
            let corners: Vec<StageGeometry> = (0..8)
                .map(|ci| geometry_from_words_at(packed.words[ci][si], sr))
                .collect();
            for ci in 0..8 {
                let coords = [
                    (ci & 1) as f32,
                    ((ci >> 1) & 1) as f32,
                    ((ci >> 2) & 1) as f32,
                ];
                assert_eq!(
                    words_from_geometry_at(&section_at(&corners, &coords, sr), sr),
                    packed.words[ci][si],
                    "corner {ci} section {si}"
                );
            }
        }
    }

    #[test]
    fn corner_weights_sum_to_one_and_pick_out_corners() {
        for coords in [
            vec![0.0f32, 0.0, 0.0],
            vec![1.0, 1.0, 1.0],
            vec![0.5, 0.5, 0.5],
            vec![0.2, 0.9, 0.35],
        ] {
            let w = corner_weights(&coords);
            assert_eq!(w.len(), 1 << coords.len());
            let sum: f32 = w.iter().sum();
            assert!((sum - 1.0).abs() < 1e-5, "weights summed to {sum}");
            assert!(w.iter().all(|x| *x >= -1e-6));
        }
        let w = corner_weights(&[1.0, 0.0, 1.0]);
        assert!((w[0b101] - 1.0).abs() < 1e-6, "{w:?}");
    }
}
