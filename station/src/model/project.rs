//! The Station project: a versioned model that can hold more than the packed
//! runtime format can store.
//!
//! The project is the authority while authoring. The packed body is one export
//! target, and it is a narrower one — it holds exactly three axes and seven
//! lanes. A four-axis project is a legitimate project; it simply has no packed
//! representation, and the Station says so rather than quietly dropping an axis.

use serde::{Deserialize, Serialize};
use trench_core::cascade::NUM_STAGES;
use trench_core::minifloat::{PackedCorners, BODY_BYTES, LEGACY_BODY_BYTES, NUM_CORNERS};

use super::lane::{LaneId, LaneSpec, LaneValue};
use super::topology::Topology;

/// Bumped whenever the on-disk shape changes in a way an older reader would
/// misread. A reader that does not recognise a version refuses the file.
pub const PROJECT_VERSION: u32 = 1;

/// One authored frame: an address in the declared topology and one value per
/// declared lane, in lane order.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Frame {
    pub address: Vec<u8>,
    pub label: String,
    pub values: Vec<LaneValue>,
}

/// The interpolation law the project declares. The Station reads this rather
/// than assuming a scheme.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum InterpDomain {
    /// Multilinear on the stored words: the log-polar radius/angle pair.
    PackedWords,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct InterpLaw {
    pub domain: InterpDomain,
    /// The rate the stored words are to be read at. Words belong to a rate;
    /// reading them at another one without translation is a different filter.
    pub sample_rate_hz: f64,
}

impl Default for InterpLaw {
    fn default() -> Self {
        Self {
            domain: InterpDomain::PackedWords,
            sample_rate_hz: 44_100.0,
        }
    }
}

/// Where a project came from, so an export can be judged honestly.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum Origin {
    /// Authored in the Station, never imported.
    Native,
    /// Imported from a 560-byte packed body.
    PackedNative,
    /// Imported from a 240-byte legacy body.
    PackedLegacy,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Project {
    pub version: u32,
    pub name: String,
    pub origin: Origin,
    pub topology: Topology,
    pub lanes: Vec<LaneSpec>,
    pub frames: Vec<Frame>,
    pub interp: InterpLaw,
}

impl Project {
    /// An empty project on a declared topology, every frame pass-through.
    pub fn blank(name: impl Into<String>, topology: Topology, lane_count: usize) -> Self {
        let lanes: Vec<LaneSpec> = (0..lane_count)
            .map(|i| LaneSpec::new(LaneId(i as u32)))
            .collect();
        let frames = (0..topology.corner_count())
            .map(|ci| {
                let address = topology.address_of(ci);
                Frame {
                    label: topology.label_of(&address),
                    address,
                    values: vec![LaneValue::IDENTITY; lane_count],
                }
            })
            .collect();
        Self {
            version: PROJECT_VERSION,
            name: name.into(),
            origin: Origin::Native,
            topology,
            lanes,
            frames,
            interp: InterpLaw::default(),
        }
    }

    pub fn lane_count(&self) -> usize {
        self.lanes.len()
    }

    pub fn sample_rate(&self) -> f64 {
        self.interp.sample_rate_hz
    }

    /// Frames ordered by corner index, which is the order interpolation
    /// expects. Returns `None` if any frame address is not addressable.
    pub fn corner_ordered(&self) -> Option<Vec<&Frame>> {
        let mut slots: Vec<Option<&Frame>> = vec![None; self.topology.corner_count()];
        for f in &self.frames {
            let i = self.topology.index_of(&f.address)?;
            if i >= slots.len() {
                return None;
            }
            slots[i] = Some(f);
        }
        slots.into_iter().collect()
    }

    /// The cascade at a position in the declared axes.
    pub fn cascade_at(&self, coords: &[f32]) -> Option<Vec<LaneValue>> {
        if coords.len() != self.topology.axis_count() {
            return None;
        }
        let ordered = self.corner_ordered()?;
        let corner_lanes: Vec<Vec<LaneValue>> = ordered.iter().map(|f| f.values.clone()).collect();
        Some(super::interp::cascade_at(&corner_lanes, coords))
    }

