use std::path::Path;

use serde_json::{json, Value};
use trench_core::cascade::NUM_STAGES;
use trench_core::minifloat::{PackedCorners, IDENTITY_STAGE, NUM_CORNERS};
use trench_core::stage_law::{
    geometry_from_words_at, words_from_geometry_at, RootPair, StageGeometry,
};

use crate::recipes::{pair_from_json, pair_to_json, CORNERS};

pub const DESIGN_SR: f64 = 39_062.5;
const AXES: [(&str, usize); 3] = [("M", 1), ("Q", 2), ("T", 4)];

pub struct Alphabet {
    doc: Value,
}

impl Alphabet {
    pub fn load(path: &Path) -> Result<Self, String> {
        let text =
            std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
        let doc = serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
        Ok(Self { doc })
    }

    pub fn empty() -> Self {
        Self { doc: json!({}) }
    }

    fn number(&self, book: &str, name: &str) -> Result<f64, String> {
        self.doc
            .get(book)
            .and_then(|b| b.get(name))
            .and_then(|v| v.as_f64())
            .ok_or_else(|| format!("the alphabet has no {book} entry named \"{name}\""))
    }

    fn anchor(&self, name: &str) -> Result<RootPair, String> {
        let v = self
            .doc
            .get("anchors")
            .and_then(|b| b.get(name))
            .ok_or_else(|| format!("the alphabet has no anchor named \"{name}\""))?;
        pair_from_json(v).ok_or_else(|| format!("anchor \"{name}\" is not a root"))
    }
}

fn resolve_number(v: &Value, alphabet: &Alphabet, book: &str) -> Result<f64, String> {
    if let Some(n) = v.as_f64() {
        return Ok(n);
    }
    if let Some(s) = v.as_str() {
        if let Some(name) = s.strip_prefix('@') {
            return alphabet.number(book, name);
        }
    }
    Err(format!("expected a number or \"@name\", found {v}"))
}

fn resolve_root(v: &Value, alphabet: &Alphabet) -> Result<RootPair, String> {
    if let Some(s) = v.as_str() {
        if let Some(name) = s.strip_prefix('@') {
            return alphabet.anchor(name);
        }
        return Err(format!("expected a root or \"@name\", found \"{s}\""));
    }
    if let Some(pair) = v.get("pair").and_then(|p| p.as_array()) {
        return Ok(RootPair::RealPair {
            root_a: pair
                .first()
                .and_then(|x| x.as_f64())
                .ok_or("real pair needs two numbers")?,
            root_b: pair
                .get(1)
                .and_then(|x| x.as_f64())
                .ok_or("real pair needs two numbers")?,
        });
    }
    let hz = resolve_number(v.get("hz").ok_or("root needs hz")?, alphabet, "radii")
        .or_else(|_| v.get("hz").and_then(|x| x.as_f64()).ok_or("hz".to_string()))?;
    let r = resolve_number(v.get("r").ok_or("root needs r")?, alphabet, "radii")?;
    Ok(RootPair::Conjugate { hz, r })
}

#[derive(Clone, Copy, Default)]
struct Move {
    pole_st: f64,
    zero_st: f64,
    pole_r_to: Option<f64>,
    zero_r_to: Option<f64>,
}

fn resolve_move(v: &Value, alphabet: &Alphabet) -> Result<Move, String> {
    let mut m = Move::default();
    if let Some(x) = v.get("pole_st") {
        m.pole_st = resolve_number(x, alphabet, "carves_st")?;
    }
    if let Some(x) = v.get("zero_st") {
        m.zero_st = resolve_number(x, alphabet, "carves_st")?;
    }
    if let Some(x) = v.get("pole_r_to") {
        m.pole_r_to = Some(resolve_number(x, alphabet, "radii")?);
    }
    if let Some(x) = v.get("zero_r_to") {
        m.zero_r_to = Some(resolve_number(x, alphabet, "radii")?);
    }
    Ok(m)
}

fn shift(pair: RootPair, st: f64, r_to: Option<f64>) -> RootPair {
    match pair {
        RootPair::Conjugate { hz, r } => RootPair::Conjugate {
            hz: hz * 2f64.powf(st / 12.0),
            r: r_to.unwrap_or(r),
        },
        other => other,
    }
}

