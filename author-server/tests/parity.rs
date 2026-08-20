use trench_core::minifloat::PackedCorners;

use author_server::{fit, parity};

fn check(bytes: &[u8], label: &str) {
    let packed = PackedCorners::from_body_bytes(bytes).expect(label);
    let vectors = parity::vectors(&packed, 512, fit::SR);
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
    let out = root.join("target").join(format!("parity_{label}.json"));
    std::fs::write(&out, vectors.to_string()).expect("write vectors");
    let status = std::process::Command::new("node")
        .arg(root.join("workstation").join("tests").join("parity.mjs"))
        .arg(&out)
        .status()
        .expect("node not found — parity test needs node on PATH");
    assert!(status.success(), "{label}: js dsp.js diverged from trench-core");
}

#[test]
fn js_dsp_matches_trench_core_on_legacy_and_native_bodies() {
    let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
    let legacy = std::fs::read(root.join("ref/presets/P2k_013_talking_hedz.bin")).expect("legacy bin");
    check(&legacy, "legacy");
    let hero_dir = root.join("recipes").join("hero");
    let native = std::fs::read_dir(&hero_dir)
        .expect("hero dir")
        .flatten()
        .map(|e| e.path())
        .find(|p| p.extension().is_some_and(|x| x == "body") && std::fs::metadata(p).map(|m| m.len() == 560).unwrap_or(false))
        .expect("no native 560B hero body");
    check(&std::fs::read(native).unwrap(), "native");
}
