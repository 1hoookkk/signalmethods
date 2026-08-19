use crate::envelope;

const SEG: usize = 4096;
const HOP: usize = 2048;
const F0_LO: f64 = 70.0;
const F0_HI: f64 = 400.0;
const VOICED_MARGIN_DB: f64 = 4.0;

pub struct Decoded {
    pub curve: Vec<f64>,
    pub f0_hz: Option<f64>,
    pub sample_rate_hz: f64,
    pub seconds: f64,
}

pub fn read_wav(path: &std::path::Path) -> Result<(Vec<f64>, f64), String> {
    let data = std::fs::read(path).map_err(|e| format!("{}: {e}", path.display()))?;
    if data.len() < 44 || &data[0..4] != b"RIFF" || &data[8..12] != b"WAVE" {
        return Err(format!("{}: not a RIFF/WAVE file", path.display()));
    }
    let mut pos = 12;
    let mut fmt: Option<(u16, u16, u32, u16)> = None;
    let mut audio: Option<&[u8]> = None;
    while pos + 8 <= data.len() {
        let id = &data[pos..pos + 4];
        let size = u32::from_le_bytes(data[pos + 4..pos + 8].try_into().unwrap()) as usize;
        let body = &data[pos + 8..(pos + 8 + size).min(data.len())];
        if id == b"fmt " && body.len() >= 16 {
            fmt = Some((
                u16::from_le_bytes(body[0..2].try_into().unwrap()),
                u16::from_le_bytes(body[2..4].try_into().unwrap()),
                u32::from_le_bytes(body[4..8].try_into().unwrap()),
                u16::from_le_bytes(body[14..16].try_into().unwrap()),
            ));
        } else if id == b"data" {
            audio = Some(body);
        }
        pos += 8 + size + (size & 1);
    }
    let (format, channels, sr, bits) = fmt.ok_or("no fmt chunk")?;
    let body = audio.ok_or("no data chunk")?;
    let channels = channels.max(1) as usize;
    let mut samples = Vec::new();
    match (format, bits) {
        (1, 16) => {
            for frame in body.chunks_exact(2 * channels) {
                let mut acc = 0.0;
                for c in 0..channels {
                    acc += i16::from_le_bytes(frame[c * 2..c * 2 + 2].try_into().unwrap()) as f64 / 32768.0;
                }
                samples.push(acc / channels as f64);
            }
        }
        (1, 24) => {
            for frame in body.chunks_exact(3 * channels) {
                let mut acc = 0.0;
                for c in 0..channels {
                    let b = &frame[c * 3..c * 3 + 3];
                    let v = ((b[2] as i32) << 24 | (b[1] as i32) << 16 | (b[0] as i32) << 8) >> 8;
                    acc += v as f64 / 8388608.0;
                }
                samples.push(acc / channels as f64);
            }
        }
        (1, 32) => {
            for frame in body.chunks_exact(4 * channels) {
                let mut acc = 0.0;
                for c in 0..channels {
                    acc += i32::from_le_bytes(frame[c * 4..c * 4 + 4].try_into().unwrap()) as f64 / 2147483648.0;
                }
                samples.push(acc / channels as f64);
            }
        }
        (3, 32) => {
            for frame in body.chunks_exact(4 * channels) {
                let mut acc = 0.0;
                for c in 0..channels {
                    acc += f32::from_le_bytes(frame[c * 4..c * 4 + 4].try_into().unwrap()) as f64;
                }
                samples.push(acc / channels as f64);
            }
        }
        _ => return Err(format!("unsupported wav format {format}/{bits}-bit")),
    }
    Ok((samples, sr as f64))
}

