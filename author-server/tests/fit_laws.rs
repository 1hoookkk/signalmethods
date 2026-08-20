use serde_json::json;
use trench_core::stage_law::StageRoots;

use author_server::{fit, json};

fn mouth_curve() -> Vec<f64> {
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
    let dvtd = root.join("recipes").join("vocal").join("dvtd").join("subject-1");
    let (_, path) = author::envelope::scan(&dvtd)
        .into_iter()
        .find(|(name, _)| name.contains("bahn-tense-a"))
        .expect("subject-1 /a/ mouth");
    let raw = author::envelope::read_vvtf(&path).expect("vvtf");
    author::envelope::on_grid(&raw)
}

#[test]
fn cold_fit_lands_a_measured_mouth() {
    let curve = mouth_curve();
    let empty = [StageRoots::IDENTITY; 7];
    let req = json!({ "curve": curve, "lanes": json::lanes_to_value(&empty), "cold": true });
    let out = fit::fit(&req).expect("fit");
    let rms = out.get("target_rms_db").unwrap().as_f64().unwrap();
    assert!(rms < 4.0, "cold fit rms {rms}");
}

#[test]
fn pinned_skeleton_poles_do_not_move_and_held_lane_is_byte_identical() {
    let curve = mouth_curve();
    let empty = [StageRoots::IDENTITY; 7];
    let seeded = fit::skeleton(&json!({
        "curve": curve,
        "lanes": json::lanes_to_value(&empty),
    }))
    .expect("skeleton");
    let lanes = json::lanes_from_value(seeded.get("lanes").unwrap()).unwrap();
    let placed = seeded.get("placed").unwrap().as_u64().unwrap() as usize;
    assert!(placed >= 3);

    let mut laws: Vec<serde_json::Value> = (0..7)
        .map(|i| {
            if i < placed {
                json!({ "writable": true, "freedom": [false, true, true, true], "zone": null })
            } else {
                json!({ "writable": true, "freedom": [true, true, true, true], "zone": null })
            }
        })
        .collect();
    laws[placed - 1] = json!({ "writable": false });

    let out = fit::fit(&json!({
        "curve": curve,
        "lanes": json::lanes_to_value(&lanes),
        "laws": laws,
    }))
    .expect("fit");
    let fitted = json::lanes_from_value(out.get("lanes").unwrap()).unwrap();
    for i in 0..placed - 1 {
        assert_eq!(fitted[i].pole_hz, lanes[i].pole_hz, "pinned pole S{} moved", i + 1);
    }
    let held = placed - 1;
    assert_eq!(fitted[held].pole_hz, lanes[held].pole_hz);
    assert_eq!(fitted[held].pole_r, lanes[held].pole_r);
    assert_eq!(fitted[held].zero_hz, lanes[held].zero_hz);
    assert_eq!(fitted[held].zero_r, lanes[held].zero_r);
    assert_eq!(fitted[held].scale, lanes[held].scale, "held lane scale was rewritten");
    let rms = out.get("target_rms_db").unwrap().as_f64().unwrap();
    assert!(rms.is_finite() && rms < 15.0, "planned fit rms {rms}");
}
