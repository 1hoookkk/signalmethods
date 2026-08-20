use std::path::{Path, PathBuf};

use author_server::store::find_root;
use trench_app::{derived, library};
use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::{DEFAULT_AUTHORING_SR, P2K_DATUM_SR};

struct Corpus {
    dir: &'static str,
    ext: &'static str,
    datum_sr_hz: f64,
}

const CORPORA: [Corpus; 6] = [
    Corpus { dir: "recipes/hero", ext: "body", datum_sr_hz: P2K_DATUM_SR },
    Corpus { dir: "recipes/extrusions", ext: "body", datum_sr_hz: P2K_DATUM_SR },
    Corpus { dir: "ref/morpheus/bodies", ext: "body", datum_sr_hz: DEFAULT_AUTHORING_SR },
    Corpus { dir: "ref/presets", ext: "bin", datum_sr_hz: P2K_DATUM_SR },
    Corpus { dir: "ref/x3", ext: "bin", datum_sr_hz: P2K_DATUM_SR },
    Corpus { dir: "ref/md_templates", ext: "bin", datum_sr_hz: P2K_DATUM_SR },
];

fn files(root: &Path, corpus: &Corpus) -> Vec<PathBuf> {
    let mut paths: Vec<PathBuf> = std::fs::read_dir(root.join(corpus.dir))
        .unwrap_or_else(|e| panic!("{}: {e}", corpus.dir))
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|x| x == corpus.ext))
        .collect();
    paths.sort();
    paths
}

#[test]
fn derived_words_equal_the_file_bit_exactly() {
    let root = find_root().expect("repository root");
    let mut checked = 0usize;
    let mut cells = 0usize;
    let mut failures: Vec<String> = Vec::new();

    for corpus in &CORPORA {
        let paths = files(&root, corpus);
        assert!(!paths.is_empty(), "{} is empty", corpus.dir);
        for path in paths {
            let bytes = std::fs::read(&path).expect("read");
            let Ok(packed) = PackedCorners::from_body_bytes(&bytes) else {
                failures.push(format!("{}: not a body ({} bytes)", path.display(), bytes.len()));
                continue;
            };
            let body = library::from_packed(&packed, corpus.datum_sr_hz, None);
            let again = derived::pack(&body);
            checked += 1;
            for ci in 0..packed.words.len() {
                for si in 0..packed.words[ci].len() {
                    cells += 1;
                    if again.words[ci][si] != packed.words[ci][si] {
                        failures.push(format!(
                            "{} C{ci} S{si}: {:04X?} -> {:04X?}",
                            path.display(),
                            packed.words[ci][si],
                            again.words[ci][si],
                        ));
                    }
                }
            }
        }
    }

    assert!(checked > 0, "no bodies found");
    assert!(
        failures.is_empty(),
        "{} of {cells} cells across {checked} bodies did not round-trip:\n{}",
        failures.len(),
        failures.iter().take(20).cloned().collect::<Vec<_>>().join("\n"),
    );
    eprintln!("{checked} bodies, {cells} cells, bit-exact");
}
