use serde_json::{json, Value};
use trench_core::stage_law::{RootPair, StageGeometry};

pub const CORNERS: [&str; 4] = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"];

pub fn pair_to_json(p: &RootPair) -> Value {
    match p {
        RootPair::Conjugate { hz, r } => json!({ "hz": hz, "r": r }),
        RootPair::RealPair { root_a, root_b } => json!({ "pair": [root_a, root_b] }),
        RootPair::Degenerate => json!({ "pair": [0.0, 0.0] }),
    }
}

pub fn pair_from_json(v: &Value) -> Option<RootPair> {
    if let Some(pair) = v.get("pair").and_then(|p| p.as_array()) {
        return Some(RootPair::RealPair {
            root_a: pair.first()?.as_f64()?,
            root_b: pair.get(1)?.as_f64()?,
        });
    }
    Some(RootPair::Conjugate {
        hz: v.get("hz")?.as_f64()?,
        r: v.get("r")?.as_f64()?,
    })
}

pub fn corner_to_json(g: &StageGeometry) -> Value {
    json!({
        "pole": pair_to_json(&g.pole),
        "zero": pair_to_json(&g.zero),
        "scale": g.scale,
        "scale_db": (2000.0 * g.scale.max(1e-12).log10()).round() / 100.0,
    })
}

pub fn corner_geometry(section: &Value, corner: &str) -> Option<StageGeometry> {
    let c = section.get("corners")?.get(corner)?;
    Some(StageGeometry {
        pole: pair_from_json(c.get("pole")?)?,
        zero: pair_from_json(c.get("zero")?)?,
        scale: c.get("scale")?.as_f64()?,
    })
}
