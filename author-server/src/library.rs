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
        "poses": items(store.poses()),
        "scaffolds": items(store.scaffolds()),
        "frames": items(store.frames()),
        "architectures": items(store.architectures()),
        "bodies": items(store.bodies()),
    })
}
