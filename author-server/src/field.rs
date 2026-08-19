use serde_json::{json, Value};
use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, NUM_CORNERS};
use trench_core::stage_law::{geometry_from_words_at, words_from_roots_at, StageRoots};

use crate::fit::{row_db, SR};
use crate::json::{lane_from_value, lanes_to_value};
use crate::store::Store;

pub fn corners_from_value(v: &Value) -> Result<[[StageRoots; NUM_STAGES]; NUM_CORNERS], String> {
    let arr = v.as_array().ok_or("corners is not an array")?;
    if arr.len() != NUM_CORNERS {
        return Err(format!("corners must have {NUM_CORNERS} entries"));
    }
    let mut corners = [[StageRoots::IDENTITY; NUM_STAGES]; NUM_CORNERS];
    for (ci, corner) in arr.iter().enumerate() {
        let lanes = corner.as_array().ok_or("corner is not an array")?;
        if lanes.len() != NUM_STAGES {
            return Err(format!("corner must have {NUM_STAGES} lanes"));
        }
        for (si, lane) in lanes.iter().enumerate() {
            corners[ci][si] = lane_from_value(lane)?;
        }
    }
    Ok(corners)
}

pub fn pack(corners: &[[StageRoots; NUM_STAGES]; NUM_CORNERS], sr: f64) -> PackedCorners {
    let mut words = [[[0u16; NUM_COEFFS]; NUM_STAGES]; NUM_CORNERS];
    for ci in 0..NUM_CORNERS {
        for si in 0..NUM_STAGES {
            words[ci][si] = words_from_roots_at(&corners[ci][si], sr);
        }
    }
    PackedCorners { words }
}

pub fn report(packed: &PackedCorners, sr: f64) -> (bool, Value) {
    let audit = author::body::audit(packed, sr);
    let grid: Vec<f64> = (0..128)
        .map(|i| 40.0 * (16_000.0f64 / 40.0).powf(i as f64 / 127.0))
        .collect();
    let mut frames_crown = f64::NEG_INFINITY;
    for corner in &packed.words {
        let rows: Vec<[f64; 5]> = corner
            .iter()
            .map(|&w| geometry_from_words_at(w, sr).biquad_at(sr))
            .collect();
        for &hz in &grid {
            let db: f64 = rows.iter().map(|r| row_db(r, hz, sr)).sum();
            frames_crown = frames_crown.max(db);
        }
    }
    let relative_ok = audit.interior_crown_db - frames_crown <= 12.0;
    let pass = audit.pass() && relative_ok;
    let mut failures = audit.failures.clone();
    if !relative_ok {
        failures.push("crown exceeds the frames ceiling".into());
    }
    (
        pass,
        json!({
            "pass": pass,
            "failures": failures,
            "corner_crown_db": audit.corner_crown_db,
            "interior_crown_db": audit.interior_crown_db,
            "crown_min_db": audit.crown_min_db,
            "crown_max_db": audit.crown_max_db,
            "parity_db": audit.crown_max_db - audit.crown_min_db,
            "frames_crown_db": frames_crown,
            "relative_ok": relative_ok,
        }),
    )
}

pub fn words_frame(payload: &[u8], sr: f64) -> Option<Vec<u8>> {
    if payload.len() != 4 + NUM_STAGES * 20 {
        return None;
    }
    let mut reply = Vec::with_capacity(4 + NUM_STAGES * NUM_COEFFS * 2);
    reply.extend_from_slice(&payload[..4]);
    for si in 0..NUM_STAGES {
        let mut f = [0.0f32; 5];
        for (k, slot) in f.iter_mut().enumerate() {
            let at = 4 + si * 20 + k * 4;
            *slot = f32::from_le_bytes(payload[at..at + 4].try_into().ok()?);
        }
        let lane = StageRoots {
            pole_hz: f[0] as f64,
            pole_r: f[1] as f64,
            zero_hz: f[2] as f64,
            zero_r: f[3] as f64,
            scale: f[4] as f64,
        };
        for word in words_from_roots_at(&lane, sr) {
            reply.extend_from_slice(&word.to_le_bytes());
        }
    }
    Some(reply)
}

