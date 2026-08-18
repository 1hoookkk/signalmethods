use crate::store::{Item, Store};
use serde_json::{json, Value};

fn items(list: Vec<Item>) -> Value {
    Value::Array(
        list.into_iter()
            .map(|i| json!({ "id": i.id, "name": i.name, "gloss": i.gloss }))
            .collect(),
    )
}

pub fn library(store: &Store) -> Value {
    json!({
        "root": store.root.to_string_lossy(),
        "pole_ceiling_r": trench_core::stage_law::max_contiguous_pole_radius(),
        "authoring_sr": crate::fit::SR,
        "mouths": items(store.mouths()),
        "recordings": items(store.recordings()),
        "poses": items(store.poses()),
        "scaffolds": items(store.scaffolds()),
        "frames": items(store.frames()),
        "architectures": items(store.architectures()),
        "bodies": items(store.bodies()),
        "stage_sources": store.stage_index,
        "vocabulary": store.vocabulary,
    })
}

fn read_json(path: std::path::PathBuf) -> Result<Value, String> {
    let text = std::fs::read_to_string(&path).map_err(|e| format!("{}: {e}", path.display()))?;
    serde_json::from_str(&text).map_err(|e| format!("{}: {e}", path.display()))
}

pub fn alphabet(store: &Store) -> Result<Value, String> {
    read_json(store.recipes().join("alphabet.json"))
}

pub fn census(store: &Store) -> Result<Value, String> {
    read_json(store.root.join("ref").join("stage_state_census.json"))
}
