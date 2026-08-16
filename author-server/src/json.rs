use serde_json::{json, Value};
use trench_core::cascade::NUM_STAGES;
use trench_core::stage_law::StageRoots;

pub fn lane_to_value(l: &StageRoots) -> Value {
    json!({
        "pole_hz": l.pole_hz,
        "pole_r": l.pole_r,
        "zero_hz": l.zero_hz,
        "zero_r": l.zero_r,
        "scale": l.scale,
    })
}

pub fn lane_from_value(v: &Value) -> Result<StageRoots, String> {
    let f = |k: &str| v.get(k).and_then(|x| x.as_f64()).ok_or(format!("lane missing {k}"));
    Ok(StageRoots {
        pole_hz: f("pole_hz")?,
        pole_r: f("pole_r")?,
        zero_hz: f("zero_hz")?,
        zero_r: f("zero_r")?,
        scale: f("scale")?,
    })
}

pub fn lanes_to_value(lanes: &[StageRoots; NUM_STAGES]) -> Value {
    Value::Array(lanes.iter().map(lane_to_value).collect())
}

pub fn lanes_from_value(v: &Value) -> Result<[StageRoots; NUM_STAGES], String> {
    let arr = v.as_array().ok_or("lanes is not an array")?;
    if arr.len() != NUM_STAGES {
        return Err(format!("lanes must have {NUM_STAGES} entries"));
    }
    let mut lanes = [StageRoots::IDENTITY; NUM_STAGES];
    for (i, item) in arr.iter().enumerate() {
        lanes[i] = lane_from_value(item)?;
    }
    Ok(lanes)
}

pub fn curve_from_value(v: &Value) -> Result<Vec<f64>, String> {
    let arr = v.as_array().ok_or("curve is not an array")?;
    arr.iter()
        .map(|x| x.as_f64().ok_or("curve entry is not a number".to_string()))
        .collect()
}

pub fn parse(body: &[u8]) -> Result<Value, String> {
    serde_json::from_slice(body).map_err(|e| format!("bad json: {e}"))
}
