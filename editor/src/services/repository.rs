use std::path::{Path, PathBuf};

use author::frame::{self, Frame};
use author::{cage, envelope};
use trench_core::minifloat::PackedCorners;

use crate::engine::fit::FieldReport;

pub enum Entry {
    Mouth { name: String, path: PathBuf },
    Kept { name: String, provenance: String, path: PathBuf },
    Cage { name: String, path: PathBuf },
}

impl Entry {
    pub fn name(&self) -> &str {
        match self {
            Entry::Mouth { name, .. } => name,
            Entry::Kept { name, .. } => name,
            Entry::Cage { name, .. } => name,
        }
    }
}

pub struct Repository {
    pub root: PathBuf,
    pub entries: Vec<Entry>,
    pub scaffolds: Vec<(String, String, PathBuf)>,
}

pub struct ScaffoldZero {
    pub section: usize,
    pub hz: f64,
    pub r: f64,
}

fn find_root() -> Option<PathBuf> {
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

impl Repository {
    pub fn new() -> Self {
        let root = find_root().unwrap_or_else(|| PathBuf::from("."));
        let mut repo = Self {
            root,
            entries: Vec::new(),
            scaffolds: Vec::new(),
        };
        repo.rescan();
        repo
    }

    pub fn rescan(&mut self) {
        self.entries.clear();
        let dvtd = self.root.join("recipes").join("vocal").join("dvtd");
        for (file, path) in envelope::scan(&dvtd) {
            let name = file.trim_end_matches("-vvtf-measured.txt").to_string();
            self.entries.push(Entry::Mouth { name, path });
        }
        let frames_dir = self.root.join("recipes").join("frames");
        let mut kept: Vec<PathBuf> = std::fs::read_dir(&frames_dir)
            .map(|rd| {
                rd.flatten()
                    .map(|e| e.path())
                    .filter(|p| p.extension().is_some_and(|x| x == "json"))
                    .collect()
            })
            .unwrap_or_default();
        kept.sort();
        for path in kept {
            if let Ok(f) = frame::read(&path) {
                self.entries.push(Entry::Kept {
                    name: f.name,
                    provenance: f.provenance,
                    path,
                });
            }
        }
        for (name, path) in cage::scan(&self.root.join("recipes").join("cages")) {
            self.entries.push(Entry::Cage { name, path });
        }
        self.scaffolds.clear();
        let dir = self.root.join("recipes").join("scaffolds");
        let mut paths: Vec<PathBuf> = std::fs::read_dir(&dir)
            .map(|rd| {
                rd.flatten()
                    .map(|e| e.path())
                    .filter(|p| p.extension().is_some_and(|x| x == "json"))
                    .collect()
            })
            .unwrap_or_default();
        paths.sort();
        for path in paths {
            if let Ok(text) = std::fs::read_to_string(&path) {
                if let Ok(v) = serde_json::from_str::<serde_json::Value>(&text) {
                    if let Some(name) = v.get("name").and_then(|n| n.as_str()) {
                        let gloss = v
                            .get("gloss")
                            .and_then(|g| g.as_str())
                            .unwrap_or("")
                            .to_string();
                        self.scaffolds.push((name.to_string(), gloss, path));
                    }
                }
            }
        }
    }

    pub fn load_scaffold(&self, index: usize) -> Result<(String, Vec<ScaffoldZero>), String> {
        let (name, _, path) = self.scaffolds.get(index).ok_or("not a scaffold")?;
        let text = std::fs::read_to_string(path).map_err(|e| e.to_string())?;
        let v: serde_json::Value = serde_json::from_str(&text).map_err(|e| e.to_string())?;
        let zeros = v
            .get("zeros")
            .and_then(|z| z.as_array())
            .ok_or("scaffold has no zeros")?
            .iter()
            .map(|z| {
                Ok(ScaffoldZero {
                    section: z.get("section").and_then(|x| x.as_u64()).ok_or("section")? as usize,
                    hz: z.get("hz").and_then(|x| x.as_f64()).ok_or("hz")?,
                    r: z.get("r").and_then(|x| x.as_f64()).ok_or("r")?,
                })
            })
            .collect::<Result<Vec<_>, &str>>()
            .map_err(|e| format!("scaffold field missing: {e}"))?;
        Ok((name.clone(), zeros))
    }

    pub fn load_mouth(&self, index: usize) -> Result<(String, Vec<f64>), String> {
        match self.entries.get(index) {
            Some(Entry::Mouth { name, path }) => {
                let curve = envelope::read_vvtf(path)?;
                Ok((name.clone(), envelope::on_grid(&curve)))
            }
            _ => Err("not a mouth".into()),
        }
    }

    pub fn load_frame(&self, index: usize) -> Result<Frame, String> {
        match self.entries.get(index) {
            Some(Entry::Kept { path, .. }) => frame::read(path),
            _ => Err("not a kept response".into()),
        }
    }

    pub fn load_cage(&self, index: usize) -> Result<cage::Cage, String> {
        match self.entries.get(index) {
            Some(Entry::Cage { path, .. }) => cage::read(path),
            _ => Err("not a cage".into()),
        }
    }

    pub fn write_frame(&mut self, frame: &mut Frame) -> Result<PathBuf, String> {
        let dir = self.root.join("recipes").join("frames");
        std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
        let mut n = 1;
        while dir.join(format!("frame-{n:03}.json")).exists() {
            n += 1;
        }
        frame.name = format!("frame-{n:03}");
        let path = dir.join(format!("{}.json", frame.name));
        frame::write(&path, frame)?;
        self.rescan();
        Ok(path)
    }

    pub fn write_static(
        &self,
        packed: &PackedCorners,
        report: &FieldReport,
    ) -> Result<PathBuf, String> {
        if !report.audit.pass() || !report.relative_ok {
            let mut why = report.audit.failures.join("; ");
            if !report.relative_ok {
                if !why.is_empty() {
                    why.push_str("; ");
                }
                why.push_str("crown exceeds the frames ceiling");
            }
            return Err(format!("audit FAIL — {why} — the body was not written"));
        }
        let dir = self.root.join("recipes").join("hero");
        std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
        let mut n = 1;
        let path = loop {
            let p = dir.join(format!("static-{n:02}.body"));
            if !p.exists() {
                break p;
            }
            n += 1;
        };
        let bytes: Vec<u8> = match packed.to_legacy_bytes() {
            Some(b) if report.legacy => b.to_vec(),
            _ => packed.to_native_bytes().to_vec(),
        };
        std::fs::write(&path, bytes).map_err(|e| e.to_string())?;
        Ok(path)
    }

    pub fn write_field(
        &self,
        packed: &PackedCorners,
        report: &FieldReport,
    ) -> Result<PathBuf, String> {
        if !report.audit.pass() || !report.relative_ok {
            return Err("audit FAIL — the field was not written".into());
        }
        let dir = self.root.join("recipes").join("hero");
        std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
        let mut n = 1;
        let path = loop {
            let name = if report.is_square {
                format!("cube-{n:02}.4.body")
            } else {
                format!("cube-{n:02}.body")
            };
            let p = dir.join(name);
            if !p.exists() {
                break p;
            }
            n += 1;
        };
        let bytes: Vec<u8> = match packed.to_legacy_bytes() {
            Some(b) if report.legacy => b.to_vec(),
            _ => packed.to_native_bytes().to_vec(),
        };
        std::fs::write(&path, bytes).map_err(|e| e.to_string())?;
        Ok(path)
    }

    pub fn read_body(&self, path: &Path) -> Result<PackedCorners, String> {
        let bytes = std::fs::read(path).map_err(|e| format!("{}: {e}", path.display()))?;
        PackedCorners::from_body_bytes(&bytes).map_err(|e| format!("{}: {e}", path.display()))
    }
}