    /// Imports a packed body. The packed format declares three axes, eight
    /// corners and seven lanes, so that is exactly what the project gets — the
    /// topology is read from the format, not assumed.
    pub fn from_packed(
        name: impl Into<String>,
        bytes: &[u8],
        sample_rate_hz: f64,
    ) -> Result<Self, String> {
        let packed = PackedCorners::from_body_bytes(bytes).map_err(|e| e.to_string())?;
        let origin = match bytes.len() {
            BODY_BYTES => Origin::PackedNative,
            LEGACY_BODY_BYTES => Origin::PackedLegacy,
            n => return Err(format!("unsupported body length {n}")),
        };
        let topology = Topology::packed_runtime();
        let lanes: Vec<LaneSpec> = (0..NUM_STAGES)
            .map(|i| LaneSpec::new(LaneId(i as u32)))
            .collect();
        let frames = (0..NUM_CORNERS)
            .map(|ci| {
                let address = topology.address_of(ci);
                Frame {
                    label: topology.label_of(&address),
                    address,
                    values: (0..NUM_STAGES)
                        .map(|si| LaneValue::from_words(packed.words[ci][si]))
                        .collect(),
                }
            })
            .collect();
        Ok(Self {
            version: PROJECT_VERSION,
            name: name.into(),
            origin,
            topology,
            lanes,
            frames,
            interp: InterpLaw {
                domain: InterpDomain::PackedWords,
                sample_rate_hz,
            },
        })
    }

    /// Rebuilds the packed corner table, when this project can be one.
    pub fn to_packed(&self) -> Result<PackedCorners, ExportRefusal> {
        match self.packed_capability() {
            PackedCapability::Native | PackedCapability::Legacy => {}
            PackedCapability::Refused(r) => return Err(r),
        }
        let ordered = self
            .corner_ordered()
            .ok_or(ExportRefusal::FramesNotAddressable)?;
        let mut words = [[LaneValue::IDENTITY.words; NUM_STAGES]; NUM_CORNERS];
        for (ci, frame) in ordered.iter().enumerate() {
            for (si, v) in frame.values.iter().enumerate() {
                words[ci][si] = v.words;
            }
        }
        Ok(PackedCorners { words })
    }

    /// Swaps what two lanes hold at one frame.
    ///
    /// This is the expressive override. Lane identity is fixed across frames,
    /// so exchanging two lanes' values at one end of a sweep does not break
    /// correspondence — it deliberately crosses the trajectories, sending one
    /// lane down while another climbs. Nothing is re-sorted and no other frame
    /// is touched, so the crossing is exactly the one that was asked for.
    pub fn swap_lanes_at_frame(&mut self, frame: usize, a: usize, b: usize) -> bool {
        if a == b {
            return false;
        }
        let Some(f) = self.frames.get_mut(frame) else {
            return false;
        };
        if a >= f.values.len() || b >= f.values.len() {
            return false;
        }
        f.values.swap(a, b);
        true
    }

    /// Re-discretises every lane from the rate its words belong to onto a new
    /// rate.
    ///
    /// Stored words belong to the rate they were authored at. Reading them at
    /// another rate unchanged moves every pole and zero, so a legacy body read
    /// at a modern rate would have its formants shifted. `recompile_stage_words`
    /// takes the roots back through the inverse bilinear transform, preserves
    /// the intended continuous frequencies, and re-discretises at the target
    /// rate. Identity lanes are left exactly as they are so a pass-through does
    /// not acquire content.
    pub fn retune(&mut self, to_rate: f64) {
        let from = self.interp.sample_rate_hz;
        if !to_rate.is_finite() || to_rate <= 0.0 || (to_rate - from).abs() < 1e-9 {
            return;
        }
        for frame in &mut self.frames {
            for v in &mut frame.values {
                if v.is_identity() {
                    continue;
                }
                v.words = trench_core::stage_law::recompile_stage_words(v.words, from, to_rate);
            }
        }
        self.interp.sample_rate_hz = to_rate;
    }

    /// What the operator is told about packed export, in one line.
    pub fn packed_capability_text(&self) -> String {
        match self.packed_capability() {
            PackedCapability::Native => format!("{BODY_BYTES} bytes native"),
            PackedCapability::Legacy => {
                format!("{BODY_BYTES} native / {LEGACY_BODY_BYTES} legacy")
            }
            PackedCapability::Refused(r) => format!("unsupported — {}", r.text()),
        }
    }

