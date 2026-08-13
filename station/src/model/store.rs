//! Station project persistence.
//!
//! The Station project format is explicitly the Station's own. It is not the
//! packed runtime body and does not pretend to be one: it can hold four axes
//! because the project model can, and writing it never implies the runtime can
//! load it. Import, project save and runtime export stay three separate acts.

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
    validate_shape(&p)?;
    Ok(p)
}

/// Structural checks a malformed file must not survive. These are shape
/// errors, not authoring opinions.
fn validate_shape(p: &Project) -> Result<(), String> {
    let corners = p.topology.corner_count();
    if p.frames.len() != corners {
        return Err(format!(
            "topology declares {corners} frames, file holds {}",
            p.frames.len()
        ));
    }
    for (i, f) in p.frames.iter().enumerate() {
        if f.address.len() != p.topology.axis_count() {
            return Err(format!(
                "frame {i} address has rank {}, topology declares {}",
                f.address.len(),
                p.topology.axis_count()
            ));
        }
        if p.topology.index_of(&f.address).is_none() {
            return Err(format!("frame {i} is not addressable in the topology"));
        }
        if f.values.len() != p.lanes.len() {
            return Err(format!(
                "frame {i} holds {} lane values, project declares {} lanes",
                f.values.len(),
                p.lanes.len()
            ));
        }
    }
    if !p.interp.sample_rate_hz.is_finite() || p.interp.sample_rate_hz <= 0.0 {
        return Err("interpolation sample rate is not a positive rate".into());
    }
    Ok(())
}

pub fn save(p: &Project, path: &Path) -> Result<(), String> {
    let text = to_json(p)?;
    std::fs::write(path, text).map_err(|e| format!("{}: {e}", path.display()))
}

pub fn load(path: &Path) -> Result<Project, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    from_json(&text)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::lane::LaneValue;
    use crate::model::project::Origin;
    use crate::model::topology::{Axis, Topology};

    fn factory() -> Vec<u8> {
        std::fs::read("../ref/presets/P2k_013_talking_hedz.bin").expect("factory preset")
    }

    #[test]
    fn imported_cube_survives_a_project_round_trip_exactly() {
        let p = Project::from_packed("hedz", &factory(), 44_100.0).unwrap();
        let back = from_json(&to_json(&p).unwrap()).unwrap();
        assert_eq!(p, back);
        // And the packed export is still byte-identical after the round trip.
        assert_eq!(p.to_packed().unwrap(), back.to_packed().unwrap());
    }

    #[test]
    fn a_four_axis_project_survives_a_round_trip() {
        let topo = Topology::new(vec![
            Axis::new("morph", "M"),
            Axis::new("q", "Q"),
            Axis::new("t2", "T"),
            Axis::new("t3", "W"),
        ]);
        let mut p = Project::blank("four", topo, 5);
        // Give it content, so the round trip is proving data and not defaults.
        p.frames[9].values[2] = LaneValue::from_words([0x1234, 0x2345, 0x3456, 0x4567, 0x5678]);
        p.origin = Origin::Native;
        let back = from_json(&to_json(&p).unwrap()).unwrap();
        assert_eq!(p, back);
        assert_eq!(back.frames.len(), 16);
        assert_eq!(back.topology.axis_count(), 4);
    }

    #[test]
    fn a_future_version_is_refused_rather_than_guessed_at() {
        let p = Project::blank("v", Topology::packed_runtime(), 7);
        let mut v: serde_json::Value = serde_json::from_str(&to_json(&p).unwrap()).unwrap();
        v["version"] = serde_json::json!(PROJECT_VERSION + 1);
        let err = from_json(&v.to_string()).unwrap_err();
        assert!(err.contains("not readable"), "{err}");
    }

    #[test]
    fn a_frame_count_that_contradicts_the_topology_is_refused() {
        let p = Project::blank("v", Topology::packed_runtime(), 7);
        let mut v: serde_json::Value = serde_json::from_str(&to_json(&p).unwrap()).unwrap();
        v["frames"].as_array_mut().unwrap().truncate(5);
        let err = from_json(&v.to_string()).unwrap_err();
        assert!(err.contains("declares 8 frames"), "{err}");
    }

    #[test]
    fn a_lane_count_that_contradicts_the_frames_is_refused() {
        let p = Project::blank("v", Topology::packed_runtime(), 7);
        let mut v: serde_json::Value = serde_json::from_str(&to_json(&p).unwrap()).unwrap();
        v["frames"][0]["values"].as_array_mut().unwrap().truncate(3);
        let err = from_json(&v.to_string()).unwrap_err();
        assert!(err.contains("lane values"), "{err}");
    }

    #[test]
    fn garbage_is_refused_safely() {
        assert!(from_json("not json at all").is_err());
        assert!(from_json("{}").is_err());
    }

    #[test]
    fn a_truncated_body_is_refused_rather_than_padded() {
        let mut raw = factory();
        raw.truncate(100);
        assert!(Project::from_packed("x", &raw, 44_100.0).is_err());
    }
}
