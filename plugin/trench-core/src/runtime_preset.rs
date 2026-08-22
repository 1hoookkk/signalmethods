//! X3 runtime preset cartridge — verbatim packed corner words, bank selection.
//!
//! The Emulator X3's xStream engine stores four pre-compiled coefficient banks
//! per filter (44.1k, 48k, 96k, 192k). The banks are **distinct designs**, not
//! rate-converted copies (proven by cross-rate re-encoding mismatch,
//! scratchpad/rate_bank_redundancy.py). Many stages fail roots_from_words_at —
//! their geometry cannot be decomposed into simple conjugate pole/zero pairs.
//!
//! Strategy: select the nearest bank, load verbatim. No recompilation.
//! The X3 never supported arbitrary sample rates; neither do we.
//!
//! Load path: raw runtime block (N stages) → pad to 6 → PackedCorners →
//! Cartridge (datum_rate=0, verbatim).

use crate::cartridge::{Cartridge, DriveBlock, NUM_CORNERS};
use crate::cascade::NUM_COEFFS;
use crate::minifloat::{LEGACY_CORNERS, LEGACY_STAGES};
use crate::minifloat::{PackedCorners, PackedStage};

/// The X3's four xStream rate families, in index order.
pub const X3_RATES: [f64; 4] = [44_100.0, 48_000.0, 96_000.0, 192_000.0];

/// The universal X3 identity/passthrough row — the same pad sentinel used by
/// Morph Designer (`DAT_1806d7500`) and by the body240 format. Decodes to the
/// exact unity biquad `[1, 0, 0, 0, 0]`.
pub const IDENTITY_ROW: PackedStage = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff];

/// A runtime preset: four banks of packed corner words, one per xStream rate.
///
/// Each bank is N active stages (1-3) packed into a 6-stage `PackedCorners`
/// (identity-padded). The words are minifloat u16 — the same codec as body240.
#[derive(Clone, Debug)]
pub struct RuntimePreset {
    pub name: String,
    /// One PackedCorners per rate family, indexed by rate-index (0=44.1k, …).
    /// A bank is `None` if the preset has no words for that rate.
    pub banks: [Option<PackedCorners>; 4],
    /// True stage count before padding (1, 2, or 3).
    pub active_stages: usize,
}

