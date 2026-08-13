//! SOS lanes and their values.
//!
//! A lane is a correspondence identity, not a position. Lane `L3` at one frame
//! interpolates strictly to lane `L3` at every other frame, whatever its
//! frequency happens to be. Nothing in this file sorts lanes by frequency, and
//! nothing may: an expressive trajectory deliberately crosses lanes, and a sort
//! would silently rewrite it into a monotonic one.
//!
//! The stored datum is the packed word set, because that is what the runtime
//! stores. Pole, zero and scale are a derived view of those words. Storing the
//! words means an imported object round-trips bit-exactly no matter what the
//! geometry view can or cannot express.

use serde::{Deserialize, Serialize};
use trench_core::cascade::NUM_COEFFS;
use trench_core::minifloat::IDENTITY_STAGE;
use trench_core::stage_law::{
    geometry_from_words_at, validate_stage_roots_at, words_from_roots_at, RootPair, RootValidity,
    StageGeometry, StageRoots,
};

/// Stable identity of a lane, independent of its index or its frequency.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub struct LaneId(pub u32);

impl std::fmt::Display for LaneId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "L{}", self.0 + 1)
    }
}

/// A declared lane. `id` is the correspondence identity carried across frames.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct LaneSpec {
    pub id: LaneId,
    pub name: String,
}

impl LaneSpec {
    pub fn new(id: LaneId) -> Self {
        Self {
            id,
            name: id.to_string(),
        }
    }
}

/// One lane's value at one frame: the packed words the runtime would store.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct LaneValue {
    pub words: [u16; NUM_COEFFS],
}

impl LaneValue {
    /// The pass-through lane: multiplies the cascade by exactly 1.
    pub const IDENTITY: LaneValue = LaneValue {
        words: IDENTITY_STAGE,
    };

    pub fn from_words(words: [u16; NUM_COEFFS]) -> Self {
        Self { words }
    }

    /// The geometry view of the stored words. Reads through geometry rather
    /// than roots so a real-rooted lane reports what it is instead of blanking.
    pub fn geometry(&self, sample_rate_hz: f64) -> StageGeometry {
        geometry_from_words_at(self.words, sample_rate_hz)
    }

    /// The editable root view, when this lane's stored words describe a
    /// conjugate pole and a conjugate zero. A lane that does not is still a
    /// valid lane; it simply cannot be edited through this view.
    pub fn roots(&self, sample_rate_hz: f64) -> Option<StageRoots> {
        let g = self.geometry(sample_rate_hz);
        match (g.pole, g.zero) {
            (
                RootPair::Conjugate {
                    hz: pole_hz,
                    r: pole_r,
                },
                RootPair::Conjugate {
                    hz: zero_hz,
                    r: zero_r,
                },
            ) => Some(StageRoots {
                pole_hz,
                pole_r,
                zero_hz,
                zero_r,
                scale: g.scale,
            }),
            _ => None,
        }
    }

    /// True when this lane is the exact pass-through word set.
    pub fn is_identity(&self) -> bool {
        self.words == IDENTITY_STAGE
    }

    /// Writes authored roots into the lane, refusing anything the encoder
    /// would alter rather than store. The refusal is the crate's, not a
    /// bound invented here.
    pub fn set_roots(
        &mut self,
        roots: &StageRoots,
        sample_rate_hz: f64,
    ) -> Result<(), RootValidity> {
        match validate_stage_roots_at(roots, sample_rate_hz) {
            RootValidity::Ok => {
                self.words = words_from_roots_at(roots, sample_rate_hz);
                Ok(())
            }
            other => Err(other),
        }
    }
}

/// Why a lane edit was refused, in words an operator can act on.
pub fn refusal_text(v: RootValidity) -> &'static str {
    match v {
        RootValidity::Ok => "ok",
        RootValidity::NotFinite => "value is not finite",
        RootValidity::DisplayDomainLow => "below the display domain",
        RootValidity::DisplayDomainHigh => "above the display domain",
        RootValidity::AuthoringFreq => "above the authoring frequency ceiling",
        RootValidity::PoleRadius => "pole radius does not survive encoding",
        RootValidity::ZeroRadius => "zero radius does not survive encoding",
        RootValidity::Scale => "scale outside the encodable span",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SR: f64 = 44_100.0;

    #[test]
    fn identity_lane_is_pass_through() {
        assert!(LaneValue::IDENTITY.is_identity());
    }

    #[test]
    fn authored_roots_round_trip_through_stored_words() {
        let mut lane = LaneValue::IDENTITY;
        let want = StageRoots {
            pole_hz: 700.0,
            pole_r: 0.97,
            zero_hz: 1400.0,
            zero_r: 0.85,
            scale: 1.0,
        };
        lane.set_roots(&want, SR).expect("authored roots are valid");
        let got = lane.roots(SR).expect("lane reads back as conjugate roots");
        // The encoder quantises; the read-back must land on the same filter,
        // not merely on a similar-sounding one.
        assert!((got.pole_hz - want.pole_hz).abs() < 5.0, "pole_hz {got:?}");
        assert!((got.pole_r - want.pole_r).abs() < 1e-3, "pole_r {got:?}");
        assert!((got.zero_hz - want.zero_hz).abs() < 10.0, "zero_hz {got:?}");
        assert!((got.zero_r - want.zero_r).abs() < 1e-3, "zero_r {got:?}");
    }

    #[test]
    fn a_pole_on_the_unit_circle_is_refused() {
        let mut lane = LaneValue::IDENTITY;
        let bad = StageRoots {
            pole_hz: 700.0,
            pole_r: 1.0,
            zero_hz: 1400.0,
            zero_r: 0.5,
            scale: 1.0,
        };
        assert_eq!(lane.set_roots(&bad, SR), Err(RootValidity::PoleRadius));
        // The refusal must leave the lane exactly as it was.
        assert!(lane.is_identity());
    }

    #[test]
    fn lane_id_is_stable_and_readable() {
        assert_eq!(LaneId(0).to_string(), "L1");
        assert_eq!(LaneId(6).to_string(), "L7");
    }
}
