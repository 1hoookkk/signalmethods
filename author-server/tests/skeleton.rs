use serde_json::json;
use trench_core::stage_law::StageRoots;

#[path = "../src/fit.rs"]
mod fit;
#[path = "../src/json.rs"]
mod json;

#[test]
fn placing_the_skeleton_seats_measured_poles_and_nothing_else() {
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
    let dvtd = root.join("recipes").join("vocal").join("dvtd").join("subject-1");
    let (_, path) = author::envelope::scan(&dvtd)
        .into_iter()
        .find(|(name, _)| name.contains("bahn-tense-a"))
        .expect("subject-1 /a/ mouth");
    let raw = author::envelope::read_vvtf(&path).expect("vvtf");
    let curve = author::envelope::on_grid(&raw);
    let grid = author::envelope::grid();
    let peaks = author::formants::peaks(&grid, &curve);
    assert!(!peaks.is_empty());

    let empty = [StageRoots::IDENTITY; 7];
    let req = json!({
        "curve": curve,
        "lanes": json::lanes_to_value(&empty),
    });
    let out = fit::skeleton(&req).expect("skeleton");
    let lanes = json::lanes_from_value(out.get("lanes").unwrap()).unwrap();
    let placed = out.get("placed").unwrap().as_u64().unwrap() as usize;
    assert_eq!(placed, peaks.len().min(7));

    let ceiling = trench_core::stage_law::max_contiguous_pole_radius();
    for (lane, peak) in lanes.iter().zip(peaks.iter()) {
        assert!((lane.pole_hz / peak.hz).ln().abs() < 0.007, "pole off its formant");
        let (_, r) = trench_core::praat_endpoint::pole_from_frequency_bandwidth(
            peak.hz,
            peak.bandwidth_hz,
            fit::SR,
        )
        .unwrap();
        assert!((lane.pole_r - r.min(ceiling)).abs() < 5e-4, "radius off the praat law");
        assert_eq!(lane.zero_r, 0.0, "invented zero");
    }
    for lane in lanes.iter().skip(placed) {
        assert!(fit::lane_is_empty(lane));
    }
}
