use std::f64::consts::{PI, TAU};

pub const RIM_DB: f64 = 96.0;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ArmaError {
    NotFinite,
    NonPositiveAngle,
    RadiusOnOrOutsideUnitCircle,
}

pub fn display_lo_hz(fs: f64) -> f64 {
    fs / 2048.0
}
pub fn display_hi_hz(fs: f64) -> f64 {
    fs * 0.5
}
pub fn theta_from_hz(hz: f64, fs: f64) -> f64 {
    TAU * (hz / fs)
}
pub fn hz_from_theta(theta: f64, fs: f64) -> f64 {
    fs * (theta / TAU)
}

pub fn theta_prime_from_theta(theta: f64) -> Result<f64, ArmaError> {
    if !theta.is_finite() {
        return Err(ArmaError::NotFinite);
    }
    if theta <= 0.0 {
        return Err(ArmaError::NonPositiveAngle);
    }
    Ok(PI * ((10.0 + (theta / PI).log2()) / 10.0))
}

pub fn theta_prime_from_hz(hz: f64, fs: f64) -> Result<f64, ArmaError> {
    if !hz.is_finite() || !fs.is_finite() || fs <= 0.0 {
        return Err(ArmaError::NotFinite);
    }
    theta_prime_from_theta(theta_from_hz(hz, fs))
}

pub fn theta_from_prime(theta_prime: f64) -> Result<f64, ArmaError> {
    if !theta_prime.is_finite() {
        return Err(ArmaError::NotFinite);
    }
    Ok(PI * (10.0 * (theta_prime / PI) - 10.0).exp2())
}

pub fn hz_from_prime(theta_prime: f64, fs: f64) -> Result<f64, ArmaError> {
    if !theta_prime.is_finite() || !fs.is_finite() || fs <= 0.0 {
        return Err(ArmaError::NotFinite);
    }
    Ok((fs * 0.5) * (10.0 * (theta_prime / PI) - 10.0).exp2())
}

pub fn on_plot(theta_prime: f64) -> bool {
    theta_prime >= 0.0 && theta_prime <= PI
}

pub fn r_prime_db_from_radius(r: f64) -> Result<f64, ArmaError> {
    if !r.is_finite() {
        return Err(ArmaError::NotFinite);
    }
    let gap = 1.0 - r;
    if gap <= 0.0 {
        return Err(ArmaError::RadiusOnOrOutsideUnitCircle);
    }
    Ok(-20.0 * gap.log10())
}

pub fn radius_from_r_prime_db(db: f64) -> Result<f64, ArmaError> {
    if !db.is_finite() {
        return Err(ArmaError::NotFinite);
    }
    Ok(1.0 - 10.0f64.powf(-db / 20.0))
}

pub fn display_radius_from_r_prime_db(db: f64, rho_max: f64) -> f64 {
    rho_max * (db / RIM_DB)
}

