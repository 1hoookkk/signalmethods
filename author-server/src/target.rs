use serde_json::{json, Value};
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::{RootPair, StageRoots};

use crate::json::lanes_to_value;
use crate::store::Store;

pub fn target(store: &Store, req: &Value) -> Result<Value, String> {
    let source = req.get("source").and_then(|s| s.as_str()).ok_or("no source")?;
    let id = req.get("id").and_then(|s| s.as_str()).ok_or("no id")?;
    if source == "stage" {
        return stage_target(store, req, id);
    }
    let path = store.resolve(id)?;
    match source {
        "mouth" => {
            let raw = author::envelope::read_vvtf(&path)?;
            let curve = author::envelope::on_grid(&raw);
            let grid = author::envelope::grid();
            let peaks = author::formants::peaks(&grid, &curve);
            let name = path
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or(id)
                .trim_end_matches("-vvtf-measured.txt")
                .to_string();
            Ok(json!({
                "name": name,
                "curve": curve,
                "peaks": peaks.iter().map(|p| json!({
                    "hz": p.hz, "db": p.db, "bandwidth_hz": p.bandwidth_hz
                })).collect::<Vec<_>>(),
            }))
        }
        "recording" => {
            let (samples, wav_sr) = author::wav::read_wav(&path)?;
            let slice_at = req.get("slice_at_seconds").and_then(|v| v.as_f64());
            let analysis = if let Some(seconds) = slice_at {
                let window = 4096usize;
                let center = (seconds * wav_sr).round().max(0.0) as usize;
                let start = center.saturating_sub(window / 2).min(samples.len().saturating_sub(window));
                &samples[start..(start + window).min(samples.len())]
            } else {
                &samples
            };
            let decoded = author::wav::envelope_from_samples(analysis, wav_sr)?;
            let grid = author::envelope::grid();
            let peaks = author::wav::formant_peaks(&grid, &decoded.curve);
            let ar = author::wav::ar_poles(analysis, wav_sr, 14);
            let name = path
                .file_stem()
                .and_then(|n| n.to_str())
                .unwrap_or(id)
                .to_string();
            Ok(json!({
                "name": name,
                "curve": decoded.curve,
                "f0_hz": decoded.f0_hz,
                "source_sr_hz": decoded.sample_rate_hz,
                "seconds": decoded.seconds,
                "source_seconds": samples.len() as f64 / wav_sr,
                "slice_at_seconds": slice_at,
                "peaks": peaks.iter().map(|p| json!({
                    "hz": p.hz, "db": p.db, "bandwidth_hz": p.bandwidth_hz
                })).collect::<Vec<_>>(),
                "ar_sections": ar.iter().map(|s| json!({
                    "pole_hz": s.pole_hz, "pole_r": s.pole_r,
                    "zero_hz": 0, "zero_r": 0
                })).collect::<Vec<_>>(),
            }))
        }
        "pose" => {
            let text = std::fs::read_to_string(&path).map_err(|e| e.to_string())?;
            let v: Value = serde_json::from_str(&text).map_err(|e| e.to_string())?;
            let name = v.get("name").and_then(|n| n.as_str()).unwrap_or(id).to_string();
            let ceiling = trench_core::stage_law::max_contiguous_pole_radius();
            let mut lanes = [StageRoots::IDENTITY; NUM_STAGES];
            let formants = v
                .get("formants")
                .and_then(|f| f.as_array())
                .ok_or("pose has no formants")?;
            let mut slot = 0;
            for f in formants.iter().take(NUM_STAGES) {
                let hz = f.get("hz").and_then(|x| x.as_f64()).ok_or("formant hz")?;
                let bw = f.get("bandwidth_hz").and_then(|x| x.as_f64()).ok_or("formant bandwidth_hz")?;
                if let Some((_, r)) =
                    trench_core::praat_endpoint::pole_from_frequency_bandwidth(hz, bw, crate::fit::SR)
                {
                    lanes[slot].pole_hz = hz;
                    lanes[slot].pole_r = r.min(ceiling);
                    lanes[slot].scale = 1.0;
                    slot += 1;
                }
            }
            if let Some(anti) = v.get("antiresonances").and_then(|a| a.as_array()) {
                for (i, a) in anti.iter().take(slot.max(1)).enumerate() {
                    let hz = a.get("hz").and_then(|x| x.as_f64()).unwrap_or(0.0);
                    let bw = a.get("bandwidth_hz").and_then(|x| x.as_f64()).unwrap_or(0.0);
                    if hz <= 0.0 || bw <= 0.0 {
                        continue;
                    }
                    if let Some((_, r)) =
                        trench_core::praat_endpoint::pole_from_frequency_bandwidth(hz, bw, crate::fit::SR)
                    {
                        lanes[i].zero_hz = hz;
                        lanes[i].zero_r = r;
                    }
                }
            }
            crate::fit::snap_to_words(&mut lanes);
            Ok(json!({ "name": name, "lanes": lanes_to_value(&lanes), "placed": slot }))
        }
        "body" => {
            body_target(&path, id)
        }
        _ => Err(format!("source {source} not implemented yet")),
    }
}

