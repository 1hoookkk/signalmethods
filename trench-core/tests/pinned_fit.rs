//! The workstation rules, held against the fitter.
//!
//! Assign SOS lane ownership before fitting. Fit the complete serial cascade.
//! Preserve correspondence. Responses multiply and dB responses add.

use trench_core::arma_endpoint::{fit_arma, fit_arma_pinned};
use trench_core::minifloat::stage_words_to_biquad;
use trench_core::response::biquad_stage_complex;
use trench_core::stage_law::{words_from_roots_at, StageRoots};

const SR: f64 = 44_100.0;

fn db(c: (f64, f64)) -> f64 {
    20.0 * (c.0 * c.0 + c.1 * c.1).sqrt().max(1e-12).log10()
}

/// A log grid, as the brief requires before anything is compared.
fn grid() -> Vec<f64> {
    let (lo, hi, n) = (40.0f64, 16_000.0f64, 512usize);
    (0..n)
        .map(|i| lo * (hi / lo).powf(i as f64 / (n - 1) as f64))
        .collect()
}

/// The target: three resonances, summed in dB because sections multiply.
fn target_from(sections: &[StageRoots]) -> Vec<(f64, f64)> {
    let biquads: Vec<[f64; 5]> = sections
        .iter()
        .map(|r| stage_words_to_biquad(words_from_roots_at(r, SR)))
        .collect();
    grid()
        .into_iter()
        .map(|f| {
            let total: f64 = biquads
                .iter()
                .map(|b| db(biquad_stage_complex(b, f, SR)))
                .sum();
            (f, total)
        })
        .collect()
}

fn known_sections() -> Vec<StageRoots> {
    vec![
        StageRoots {
            pole_hz: 300.0,
            pole_r: 0.96,
            zero_hz: 300.0,
            zero_r: 0.0,
            scale: 1.0,
        },
        StageRoots {
            pole_hz: 1200.0,
            pole_r: 0.95,
            zero_hz: 1200.0,
            zero_r: 0.0,
            scale: 1.0,
        },
        StageRoots {
            pole_hz: 4000.0,
            pole_r: 0.93,
            zero_hz: 4000.0,
            zero_r: 0.0,
            scale: 1.0,
        },
    ]
}

/// Correspondence is preserved: a pinned section's pole stays exactly where
/// the operator put it, whatever the descent does to the rest.
#[test]
fn pinned_poles_do_not_move() {
    let target = target_from(&known_sections());
    let pins = [300.0, 1200.0, 4000.0];
    let fit = fit_arma_pinned(&target, SR, &pins).expect("fit converges");

    for (s, want) in pins.iter().enumerate() {
        let got = fit.roots[s].pole_hz;
        assert!(
            (got - want).abs() < 1.0,
            "section {s} was pinned at {want} Hz and came back at {got} Hz"
        );
    }
}

/// Ownership decides which section answers for which feature. Pin the same
/// three resonances in a different order and each section takes the one it
/// was given, not the nearest one.
#[test]
fn ownership_and_not_frequency_order_decides_the_lane() {
    let target = target_from(&known_sections());
    let pins = [4000.0, 300.0, 1200.0];
    let fit = fit_arma_pinned(&target, SR, &pins).expect("fit converges");

    for (s, want) in pins.iter().enumerate() {
        let got = fit.roots[s].pole_hz;
        assert!(
            (got - want).abs() < 1.0,
            "section {s} owned {want} Hz but holds {got} Hz; lanes were re-sorted"
        );
    }
}

/// The fit is judged on the sum of the section curves, so a cascade built
/// from a known set is recovered to a small whole-cascade residual.
#[test]
fn the_whole_cascade_is_fitted_not_each_section() {
    let target = target_from(&known_sections());
    let fit = fit_arma_pinned(&target, SR, &[300.0, 1200.0, 4000.0]).expect("fit converges");
    assert!(
        fit.target_rms_db < 3.0,
        "whole-cascade residual {:.3} dB is too large",
        fit.target_rms_db
    );
}

/// Unpinned, the fitter is free to seat sections wherever the residual is
/// strongest. That freedom is exactly what pinning removes.
#[test]
fn without_pins_the_fitter_seats_sections_itself() {
    let target = target_from(&known_sections());
    let fit = fit_arma(&target, SR).expect("fit converges");
    assert!(fit.sections_used > 0, "an unpinned fit used no sections");
    assert!(
        fit.target_rms_db.is_finite(),
        "unpinned fit produced no residual"
    );
}

/// A root the words cannot hold is never proposed: everything returned packs
/// and reads back as the same filter.
#[test]
fn every_fitted_root_survives_the_encoder() {
    let target = target_from(&known_sections());
    let fit = fit_arma_pinned(&target, SR, &[300.0, 1200.0, 4000.0]).expect("fit converges");
    for (s, r) in fit.roots.iter().enumerate() {
        assert_eq!(
            fit.words[s],
            words_from_roots_at(r, SR),
            "section {s} words disagree with its roots"
        );
        assert!(
            r.pole_r < 1.0,
            "section {s} pole radius {} is not inside",
            r.pole_r
        );
    }
}
