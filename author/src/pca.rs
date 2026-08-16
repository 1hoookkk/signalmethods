#[derive(Clone)]
pub struct Basis {
    pub mean: Vec<f64>,
    pub components: Vec<Vec<f64>>,
    pub sigma: Vec<f64>,
    pub explained: f64,
}

pub fn fit(rows: &[Vec<f64>], keep: usize) -> Result<Basis, String> {
    let n = rows.len();
    if n < 3 {
        return Err(format!("{n} envelopes is not a basis"));
    }
    let d = rows[0].len();
    if rows.iter().any(|r| r.len() != d) {
        return Err("envelopes disagree on grid length".into());
    }
    let mut mean = vec![0.0; d];
    for row in rows {
        for (m, v) in mean.iter_mut().zip(row) {
            *m += v / n as f64;
        }
    }
    let centred: Vec<Vec<f64>> = rows
        .iter()
        .map(|row| row.iter().zip(&mean).map(|(v, m)| v - m).collect())
        .collect();
    let mut gram = vec![vec![0.0; n]; n];
    for i in 0..n {
        for j in i..n {
            let dot: f64 = centred[i].iter().zip(&centred[j]).map(|(a, b)| a * b).sum();
            gram[i][j] = dot;
            gram[j][i] = dot;
        }
    }
    let (eigenvalues, eigenvectors) = jacobi(gram);
    let mut order: Vec<usize> = (0..n).collect();
    order.sort_by(|&a, &b| eigenvalues[b].total_cmp(&eigenvalues[a]));
    let total: f64 = eigenvalues.iter().map(|l| l.max(0.0)).sum();
    let keep = keep.min(n - 1);
    let mut components = Vec::with_capacity(keep);
    let mut sigma = Vec::with_capacity(keep);
    let mut kept = 0.0;
    for &k in order.iter().take(keep) {
        let lambda = eigenvalues[k].max(0.0);
        if lambda <= 1e-12 {
            break;
        }
        kept += lambda;
        let mut axis = vec![0.0; d];
        for i in 0..n {
            let u = eigenvectors[i][k];
            for (a, c) in axis.iter_mut().zip(&centred[i]) {
                *a += u * c;
            }
        }
        let norm = axis.iter().map(|v| v * v).sum::<f64>().sqrt().max(1e-12);
        for a in axis.iter_mut() {
            *a /= norm;
        }
        components.push(axis);
        sigma.push((lambda / (n - 1) as f64).sqrt());
    }
    Ok(Basis {
        mean,
        components,
        sigma,
        explained: if total > 0.0 { kept / total } else { 0.0 },
    })
}

impl Basis {
    pub fn project(&self, row: &[f64]) -> Vec<f64> {
        self.components
            .iter()
            .map(|axis| {
                axis.iter()
                    .zip(row.iter().zip(&self.mean))
                    .map(|(a, (v, m))| a * (v - m))
                    .sum()
            })
            .collect()
    }

    pub fn reconstruct(&self, scores: &[f64]) -> Vec<f64> {
        let mut out = self.mean.clone();
        for (axis, s) in self.components.iter().zip(scores) {
            for (o, a) in out.iter_mut().zip(axis) {
                *o += s * a;
            }
        }
        out
    }
}

fn jacobi(mut a: Vec<Vec<f64>>) -> (Vec<f64>, Vec<Vec<f64>>) {
    let n = a.len();
    let mut v = vec![vec![0.0; n]; n];
    for (i, row) in v.iter_mut().enumerate() {
        row[i] = 1.0;
    }
    for _ in 0..100 {
        let mut off = 0.0;
        for i in 0..n {
            for j in (i + 1)..n {
                off += a[i][j] * a[i][j];
            }
        }
        if off < 1e-18 {
            break;
        }
        for p in 0..n {
            for q in (p + 1)..n {
                if a[p][q].abs() < 1e-18 {
                    continue;
                }
                let theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                let t = theta.signum() / (theta.abs() + (theta * theta + 1.0).sqrt());
                let c = 1.0 / (t * t + 1.0).sqrt();
                let s = t * c;
                for k in 0..n {
                    let akp = a[k][p];
                    let akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for k in 0..n {
                    let apk = a[p][k];
                    let aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for k in 0..n {
                    let vkp = v[k][p];
                    let vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    ((0..n).map(|i| a[i][i]).collect(), v)
}
