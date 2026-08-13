//! Station project persistence.

use std::path::Path;

use super::project::{Project, PROJECT_VERSION};

pub const PROJECT_EXTENSION: &str = "station.json";

pub fn to_json(p: &Project) -> Result<String, String> {
    serde_json::to_string_pretty(p).map_err(|e| e.to_string())
}

pub fn from_json(text: &str) -> Result<Project, String> {
    let p: Project = serde_json::from_str(text).map_err(|e| format!("malformed project: {e}"))?;
    if p.version != PROJECT_VERSION {
        return Err(format!(
            "project version {} is not readable by this Station (expects {PROJECT_VERSION})",
            p.version
        ));
    }
    if let Some(o) = &p.object {
        let want = o.form.corner_count();
        if o.frames.len() != want {
            return Err(format!(
                "a {} holds {want} corners, file holds {}",
                o.form.name(),
                o.frames.len()
            ));
        }
        for (i, f) in o.frames.iter().enumerate() {
            if o.form.index_of(&f.address).is_none() {
                return Err(format!(
                    "corner {i} is not addressable in a {}",
                    o.form.name()
                ));
            }
            if f.values.len() != o.lanes.len() {
                return Err(format!(
                    "corner {i} holds {} sections, object declares {}",
                    f.values.len(),
                    o.lanes.len()
                ));
            }
        }
        if !o.sample_rate_hz.is_finite() || o.sample_rate_hz <= 0.0 {
            return Err("sample rate is not a positive rate".into());
        }
    }
    Ok(p)
}

pub fn save(p: &Project, path: &Path) -> Result<(), String> {
    std::fs::write(path, to_json(p)?).map_err(|e| format!("{}: {e}", path.display()))
}

pub fn load(path: &Path) -> Result<Project, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    from_json(&text)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::object::ObjectForm;

    fn factory() -> Vec<u8> {
        std::fs::read("../ref/presets/P2k_013_talking_hedz.bin").expect("factory preset")
    }

    #[test]
    fn an_empty_project_round_trips() {
        let p = Project::empty();
        let back = from_json(&to_json(&p).unwrap()).unwrap();
        assert_eq!(p, back);
        assert!(back.is_empty());
    }

    #[test]
    fn an_imported_object_round_trips_and_still_exports_identically() {
        let raw = factory();
        let p = Project::from_packed("f", &raw, 44_100.0).unwrap();
        let back = from_json(&to_json(&p).unwrap()).unwrap();
        assert_eq!(p, back);
        assert_eq!(back.to_body_bytes().unwrap(), raw);
    }

    #[test]
    fn both_forms_round_trip() {
        for form in [ObjectForm::Square, ObjectForm::Cube] {
            let p = Project::new_object("t", form, 44_100.0);
            let back = from_json(&to_json(&p).unwrap()).unwrap();
            assert_eq!(p, back);
            assert_eq!(back.form(), Some(form));
        }
    }

    #[test]
    fn a_future_version_is_refused() {
        let p = Project::empty();
        let mut v: serde_json::Value = serde_json::from_str(&to_json(&p).unwrap()).unwrap();
        v["version"] = serde_json::json!(PROJECT_VERSION + 1);
        assert!(from_json(&v.to_string())
            .unwrap_err()
            .contains("not readable"));
    }

    #[test]
    fn a_corner_count_that_contradicts_the_form_is_refused() {
        let p = Project::new_object("t", ObjectForm::Cube, 44_100.0);
        let mut v: serde_json::Value = serde_json::from_str(&to_json(&p).unwrap()).unwrap();
        v["object"]["frames"].as_array_mut().unwrap().truncate(5);
        let e = from_json(&v.to_string()).unwrap_err();
        assert!(e.contains("8 corners"), "{e}");
    }

    #[test]
    fn garbage_is_refused_safely() {
        assert!(from_json("not json").is_err());
        assert!(from_json("{}").is_err());
    }
}
