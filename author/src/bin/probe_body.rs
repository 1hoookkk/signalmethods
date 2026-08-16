use trench_core::cascade::NUM_STAGES;
use trench_core::minifloat::PackedCorners;

fn db(rows: &[[f64; 5]; NUM_STAGES], hz: f64, sr: f64) -> f64 {
    let w = std::f64::consts::TAU * hz / sr;
    let (cw, sw) = (w.cos(), w.sin());
    let (c2, s2) = ((2.0 * w).cos(), (2.0 * w).sin());
    rows.iter().map(|c| {
        let nr = c[0] + c[1] * cw + c[2] * c2;
        let ni = -(c[1] * sw + c[2] * s2);
        let dr = 1.0 + c[3] * cw + c[4] * c2;
        let di = -(c[3] * sw + c[4] * s2);
        10.0 * ((nr * nr + ni * ni).max(1e-30) / (dr * dr + di * di).max(1e-30)).log10()
    }).sum()
}

fn main() {
    let bytes = std::fs::read(std::env::args().nth(1).unwrap()).unwrap();
    let p = PackedCorners::from_body_bytes(&bytes).unwrap();
    let sr = 39_062.5;
    for m in [0.0f32, 0.5, 1.0] {
        let rows = p.interpolate_biquad(m, 0.0, 0.0);
        let peaks: Vec<String> = (1..15).map(|k| {
            let hz = 100.0 * (160.0f64).powf(k as f64 / 14.0);
            format!("{:.0}", db(&rows, hz, sr))
        }).collect();
        println!("M{:.1}: {}", m, peaks.join(" "));
    }
}
