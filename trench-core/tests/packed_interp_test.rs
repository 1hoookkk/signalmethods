mod tests {
    use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
    use trench_core::minifloat::{decode, encode, lerp_u16, LegacyCornerData, PackedCorners};

    fn synthetic_corners() -> [LegacyCornerData; 4] {
        let m0q0: LegacyCornerData = [
            [0.5625, 0.125, 0.25, 0.0625, 0.25],
            [0.4375, 0.0625, 0.125, 0.03125, 0.1875],
            [0.625, 0.0, 0.5, 0.125, 0.5],
            [0.5, 0.0625, 0.375, 0.0625, 0.375],
            [0.6875, 0.125, 0.625, 0.0625, 0.4375],
            [0.75, 0.0, 0.5, 0.0, 0.5],
        ];
        let m100q0: LegacyCornerData = [
            [0.75, 0.25, 0.5, 0.125, 0.5],
            [0.625, 0.125, 0.375, 0.0625, 0.375],
            [0.875, 0.0625, 0.625, 0.0625, 0.625],
            [0.625, 0.125, 0.5, 0.125, 0.5],
            [0.9375, 0.0625, 0.75, 0.0625, 0.625],
            [0.875, 0.0625, 0.625, 0.0625, 0.625],
        ];
        let m0q100: LegacyCornerData = [
            [0.4375, 0.0625, 0.125, 0.0, 0.125],
            [0.375, 0.0, 0.0625, 0.0, 0.0625],
            [0.5, 0.0, 0.25, 0.0625, 0.25],
            [0.4375, 0.0625, 0.25, 0.0, 0.25],
            [0.5625, 0.0625, 0.375, 0.0625, 0.25],
            [0.5625, 0.0, 0.375, 0.0, 0.375],
        ];
        let m100q100: LegacyCornerData = [
            [0.625, 0.125, 0.375, 0.0625, 0.375],
            [0.5, 0.0625, 0.25, 0.0, 0.25],
            [0.75, 0.0625, 0.5, 0.0625, 0.5],
            [0.5625, 0.0625, 0.375, 0.0625, 0.375],
            [0.8125, 0.0, 0.625, 0.0625, 0.4375],
            [0.75, 0.0625, 0.5, 0.0625, 0.5],
        ];
        [m0q0, m100q0, m0q100, m100q100]
    }

    fn decoded_float_bilinear(
        corners: &[LegacyCornerData; 4],
        morph: f64,
        q: f64,
    ) -> LegacyCornerData {
        let mut result = [[0.0f64; NUM_COEFFS]; trench_core::minifloat::LEGACY_STAGES];
        for si in 0..trench_core::minifloat::LEGACY_STAGES {
            for ki in 0..NUM_COEFFS {
                let q_m0 = corners[0][si][ki] + (corners[2][si][ki] - corners[0][si][ki]) * q;
                let q_m1 = corners[1][si][ki] + (corners[3][si][ki] - corners[1][si][ki]) * q;
                result[si][ki] = q_m0 + (q_m1 - q_m0) * morph;
            }
        }
        result
    }

    #[test]
    fn lerp_u16_known_scalar_values() {
        assert_eq!(lerp_u16(0x1000, 0x3000, 0.5), 0x2000);

        assert_eq!(lerp_u16(0x0000, 0x0000, 0.75), 0x0000);

        assert_eq!(lerp_u16(0x0000, 0xFFFF, 1.0), 0xFFFF);

        assert_eq!(lerp_u16(0x8000, 0x8000, 0.333), 0x8000);

        assert_eq!(lerp_u16(0x4000, 0xC000, 0.25), 0x6000);
    }

    #[test]
    fn decode_boundary_and_denormal() {
        assert_eq!(decode(0x0000), 0.0);
        assert_eq!(decode(0xFFFF), 1.0);

        let expected = 2.0 / 4096.0 * (2.0f64).powi(-15);
        let got = decode(0x0001);
        assert!(
            (got - expected).abs() < 1e-18,
            "decode(0x0001)={got} want {expected}"
        );

        let expected = 0.5 * (2.0f64).powi(-14);
        let got = decode(0x0FFF);
        assert!(
            (got - expected).abs() < 1e-18,
            "decode(0x0FFF)={got} want {expected}"
        );
    }

    #[test]
    fn encode_decode_roundtrip() {
        for w in [
            0x0000u16, 0x0001, 0x00FF, 0x0FFF, 0x1000, 0x4000, 0x7FFF, 0x8000, 0xBFFF, 0xC000,
            0xFFFE, 0xFFFF,
        ] {
            let v = decode(w);
            let w2 = encode(v);
            assert_eq!(
                w, w2,
                "roundtrip failed for word {w:#06x}: v={v}, re-encoded={w2:#06x}"
            );
        }
    }

    #[test]
    fn packed_differs_from_decoded_float_at_midpoint() {
        let corners = synthetic_corners();
        let packed = PackedCorners::from_legacy_corner_data(&corners);

        let p_interp = packed.interpolate(0.5, 0.5, 0.0);
        let f_interp = decoded_float_bilinear(&corners, 0.5, 0.5);

        let max_diff = (0..trench_core::minifloat::LEGACY_STAGES)
            .flat_map(|si| (0..NUM_COEFFS).map(move |ki| (si, ki)))
            .map(|(si, ki)| (p_interp[si][ki] - f_interp[si][ki]).abs())
            .fold(0.0f64, f64::max);

        println!("packed vs decoded-float midpoint max |Δ| = {max_diff:.6e}");
        assert!(
            max_diff > 1e-5,
            "packed and decoded-float paths gave same result at midpoint; \
             quantisation nonlinearity should cause visible divergence. max_diff={max_diff}"
        );
    }

    #[test]
    fn packed_interp_diagonal_points() {
        let corners = synthetic_corners();
        let packed = PackedCorners::from_legacy_corner_data(&corners);

        let test_points: &[(f32, f32, &str)] = &[
            (0.25, 0.25, "diag_25"),
            (0.5, 0.5, "diag_50"),
            (0.75, 0.75, "diag_75"),
            (0.21875, 0.21875, "offset_near_25"),
            (0.46875, 0.46875, "offset_near_50"),
            (0.71875, 0.71875, "offset_near_75"),
        ];

        for &(morph, q, label) in test_points {
            let result = packed.interpolate(morph, q, 0.0);
            for si in 0..NUM_STAGES {
                for ki in 0..NUM_COEFFS {
                    assert!(
                        result[si][ki].is_finite(),
                        "{label} stage {si} coeff {ki} is non-finite"
                    );
                    assert!(
                        result[si][ki] >= 0.0,
                        "{label} stage {si} coeff {ki} is negative: {}",
                        result[si][ki]
                    );
                }
            }
            println!(
                "{label} morph={morph:.5} q={q:.5}: stage0 [c0={:.6} c1={:.6} c2={:.6} c3={:.6} c4={:.6}]",
                result[0][0], result[0][1], result[0][2], result[0][3], result[0][4]
            );
        }
    }

    #[test]
    fn packed_recovers_corners_within_quantisation() {
        let corners = synthetic_corners();
        let packed = PackedCorners::from_legacy_corner_data(&corners);

        let corner_params: [(f32, f32, usize); 4] = [
            (0.0, 0.0, 0),
            (1.0, 0.0, 1),
            (0.0, 1.0, 2),
            (1.0, 1.0, 3),
        ];

        for (morph, q, ci) in corner_params {
            let result = packed.interpolate(morph, q, 0.0);
            for si in 0..trench_core::minifloat::LEGACY_STAGES {
                for ki in 0..NUM_COEFFS {
                    let got = result[si][ki];
                    let want = corners[ci][si][ki];
                    let tol = 1e-3;
                    assert!(
                        (got - want).abs() < tol,
                        "corner {ci} stage {si} coeff {ki}: want {want:.6}, got {got:.6}, diff {:.2e}",
                        (got - want).abs()
                    );
                }
            }
        }
    }

    #[test]
    fn morph_first_order_matters() {
        let corners = synthetic_corners();
        let packed = PackedCorners::from_legacy_corner_data(&corners);

        let morph_first = packed.interpolate(0.25, 0.75, 0.0);
        let q_first = packed.interpolate(0.75, 0.25, 0.0);

        let any_diff = (0..NUM_STAGES)
            .flat_map(|si| (0..NUM_COEFFS).map(move |ki| (si, ki)))
            .any(|(si, ki)| (morph_first[si][ki] - q_first[si][ki]).abs() > 1e-9);

        assert!(
            any_diff,
            "morph-first (0.25,0.75) and swapped (0.75,0.25) gave same result; corners may be symmetric"
        );
    }
}
