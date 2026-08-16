use serde_json::json;
use trench_core::stage_law::StageRoots;

#[path = "../src/field.rs"]
mod field;
#[path = "../src/fit.rs"]
mod fit;
#[path = "../src/json.rs"]
mod json;
#[path = "../src/store.rs"]
mod store;

fn corners_json(pole_r: f64) -> serde_json::Value {
    let mut lane = StageRoots::IDENTITY;
    lane.pole_hz = 1000.0;
    lane.pole_r = pole_r;
    lane.zero_hz = 8000.0;
    lane.zero_r = 0.5;
    lane.scale = 0.5;
    let mut lanes = [StageRoots::IDENTITY; 7];
    lanes[0] = lane;
    let corner = json::lanes_to_value(&lanes);
    serde_json::Value::Array((0..8).map(|_| corner.clone()).collect())
}

fn temp_store(tag: &str) -> store::Store {
    let root = std::env::temp_dir().join(format!("trench-gate-{tag}-{}", std::process::id()));
    std::fs::create_dir_all(root.join("recipes").join("hero")).unwrap();
    store::Store { root }
}

#[test]
fn failing_audit_refuses_and_writes_nothing() {
    let s = temp_store("fail");
    let req = json!({ "corners": corners_json(0.99995), "kind": "cube" });
    let err = field::write_body(&s, &req).expect_err("gate should refuse");
    assert!(err.contains("audit FAIL"), "{err}");
    let hero = s.root.join("recipes").join("hero");
    assert_eq!(std::fs::read_dir(&hero).unwrap().count(), 0, "a file was written");
    std::fs::remove_dir_all(&s.root).ok();
}

#[test]
fn passing_audit_writes_560_bytes_that_reload() {
    let s = temp_store("pass");
    let req = json!({ "corners": corners_json(0.95), "kind": "cube" });
    let out = field::write_body(&s, &req).expect("gate should pass");
    assert_eq!(out["bytes"].as_u64(), Some(560));
    let path = out["path"].as_str().unwrap();
    let bytes = std::fs::read(path).unwrap();
    assert_eq!(bytes.len(), 560);
    let reloaded = trench_core::minifloat::PackedCorners::from_body_bytes(&bytes).unwrap();
    let corners = field::corners_from_value(&corners_json(0.95)).unwrap();
    assert_eq!(reloaded, field::pack(&corners, fit::SR), "bytes do not round-trip");
    std::fs::remove_dir_all(&s.root).ok();
}
