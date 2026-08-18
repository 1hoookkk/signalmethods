use serde_json::{json, Value};
use std::collections::HashMap;
use std::path::Path;
use trench_core::stage_law::{geometry_from_words_at, RootPair};

pub const TYPE_ORDER: [&str; 10] = [
    "LPF", "EQ+", "EQ-", "VOW", "PHA", "FLG", "REZ", "WAH", "DST", "SFX",
];

const FREQ_CENTS: f64 = 10.0;
const WIDTH_CENTS: f64 = 85.0;
const REAL_BUCKET: f64 = 5e-4;

fn normalize(name: &str) -> String {
    name.chars()
        .filter(|c| c.is_ascii_alphanumeric())
        .map(|c| c.to_ascii_lowercase())
        .collect()
}

struct Descriptor {
    kind: String,
    order: i64,
    sentence: String,
}

fn descriptors(root: &Path) -> HashMap<String, Descriptor> {
    let mut out = HashMap::new();
    let path = root.join("recipes").join("descriptors.json");
    let Ok(text) = std::fs::read_to_string(path) else {
        return out;
    };
    let Ok(value) = serde_json::from_str::<Value>(&text) else {
        return out;
    };
    let Some(filters) = value.get("filters").and_then(Value::as_array) else {
        return out;
    };
    for filter in filters {
        let Some(name) = filter.get("name").and_then(Value::as_str) else {
            continue;
        };
        let Some(kind) = filter.get("type").and_then(Value::as_str) else {
            continue;
        };
        out.insert(
            normalize(name),
            Descriptor {
                kind: kind.to_string(),
                order: filter.get("order").and_then(Value::as_i64).unwrap_or(0),
                sentence: filter
                    .get("sentence")
                    .and_then(Value::as_str)
                    .unwrap_or("")
                    .to_string(),
            },
        );
    }
    out
}

fn bandwidth_hz(r: f64, sample_rate_hz: f64) -> f64 {
    if r <= 0.0 {
        return 0.0;
    }
    if r >= 1.0 {
        return 0.01;
    }
    ((-r.ln()) * sample_rate_hz / std::f64::consts::PI).max(0.01)
}

fn cents_bucket(hz: f64, resolution_cents: f64) -> i64 {
    ((1200.0 * hz.max(1e-6).log2()) / resolution_cents).round() as i64
}

fn pair_key(pair: RootPair, sample_rate_hz: f64) -> (u8, i64, i64) {
    match pair {
        RootPair::Degenerate => (0, 0, 0),
        RootPair::RealPair { root_a, root_b } => (
            1,
            (root_a / REAL_BUCKET).round() as i64,
            (root_b / REAL_BUCKET).round() as i64,
        ),
        RootPair::Conjugate { hz, r } => {
            if hz <= 0.0 || r <= 0.0 {
                (0, 0, 0)
            } else {
                (
                    2,
                    cents_bucket(hz, FREQ_CENTS),
                    cents_bucket(bandwidth_hz(r, sample_rate_hz), WIDTH_CENTS),
                )
            }
        }
    }
}

fn pair_json(pair: RootPair, sample_rate_hz: f64) -> Value {
    match pair {
        RootPair::Degenerate => json!({ "kind": "off" }),
        RootPair::RealPair { root_a, root_b } => {
            json!({ "kind": "real", "pair": [root_a, root_b] })
        }
        RootPair::Conjugate { hz, r } => json!({
            "kind": "conj",
            "hz": hz,
            "r": r,
            "bw_hz": bandwidth_hz(r, sample_rate_hz),
        }),
    }
}

fn is_off(pair: RootPair) -> bool {
    match pair {
        RootPair::Degenerate => true,
        RootPair::RealPair { .. } => false,
        RootPair::Conjugate { hz, r } => hz <= 0.0 || r <= 0.0,
    }
}

struct Cluster {
    stage: usize,
    words: HashMap<[u16; 5], usize>,
    seats: Vec<(String, usize, usize, [u16; 5])>,
    objects: Vec<String>,
    types: HashMap<String, usize>,
    sample_rate_hz: f64,
}

