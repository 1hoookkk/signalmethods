//! View settings: the span the response is drawn over, and the radius at which
//! a pole is called out. Nothing here is an authoring rule.

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Settings {
    pub display_lo_hz: f64,
    pub display_hi_hz: f64,
    /// Radius above which a pole is marked as near the circle.
    pub pole_radius_watch: f64,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            display_lo_hz: 20.0,
            display_hi_hz: 20_000.0,
            pole_radius_watch: 0.995,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_span_the_audio_band() {
        let s = Settings::default();
        assert!(s.display_lo_hz > 0.0 && s.display_hi_hz > s.display_lo_hz);
        assert!(s.pole_radius_watch < 1.0);
    }
}
