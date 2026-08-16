pub const GRID_POINTS: usize = 1024;
pub const GRID_LO_HZ: f64 = 40.0;
pub const GRID_HI_HZ: f64 = 16_000.0;

pub fn grid() -> Vec<f64> {
    (0..GRID_POINTS)
        .map(|i| {
            let t = i as f64 / (GRID_POINTS - 1) as f64;
            GRID_LO_HZ * (GRID_HI_HZ / GRID_LO_HZ).powf(t)
        })
        .collect()
}

pub fn read_vvtf(path: &std::path::Path) -> Result<Vec<(f64, f64)>, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let mut curve = Vec::new();
    for line in text.lines() {
        let mut parts = line.split_whitespace();
        let (Some(f), Some(m)) = (parts.next(), parts.next()) else {
            continue;
        };
        let (Ok(f), Ok(m)) = (f.parse::<f64>(), m.parse::<f64>()) else {
            continue;
        };
        if f > 0.0 && m.is_finite() {
            curve.push((f, 20.0 * m.max(1e-6).log10()));
        }
    }
    if curve.len() < 32 || curve.windows(2).any(|w| w[1].0 <= w[0].0) {
        return Err(format!("{}: not a transfer function table", path.display()));
    }
    Ok(curve)
}

pub fn on_grid(curve: &[(f64, f64)]) -> Vec<f64> {
    grid()
        .iter()
        .map(|&hz| {
            let upper = curve.partition_point(|(f, _)| *f < hz);
            if upper == 0 {
                return curve[0].1;
            }
            if upper >= curve.len() {
                return curve[curve.len() - 1].1;
            }
            let (f0, y0) = curve[upper - 1];
            let (f1, y1) = curve[upper];
            let span = f1.log2() - f0.log2();
            if span.abs() < 1e-15 {
                return y0;
            }
            let t = ((hz.log2() - f0.log2()) / span).clamp(0.0, 1.0);
            y0 + t * (y1 - y0)
        })
        .collect()
}

pub fn scan(dir: &std::path::Path) -> Vec<(String, std::path::PathBuf)> {
    let mut found = Vec::new();
    let mut stack = vec![dir.to_path_buf()];
    while let Some(d) = stack.pop() {
        let Ok(entries) = std::fs::read_dir(&d) else {
            continue;
        };
        for entry in entries.flatten() {
            let p = entry.path();
            if p.is_dir() {
                stack.push(p);
            } else if p
                .file_name()
                .and_then(|n| n.to_str())
                .is_some_and(|n| n.ends_with("-vvtf-measured.txt"))
            {
                let name = p.file_name().unwrap().to_string_lossy().into_owned();
                found.push((name, p));
            }
        }
    }
    found.sort();
    found
}
