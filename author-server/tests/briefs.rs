use author_server::{brief, store};

#[test]
fn every_architecture_resolves_to_a_nonempty_brief() {
    let s = store::Store::bare(std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join(".."));
    let unwritten = [10, 20, 21, 23, 24, 25, 26, 30, 32];
    for (i, item) in s.architectures().iter().enumerate() {
        let result = brief::brief(&s, &format!("id={}", item.id));
        if unwritten.contains(&i) {
            assert!(result.is_err(), "{}: INTENT gained an entry — update this list", item.id);
            continue;
        }
        let v = result.expect(&item.id);
        let intent = v["intent"].as_str().unwrap();
        assert!(intent.len() > 100, "{}: brief too short", item.id);
        assert!(intent.contains("RECIPE"), "{}: no RECIPE line", item.id);
    }
}
