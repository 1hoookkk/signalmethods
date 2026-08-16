use author::{body, envelope, extrude};
use trench_core::arma_endpoint::{fit_arma, fit_arma_planned, FREE, NO_ZONES};
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::words_from_roots_at;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 4 {
        eprintln!("usage: build_square <mouth A> <mouth B> <out.body>");
        std::process::exit(2);
    }
    let sr = extrude::AUTHORING_SR;
    let read = |p: &str| -> Vec<(f64, f64)> {
        let curve = envelope::on_grid(&envelope::read_vvtf(p.as_ref()).expect("read mouth"));
        envelope::grid().into_iter().zip(curve).collect()
    };
    let a = read(&args[1]);
    let b = read(&args[2]);

    let fit_a = fit_arma(&a, sr).expect("cold fit A");
    eprintln!("M0 cold fit: {:.2} dB rms · {} sections", fit_a.target_rms_db, fit_a.sections_used);
    let fit_b = fit_arma_planned(&b, sr, &fit_a.roots, &FREE, &[true; NUM_STAGES], &NO_ZONES)
        .expect("warm fit B");
    eprintln!("M100 warm fit: {:.2} dB rms (seeded from M0 — lanes correspond)", fit_b.target_rms_db);

    let mut words = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
    for ci in 0..NUM_CORNERS {
        let roots = if ci & 1 == 0 { &fit_a.roots } else { &fit_b.roots };
        for (si, lane) in roots.iter().enumerate() {
            words[ci][si] = words_from_roots_at(lane, sr);
        }
    }
    let packed = PackedCorners { words };
    let audit = body::audit(&packed, sr);
    eprintln!(
        "audit: crown {:.1}..{:.1} dB · parity {:.1} · surge {:+.1} · {}",
        audit.crown_min_db,
        audit.crown_max_db,
        audit.parity_db,
        audit.surge_db,
        if audit.pass() { "PASS" } else { "FAIL" }
    );
    let bytes: Vec<u8> = match packed.to_legacy_bytes() {
        Some(b) => {
            eprintln!("240-byte legacy");
            b.to_vec()
        }
        None => {
            eprintln!("560-byte native");
            packed.to_native_bytes().to_vec()
        }
    };
    std::fs::write(&args[3], &bytes).expect("write body");
    eprintln!("wrote {} ({} bytes)", args[3], bytes.len());
    if !audit.pass() {
        std::process::exit(1);
    }
}
