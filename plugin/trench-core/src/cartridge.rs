use crate::cascade::{NUM_COEFFS, NUM_STAGES};
pub use crate::minifloat::BODY_BYTES;
use crate::minifloat::{PackedCorners, PackedStage};
use serde::Deserialize;
#[derive(Debug, Clone, Deserialize)]
pub struct DriveBlock {
    #[serde(rename = "input_gain_dB", default)]
    pub input_gain_db: f32,
    #[serde(default = "default_mackie_model")]
    pub model: String,
}
fn default_mackie_model() -> String {
    "mackie_1202".to_string()
}
impl Default for DriveBlock {
    fn default() -> Self {
        Self {
            input_gain_db: 0.0,
            model: default_mackie_model(),
        }
    }
}
pub type LawCoeffs6 = [f32; 6];
pub type BandLawCoeffs12 = [f32; 12];
#[derive(Debug, Clone, Deserialize)]
pub struct BandChannelCoeffs {
    pub low: BandLawCoeffs12,
    pub mid: BandLawCoeffs12,
    pub high: BandLawCoeffs12,
}
#[derive(Debug, Clone, Deserialize)]
pub struct BandCoeffs {
    pub l: BandChannelCoeffs,
    pub r: BandChannelCoeffs,
}
#[derive(Debug, Clone, Deserialize)]
pub struct SpatialProfile {
    pub azimuth: f32,
    pub distance: f32,
    pub elevation: f32,
    pub itd_coeffs: LawCoeffs6,
    pub ild_coeffs: LawCoeffs6,
    pub band_coeffs: BandCoeffs,
}
pub use crate::minifloat::{LegacyCornerData, LEGACY_CORNERS, LEGACY_STAGES, NUM_CORNERS};
pub type CornerData = [[f64; NUM_COEFFS]; NUM_STAGES];
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct KeyframeJson {
    label: String,
    boost: f64,
    #[serde(rename = "packedWords")]
    packed_words: Vec<PackedStage>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct CartridgeJson {
    format: String,
    name: String,
    #[serde(rename = "sampleRate")]
    sample_rate: f64,
    keyframes: Vec<KeyframeJson>,
    #[serde(default)]
    drive: Option<DriveBlock>,
    #[serde(default, rename = "spatial_profile")]
    spatial_profile: Option<SpatialProfile>,
}
#[derive(Clone, Debug)]
pub struct Cartridge {
    pub name: String,
    pub boosts: [f64; NUM_CORNERS],
    /// Words compiled at `compiled_rate` — what the runtime interpolates.
    pub packed: PackedCorners,
    /// The interchange words as authored. When `datum_rate` is positive the
    /// words are Hz-anchored at that rate and `packed` is always re-derived
    /// from them at the runtime rate — the factory law: the E-mu binary ships
    /// one bank per host rate and those banks preserve Hz (rate_bank_law_audit
    /// 2026-07-29, median 0.1 cent). `datum_rate == 0` marks a verbatim
    /// carrier whose theta-space words play as stored at any rate.
    pub datum_packed: PackedCorners,
    pub datum_rate: f64,
    compiled_rate: f64,
    pub drive: DriveBlock,
    pub spatial_profile: Option<SpatialProfile>,
}
impl Cartridge {
    pub fn from_packed(
        name: String,
        packed: PackedCorners,
        datum_rate: f64,
        boosts: [f64; NUM_CORNERS],
        drive: DriveBlock,
        spatial_profile: Option<SpatialProfile>,
    ) -> Self {
        Self {
            name,
            boosts,
            datum_packed: packed.clone(),
            packed,
            datum_rate,
            compiled_rate: datum_rate,
            drive,
            spatial_profile,
        }
    }
    /// Recompiles the datum words at `rate`. Always derived from the datum,
    /// so repeated rate changes never accumulate drift. No allocation — safe
    /// on the install path.
    pub fn compile_at(&mut self, rate: f64) {
        if self.datum_rate <= 0.0 {
            return; // verbatim carrier: the stored words ARE the filter
        }
        if !rate.is_finite() || rate <= 0.0 || rate == self.compiled_rate {
            return;
        }
        for (corner, datum_corner) in self
            .packed
            .words
            .iter_mut()
            .zip(self.datum_packed.words.iter())
        {
            for (row, datum_row) in corner.iter_mut().zip(datum_corner.iter()) {
                *row = crate::stage_law::recompile_stage_words(*datum_row, self.datum_rate, rate);
            }
        }
        self.compiled_rate = rate;
    }
    pub fn compiled_rate(&self) -> f64 {
        self.compiled_rate
    }
    /// ROM/heritage interchange: Hz-anchored at the proven 44,100 datum, the
    /// same law the E-mu factory rate banks obey (rate_bank_law_audit,
    /// 2026-07-29).
    pub fn from_body_bytes(name: &str, bytes: &[u8], boost: f64) -> Result<Self, String> {
        Self::from_body_bytes_at(name, bytes, boost, crate::compiler::DEFAULT_AUTHORING_SR)
    }
    /// A positive `datum_rate` declares the words Hz-anchored at that rate;
    /// zero declares a verbatim carrier.
    pub fn from_body_bytes_at(
        name: &str,
        bytes: &[u8],
        boost: f64,
        datum_rate: f64,
    ) -> Result<Self, String> {
        if !datum_rate.is_finite() || datum_rate < 0.0 {
            return Err("datum rate must be finite and non-negative".to_string());
        }
        let packed = PackedCorners::from_body_bytes(bytes).map_err(|e| e.to_string())?;
        Ok(Self::from_packed(
            name.to_string(),
            packed,
            datum_rate,
            [boost; NUM_CORNERS],
            DriveBlock::default(),
            None,
        ))
    }
    pub fn from_json(json: &str) -> Result<Self, String> {
        let raw: CartridgeJson =
            serde_json::from_str(json).map_err(|e| format!("JSON parse error: {e}"))?;
        if raw.format != "compiled-v1" {
            return Err(format!("unsupported cartridge format '{}'", raw.format));
        }
        if !raw.sample_rate.is_finite() || raw.sample_rate <= 0.0 {
            return Err("sampleRate must be finite and positive".to_string());
        }
        if raw.keyframes.len() != LEGACY_CORNERS {
            return Err(format!(
                "keyframes has {} entries, expected {LEGACY_CORNERS}",
                raw.keyframes.len()
            ));
        }
        const LABELS: [&str; LEGACY_CORNERS] = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"];
        let mut packed_words =
            [[crate::minifloat::IDENTITY_STAGE; NUM_STAGES]; NUM_CORNERS];
        let mut boosts = [1.0f64; NUM_CORNERS];
        for (idx, (kf, expected_label)) in raw.keyframes.iter().zip(LABELS).enumerate() {
            if kf.label != expected_label {
                return Err(format!(
                    "keyframe {idx} is '{}', expected '{expected_label}'",
                    kf.label
                ));
            }
            if kf.packed_words.len() != LEGACY_STAGES {
                return Err(format!(
                    "{expected_label}.packedWords has {} rows, expected {LEGACY_STAGES}",
                    kf.packed_words.len()
                ));
            }
            boosts[idx] = kf.boost;
            boosts[idx + LEGACY_CORNERS] = kf.boost;
            for (stage_index, words) in kf.packed_words.iter().copied().enumerate() {
                packed_words[idx][stage_index] = words;
                packed_words[idx + LEGACY_CORNERS][stage_index] = words;
            }
        }
        Ok(Self::from_packed(
            raw.name,
            PackedCorners {
                words: packed_words,
            },
            raw.sample_rate,
            boosts,
            raw.drive.unwrap_or_default(),
            raw.spatial_profile,
        ))
    }
    pub fn interpolate(&self, morph: f64, q: f64, z: f64) -> CornerData {
        self.packed
            .interpolate_biquad(morph as f32, q as f32, z as f32)
    }
    pub fn interpolate_boost(&self, morph: f64, q: f64, z: f64) -> f64 {
        let plane = |base: usize| {
            let q_m0 = self.boosts[base] + (self.boosts[base + 2] - self.boosts[base]) * q;
            let q_m1 = self.boosts[base + 1] + (self.boosts[base + 3] - self.boosts[base + 1]) * q;
            q_m0 + (q_m1 - q_m0) * morph
        };
        let near = plane(0);
        let far = plane(LEGACY_CORNERS);
        near + (far - near) * z
    }
}
