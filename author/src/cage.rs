use std::path::{Path, PathBuf};

use trench_core::cascade::NUM_STAGES;

use crate::frame::LaneLaw;

pub struct Cage {
    pub name: String,
    pub laws: [LaneLaw; NUM_STAGES],
}

pub fn write(path: &Path, cage: &Cage) -> Result<(), String> {
    let laws: Vec<serde_json::Value> = cage
        .laws
        .iter()
        .map(|w| {
            serde_json::json!({
                "writable": w.writable,
                "freedom": w.freedom,
                "zone": if w.has_zone() {
                    serde_json::json!([w.zone[0], w.zone[1]])
                } else {
                    serde_json::Value::Null
                },
            })
        })
        .collect();
    let doc = serde_json::json!({
        "schema": "trench-cage-v1",
        "name": cage.name,
        "laws": laws,
    });
    let text = serde_json::to_string_pretty(&doc).map_err(|e| format!("{}: {e}", path.display()))?;
    std::fs::write(path, text).map_err(|e| format!("{}: {e}", path.display()))
}

pub fn read(path: &Path) -> Result<Cage, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let doc: serde_json::Value =
        serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
    if doc.get("schema").and_then(|v| v.as_str()) != Some("trench-cage-v1") {
        return Err(format!("{}: not a trench cage", path.display()));
    }
    let name = doc
        .get("name")
        .and_then(|v| v.as_str())
        .unwrap_or("unnamed")
        .to_string();
    let mut laws = [LaneLaw::OPEN; NUM_STAGES];
    if let Some(rows) = doc.get("laws").and_then(|v| v.as_array()) {
        for (si, row) in rows.iter().enumerate().take(NUM_STAGES) {
            let mut law = LaneLaw::OPEN;
            law.writable = row
                .get("writable")
                .and_then(|v| v.as_bool())
                .unwrap_or(true);
            if let Some(f) = row.get("freedom").and_then(|v| v.as_array()) {
                for (j, flag) in f.iter().enumerate().take(4) {
                    law.freedom[j] = flag.as_bool().unwrap_or(true);
                }
            }
            if let Some(z) = row.get("zone").and_then(|v| v.as_array()) {
                if z.len() == 2 {
                    if let (Some(lo), Some(hi)) = (z[0].as_f64(), z[1].as_f64()) {
                        law.zone = [lo, hi];
                    }
                }
            }
            laws[si] = law;
        }
    }
    Ok(Cage { name, laws })
}

pub fn scan(dir: &Path) -> Vec<(String, PathBuf)> {
    let mut out: Vec<(String, PathBuf)> = std::fs::read_dir(dir)
        .map(|rd| {
            rd.flatten()
                .map(|e| e.path())
                .filter(|p| p.extension().is_some_and(|x| x == "json"))
                .filter_map(|p| read(&p).ok().map(|c| (c.name, p)))
                .collect()
        })
        .unwrap_or_default();
    out.sort();
    out
}
