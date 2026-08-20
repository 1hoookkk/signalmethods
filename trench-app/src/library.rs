use std::path::{Path, PathBuf};

use author_server::store::{find_root, Store};
use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::{geometry_from_words_at, P2K_DATUM_SR};

use crate::document::{Body, Corner, Hold, Section};

pub struct Entry {
    pub id: String,
    pub name: String,
    pub path: PathBuf,
    pub datum_sr_hz: f64,
}

pub struct Library {
    pub root: PathBuf,
    pub bodies: Vec<Entry>,
    pub mouths: Vec<Entry>,
}

impl Library {
    pub fn open() -> Result<Self, String> {
        let root = find_root().ok_or("no recipes/vocal/dvtd found above cwd or exe")?;
        let store = Store::bare(root.clone());
        let bodies = store
            .bodies()
            .into_iter()
            .filter_map(|item| {
                let path = store.resolve(&item.id).ok()?;
                Some(Entry {
                    id: item.id,
                    name: item.name,
                    path,
                    datum_sr_hz: P2K_DATUM_SR,
                })
            })
            .collect();
        let mouths = store
            .mouths()
            .into_iter()
            .filter_map(|item| {
                let path = store.resolve(&item.id).ok()?;
                Some(Entry {
                    id: item.id,
                    name: item.name,
                    path,
                    datum_sr_hz: P2K_DATUM_SR,
                })
            })
            .collect();
        Ok(Self {
            root,
            bodies,
            mouths,
        })
    }
}

pub fn read_body(path: &Path, datum_sr_hz: f64, name: Option<String>) -> Result<Body, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let packed = PackedCorners::from_body_bytes(&bytes)
        .map_err(|e| format!("{}: {e}", path.display()))?;
    Ok(from_packed(&packed, datum_sr_hz, name))
}

pub fn from_packed(packed: &PackedCorners, datum_sr_hz: f64, name: Option<String>) -> Body {
    let corners = std::array::from_fn(|ci| Corner {
        sections: std::array::from_fn(|si| Section {
            geometry: geometry_from_words_at(packed.words[ci][si], datum_sr_hz),
            hold: Hold::FREE,
        }),
        target: None,
        provenance: None,
    });
    Body {
        corners,
        datum_sr_hz,
        name,
    }
}

pub fn read_mouth(path: &Path) -> Result<Vec<f64>, String> {
    let curve = author::envelope::read_vvtf(path)?;
    Ok(author::envelope::on_grid(&curve))
}