impl RuntimePreset {
    /// Build a `RuntimePreset` from the raw runtime-block words at one rate.
    ///
    /// `words` is a flat slice of u16 LE words: 4 corners × N stages × 5 words.
    /// `rate_index` selects which bank slot to populate (0=44.1k … 3=192k).
    /// `active_stages` is the unpadded stage count (1, 2, or 3).
    pub fn from_raw_words(
        name: &str,
        words: &[u16],
        rate_index: usize,
        active_stages: usize,
    ) -> Result<Self, &'static str> {
        assert!(rate_index < 4, "rate_index out of range");
        assert!(active_stages >= 1 && active_stages <= 3, "active_stages must be 1-3");
        let expected = LEGACY_CORNERS * active_stages * NUM_COEFFS;
        if words.len() != expected {
            return Err("word count does not match 4 corners × active_stages × 5");
        }
        let mut packed = [[IDENTITY_ROW; LEGACY_STAGES]; LEGACY_CORNERS];
        for corner in 0..LEGACY_CORNERS {
            for stage in 0..active_stages {
                let base = corner * active_stages * NUM_COEFFS + stage * NUM_COEFFS;
                packed[corner][stage] = [
                    words[base],
                    words[base + 1],
                    words[base + 2],
                    words[base + 3],
                    words[base + 4],
                ];
            }
        }
        let mut banks: [Option<PackedCorners>; 4] = [None, None, None, None];
        banks[rate_index] = Some(PackedCorners::from_legacy_words(&packed));
        Ok(Self {
            name: name.to_string(),
            banks,
            active_stages,
        })
    }

    /// Add another rate bank from raw words.
    pub fn add_bank(
        &mut self,
        words: &[u16],
        rate_index: usize,
        active_stages: usize,
    ) -> Result<(), &'static str> {
        assert!(rate_index < 4, "rate_index out of range");
        let expected = LEGACY_CORNERS * active_stages * NUM_COEFFS;
        if words.len() != expected {
            return Err("word count does not match");
        }
        let mut packed = [[IDENTITY_ROW; LEGACY_STAGES]; LEGACY_CORNERS];
        for corner in 0..LEGACY_CORNERS {
            for stage in 0..active_stages {
                let base = corner * active_stages * NUM_COEFFS + stage * NUM_COEFFS;
                packed[corner][stage] = [
                    words[base],
                    words[base + 1],
                    words[base + 2],
                    words[base + 3],
                    words[base + 4],
                ];
            }
        }
        self.banks[rate_index] = Some(PackedCorners::from_legacy_words(&packed));
        Ok(())
    }

    /// Select the bank whose authored rate is closest to `host_rate`.
    ///
    /// Returns the rate-index and the authored rate in Hz. Panics if no
    /// banks are populated.
    pub fn select_bank(&self, host_rate: f64) -> (usize, f64) {
        let mut best_idx = 0usize;
        let mut best_dist = f64::INFINITY;
        for (i, bank) in self.banks.iter().enumerate() {
            if bank.is_some() {
                let dist = (X3_RATES[i] - host_rate).abs();
                if dist < best_dist {
                    best_dist = dist;
                    best_idx = i;
                }
            }
        }
        assert!(best_dist.is_finite(), "no populated banks in RuntimePreset");
        (best_idx, X3_RATES[best_idx])
    }

    /// Produce a `Cartridge` for the given host rate.
    ///
    /// Selects the nearest bank and loads it verbatim (`datum_rate = 0`).
    /// No recompilation — the X3 banks are distinct designs that cannot be
    /// decomposed into Hz-anchored roots for all stage types.
    ///
    /// `boost` is the broadband corner gain (1.0 = unity). Values ≤ 0 are
    /// clamped to 1.0 — a runtime preset carries its own SCALE words; the
    /// boost parameter is for subsequent level automation, not for gating
    /// the cartridge itself.
    ///
    /// Returns `(cartridge, bank_rate_hz)` so the caller knows which bank
    /// was selected.
    pub fn to_cartridge(&self, host_rate: f64, boost: f64) -> (Cartridge, f64) {
        let boost = if boost > 0.0 { boost } else { 1.0 };
        let (idx, bank_rate) = self.select_bank(host_rate);
        let packed = self.banks[idx]
            .clone()
            .expect("selected bank must be populated");
        let cart = Cartridge::from_packed(
            self.name.clone(),
            packed,
            0.0, // datum_rate=0: verbatim — no recompilation
            [boost; NUM_CORNERS],
            DriveBlock::default(),
            None,
        );
        (cart, bank_rate)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn identity_row_is_unity() {
        let k = crate::minifloat::stage_words_to_kernel(IDENTITY_ROW);
        let bq = crate::minifloat::kernel_to_biquad(k);
        // Unity biquad: [1, 0, 0, 0, 0]
        assert!((bq[0] - 1.0).abs() < 1e-12);
        assert!(bq[1].abs() < 1e-12);
        assert!(bq[2].abs() < 1e-12);
        assert!(bq[3].abs() < 1e-12);
        assert!(bq[4].abs() < 1e-12);
    }

    #[test]
    fn from_raw_words_pads_to_seven_stages() {
        // 1-stage preset: 4 corners × 1 stage × 5 words = 20 words
        // Each word = 0x1000 (a valid minifloat value)
        let words: Vec<u16> = (0..20).map(|_| 0x1000u16).collect();
        let preset = RuntimePreset::from_raw_words("test", &words, 0, 1).unwrap();
        let packed = preset.banks[0].as_ref().unwrap();
        // stages 0 = custom, stages 1-5 = identity
        assert_eq!(packed.words[0][0], [0x1000u16; 5]);
        assert_eq!(packed.words[0][1], IDENTITY_ROW);
        assert_eq!(packed.words[0][5], IDENTITY_ROW);
    }

    #[test]
    fn select_bank_nearest() {
        let mut preset =
            RuntimePreset::from_raw_words("test", &vec![0x1000u16; 20], 0, 1).unwrap();
        preset.add_bank(&vec![0x2000u16; 20], 1, 1).unwrap();
        preset.add_bank(&vec![0x3000u16; 20], 2, 1).unwrap();

        // Exact matches
        assert_eq!(preset.select_bank(44_100.0).0, 0);
        assert_eq!(preset.select_bank(48_000.0).0, 1);
        assert_eq!(preset.select_bank(96_000.0).0, 2);

        // Non-standard: 88.2k → nearest is 96k
        assert_eq!(preset.select_bank(88_200.0).0, 2);
    }

    #[test]
    fn to_cartridge_verbatim() {
        let words: Vec<u16> = (0..20).map(|i| 0x1000u16 + i as u16).collect();
        let preset = RuntimePreset::from_raw_words("test", &words, 0, 1).unwrap();
        let (cart, bank_rate) = preset.to_cartridge(48_000.0, 0.0);
        assert_eq!(bank_rate, 44_100.0); // only bank populated
        // datum_rate=0 → verbatim
        assert_eq!(cart.datum_rate, 0.0);
        assert_eq!(cart.packed.words[0][0][0], 0x1000);
    }
}
