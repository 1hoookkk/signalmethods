use std::path::{Path, PathBuf};

pub struct Store {
    pub root: PathBuf,
    pub stage_index: serde_json::Value,
    pub vocabulary: serde_json::Value,
}

pub struct Item {
    pub id: String,
    pub name: String,
    pub gloss: String,
}

pub fn find_root() -> Option<PathBuf> {
    let mut bases = Vec::new();
    if let Ok(d) = std::env::current_dir() {
        bases.push(d);
    }
    if let Ok(e) = std::env::current_exe() {
        bases.push(e);
    }
    for base in bases {
        for a in base.ancestors() {
            if a.join("recipes").join("vocal").join("dvtd").is_dir() {
                return Some(a.to_path_buf());
            }
        }
    }
    None
}

impl Store {
    pub fn new() -> Result<Self, String> {
        let root = find_root().ok_or("no recipes/vocal/dvtd found above cwd or exe")?;
        let stage_index = factory_stage_index(&root);
        let vocabulary = crate::vocab::build(&root, &stage_index);
        Ok(Self {
            root,
            stage_index,
            vocabulary,
        })
    }

    pub fn recipes(&self) -> PathBuf {
        self.root.join("recipes")
    }

    pub fn resolve(&self, id: &str) -> Result<PathBuf, String> {
        if id.contains("..") || id.starts_with('/') || id.contains('\\') || id.contains(':') {
            return Err(format!("bad id: {id}"));
        }
        let path = self.recipes().join(id);
        if !path.exists() {
            return Err(format!("no such entry: {id}"));
        }
        Ok(path)
    }

    fn id_for(&self, path: &Path) -> String {
        path.strip_prefix(self.recipes())
            .unwrap_or(path)
            .to_string_lossy()
            .replace('\\', "/")
    }

    fn json_items(&self, dir: &str, name_key: &str, gloss_key: &str) -> Vec<Item> {
        let mut paths: Vec<PathBuf> = std::fs::read_dir(self.recipes().join(dir))
            .map(|rd| {
                rd.flatten()
                    .map(|e| e.path())
                    .filter(|p| p.extension().is_some_and(|x| x == "json"))
                    .collect()
            })
            .unwrap_or_default();
        paths.sort();
        paths
            .into_iter()
            .filter_map(|path| {
                let text = std::fs::read_to_string(&path).ok()?;
                let v: serde_json::Value = serde_json::from_str(&text).ok()?;
                let name = v
                    .get(name_key)
                    .and_then(|n| n.as_str())
                    .unwrap_or(path.file_stem()?.to_str()?)
                    .to_string();
                let gloss = v
                    .get(gloss_key)
                    .and_then(|g| g.as_str())
                    .unwrap_or("")
                    .to_string();
                Some(Item {
                    id: self.id_for(&path),
                    name,
                    gloss,
                })
            })
            .collect()
    }

    pub fn mouths(&self) -> Vec<Item> {
        author::envelope::scan(&self.recipes().join("vocal").join("dvtd"))
            .into_iter()
            .map(|(file, path)| Item {
                id: self.id_for(&path),
                name: file.trim_end_matches("-vvtf-measured.txt").to_string(),
                gloss: String::new(),
            })
            .collect()
    }

    pub fn recordings(&self) -> Vec<Item> {
        let mut found = author::wav::scan(&self.recipes().join("recordings"));
        found.extend(author::wav::scan(&self.recipes().join("08_clean_instruments")));
        found
            .into_iter()
            .map(|(name, path)| Item {
                id: self.id_for(&path),
                name,
                gloss: String::new(),
            })
            .collect()
    }

    pub fn poses(&self) -> Vec<Item> {
        self.json_items("poses", "name", "gloss")
    }

    pub fn scaffolds(&self) -> Vec<Item> {
        self.json_items("scaffolds", "name", "gloss")
    }

    pub fn architectures(&self) -> Vec<Item> {
        self.json_items("architectures", "name", "provenance")
    }

    pub fn frames(&self) -> Vec<Item> {
        let mut paths: Vec<PathBuf> = std::fs::read_dir(self.recipes().join("frames"))
            .map(|rd| {
                rd.flatten()
                    .map(|e| e.path())
                    .filter(|p| p.extension().is_some_and(|x| x == "json"))
                    .collect()
            })
            .unwrap_or_default();
        paths.sort();
        paths
            .into_iter()
            .filter_map(|path| {
                let f = author::frame::read(&path).ok()?;
                Some(Item {
                    id: self.id_for(&path),
                    name: f.name,
                    gloss: f.provenance,
                })
            })
            .collect()
    }