fn pair_value(pair: RootPair) -> Value {
    match pair {
        RootPair::Degenerate => json!({ "kind": "off" }),
        RootPair::Conjugate { hz, r } => json!({ "kind": "conj", "hz": hz, "r": r }),
        RootPair::RealPair { root_a, root_b } => json!({ "kind": "real", "pair": [root_a, root_b] }),
    }
}

fn stage_target(store: &Store, req: &Value, id: &str) -> Result<Value, String> {
    let stage = req.get("stage").and_then(|v| v.as_u64()).ok_or("no stage")? as usize;
    let (path, corners, stages, source_sr) = store.resolve_factory(id)?;
    if stage >= stages { return Err(format!("stage {} outside source", stage + 1)); }
    let packed = trench_core::minifloat::PackedCorners::from_body_bytes(&std::fs::read(path).map_err(|e| e.to_string())?)?;
    let mut cells = Vec::new();
    for ci in 0..corners {
        let words = packed.words[ci][stage];
        let g = trench_core::stage_law::geometry_from_words_at(words, source_sr);
        let root = |p: RootPair| match p { RootPair::Conjugate { hz, r } => Some((hz, r)), RootPair::Degenerate => Some((0.0, 0.0)), RootPair::RealPair { .. } => None };
        let lane = match (root(g.pole), root(g.zero)) {
            (Some((pole_hz, pole_r)), Some((zero_hz, zero_r))) => StageRoots { pole_hz, pole_r, zero_hz, zero_r, scale: g.scale },
            _ => StageRoots::IDENTITY,
        };
        cells.push(json!({ "corner": ci, "words": words, "lane": {
            "pole_hz": lane.pole_hz, "pole_r": lane.pole_r,
            "zero_hz": lane.zero_hz, "zero_r": lane.zero_r,
            "scale": lane.scale
        }, "geometry": {
            "pole": pair_value(g.pole),
            "zero": pair_value(g.zero),
            "scale": g.scale,
            "real_pair": matches!(g.pole, RootPair::RealPair { .. }) || matches!(g.zero, RootPair::RealPair { .. })
        }, "citation": format!("{id} / C{ci} / S{}", stage + 1) }));
    }
    Ok(json!({ "id": id, "stage": stage, "corner_count": corners, "stage_count": stages, "source_sr_hz": source_sr, "cells": cells }))
}

fn body_target(path: &std::path::Path, id: &str) -> Result<Value, String> {
    let bytes = std::fs::read(path).map_err(|e| e.to_string())?;
    let packed = trench_core::minifloat::PackedCorners::from_body_bytes(&bytes)?;
    let name = path.file_stem().and_then(|n| n.to_str()).unwrap_or(id).split_once('_').map(|x| x.1).unwrap_or(id).to_string();
    let mut corners = Vec::new();
    let mut skipped = 0;
    for corner in &packed.words {
        let mut lanes = [StageRoots::IDENTITY; NUM_STAGES];
        for (si, &w) in corner.iter().enumerate() {
            let g = trench_core::stage_law::geometry_from_words_at(w, crate::fit::SR);
            let c = |p: RootPair| match p {
                RootPair::Conjugate { hz, r } => Some((hz, r)),
                RootPair::Degenerate => Some((0.0, 0.0)),
                RootPair::RealPair { .. } => None,
            };
            match (c(g.pole), c(g.zero)) {
                (Some((ph, pr)), Some((zh, zr))) => lanes[si] = StageRoots { pole_hz: ph, pole_r: pr, zero_hz: zh, zero_r: zr, scale: g.scale },
                _ => skipped += 1,
            }
        }
        corners.push(lanes_to_value(&lanes));
    }
    Ok(json!({ "name": name, "corners": corners, "words": packed.words, "real_pair_sections_skipped": skipped }))
}

fn packed_words(req: &Value) -> Result<trench_core::minifloat::PackedCorners, String> {
    let words = serde_json::from_value(req.get("words").ok_or("no words")?.clone())
        .map_err(|e| format!("bad packed words: {e}"))?;
    Ok(trench_core::minifloat::PackedCorners { words })
}

pub fn audit_words(req: &Value) -> Result<Value, String> {
    let packed = packed_words(req)?;
    let (_, report) = crate::field::report(&packed, crate::fit::SR);
    Ok(report)
}

pub fn write_words(store: &Store, req: &Value) -> Result<Value, String> {
    let packed = packed_words(req)?;
    let (pass, report) = crate::field::report(&packed, crate::fit::SR);
    if !pass {
        return Err(format!("audit FAIL — {} — the body was not written", report["failures"].as_array().map(|a| a.iter().filter_map(|f| f.as_str()).collect::<Vec<_>>().join("; ")).unwrap_or_default()));
    }
    let square = req.get("kind").and_then(|k| k.as_str()) == Some("square");
    let dir = store.recipes().join("hero");
    std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    let mut n = 1;
    let path = loop {
        let name = if square { format!("cube-{n:02}.4.body") } else { format!("cube-{n:02}.body") };
        let path = dir.join(name);
        if !path.exists() { break path; }
        n += 1;
    };
    let bytes = packed.to_native_bytes();
    std::fs::write(&path, bytes).map_err(|e| e.to_string())?;
    Ok(json!({ "path": path.to_string_lossy(), "bytes": bytes.len(), "report": report }))
}
