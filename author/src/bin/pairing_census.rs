use trench_core::stage_law::RootPair;

fn main() {
    let dir = std::path::Path::new("recipes/architectures");
    let mut intervals: Vec<f64> = Vec::new();
    let mut zero_rs: Vec<f64> = Vec::new();
    let mut cross = 0usize;
    let mut local = 0usize;
    let mut unit = 0usize;
    let mut real_pairs = 0usize;
    let mut sections = 0usize;
    let mut entries: Vec<_> = std::fs::read_dir(dir).unwrap().flatten().collect();
    entries.sort_by_key(|e| e.path());
    for e in entries {
        let path = e.path();
        if path.extension().is_none_or(|x| x != "json") {
            continue;
        }
        let j: serde_json::Value =
            serde_json::from_str(&std::fs::read_to_string(&path).unwrap()).unwrap();
        let Some(list) = j.get("sections").and_then(|s| s.as_array()) else {
            continue;
        };
        for s in list {
            for corner in ["M0_Q0", "M100_Q0"] {
                let Some(g) = author::recipes::corner_geometry(s, corner) else {
                    continue;
                };
                sections += 1;
                let (ph, _pr) = match g.pole {
                    RootPair::Conjugate { hz, r } => (hz, r),
                    RootPair::RealPair { .. } => {
                        real_pairs += 1;
                        continue;
                    }
                    RootPair::Degenerate => continue,
                };
                let (zh, zr) = match g.zero {
                    RootPair::Conjugate { hz, r } => (hz, r),
                    RootPair::RealPair { .. } => {
                        real_pairs += 1;
                        continue;
                    }
                    RootPair::Degenerate => continue,
                };
                if ph <= 0.0 || zh <= 0.0 {
                    continue;
                }
                let st = 12.0 * (zh / ph).log2();
                intervals.push(st);
                zero_rs.push(zr);
                if st.abs() > 24.0 {
                    cross += 1;
                } else {
                    local += 1;
                }
                if zr >= 0.9995 {
                    unit += 1;
                }
            }
        }
    }
    intervals.sort_by(|a, b| a.total_cmp(b));
    zero_rs.sort_by(|a, b| a.total_cmp(b));
    let pct = |v: &Vec<f64>, p: f64| v[((v.len() - 1) as f64 * p) as usize];
    println!("sections seen              {sections}");
    println!("pole+zero conjugate pairs  {}", intervals.len());
    println!("real-axis letters          {real_pairs}");
    println!(
        "local pairs (|st|<=24)     {local}  ({:.0}%)",
        100.0 * local as f64 / (local + cross) as f64
    );
    println!(
        "cross pairs (|st|>24)      {cross}  ({:.0}%)",
        100.0 * cross as f64 / (local + cross) as f64
    );
    println!(
        "zero on the circle         {unit}  ({:.0}%)",
        100.0 * unit as f64 / zero_rs.len() as f64
    );
    println!(
        "interval st  p10 {:+.1}  p25 {:+.1}  median {:+.1}  p75 {:+.1}  p90 {:+.1}",
        pct(&intervals, 0.10),
        pct(&intervals, 0.25),
        pct(&intervals, 0.50),
        pct(&intervals, 0.75),
        pct(&intervals, 0.90)
    );
    let locals: Vec<f64> = intervals.iter().copied().filter(|s| s.abs() <= 24.0).collect();
    println!(
        "local-pair interval st     p25 {:+.1}  median {:+.1}  p75 {:+.1}",
        pct(&locals, 0.25),
        pct(&locals, 0.50),
        pct(&locals, 0.75)
    );
    println!(
        "zero radius  p10 {:.4}  p25 {:.4}  median {:.4}  p75 {:.4}  p90 {:.4}",
        pct(&zero_rs, 0.10),
        pct(&zero_rs, 0.25),
        pct(&zero_rs, 0.50),
        pct(&zero_rs, 0.75),
        pct(&zero_rs, 0.90)
    );
}
