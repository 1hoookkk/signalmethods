use serde_json::{json, Value};
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::{RootPair, StageRoots};

use crate::json::lanes_to_value;
use crate::store::Store;

pub fn target(store: &Store, req: &Value) -> Result<Value, String> {
    let source = req.get("source").and_then(|s| s.as_str()).ok_or("no source")?;
    let id = req.get("id").and_then(|s| s.as_str()).ok_or("no id")?;
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
            let bytes = std::fs::read(&path).map_err(|e| e.to_string())?;
            let packed = trench_core::minifloat::PackedCorners::from_body_bytes(&bytes)?;
            let name = path
                .file_stem()
                .and_then(|n| n.to_str())
                .unwrap_or(id)
                .to_string();
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
                        (Some((ph, pr)), Some((zh, zr))) => {
                            lanes[si] = StageRoots {
                                pole_hz: ph,
                                pole_r: pr,
                                zero_hz: zh,
                                zero_r: zr,
                                scale: g.scale,
                            };
                        }
                        _ => skipped += 1,
                    }
                }
                corners.push(lanes_to_value(&lanes));
            }
            Ok(json!({
                "name": name,
                "corners": corners,
                "words": packed.words,
                "real_pair_sections_skipped": skipped
            }))
        }
        _ => Err(format!("source {source} not implemented yet")),
    }
}
