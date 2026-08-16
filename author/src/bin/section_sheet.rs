use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{stage_words_to_biquad, PackedCorners, NUM_CORNERS};
use trench_core::stage_law::DEFAULT_AUTHORING_SR;

const BINS: usize = 512;

fn row_db(c: &[f64; NUM_COEFFS], hz: f64, sr: f64) -> f64 {
    let w = std::f64::consts::TAU * hz / sr;
    let (cw, sw) = (w.cos(), w.sin());
    let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
    let nr = c[0] + c[1] * cw + c[2] * c2;
    let ni = -(c[1] * sw + c[2] * s2);
    let dr = 1.0 + c[3] * cw + c[4] * c2;
    let di = -(c[3] * sw + c[4] * s2);
    10.0 * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30)).log10()
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: section_sheet <body file> <out csv>");
        std::process::exit(2);
    }
    let bytes = std::fs::read(&args[1]).expect("read body");
    let packed = PackedCorners::from_body_bytes(&bytes).expect("decode body");
    let sr = DEFAULT_AUTHORING_SR;
    let mut out = String::from("hz");
    for ci in 0..NUM_CORNERS {
        for si in 0..NUM_STAGES {
            out.push_str(&format!(",c{ci}s{}", si + 1));
        }
        out.push_str(&format!(",c{ci}sum"));
    }
    out.push('\n');
    for k in 0..BINS {
        let t = k as f64 / (BINS - 1) as f64;
        let hz = 40.0 * (16_000.0f64 / 40.0).powf(t);
        out.push_str(&format!("{hz:.3}"));
        for ci in 0..NUM_CORNERS {
            let mut sum = 0.0;
            for si in 0..NUM_STAGES {
                let row = stage_words_to_biquad(packed.words[ci][si]);
                let db = row_db(&row, hz, sr);
                sum += db;
                out.push_str(&format!(",{db:.4}"));
            }
            out.push_str(&format!(",{sum:.4}"));
        }
        out.push('\n');
    }
    std::fs::write(&args[2], out).expect("write csv");
    println!("wrote {}", args[2]);
}