pub struct Design {
    pub name: String,
    pub is_square: bool,
    pub corners: [[StageGeometry; NUM_STAGES]; NUM_CORNERS],
}

pub fn compile(path: &Path, alphabet: &Alphabet) -> Result<Design, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let doc: Value =
        serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))?;
    if doc.get("schema").and_then(|v| v.as_str()) != Some("trench-design-v1") {
        return Err(format!("{}: not a trench design", path.display()));
    }
    let name = doc
        .get("name")
        .and_then(|v| v.as_str())
        .unwrap_or("unnamed")
        .to_string();
    let lanes = doc
        .get("lanes")
        .and_then(|v| v.as_array())
        .ok_or_else(|| format!("{name}: no lanes"))?;

    let mut corners = [[StageGeometry::IDENTITY; NUM_STAGES]; NUM_CORNERS];
    let mut any_t = false;
    for (li, lane) in lanes.iter().enumerate().take(NUM_STAGES) {
        let slot = lane
            .get("slot")
            .and_then(|v| v.as_u64())
            .map(|s| s as usize - 1)
            .unwrap_or(li);
        if slot >= NUM_STAGES {
            return Err(format!("{name}: slot {} is out of the cascade", slot + 1));
        }
        let where_ = |e: String| format!("{name}: lane {}: {e}", slot + 1);

        if let Some(over) = lane.get("corners") {
            for (ci, _) in CORNERS.iter().enumerate() {
                for t in 0..2 {
                    let key = format!("{}_T{}", CORNERS[ci], t * 100);
                    let v = over
                        .get(&key)
                        .or_else(|| if t == 0 { over.get(CORNERS[ci]) } else { None });
                    if let Some(v) = v {
                        let g = StageGeometry {
                            pole: resolve_root(v.get("pole").ok_or("pole").map_err(|e| where_(e.into()))?, alphabet)
                                .map_err(where_)?,
                            zero: resolve_root(v.get("zero").ok_or("zero").map_err(|e| where_(e.into()))?, alphabet)
                                .map_err(where_)?,
                            scale: v.get("scale").and_then(|s| s.as_f64()).unwrap_or_else(|| {
                                10f64.powf(
                                    v.get("scale_db").and_then(|s| s.as_f64()).unwrap_or(0.0)
                                        / 20.0,
                                )
                            }),
                        };
                        corners[ci + 4 * t][slot] = g;
                        if t == 1 && corners[ci][slot] != g {
                            any_t = true;
                        }
                    } else if t == 1 {
                        corners[ci + 4][slot] = corners[ci][slot];
                    }
                }
            }
            continue;
        }

        let pole = resolve_root(lane.get("pole").ok_or_else(|| where_("no pole".into()))?, alphabet)
            .map_err(where_)?;
        let zero_spec = lane.get("zero").ok_or_else(|| where_("no zero".into()))?;
        let tied_interval = zero_spec
            .get("interval_st")
            .map(|x| resolve_number(x, alphabet, "carves_st"))
            .transpose()
            .map_err(where_)?;
        let zero = if let Some(interval) = tied_interval {
            let r = resolve_number(
                zero_spec.get("r").ok_or_else(|| where_("tied zero needs r".into()))?,
                alphabet,
                "radii",
            )
            .map_err(where_)?;
            match pole {
                RootPair::Conjugate { hz, .. } => RootPair::Conjugate {
                    hz: hz * 2f64.powf(interval / 12.0),
                    r,
                },
                _ => return Err(where_("a tied zero needs a conjugate pole".into())),
            }
        } else {
            resolve_root(zero_spec, alphabet).map_err(where_)?
        };
        let scale = 10f64.powf(
            lane.get("scale_db")
                .map(|x| resolve_number(x, alphabet, "radii"))
                .transpose()
                .map_err(where_)?
                .unwrap_or(0.0)
                / 20.0,
        );

        let moves = lane.get("moves").cloned().unwrap_or_else(|| json!({}));
        let mut axis_moves = [Move::default(); 3];
        for (ai, (axis, _)) in AXES.iter().enumerate() {
            if let Some(v) = moves.get(*axis) {
                axis_moves[ai] = resolve_move(v, alphabet).map_err(where_)?;
                if *axis == "T" {
                    any_t = true;
                }
            }
        }

        for ci in 0..NUM_CORNERS {
            let mut pole_c = pole;
            let mut zero_c = zero;
            for (ai, (_, bit)) in AXES.iter().enumerate() {
                if ci & bit != 0 {
                    let m = axis_moves[ai];
                    pole_c = shift(pole_c, m.pole_st, m.pole_r_to);
                    zero_c = if let Some(interval) = tied_interval {
                        match (pole_c, zero_c) {
                            (RootPair::Conjugate { hz, .. }, RootPair::Conjugate { r, .. }) => {
                                RootPair::Conjugate {
                                    hz: hz * 2f64.powf(interval / 12.0),
                                    r: m.zero_r_to.unwrap_or(r),
                                }
                            }
                            _ => zero_c,
                        }
                    } else {
                        shift(zero_c, m.zero_st, m.zero_r_to)
                    };
                }
            }
            corners[ci][slot] = StageGeometry {
                pole: pole_c,
                zero: zero_c,
                scale,
            };
        }
    }

    Ok(Design {
        name,
        is_square: !any_t,
        corners,
    })
}

