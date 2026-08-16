use serde_json::{json, Value};

use crate::store::Store;

pub fn brief(store: &Store, query: &str) -> Result<Value, String> {
    let id = crate::http::query_param(query, "id").ok_or("no id")?;
    let file = id.rsplit('/').next().unwrap_or(id);
    let index: usize = file
        .strip_prefix("P2k_")
        .and_then(|s| s.get(..3))
        .and_then(|s| s.parse().ok())
        .ok_or("id is not a P2k architecture")?;
    let name = file
        .trim_end_matches(".json")
        .splitn(3, '_')
        .nth(2)
        .unwrap_or(file)
        .to_string();

    let intent_text = std::fs::read_to_string(store.recipes().join("briefs").join("INTENT.md"))
        .map_err(|e| format!("briefs/INTENT.md: {e}"))?;
    let marker = format!("**{index:02} ");
    let start = intent_text
        .lines()
        .position(|l| l.starts_with(&marker))
        .ok_or(format!("no INTENT entry for {index:02}"))?;
    let lines: Vec<&str> = intent_text.lines().collect();
    let mut section = Vec::new();
    for line in &lines[start..] {
        if !section.is_empty() && line.starts_with("**") {
            break;
        }
        section.push(*line);
    }
    let intent = section.join("\n").trim().to_string();

    let mut sentence = String::new();
    if let Ok(text) = std::fs::read_to_string(store.recipes().join("descriptors.json")) {
        if let Ok(v) = serde_json::from_str::<Value>(&text) {
            if let Some(filters) = v.get("filters").and_then(|f| f.as_array()) {
                let squashed = name.to_lowercase().replace(['-', '_', ' '], "");
                for f in filters {
                    let fname = f.get("name").and_then(|n| n.as_str()).unwrap_or("");
                    if fname.to_lowercase().replace(['-', '_', ' '], "") == squashed {
                        sentence = f
                            .get("sentence")
                            .and_then(|s| s.as_str())
                            .unwrap_or("")
                            .to_string();
                        break;
                    }
                }
            }
        }
    }

    Ok(json!({ "index": index, "name": name, "intent": intent, "sentence": sentence }))
}