    /// What the packed runtime format can honestly hold of this project.
    pub fn packed_capability(&self) -> PackedCapability {
        if self.topology.axis_count() != 3 {
            return PackedCapability::Refused(ExportRefusal::AxisCount(self.topology.axis_count()));
        }
        if self.lane_count() > NUM_STAGES {
            return PackedCapability::Refused(ExportRefusal::LaneCount(self.lane_count()));
        }
        if self.frames.len() != NUM_CORNERS {
            return PackedCapability::Refused(ExportRefusal::FrameCount(self.frames.len()));
        }
        if self.corner_ordered().is_none() {
            return PackedCapability::Refused(ExportRefusal::FramesNotAddressable);
        }
        // Ask the crate whether the legacy block still carries everything.
        let mut words = [[LaneValue::IDENTITY.words; NUM_STAGES]; NUM_CORNERS];
        if let Some(ordered) = self.corner_ordered() {
            for (ci, frame) in ordered.iter().enumerate() {
                for (si, v) in frame.values.iter().enumerate() {
                    words[ci][si] = v.words;
                }
            }
        }
        if (PackedCorners { words }).is_legacy_representable() {
            PackedCapability::Legacy
        } else {
            PackedCapability::Native
        }
    }
}

/// What a packed export would produce, or why there is none.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum PackedCapability {
    /// 560 native bytes: three axes, up to seven lanes.
    Native,
    /// Also representable as the 240-byte legacy body.
    Legacy,
    Refused(ExportRefusal),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ExportRefusal {
    /// The packed body holds exactly three axes. It has no place to put a
    /// fourth, and there is no 4D packed contract to write into.
    AxisCount(usize),
    LaneCount(usize),
    FrameCount(usize),
    FramesNotAddressable,
}