pub fn corners_words(req: &Value) -> Result<Value, String> {
    let corners = corners_from_value(req.get("corners").ok_or("no corners")?)?;
    let sr = req.get("sr").and_then(|x| x.as_f64()).unwrap_or(SR);
    Ok(json!({ "words": pack(&corners, sr).words }))
}

pub fn audit(req: &Value) -> Result<Value, String> {
    let corners = corners_from_value(req.get("corners").ok_or("no corners")?)?;
    let sr = req.get("sr").and_then(|x| x.as_f64()).unwrap_or(SR);
    let (_, rep) = report(&pack(&corners, sr), sr);
    Ok(rep)
}

pub fn write_body(store: &Store, req: &Value) -> Result<Value, String> {
    let corners = corners_from_value(req.get("corners").ok_or("no corners")?)?;
    let sr = req.get("sr").and_then(|x| x.as_f64()).unwrap_or(SR);
    let square = req.get("kind").and_then(|k| k.as_str()) == Some("square");
    let packed = pack(&corners, sr);
    let (pass, rep) = report(&packed, sr);
    if !pass {
        return Err(format!(
            "audit FAIL — {} — the body was not written",
            rep["failures"]
                .as_array()
                .map(|a| a.iter().filter_map(|f| f.as_str()).collect::<Vec<_>>().join("; "))
                .unwrap_or_default()
        ));
    }
    let dir = store.recipes().join("hero");
    std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    let mut n = 1;
    let path = loop {
        let name = if square {
            format!("cube-{n:02}.4.body")
        } else {
            format!("cube-{n:02}.body")
        };
        let p = dir.join(name);
        if !p.exists() {
            break p;
        }
        n += 1;
    };
    let bytes = packed.to_native_bytes().to_vec();
    std::fs::write(&path, &bytes).map_err(|e| e.to_string())?;
    Ok(json!({
        "path": path.to_string_lossy(),
        "bytes": bytes.len(),
        "report": rep,
    }))
}

pub fn write_frame(store: &Store, req: &Value) -> Result<Value, String> {
    let lanes = crate::json::lanes_from_value(req.get("lanes").ok_or("no lanes")?)?;
    let sr = req.get("sr").and_then(|x| x.as_f64()).unwrap_or(SR);
    let provenance = req
        .get("provenance")
        .and_then(|p| p.as_str())
        .unwrap_or("")
        .to_string();
    let (freedom, writable, _grow, zones) = crate::fit::laws_from_value(req.get("laws"))?;
    let mut laws = [author::frame::LaneLaw::OPEN; NUM_STAGES];
    for i in 0..NUM_STAGES {
        laws[i] = author::frame::LaneLaw {
            writable: writable[i],
            freedom: freedom[i],
            zone: zones[i],
        };
    }
    let dir = store.recipes().join("frames");
    std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    let mut n = 1;
    while dir.join(format!("frame-{n:03}.json")).exists() {
        n += 1;
    }
    let name = format!("frame-{n:03}");
    let path = dir.join(format!("{name}.json"));
    let frame = author::frame::Frame {
        name: name.clone(),
        sr_hz: sr,
        provenance,
        words: String::new(),
        lanes,
        laws,
    };
    author::frame::write(&path, &frame)?;
    Ok(json!({ "path": path.to_string_lossy(), "name": name }))
}

pub fn load_frame(store: &Store, req: &Value) -> Result<Value, String> {
    let id = req.get("id").and_then(|s| s.as_str()).ok_or("no id")?;
    let path = store.resolve(id)?;
    let f = author::frame::read(&path)?;
    let laws: Vec<Value> = f
        .laws
        .iter()
        .map(|w| {
            json!({
                "writable": w.writable,
                "freedom": w.freedom,
                "zone": if w.has_zone() { json!([w.zone[0], w.zone[1]]) } else { Value::Null },
            })
        })
        .collect();
    Ok(json!({
        "name": f.name,
        "provenance": f.provenance,
        "lanes": lanes_to_value(&f.lanes),
        "laws": laws,
        "sr_hz": f.sr_hz,
    }))
}
