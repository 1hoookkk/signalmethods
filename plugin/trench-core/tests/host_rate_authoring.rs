use trench_core::minifloat::PackedCorners;
use trench_core::praat_endpoint::pole_from_frequency_bandwidth;
use trench_core::stage_law::{roots_from_words_at, words_from_roots_at, StageRoots};

const HOST_RATES: [f64; 6] = [44_100.0, 48_000.0, 88_200.0, 96_000.0, 176_400.0, 192_000.0];

#[test]
fn praat_frequency_and_bandwidth_are_authored_at_each_actual_rate() {
    const FREQUENCY_HZ: f64 = 2_137.0;
    const BANDWIDTH_HZ: f64 = 173.0;
    let mut banks = Vec::new();

    for rate in HOST_RATES {
        let (theta, radius) =
            pole_from_frequency_bandwidth(FREQUENCY_HZ, BANDWIDTH_HZ, rate).unwrap();
        assert!((theta - core::f64::consts::TAU * FREQUENCY_HZ / rate).abs() < 1.0e-15);
        assert!((radius - (-core::f64::consts::PI * BANDWIDTH_HZ / rate).exp()).abs() < 1.0e-15);

        let roots = StageRoots {
            pole_hz: FREQUENCY_HZ,
            pole_r: radius,
            zero_hz: 3_411.0,
            zero_r: (-core::f64::consts::PI * 260.0 / rate).exp(),
            scale: 0.72,
        };
        let words = words_from_roots_at(&roots, rate);
        let decoded = roots_from_words_at(words, rate).expect("host-authored conjugate row");
        assert!(
            (decoded.pole_hz - FREQUENCY_HZ).abs() < 5.0,
            "{rate} Hz authoring decoded pole to {} Hz",
            decoded.pole_hz
        );
        let decoded_bandwidth = -decoded.pole_r.ln() * rate / core::f64::consts::PI;
        assert!(
            (decoded_bandwidth - BANDWIDTH_HZ).abs() < 5.0,
            "{rate} Hz authoring decoded bandwidth to {decoded_bandwidth} Hz"
        );
        banks.push(words);
    }

    assert!(
        banks.windows(2).all(|pair| pair[0] != pair[1]),
        "different runtime rates must be authored as different packed banks"
    );
}

#[test]
fn loading_and_probing_never_reinterpret_body_words() {
    let roots = StageRoots {
        pole_hz: 1_200.0,
        pole_r: 0.96,
        zero_hz: 900.0,
        zero_r: 0.82,
        scale: 0.8,
    };
    let row = words_from_roots_at(&roots, 48_000.0);
    let identity = words_from_roots_at(&StageRoots::IDENTITY, 48_000.0);
    let packed = PackedCorners::from_legacy_words(&[[
        row, identity, identity, identity, identity, identity,
    ]; 4]);
    let body = packed.to_rom_bytes();
    let mut reference = [0.0f64; 30];

    for (index, rate) in HOST_RATES.into_iter().enumerate() {
        let mut biquads = [0.0f64; 30];
        let mut max_radius = 0.0;
        let mut unstable = 0u32;
        let mut nonfinite = 0u32;
        let rc = unsafe {
            trench_core::ffi::trench_packed_probe_at(
                body.as_ptr(),
                body.len(),
                0.0,
                0.0,
                rate,
                biquads.as_mut_ptr(),
                &mut max_radius,
                &mut unstable,
                &mut nonfinite,
            )
        };
        assert_eq!(rc, 0);
        assert_eq!(unstable, 0);
        assert_eq!(nonfinite, 0);
        if index == 0 {
            reference = biquads;
        } else {
            assert_eq!(
                biquads, reference,
                "the loader/probe must consume stored words verbatim at {rate} Hz"
            );
        }
    }
}