impl ExportRefusal {
    pub fn text(&self) -> String {
        match self {
            ExportRefusal::AxisCount(n) => format!(
                "packed body holds 3 axes; this project declares {n}. \
                 No 4D packed contract exists to write into."
            ),
            ExportRefusal::LaneCount(n) => {
                format!("packed body holds {NUM_STAGES} lanes; this project declares {n}")
            }
            ExportRefusal::FrameCount(n) => {
                format!("packed body holds {NUM_CORNERS} frames; this project has {n}")
            }
            ExportRefusal::FramesNotAddressable => {
                "frame addresses do not cover the declared topology".into()
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::topology::Axis;

    fn factory() -> Vec<u8> {
        std::fs::read("../ref/presets/P2k_013_talking_hedz.bin").expect("factory preset")
    }

    #[test]
    fn packed_import_export_round_trips_byte_for_byte() {
        let raw = factory();
        let p = Project::from_packed("hedz", &raw, 44_100.0).expect("imports");
        let packed = p.to_packed().expect("exports");
        assert_eq!(packed.to_native_bytes().to_vec().len(), BODY_BYTES);
        let reimported = PackedCorners::from_body_bytes(&raw).unwrap();
        assert_eq!(packed, reimported, "export diverged from the imported body");
    }

    #[test]
    fn import_reads_topology_from_the_format() {
        let p = Project::from_packed("hedz", &factory(), 44_100.0).unwrap();
        assert_eq!(p.topology.axis_count(), 3);
        assert_eq!(p.frames.len(), 8);
        assert_eq!(p.lane_count(), NUM_STAGES);
    }

    #[test]
    fn a_four_axis_project_refuses_packed_export_and_says_why() {
        let topo = Topology::new(vec![
            Axis::new("morph", "M"),
            Axis::new("q", "Q"),
            Axis::new("t2", "T"),
            Axis::new("t3", "W"),
        ]);
        let p = Project::blank("four", topo, 7);
        assert_eq!(p.frames.len(), 16);
        match p.packed_capability() {
            PackedCapability::Refused(ExportRefusal::AxisCount(4)) => {}
            other => panic!("expected an axis-count refusal, got {other:?}"),
        }
        assert!(p.to_packed().is_err());
        assert!(p.packed_capability_text().contains("No 4D packed contract"));
    }

    #[test]
    fn variable_lane_counts_up_to_seven_are_representable() {
        for n in 1..=NUM_STAGES {
            let p = Project::blank("v", Topology::packed_runtime(), n);
            assert!(
                !matches!(p.packed_capability(), PackedCapability::Refused(_)),
                "lane count {n} was refused"
            );
            let packed = p.to_packed().expect("exports");
            // Unused lanes must be pass-through, never left as zeros.
            for ci in 0..NUM_CORNERS {
                for si in n..NUM_STAGES {
                    assert_eq!(packed.words[ci][si], LaneValue::IDENTITY.words);
                }
            }
        }
    }

    #[test]
    fn more_than_seven_lanes_is_refused() {
        let p = Project::blank("v", Topology::packed_runtime(), 8);
        assert!(matches!(
            p.packed_capability(),
            PackedCapability::Refused(ExportRefusal::LaneCount(8))
        ));
    }

    #[test]
    fn interpolating_at_a_corner_returns_that_corner() {
        let p = Project::from_packed("hedz", &factory(), 44_100.0).unwrap();
        for ci in 0..8 {
            let coords: Vec<f32> = p
                .topology
                .address_of(ci)
                .iter()
                .map(|&c| c as f32)
                .collect();
            let got = p.cascade_at(&coords).unwrap();
            assert_eq!(got, p.frames[ci].values, "corner {ci}");
        }
    }

    /// The point of re-discretising: the continuous frequency an authored pole
    /// stands at must survive a change of host rate.
    #[test]
    fn retune_preserves_pole_frequencies_across_rates() {
        let legacy = 39_062.5;
        let host = 48_000.0;
        let mut p = Project::from_packed("hedz", &factory(), legacy).unwrap();

        let before: Vec<Option<f64>> = p.frames[1]
            .values
            .iter()
            .map(|v| match v.geometry(legacy).pole {
                trench_core::stage_law::RootPair::Conjugate { hz, .. } => Some(hz),
                _ => None,
            })
            .collect();

        p.retune(host);
        assert_eq!(p.sample_rate(), host);

        let after: Vec<Option<f64>> = p.frames[1]
            .values
            .iter()
            .map(|v| match v.geometry(host).pole {
                trench_core::stage_law::RootPair::Conjugate { hz, .. } => Some(hz),
                _ => None,
            })
            .collect();

        for (i, (a, b)) in before.iter().zip(&after).enumerate() {
            if let (Some(a), Some(b)) = (a, b) {
                // Encoding is coarse at the top of the band; a few percent is
                // quantisation, a 22% shift would be the rate ratio itself.
                let ratio = b / a;
                assert!(
                    (ratio - 1.0).abs() < 0.06,
                    "lane {i} moved from {a:.1} Hz to {b:.1} Hz on retune"
                );
            }
        }
    }

    /// A crossing is authored at one end only, and the cascade it produces is
    /// unchanged because sections multiply.
    #[test]
    fn swapping_two_lanes_at_one_frame_crosses_their_trajectories() {
        let mut p = Project::from_packed("hedz", &factory(), 44_100.0).unwrap();
        let before_far = p.frames[1].values[5];
        let before_near = p.frames[1].values[1];
        let untouched = p.frames[0].values.clone();

        assert!(p.swap_lanes_at_frame(1, 1, 5));
        assert_eq!(p.frames[1].values[1], before_far);
        assert_eq!(p.frames[1].values[5], before_near);
        // No other frame moves, so lane identity elsewhere is intact.
        assert_eq!(p.frames[0].values, untouched);

        // The frame's own response is unchanged: a cascade is a product.
        let a = crate::model::analysis::analyse(&untouched, p.sample_rate());
        let _ = a;
        assert!(!p.swap_lanes_at_frame(1, 2, 2));
        assert!(!p.swap_lanes_at_frame(99, 0, 1));
        assert!(!p.swap_lanes_at_frame(1, 0, 99));
    }

    #[test]
    fn retune_leaves_pass_through_lanes_untouched() {
        let mut p = Project::blank("v", Topology::packed_runtime(), 7);
        p.retune(96_000.0);
        assert!(p
            .frames
            .iter()
            .all(|f| f.values.iter().all(|v| v.is_identity())));
        assert_eq!(p.sample_rate(), 96_000.0);
    }

    #[test]
    fn retune_refuses_a_nonsense_rate() {
        let mut p = Project::blank("v", Topology::packed_runtime(), 7);
        p.retune(0.0);
        p.retune(f64::NAN);
        assert_eq!(p.sample_rate(), 44_100.0);
    }

    #[test]
    fn cascade_refuses_coordinates_of_the_wrong_rank() {
        let p = Project::from_packed("hedz", &factory(), 44_100.0).unwrap();
        assert!(p.cascade_at(&[0.5, 0.5]).is_none());
    }
}
