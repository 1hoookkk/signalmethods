use std::path::Path;

use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::StageRoots;

#[derive(Clone, Copy)]
pub struct LaneLaw {
    pub writable: bool,
    pub freedom: [bool; 4],
    pub zone: [f64; 2],
}

impl LaneLaw {
    pub const OPEN: LaneLaw = LaneLaw {
        writable: true,
        freedom: [true; 4],
        zone: [0.0, f64::INFINITY],
    };

    pub fn has_zone(&self) -> bool {
        self.zone[1].is_finite()
    }
}

#[derive(Clone)]
pub struct Frame {
    pub name: String,
    pub sr_hz: f64,
    pub provenance: String,
    pub words: String,
    pub lanes: [StageRoots; NUM_STAGES],
    pub laws: [LaneLaw; NUM_STAGES],
}

pub fn to_json(frame: &Frame) -> serde_json::Value {
    let lanes: Vec<serde_json::Value> = frame
        .lanes
        .iter()
        .map(|l| {
            serde_json::json!({
                "pole_hz": l.pole_hz,
                "pole_r": l.pole_r,
                "zero_hz": l.zero_hz,
                "zero_r": l.zero_r,
                "scale_db": 20.0 * l.scale.max(1e-9).log10(),
            })
        })
        .collect();
    let laws: Vec<serde_json::Value> = frame
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
    serde_json::json!({
        "schema": "trench-frame-v1",
        "name": frame.name,
        "sr_hz": frame.sr_hz,
        "provenance": frame.provenance,
        "words": frame.words,
        "lanes": lanes,
        "laws": laws,
    })
}

pub fn write(path: &Path, frame: &Frame) -> Result<(), String> {
    let text = serde_json::to_string_pretty(&to_json(frame))
        .map_err(|e| format!("{}: {e}", path.display()))?;
    std::fs::write(path, text).map_err(|e| format!("{}: {e}", path.display()))
}

pub fn read(path: &Path) -> Result<Frame, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let doc: serde_json::Value =
        serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
    if doc.get("schema").and_then(|v| v.as_str()) != Some("trench-frame-v1") {
        return Err(format!("{}: not a trench frame", path.display()));
    }
    let name = doc
        .get("name")
        .and_then(|v| v.as_str())
        .unwrap_or("unnamed")
        .to_string();
    let sr_hz = doc
        .get("sr_hz")
        .and_then(|v| v.as_f64())
        .ok_or_else(|| format!("{name}: no sr_hz"))?;
    let provenance = doc
        .get("provenance")
        .and_then(|v| v.as_str())
        .unwrap_or_default()
        .to_string();
    let words = doc
        .get("words")
        .and_then(|v| v.as_str())
        .unwrap_or_default()
        .to_string();
    let mut lanes = [StageRoots::IDENTITY; NUM_STAGES];
    let rows = doc
        .get("lanes")
        .and_then(|v| v.as_array())
        .ok_or_else(|| format!("{name}: no lanes"))?;
    for (si, row) in rows.iter().enumerate().take(NUM_STAGES) {
        let get = |key: &str| {
            row.get(key)
                .and_then(|v| v.as_f64())
                .ok_or_else(|| format!("{name}: lane {si} missing {key}"))
        };
        lanes[si] = StageRoots {
            pole_hz: get("pole_hz")?,
            pole_r: get("pole_r")?,
            zero_hz: get("zero_hz")?,
            zero_r: get("zero_r")?,
            scale: 10f64.powf(get("scale_db")? / 20.0),
        };
    }
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
    Ok(Frame {
        name,
        sr_hz,
        provenance,
        words,
        lanes,
        laws,
    })
}
