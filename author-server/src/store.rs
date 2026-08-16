use std::path::{Path, PathBuf};

pub struct Store {
    pub root: PathBuf,
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
        Ok(Self { root })
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

    pub fn bodies(&self) -> Vec<Item> {
        let mut paths: Vec<PathBuf> = std::fs::read_dir(self.recipes().join("hero"))
            .map(|rd| {
                rd.flatten()
                    .map(|e| e.path())
                    .filter(|p| p.extension().is_some_and(|x| x == "body"))
                    .collect()
            })
            .unwrap_or_default();
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