pub fn pack(design: &Design) -> PackedCorners {
    let mut words = [[IDENTITY_STAGE; NUM_STAGES]; NUM_CORNERS];
    for ci in 0..NUM_CORNERS {
        for si in 0..NUM_STAGES {
            words[ci][si] = words_from_geometry_at(&design.corners[ci][si], DESIGN_SR);
        }
    }
    PackedCorners { words }
}

pub fn decompile(packed: &PackedCorners, name: &str) -> Value {
    let corners: [[StageGeometry; NUM_STAGES]; NUM_CORNERS] = std::array::from_fn(|ci| {
        std::array::from_fn(|si| geometry_from_words_at(packed.words[ci][si], DESIGN_SR))
    });
    let is_square = (0..4).all(|ci| {
        (0..NUM_STAGES).all(|si| packed.words[ci][si] == packed.words[ci + 4][si])
    });
    let mut lanes = Vec::new();
    for si in 0..NUM_STAGES {
        if (0..NUM_CORNERS).all(|ci| corners[ci][si] == StageGeometry::IDENTITY) {
            continue;
        }
        let mut over = serde_json::Map::new();
        for t in 0..if is_square { 1 } else { 2 } {
            for (ci, corner) in CORNERS.iter().enumerate() {
                let g = &corners[ci + 4 * t][si];
                let key = if is_square {
                    corner.to_string()
                } else {
                    format!("{corner}_T{}", t * 100)
                };
                over.insert(
                    key,
                    json!({
                        "pole": pair_to_json(&g.pole),
                        "zero": pair_to_json(&g.zero),
                        "scale": g.scale,
                    }),
                );
            }
        }
        lanes.push(json!({ "slot": si + 1, "corners": over }));
    }
    json!({
        "schema": "trench-design-v1",
        "name": name,
        "note": "decompiled corners are verbatim; replace literals with @names from your alphabet, or restate a lane as anchor + moves",
        "lanes": lanes,
    })
}

pub fn verify_null(design_path: &Path, alphabet: &Alphabet, original: &PackedCorners) -> Result<bool, String> {
    let design = compile(design_path, alphabet)?;
    Ok(pack(&design).words == original.words)
}