    pub fn resolve_factory(&self, id: &str) -> Result<(PathBuf, usize, usize, f64), String> {
        let (kind, file) = id.strip_prefix("factory/").and_then(|s| s.split_once('/')).ok_or_else(|| format!("bad factory id: {id}"))?;
        if file.contains('/') || file.contains('\\') || file.contains("..") { return Err(format!("bad factory id: {id}")); }
        let (path, corners, stages, sr) = match kind {
            "morpheus" if file.ends_with(".body") => (self.root.join("ref/morpheus/bodies").join(file), 8, 7, crate::fit::SR),
            "p2k" if file.ends_with(".bin") => (self.root.join("ref/presets").join(file), 4, 6, author::extrude::AUTHORING_SR),
            _ => return Err(format!("bad factory id: {id}")),
        };
        if !path.is_file() { return Err(format!("no such factory body: {id}")); }
        Ok((path, corners, stages, sr))
    }

    pub fn bodies(&self) -> Vec<Item> {
        let mut paths: Vec<PathBuf> = ["hero", "extrusions"]
            .iter()
            .flat_map(|dir| {
                std::fs::read_dir(self.recipes().join(dir))
                    .map(|rd| {
                        rd.flatten()
                            .map(|e| e.path())
                            .filter(|p| p.extension().is_some_and(|x| x == "body"))
                            .collect::<Vec<_>>()
                    })
                    .unwrap_or_default()
            })
            .collect();
        paths.sort();
        paths
            .into_iter()
            .filter_map(|path| {
                Some(Item {
                    id: self.id_for(&path),
                    name: path.file_stem()?.to_str()?.to_string(),
                    gloss: String::new(),
                })
            })
            .collect()
    }
}

fn factory_stage_index(root: &Path) -> serde_json::Value {
    let mut sources = Vec::new();
    for (kind, dir, ext, corners, stages, sr) in [
        ("morpheus", root.join("ref/morpheus/bodies"), "body", 8usize, 7usize, crate::fit::SR),
        ("p2k", root.join("ref/presets"), "bin", 4usize, 6usize, author::extrude::AUTHORING_SR),
    ] {
        let mut paths: Vec<PathBuf> = std::fs::read_dir(dir).map(|rd| rd.flatten().map(|e| e.path()).filter(|p| p.extension().is_some_and(|x| x == ext)).collect()).unwrap_or_default();
        paths.sort();
        for path in paths {
            let Ok(bytes) = std::fs::read(&path) else { continue };
            let Ok(packed) = trench_core::minifloat::PackedCorners::from_body_bytes(&bytes) else { continue };
            let Some(file) = path.file_name().and_then(|n| n.to_str()) else { continue };
            let stem = path.file_stem().and_then(|n| n.to_str()).unwrap_or(file);
            let fallback = stem.split_once('_').map(|x| x.1).unwrap_or(stem);
            let name = if kind == "p2k" {
                p2k_architecture_name(root, file).unwrap_or_else(|| fallback.to_string())
            } else {
                fallback.to_string()
            };
            let tracks = (0..stages)
                .map(|stage| (0..corners).map(|corner| packed.words[corner][stage]).collect::<Vec<_>>())
                .collect::<Vec<_>>();
            sources.push(serde_json::json!({
                "id": format!("factory/{kind}/{file}"),
                "name": name,
                "family": kind,
                "corners": corners,
                "stage_count": stages,
                "source_sr_hz": sr,
                "tracks": tracks
            }));
        }
    }
    serde_json::Value::Array(sources)
}

fn p2k_architecture_name(root: &Path, preset_file: &str) -> Option<String> {
    let index = preset_file.split('_').nth(1)?;
    let prefix = format!("P2k_{index}_");
    let dir = root.join("recipes").join("architectures");
    let path = std::fs::read_dir(dir)
        .ok()?
        .flatten()
        .map(|entry| entry.path())
        .find(|path| path.file_name().and_then(|n| n.to_str()).is_some_and(|name| name.starts_with(&prefix) && name.ends_with(".json")))?;
    let value: serde_json::Value = serde_json::from_str(&std::fs::read_to_string(path).ok()?).ok()?;
    value.get("name")?.as_str().map(str::to_string)
}