pub fn build(root: &Path, stage_index: &Value) -> Value {
    let table = descriptors(root);
    let empty = Vec::new();
    let sources = stage_index.as_array().unwrap_or(&empty);

    let mut members: HashMap<String, Vec<Value>> = HashMap::new();
    let mut untyped = Vec::new();
    let mut clusters: HashMap<((u8, i64, i64), (u8, i64, i64), usize), Cluster> = HashMap::new();
    let mut stages_seen = 0usize;

    for source in sources {
        if source.get("family").and_then(Value::as_str) != Some("p2k") {
            continue;
        }
        let id = source.get("id").and_then(Value::as_str).unwrap_or("");
        let name = source.get("name").and_then(Value::as_str).unwrap_or("");
        let sample_rate_hz = source
            .get("source_sr_hz")
            .and_then(Value::as_f64)
            .unwrap_or(trench_core::stage_law::DEFAULT_AUTHORING_SR);

        let kind = match table.get(&normalize(name)) {
            Some(descriptor) => {
                members
                    .entry(descriptor.kind.clone())
                    .or_default()
                    .push(json!({
                        "id": id,
                        "name": name,
                        "order": descriptor.order,
                        "sentence": descriptor.sentence,
                    }));
                Some(descriptor.kind.clone())
            }
            None => {
                untyped.push(name.to_string());
                None
            }
        };

        let Some(tracks) = source.get("tracks").and_then(Value::as_array) else {
            continue;
        };
        stages_seen = stages_seen.max(tracks.len());
        for (stage, corners) in tracks.iter().enumerate() {
            let Some(corners) = corners.as_array() else {
                continue;
            };
            for (corner, packed) in corners.iter().enumerate() {
                let Some(list) = packed.as_array() else {
                    continue;
                };
                if list.len() != 5 {
                    continue;
                }
                let mut words = [0u16; 5];
                for (slot, value) in list.iter().enumerate() {
                    words[slot] = value.as_u64().unwrap_or(0) as u16;
                }
                let geometry = geometry_from_words_at(words, sample_rate_hz);
                if is_off(geometry.pole) && is_off(geometry.zero) {
                    continue;
                }
                let key = (
                    pair_key(geometry.pole, sample_rate_hz),
                    pair_key(geometry.zero, sample_rate_hz),
                    stage,
                );
                let cluster = clusters.entry(key).or_insert_with(|| Cluster {
                    stage,
                    words: HashMap::new(),
                    seats: Vec::new(),
                    objects: Vec::new(),
                    types: HashMap::new(),
                    sample_rate_hz,
                });
                *cluster.words.entry(words).or_insert(0) += 1;
                cluster.seats.push((id.to_string(), corner, stage, words));
                if let Some(kind) = &kind {
                    *cluster.types.entry(kind.clone()).or_insert(0) += 1;
                }
                if !cluster.objects.iter().any(|held| held == id) {
                    cluster.objects.push(id.to_string());
                }
            }
        }
    }

    let mut states: Vec<Value> = clusters
        .into_values()
        .map(|cluster| {
            let mut ranked: Vec<(&[u16; 5], &usize)> = cluster.words.iter().collect();
            ranked.sort_by(|a, b| b.1.cmp(a.1).then(a.0.cmp(b.0)));
            let representative = *ranked[0].0;
            let seat = cluster
                .seats
                .iter()
                .find(|seat| seat.3 == representative)
                .cloned()
                .unwrap_or_default();
            let geometry = geometry_from_words_at(representative, cluster.sample_rate_hz);
            json!({
                "stage": cluster.stage,
                "count": cluster.seats.len(),
                "objects": cluster.objects.len(),
                "types": cluster.types,
                "words": representative,
                "pole": pair_json(geometry.pole, cluster.sample_rate_hz),
                "zero": pair_json(geometry.zero, cluster.sample_rate_hz),
                "scale": geometry.scale,
                "scale_db": 20.0 * geometry.scale.max(1e-9).log10(),
                "seat": { "id": seat.0, "corner": seat.1, "stage": seat.2 },
            })
        })
        .collect();
    states.sort_by(|a, b| {
        let stage = a["stage"].as_u64().cmp(&b["stage"].as_u64());
        let count = b["count"].as_u64().cmp(&a["count"].as_u64());
        stage.then(count)
    });

    let templates: Vec<Value> = TYPE_ORDER
        .iter()
        .map(|kind| {
            let mut list = members.remove(*kind).unwrap_or_default();
            list.sort_by(|a, b| a["id"].as_str().cmp(&b["id"].as_str()));
            json!({
                "type": kind,
                "template": list.first().cloned(),
                "members": list,
            })
        })
        .collect();

    json!({
        "type_order": TYPE_ORDER,
        "templates": templates,
        "stage_count": stages_seen,
        "states": states,
        "untyped": untyped,
        "tolerance": {
            "freq_cents": FREQ_CENTS,
            "width_cents": WIDTH_CENTS,
            "real_bucket": REAL_BUCKET,
        },
    })
}
