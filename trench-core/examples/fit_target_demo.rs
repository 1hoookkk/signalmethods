use trench_core::arma_endpoint::fit_arma;

fn biquad_db(b: &[f64; 5], hz: f64, sr: f64) -> f64 {
    let w = 2.0 * std::f64::consts::PI * hz / sr;
    let cos_w = w.cos();
    let cos_2w = (2.0 * w).cos();
    let sin_w = w.sin();
    let sin_2w = (2.0 * w).sin();

    let num_re = b[0] + b[1] * cos_w + b[2] * cos_2w;
    let num_im = -(b[1] * sin_w + b[2] * sin_2w);
    let den_re = 1.0 + b[3] * cos_w + b[4] * cos_2w;
    let den_im = -(b[3] * sin_w + b[4] * sin_2w);

    let mag2 = (num_re * num_re + num_im * num_im) / (den_re * den_re + den_im * den_im).max(1e-12);
    10.0 * mag2.max(1e-12).log10()
}

fn main() {
    let sr = 39062.5;

    // Define a 3-formant target response curve (vowel /i/: F1=300Hz, F2=2300Hz, F3=3000Hz with notches)
    let mut target_pairs = Vec::new();
    for i in 0..128 {
        let t = i as f64 / 127.0;
        let hz = 40.0 * (16000.0f64 / 40.0).powf(t);
        
        let f1 = 12.0 / (1.0 + ((hz - 300.0) / 60.0).powi(2));
        let f2 = 15.0 / (1.0 + ((hz - 2300.0) / 200.0).powi(2));
        let f3 = 10.0 / (1.0 + ((hz - 3000.0) / 300.0).powi(2));
        let n1 = -18.0 / (1.0 + ((hz - 1200.0) / 150.0).powi(2));
        let rolloff = if hz > 4000.0 { -6.0 * (hz / 4000.0).log2() } else { 0.0 };
        
        let target_db = f1 + f2 + f3 + n1 + rolloff;
        target_pairs.push((hz, target_db));
    }

    println!("=========================================================================================");
    println!("FIT OPTIMIZATION: TARGET CURVE -> LEGAL MORPHEUS ENCODED WORDS");
    println!("=========================================================================================");

    let fit = fit_arma(&target_pairs, sr).expect("FIT failed to converge");

    println!("Optimized in {} active sections. Final RMS Error = {:.3} dB", fit.sections_used, fit.target_rms_db);
    println!("Packed Morpheus Words RMS Error = {:.3} dB\n", fit.intended_packed_rms_db);

    println!("-----------------------------------------------------------------------------------------");
    println!("LEGAL MORPHEUS WORDS & PHYSICAL ROOTS FOR ALL 7 STAGES:");
    println!("-----------------------------------------------------------------------------------------");
    println!("Stg | Pole (Hz) | Pole R  | Zero (Hz) | Zero R  | Gain (dB) | Legal Encoded Words (u16 x 5)");
    println!("-----------------------------------------------------------------------------------------");
    for (s, (r, w)) in fit.roots.iter().zip(fit.words.iter()).enumerate() {
        let gain_db = 20.0 * r.scale.max(1e-6).log10();
        println!(
            "{:3} | {:9.1} | {:7.5} | {:9.1} | {:7.5} | {:8.2}  | [{:5}, {:5}, {:5}, {:5}, {:5}]",
            s + 1, r.pole_hz, r.pole_r, r.zero_hz, r.zero_r, gain_db,
            w[0], w[1], w[2], w[3], w[4]
        );
    }

    println!("\n-----------------------------------------------------------------------------------------");
    println!("MAGNITUDE RESPONSE COMPARISON (TARGET vs REAL MORPHEUS RESPONSE):");
    println!("-----------------------------------------------------------------------------------------");
    println!("{:>15} | {:>15} | {:>18} | {:>15}", "Frequency (Hz)", "Target (dB)", "Morpheus (dB)", "Error (dB)");
    println!("-----------------------------------------------------------------------------------------");
    
    let key_freqs = [100.0, 300.0, 600.0, 1200.0, 1800.0, 2300.0, 3000.0, 4500.0, 8000.0, 14000.0];
    let biquads: Vec<[f64; 5]> = fit.roots.iter().map(|r| r.biquad_at(sr)).collect();

    for &kf in &key_freqs {
        let (mut target_db, mut min_d) = (0.0, f64::INFINITY);
        for &(hz, db) in &target_pairs {
            let d = (hz - kf).abs();
            if d < min_d { min_d = d; target_db = db; }
        }

        let total_db: f64 = biquads.iter().map(|b| biquad_db(b, kf, sr)).sum();
        let err = total_db - target_db;
        println!("{:15.1} | {:15.2} | {:18.2} | {:+15.2}", kf, target_db, total_db, err);
    }
}