fn fft(re: &mut [f64], im: &mut [f64]) {
    let n = re.len();
    let mut j = 0;
    for i in 1..n {
        let mut bit = n >> 1;
        while j & bit != 0 {
            j ^= bit;
            bit >>= 1;
        }
        j |= bit;
        if i < j {
            re.swap(i, j);
            im.swap(i, j);
        }
    }
    let mut len = 2;
    while len <= n {
        let ang = -std::f64::consts::TAU / len as f64;
        let (wr, wi) = (ang.cos(), ang.sin());
        let mut i = 0;
        while i < n {
            let (mut cr, mut ci) = (1.0, 0.0);
            for k in 0..len / 2 {
                let (ur, ui) = (re[i + k], im[i + k]);
                let (vr, vi) = (
                    re[i + k + len / 2] * cr - im[i + k + len / 2] * ci,
                    re[i + k + len / 2] * ci + im[i + k + len / 2] * cr,
                );
                re[i + k] = ur + vr;
                im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr;
                im[i + k + len / 2] = ui - vi;
                let ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
            i += len;
        }
        len <<= 1;
    }
}

fn welch_psd_db(samples: &[f64]) -> Vec<f64> {
    let hann: Vec<f64> = (0..SEG)
        .map(|i| 0.5 - 0.5 * (std::f64::consts::TAU * i as f64 / SEG as f64).cos())
        .collect();
    let mut acc = vec![0.0f64; SEG / 2 + 1];
    let mut count = 0usize;
    let mut start = 0;
    while start + SEG <= samples.len() || count == 0 {
        let mut re = vec![0.0; SEG];
        let mut im = vec![0.0; SEG];
        for i in 0..SEG {
            let s = samples.get(start + i).copied().unwrap_or(0.0);
            re[i] = s * hann[i];
        }
        fft(&mut re, &mut im);
        for k in 0..=SEG / 2 {
            acc[k] += re[k] * re[k] + im[k] * im[k];
        }
        count += 1;
        start += HOP;
    }
    acc.iter()
        .map(|p| 10.0 * (p / count as f64).max(1e-20).log10())
        .collect()
}

fn estimate_f0(psd_db: &[f64], bin_hz: f64) -> Option<f64> {
    let at = |hz: f64| -> f64 {
        let x = hz / bin_hz;
        let i = x.floor() as usize;
        if i + 1 >= psd_db.len() {
            return psd_db[psd_db.len() - 1];
        }
        let t = x - i as f64;
        psd_db[i] * (1.0 - t) + psd_db[i + 1] * t
    };
    let mut scores = Vec::new();
    let mut cand = F0_LO;
    while cand <= F0_HI {
        let mut s = 0.0;
        let mut n = 0;
        let mut h = cand;
        while h < 5000.0 && n < 12 {
            s += at(h);
            n += 1;
            h += cand;
        }
        scores.push((cand, s / n as f64));
        cand += 0.5;
    }
    let best = scores
        .iter()
        .cloned()
        .max_by(|a, b| a.1.total_cmp(&b.1))?;
    let mut vals: Vec<f64> = scores.iter().map(|s| s.1).collect();
    vals.sort_by(|a, b| a.total_cmp(b));
    let median = vals[vals.len() / 2];
    if best.1 - median > VOICED_MARGIN_DB {
        Some(best.0)
    } else {
        None
    }
}

pub fn envelope_from_samples(samples: &[f64], sr: f64) -> Result<Decoded, String> {
    if samples.len() < SEG {
        return Err("recording shorter than one analysis window".into());
    }
    let psd = welch_psd_db(samples);
    let bin_hz = sr / SEG as f64;
    let f0 = estimate_f0(&psd, bin_hz);
    let nyq = sr / 2.0;
    let grid = envelope::grid();
    let mut curve = Vec::with_capacity(grid.len());
    for &hz in &grid {
        if hz > nyq * 0.98 {
            curve.push(f64::NEG_INFINITY);
            continue;
        }
        let half = match f0 {
            Some(f) => 0.5 * f,
            None => (hz * 0.06).max(bin_hz),
        };
        let lo = (((hz - half) / bin_hz).floor().max(0.0)) as usize;
        let hi = (((hz + half) / bin_hz).ceil() as usize).min(psd.len() - 1);
        let mut v = f64::NEG_INFINITY;
        match f0 {
            Some(_) => {
                for k in lo..=hi {
                    if psd[k] > v {
                        v = psd[k];
                    }
                }
            }
            None => {
                let mut s = 0.0;
                for k in lo..=hi {
                    s += psd[k];
                }
                v = s / (hi - lo + 1) as f64;
            }
        }
        curve.push(v);
    }
    let floor = curve.iter().cloned().filter(|v| v.is_finite()).fold(f64::INFINITY, f64::min);
    for v in curve.iter_mut() {
        if !v.is_finite() {
            *v = floor;
        }
    }
    let smoothed: Vec<f64> = match f0 {
        Some(f) => {
            let mut harm_hz = Vec::new();
            let mut harm_db = Vec::new();
            let mut h = f;
            while h < nyq * 0.95 {
                let gi = grid.partition_point(|&g| g < h).min(grid.len() - 1);
                let lo = gi.saturating_sub(3);
                let hi = (gi + 4).min(curve.len());
                let best = (lo..hi).max_by(|&a, &b| curve[a].total_cmp(&curve[b])).unwrap_or(gi);
                harm_hz.push(grid[best]);
                harm_db.push(curve[best]);
                h += f;
            }
            if harm_hz.len() < 2 {
                curve.clone()
            } else {
                grid.iter()
                    .map(|&hz| {
                        if hz <= harm_hz[0] {
                            return harm_db[0];
                        }
                        if hz >= *harm_hz.last().unwrap() {
                            return *harm_db.last().unwrap();
                        }
                        let j = harm_hz.partition_point(|&h| h < hz);
                        if j == 0 { return harm_db[0]; }
                        let t = (hz.ln() - harm_hz[j - 1].ln())
                            / (harm_hz[j].ln() - harm_hz[j - 1].ln());
                        harm_db[j - 1] + t * (harm_db[j] - harm_db[j - 1])
                    })
                    .collect()
            }
        }
        None => {
            let win = 3usize;
            (0..curve.len())
                .map(|i| {
                    let lo = i.saturating_sub(win);
                    let hi = (i + win + 1).min(curve.len());
                    curve[lo..hi].iter().sum::<f64>() / (hi - lo) as f64
                })
                .collect()
        }
    };
    let peak = smoothed.iter().cloned().fold(f64::NEG_INFINITY, f64::max);
    let curve: Vec<f64> = smoothed.iter().map(|v| v - peak).collect();
    Ok(Decoded {
        curve,
        f0_hz: f0,
        sample_rate_hz: sr,
        seconds: samples.len() as f64 / sr,
    })
}

pub fn radiation_emphasis(grid: &[f64], curve: &[f64]) -> Vec<f64> {
    grid.iter()
        .zip(curve)
        .map(|(&hz, &db)| if hz > 500.0 { db + 6.0 * (hz / 500.0).log2() } else { db })
        .collect()
}

fn sliding_median(curve: &[f64], half: usize) -> Vec<f64> {
    (0..curve.len())
        .map(|i| {
            let lo = i.saturating_sub(half);
            let hi = (i + half + 1).min(curve.len());
            let mut w: Vec<f64> = curve[lo..hi].to_vec();
            w.sort_by(|a, b| a.total_cmp(b));
            w[w.len() / 2]
        })
        .collect()
}

pub fn formant_peaks(grid: &[f64], curve: &[f64]) -> Vec<crate::formants::Peak> {
    let emphasized = radiation_emphasis(grid, curve);
    let pts_per_oct = (grid.len() - 1) as f64 / (envelope::GRID_HI_HZ / envelope::GRID_LO_HZ).log2();
    let baseline = sliding_median(&emphasized, (0.75 * pts_per_oct) as usize);
    let local: Vec<f64> = emphasized.iter().zip(&baseline).map(|(v, b)| v - b).collect();
    let mut found = crate::formants::peaks(grid, &local);
    let reach = (0.15 * pts_per_oct) as usize;
    for p in found.iter_mut() {
        let i = grid.partition_point(|&g| g < p.hz).min(grid.len() - 1);
        let lo = i.saturating_sub(reach);
        let hi = (i + reach + 1).min(grid.len());
        let mut best = lo;
        for k in lo..hi {
            if curve[k] > curve[best] {
                best = k;
            }
        }
        p.hz = grid[best];
        p.db = curve[best];
    }
    found
}

pub struct ArSection {
    pub pole_hz: f64,
    pub pole_r: f64,
}

pub fn ar_poles(samples: &[f64], sr: f64, order: usize) -> Vec<ArSection> {
    let n = samples.len();
    if n < order + 1 {
        return Vec::new();
    }
    let mut ef: Vec<f64> = samples.to_vec();
    let mut eb: Vec<f64> = samples.to_vec();
    let mut a: Vec<f64> = vec![0.0; order + 1];
    a[0] = 1.0;

    for k in 1..=order {
        let mut num = 0.0;
        let mut den = 0.0;
        for j in k..n {
            num += ef[j] * eb[j - 1];
            den += ef[j] * ef[j] + eb[j - 1] * eb[j - 1];
        }
        let lambda = if den.abs() > 1e-30 { -2.0 * num / den } else { 0.0 };
        let mut a_new = vec![0.0; order + 1];
        for i in 0..=k {
            a_new[i] = a[i] + lambda * a[k - i];
        }
        a = a_new;
        let mut ef_new = vec![0.0; n];
        let mut eb_new = vec![0.0; n];
        for j in k..n {
            ef_new[j] = ef[j] + lambda * eb[j - 1];
            eb_new[j] = eb[j - 1] + lambda * ef[j];
        }
        ef = ef_new;
        eb = eb_new;
    }

    let mut roots = Vec::new();
    let coeffs: Vec<f64> = a[..=order].to_vec();
    let companion = companion_matrix(&coeffs);
    let eigenvalues = qr_eigenvalues(&companion, 200);
    for (re, im) in eigenvalues {
        let r = (re * re + im * im).sqrt();
        if r < 0.3 || r > 0.9999 {
            continue;
        }
        let angle = im.atan2(re).abs();
        let hz = angle * sr / std::f64::consts::TAU;
        if hz < 20.0 || hz > sr / 2.0 - 20.0 {
            continue;
        }
        if im < 0.0 {
            continue;
        }
        roots.push(ArSection { pole_hz: hz, pole_r: r });
    }
    roots.sort_by(|a, b| a.pole_hz.partial_cmp(&b.pole_hz).unwrap());
    roots
}

fn companion_matrix(a: &[f64]) -> Vec<Vec<f64>> {
    let n = a.len() - 1;
    let mut m = vec![vec![0.0; n]; n];
    for i in 0..n - 1 {
        m[i + 1][i] = 1.0;
    }
    for i in 0..n {
        m[i][n - 1] = -a[n - i] / a[0];
    }
    m
}

fn qr_eigenvalues(mat: &[Vec<f64>], max_iter: usize) -> Vec<(f64, f64)> {
    let n = mat.len();
    let mut h: Vec<Vec<f64>> = mat.to_vec();
    for _ in 0..max_iter {
        let shift = h[n - 1][n - 1];
        for i in 0..n {
            h[i][i] -= shift;
        }
        let (q, r) = qr_decompose(&h);
        h = mat_mul(&r, &q);
        for i in 0..n {
            h[i][i] += shift;
        }
        let mut converged = true;
        for i in 1..n {
            if h[i][i - 1].abs() > 1e-12 {
                converged = false;
                break;
            }
        }
        if converged {
            break;
        }
    }
    let mut eigs = Vec::new();
    let mut i = 0;
    while i < n {
        if i + 1 < n && h[i + 1][i].abs() > 1e-10 {
            let a = h[i][i];
            let b = h[i][i + 1];
            let c = h[i + 1][i];
            let d = h[i + 1][i + 1];
            let tr = a + d;
            let det = a * d - b * c;
            let disc = tr * tr - 4.0 * det;
            if disc < 0.0 {
                let re = tr / 2.0;
                let im = (-disc).sqrt() / 2.0;
                eigs.push((re, im));
                eigs.push((re, -im));
            } else {
                let sq = disc.sqrt();
                eigs.push(((tr + sq) / 2.0, 0.0));
                eigs.push(((tr - sq) / 2.0, 0.0));
            }
            i += 2;
        } else {
            eigs.push((h[i][i], 0.0));
            i += 1;
        }
    }
    eigs
}

fn qr_decompose(a: &[Vec<f64>]) -> (Vec<Vec<f64>>, Vec<Vec<f64>>) {
    let n = a.len();
    let mut q = vec![vec![0.0; n]; n];
    let mut r = vec![vec![0.0; n]; n];
    let mut cols: Vec<Vec<f64>> = (0..n).map(|j| (0..n).map(|i| a[i][j]).collect()).collect();
    for j in 0..n {
        let mut u = cols[j].clone();
        for k in 0..j {
            let dot: f64 = (0..n).map(|i| cols[j][i] * q[i][k]).sum();
            for i in 0..n {
                u[i] -= dot * q[i][k];
            }
            r[k][j] = dot;
        }
        let norm: f64 = u.iter().map(|x| x * x).sum::<f64>().sqrt();
        if norm > 1e-30 {
            for i in 0..n {
                q[i][j] = u[i] / norm;
            }
            r[j][j] = norm;
        }
    }
    (q, r)
}

fn mat_mul(a: &[Vec<f64>], b: &[Vec<f64>]) -> Vec<Vec<f64>> {
    let n = a.len();
    let mut c = vec![vec![0.0; n]; n];
    for i in 0..n {
        for j in 0..n {
            for k in 0..n {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    c
}

pub fn envelope_from_wav(path: &std::path::Path) -> Result<Decoded, String> {
    let (samples, sr) = read_wav(path)?;
    envelope_from_samples(&samples, sr)
}

pub fn scan(dir: &std::path::Path) -> Vec<(String, std::path::PathBuf)> {
    let mut found = Vec::new();
    let Ok(entries) = std::fs::read_dir(dir) else {
        return found;
    };
    for entry in entries.flatten() {
        let p = entry.path();
        if p.extension().is_some_and(|x| x.eq_ignore_ascii_case("wav")) {
            found.push((p.file_stem().unwrap_or_default().to_string_lossy().into_owned(), p));
        }
    }
    found.sort();
    found
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::formants;

    fn resonator(f: f64, bw: f64, sr: f64) -> [f64; 5] {
        let r = (-std::f64::consts::PI * bw / sr).exp();
        let th = std::f64::consts::TAU * f / sr;
        [1.0 - r, 0.0, 0.0, -2.0 * r * th.cos(), r * r]
    }

    #[test]
    fn synthetic_vowel_yields_formants_not_harmonics() {
        let sr = 44_100.0;
        let f0 = 120.0;
        let n = (sr * 2.0) as usize;
        let mut x = vec![0.0f64; n];
        let period = (sr / f0) as usize;
        let mut i = 0;
        while i < n {
            x[i] = 1.0;
            i += period;
        }
        let bands = [(700.0, 90.0), (1200.0, 120.0), (2600.0, 200.0)];
        for &(f, bw) in &bands {
            let c = resonator(f, bw, sr);
            let mut s1 = 0.0;
            let mut s2 = 0.0;
            for v in x.iter_mut() {
                let inp = *v;
                let y = c[0] * inp + s1;
                s1 = c[1] * inp - c[3] * y + s2;
                s2 = c[2] * inp - c[4] * y;
                *v = y;
            }
        }
        let d = envelope_from_samples(&x, sr).unwrap();
        let f0_est = d.f0_hz.expect("voiced");
        assert!((f0_est / f0).log2().abs() < 0.06, "f0 {f0_est}");
        let grid = envelope::grid();
        let peaks = formant_peaks(&grid, &d.curve);
        for &(f, _) in &bands {
            assert!(
                peaks.iter().any(|p| (p.hz / f).log2().abs() < 0.12),
                "no formant near {f}: {peaks:?}"
            );
        }
        for p in &peaks {
            assert!(
                bands.iter().any(|&(f, _)| (p.hz / f).log2().abs() < 0.25),
                "spurious peak at {} Hz — harmonic leaked through pitch correction",
                p.hz
            );
        }
    }
}
