use std::path::{Path, PathBuf};

use trench_core::cascade::{NUM_COEFFS, NUM_STAGES};
use trench_core::minifloat::{PackedCorners, BODY_BYTES, LEGACY_STAGES, NUM_CORNERS};
use trench_core::response::{biquad_cascade_mag_db, biquad_stage_mag_db, log_frequency_grid};
use trench_core::stage_law::{geometry_from_words_at, RootPair};

pub const POINTS: usize = 320;

/// The eight frames, in the order the runtime addresses them: m | q<<1 | z<<2.
pub const FRAME_NAME: [&str; NUM_CORNERS] = [
    "M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100",
    "M0 Q0 T2", "M100 Q0 T2", "M0 Q100 T2", "M100 Q100 T2",
];

pub struct Body {
    pub path: PathBuf,
    pub packed: PackedCorners,
    pub bytes: usize,
    pub rate: f64,
    pub grid: Vec<f64>,
}

impl Body {
    pub fn load(path: &Path, rate: f64) -> Result<Self, String> {
        let raw = std::fs::read(path).map_err(|e| format!("{}: {e}", path.display()))?;
        let packed = PackedCorners::from_body_bytes(&raw).map_err(|e| e.to_string())?;
        Ok(Self {
            path: path.to_path_buf(),
            packed,
            bytes: raw.len(),
            rate,
            grid: log_frequency_grid(20.0, 20_000.0, POINTS),
        })
    }

    /// Ask the crate, never a literal: the day a section or an axis is added,
    /// every hardcoded byte count is wrong and the constants are already right.
    pub fn is_cube(&self) -> bool {
        self.bytes == BODY_BYTES
    }

    pub fn stages(&self) -> usize {
        if self.is_cube() { NUM_STAGES } else { LEGACY_STAGES }
    }

    pub fn rows_at(&self, m: f32, q: f32, z: f32) -> Vec<[f64; NUM_COEFFS]> {
        self.packed.interpolate_biquad(m, q, z).to_vec()
    }

    pub fn total(&self, m: f32, q: f32, z: f32) -> Vec<f64> {
        let rows = self.rows_at(m, q, z);
        self.grid
            .iter()
            .map(|&f| biquad_cascade_mag_db(&rows, f, self.rate))
            .collect()
    }

    pub fn section(&self, m: f32, q: f32, z: f32, s: usize) -> Vec<f64> {
        let rows = self.rows_at(m, q, z);
        self.grid
            .iter()
            .map(|&f| biquad_stage_mag_db(&rows[s], f, self.rate))
            .collect()
    }

    pub fn frame_axes(i: usize) -> (f32, f32, f32) {
        ((i & 1) as f32, ((i >> 1) & 1) as f32, ((i >> 2) & 1) as f32)
    }

    pub fn frame_total(&self, i: usize) -> Vec<f64> {
        let (m, q, z) = Self::frame_axes(i);
        self.total(m, q, z)
    }

    /// Pole and zero of one stored row, as Hz and radius. Reads through
    /// geometry, not roots, so real-rooted rows survive instead of blanking.
    pub fn roots(&self, frame: usize, s: usize) -> (Option<(f64, f64)>, Option<(f64, f64)>) {
        let g = geometry_from_words_at(self.packed.words[frame][s], self.rate);
        let one = |p: RootPair| match p {
            RootPair::Conjugate { hz, r } => Some((hz, r)),
            _ => None,
        };
        (one(g.pole), one(g.zero))
    }

    pub fn scale(&self, frame: usize, s: usize) -> f64 {
        geometry_from_words_at(self.packed.words[frame][s], self.rate).scale
    }
}