pub fn r_prime_db_from_display_radius(rho: f64, rho_max: f64) -> f64 {
    RIM_DB * (rho / rho_max)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::stage_law::authoring_limits_at;

    const RATES: [f64; 3] = [44_100.0, 48_000.0, 96_000.0];

    fn close(got: f64, want: f64, tol: f64, what: &str) {
        assert!(
            (got - want).abs() <= tol,
            "{what}: got {got:.17} want {want:.17} (delta {:.3e} > {tol:.3e})",
            (got - want).abs()
        );
    }
    fn close_rel(got: f64, want: f64, rel: f64, what: &str) {
        assert!(
            (got - want).abs() <= rel * want.abs(),
            "{what}: got {got:.17} want {want:.17} (rel {:.3e} > {rel:.3e})",
            (got - want).abs() / want.abs()
        );
    }

    #[test]
    fn anchors_at_both_ends_of_the_domain() {
        for fs in RATES {
            let lo = display_lo_hz(fs);
            close_rel(
                theta_from_hz(lo, fs),
                PI / 1024.0,
                1e-15,
                "fs/2048 -> pi/1024",
            );
            close(
                theta_prime_from_hz(lo, fs).unwrap(),
                0.0,
                1e-12,
                "fs/2048 -> theta' = 0",
            );

            let hi = display_hi_hz(fs);
            close_rel(theta_from_hz(hi, fs), PI, 1e-15, "fs/2 -> theta = pi");
            close(
                theta_prime_from_hz(hi, fs).unwrap(),
                PI,
                1e-12,
                "fs/2 -> theta' = pi",
            );
        }
    }

    #[test]
    fn a_doubling_advances_theta_prime_by_exactly_pi_over_ten() {
        let step = PI / 10.0;
        for fs in RATES {
            let mut f = display_lo_hz(fs);
            while f * 2.0 <= display_hi_hz(fs) * 1.000_000_1 {
                let a = theta_prime_from_hz(f, fs).unwrap();
                let b = theta_prime_from_hz(f * 2.0, fs).unwrap();
                close(b - a, step, 1e-12, "doubling advances theta' by pi/10");
                f *= 1.37;
            }
        }
    }

    #[test]
    fn frequency_round_trips_across_the_whole_domain() {
        for fs in RATES {
            let (lo, hi) = (display_lo_hz(fs), display_hi_hz(fs));
            let n = 401;
            for i in 0..n {
                let f = lo * (hi / lo).powf(i as f64 / (n - 1) as f64);
                let tp = theta_prime_from_hz(f, fs).unwrap();
                close_rel(hz_from_prime(tp, fs).unwrap(), f, 1e-9, "f -> theta' -> f");
                assert!(
                    tp >= -1e-12 && tp <= PI + 1e-12,
                    "theta' stays inside the semicircle: {tp}"
                );
            }
        }
    }

    #[test]
    fn the_radial_law_round_trips_and_is_strictly_monotone() {
        close(
            r_prime_db_from_radius(0.0).unwrap(),
            0.0,
            1e-15,
            "r = 0 -> R' = 0 dB",
        );

        let mut prev = -1.0f64;
        for i in 0..=400 {
            let r = i as f64 / 401.0;
            let d = r_prime_db_from_radius(r).unwrap();
            close(radius_from_r_prime_db(d).unwrap(), r, 1e-12, "R -> R' -> R");
            assert!(d > prev, "R' is strictly monotonic in R at r = {r}");
            prev = d;
        }

        let mut d = 0.0f64;
        while d <= 120.0 {
            let r = radius_from_r_prime_db(d).unwrap();
            close(r_prime_db_from_radius(r).unwrap(), d, 1e-9, "R' -> R -> R'");
            d += 3.0;
        }
    }

    #[test]
    fn the_radius_precision_ceiling_is_where_we_think_it_is() {
        let r = radius_from_r_prime_db(133.0).unwrap();
        close(
            r_prime_db_from_radius(r).unwrap(),
            133.0,
            1e-9,
            "R' exact to 133 dB",
        );

        let r = radius_from_r_prime_db(196.0).unwrap();
        let back = r_prime_db_from_radius(r).unwrap();
        assert!(
            (back - 196.0).abs() > 1e-9,
            "past ~140 dB the gap has lost its low bits"
        );
        close(
            back,
            196.0,
            1e-5,
            "and degrades gracefully, not catastrophically",
        );
    }

    #[test]
    fn equal_db_increments_are_equal_display_increments() {
        let rho_max = 480.0;
        let first = display_radius_from_r_prime_db(12.0, rho_max)
            - display_radius_from_r_prime_db(0.0, rho_max);
        let mut d = 0.0f64;
        while d + 12.0 <= RIM_DB {
            let gap = display_radius_from_r_prime_db(d + 12.0, rho_max)
                - display_radius_from_r_prime_db(d, rho_max);
            close(gap, first, 1e-12, "equal dB steps are equal display steps");
            d += 12.0;
        }
        let mut d = 0.0f64;
        while d <= RIM_DB {
            let rho = display_radius_from_r_prime_db(d, rho_max);
            close(
                r_prime_db_from_display_radius(rho, rho_max),
                d,
                1e-12,
                "display round-trip",
            );
            d += 6.0;
        }
    }

    #[test]
    fn the_authoring_ceiling_fits_inside_the_arc() {
        for fs in RATES {
            let lim = authoring_limits_at(fs);
            let tp = theta_prime_from_hz(lim.authoring_freq_max_hz, fs).unwrap();
            assert!(
                tp < PI,
                "the authoring ceiling is strictly inside the semicircle"
            );
            assert!(tp > 0.0, "the authoring ceiling is above the domain floor");
            close(
                lim.display_freq_min_hz,
                display_lo_hz(fs),
                0.0,
                "stage_law's floor IS this module's floor",
            );
            close(
                lim.display_freq_max_hz,
                display_hi_hz(fs),
                0.0,
                "stage_law's ceiling IS this module's ceiling",
            );
        }
    }

    #[test]
    fn the_domain_is_ten_octaves_and_tracks_fs() {
        for w in RATES.windows(2) {
            assert!(
                display_lo_hz(w[1]) > display_lo_hz(w[0]),
                "floor rises with fs"
            );
            assert!(
                display_hi_hz(w[1]) > display_hi_hz(w[0]),
                "ceiling rises with fs"
            );
        }
        close_rel(
            display_lo_hz(48_000.0),
            23.4375,
            1e-15,
            "48k floor is 23.4375 Hz",
        );
        close_rel(
            display_hi_hz(48_000.0),
            24_000.0,
            1e-15,
            "48k ceiling is 24000 Hz",
        );
        for fs in RATES {
            close_rel(
                display_hi_hz(fs) / display_lo_hz(fs),
                1024.0,
                1e-12,
                "ten octaves",
            );
        }
    }

    #[test]
    fn bad_input_is_reported_never_substituted() {
        let nan = f64::NAN;
        let inf = f64::INFINITY;

        assert_eq!(theta_prime_from_theta(nan), Err(ArmaError::NotFinite));
        assert_eq!(theta_prime_from_theta(inf), Err(ArmaError::NotFinite));
        assert_eq!(
            theta_prime_from_theta(0.0),
            Err(ArmaError::NonPositiveAngle)
        );
        assert_eq!(
            theta_prime_from_theta(-1.0),
            Err(ArmaError::NonPositiveAngle)
        );

        assert_eq!(
            theta_prime_from_hz(nan, 48_000.0),
            Err(ArmaError::NotFinite)
        );
        assert_eq!(theta_prime_from_hz(1000.0, 0.0), Err(ArmaError::NotFinite));
        assert_eq!(theta_prime_from_hz(1000.0, nan), Err(ArmaError::NotFinite));
        assert_eq!(hz_from_prime(nan, 48_000.0), Err(ArmaError::NotFinite));
        assert_eq!(theta_from_prime(inf), Err(ArmaError::NotFinite));

        assert_eq!(r_prime_db_from_radius(nan), Err(ArmaError::NotFinite));
        assert_eq!(radius_from_r_prime_db(nan), Err(ArmaError::NotFinite));
    }

    #[test]
    fn the_floor_is_excluded_not_clamped() {
        let fs = 48_000.0;
        let below = display_lo_hz(fs) * 0.5;
        let tp = theta_prime_from_hz(below, fs).expect("below the floor still transforms");
        close(
            tp,
            -PI / 10.0,
            1e-12,
            "half the floor frequency is one octave below zero",
        );
        assert!(!on_plot(tp), "a sub-floor pole is off the plot");
        assert!(
            on_plot(theta_prime_from_hz(display_lo_hz(fs), fs).unwrap()),
            "the floor is on"
        );
        assert!(
            on_plot(theta_prime_from_hz(display_hi_hz(fs), fs).unwrap()),
            "Nyquist is on"
        );
        assert!(
            !on_plot(theta_prime_from_hz(display_hi_hz(fs) * 1.5, fs).unwrap()),
            "past Nyquist is off the plot"
        );
    }

    #[test]
    fn a_unit_circle_zero_reports_unbounded_not_undrawable() {
        assert_eq!(
            r_prime_db_from_radius(1.0),
            Err(ArmaError::RadiusOnOrOutsideUnitCircle)
        );
        assert_eq!(
            r_prime_db_from_radius(1.5),
            Err(ArmaError::RadiusOnOrOutsideUnitCircle)
        );
        assert_ne!(
            r_prime_db_from_radius(1.0),
            Err(ArmaError::NotFinite),
            "a null must be distinguishable from a broken number"
        );
        assert!(r_prime_db_from_radius(1.0 - 1e-9).unwrap() > 170.0);
    }

    #[test]
    fn this_module_carries_no_authoring_policy() {
        let src = include_str!("armadillo.rs");
        let code = src
            .split("#[cfg(test)]")
            .next()
            .expect("the module has a body above its tests");
        assert!(code.len() > 1000, "the policy scan found the body");

        for token in [
            "log2(hz / 20",
            "log2(hz/20",
            "(hz / 20.0).log2",
            "44100",
            "44_100",
            "48000",
            "48_000",
            "96000",
            "96_000",
            "0.99997",
            "0.49",
            "0.499",
            "FREQ_MAX",
            "DEFAULT_AUTHORING_SR",
            "authoring_limits",
            ".clamp(",
            ".min(",
            ".max(",
        ] {
            assert!(!code.contains(token), "policy token in the map: {token:?}");
        }

        assert!(
            code.contains("2048"),
            "the map derives its floor as fs/2048"
        );
        assert!(code.contains("fs: f64"), "the map takes fs as a parameter");
    }
}