pub fn write_body(design: &Design, out: &Path) -> Result<String, String> {
    let packed = pack(design);
    let audit = crate::body::audit(&packed, DESIGN_SR);
    if !audit.pass() {
        return Err(format!(
            "{}: audit FAIL ({}) — not written",
            design.name,
            audit.failures.join("; ")
        ));
    }
    let (bytes, kind): (Vec<u8>, &str) = match packed.to_legacy_bytes() {
        Some(b) if design.is_square => (b.to_vec(), "240-byte legacy"),
        _ => (packed.to_native_bytes().to_vec(), "560-byte native"),
    };
    std::fs::write(out, bytes).map_err(|e| format!("{}: {e}", out.display()))?;
    Ok(format!(
        "{} · crown {:.1}..{:.1} dB · parity {:.1} · PASS · {kind}",
        design.name, audit.crown_min_db, audit.crown_max_db, audit.parity_db
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_named_design_compiles_deterministically_and_moves_compose() {
        let alphabet = Alphabet {
            doc: json!({
                "radii": { "tight": 0.97, "soft": 0.6 },
                "carves_st": { "up-a-fifth": 7.0 },
                "anchors": { "chest": { "hz": 200.0, "r": 0.98 } }
            }),
        };
        let design_doc = json!({
            "schema": "trench-design-v1",
            "name": "test",
            "lanes": [
                { "slot": 1, "pole": "@chest", "zero": { "hz": 6000.0, "r": "@soft" },
                  "moves": { "M": { "pole_st": 12.0 } } },
                { "slot": 2, "pole": { "hz": 900.0, "r": "@tight" },
                  "zero": { "interval_st": "@up-a-fifth", "r": 0.9 },
                  "moves": { "M": { "pole_st": 12.0 }, "Q": { "pole_r_to": 0.99 } } }
            ]
        });
        let dir = std::env::temp_dir().join("trench-design-test.json");
        std::fs::write(&dir, serde_json::to_string(&design_doc).unwrap()).unwrap();
        let design = compile(&dir, &alphabet).unwrap();
        assert!(design.is_square);

        let anchor = design.corners[0][1];
        let m1 = design.corners[1][1];
        let m1q1 = design.corners[3][1];
        let (RootPair::Conjugate { hz: hz0, .. }, RootPair::Conjugate { hz: hz1, r: r1 }) =
            (anchor.pole, m1.pole)
        else {
            panic!("conjugate poles expected");
        };
        assert!((hz1 / hz0 - 2.0).abs() < 1e-12, "M travel is one octave");
        assert!((r1 - 0.97).abs() < 1e-12, "M leaves the radius alone");
        let RootPair::Conjugate { hz: hzq, r: rq } = m1q1.pole else {
            panic!()
        };
        assert!((rq - 0.99).abs() < 1e-12, "Q lifts the radius");
        assert!((hzq / hz0 - 2.0).abs() < 1e-12, "moves compose per axis");
        let (RootPair::Conjugate { hz: phz, .. }, RootPair::Conjugate { hz: zhz, .. }) =
            (m1q1.pole, m1q1.zero)
        else {
            panic!()
        };
        assert!(
            (12.0 * (zhz / phz).log2() - 7.0).abs() < 1e-9,
            "the tied zero rides its pole at the named interval"
        );

        let missing = json!({
            "schema": "trench-design-v1", "name": "bad",
            "lanes": [ { "slot": 1, "pole": "@nowhere", "zero": { "hz": 100.0, "r": 0.5 } } ]
        });
        std::fs::write(&dir, serde_json::to_string(&missing).unwrap()).unwrap();
        let Err(err) = compile(&dir, &alphabet).map(|_| ()) else {
            panic!("a design with an unknown name must not compile");
        };
        assert!(err.contains("nowhere"), "unresolved names fail loudly: {err}");
    }

    #[test]
    fn decompile_recompile_nulls() {
        let alphabet = Alphabet::empty();
        let mut words = [[IDENTITY_STAGE; NUM_STAGES]; NUM_CORNERS];
        let stage = StageGeometry {
            pole: RootPair::Conjugate { hz: 900.0, r: 0.97 },
            zero: RootPair::RealPair { root_a: 0.8, root_b: -0.3 },
            scale: 0.5,
        };
        for ci in 0..NUM_CORNERS {
            words[ci][0] = words_from_geometry_at(&stage, DESIGN_SR);
            words[ci][1] = words_from_geometry_at(
                &StageGeometry {
                    pole: RootPair::Conjugate { hz: 400.0 * (ci + 1) as f64, r: 0.9 },
                    zero: RootPair::Conjugate { hz: 5_000.0, r: 0.7 },
                    scale: 1.0,
                },
                DESIGN_SR,
            );
        }
        let original = PackedCorners { words };
        let doc = decompile(&original, "roundtrip");
        let path = std::env::temp_dir().join("trench-design-roundtrip.json");
        std::fs::write(&path, serde_json::to_string(&doc).unwrap()).unwrap();
        assert!(
            verify_null(&path, &alphabet, &original).unwrap(),
            "a decompiled design must recompile to the same bytes"
        );
    }
}
