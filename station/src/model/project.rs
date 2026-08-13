//! The Station project.
//!
//! A project either holds an object or it does not. An empty project has no
//! corners, no sections and no response, and the Station shows exactly that
//! rather than a manufactured pass-through filter.

use serde::{Deserialize, Serialize};
use trench_core::minifloat::PackedCorners;

use super::lane::{LaneId, LaneSpec, LaneValue};
use super::object::ObjectForm;

/// Bumped when the on-disk shape changes in a way an older reader would
/// misread.
pub const PROJECT_VERSION: u32 = 2;

/// One authored corner.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Frame {
    pub address: Vec<u8>,
    pub label: String,
    pub values: Vec<LaneValue>,
}

/// Where the object came from.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum Origin {
    Authored,
    Imported,
}

/// The object itself. A project without one is empty.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Object {
    pub form: ObjectForm,
    pub origin: Origin,
    /// One entry per declared section, in correspondence order.
    pub lanes: Vec<LaneSpec>,
    pub frames: Vec<Frame>,
    /// The rate the stored words belong to.
    pub sample_rate_hz: f64,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Project {
    pub version: u32,
    pub name: String,
    pub object: Option<Object>,
}

impl Project {
    /// A project with nothing in it. This is what the Station starts on.
    pub fn empty() -> Self {
        Self {
            version: PROJECT_VERSION,
            name: String::new(),
            object: None,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.object.is_none()
    }

    /// A new object of the given form. Sections start at the form's capacity
    /// but every one of them is pass-through, and `active_lanes` reports zero,
    /// so nothing is drawn as authored until it is.
    pub fn new_object(name: impl Into<String>, form: ObjectForm, sample_rate_hz: f64) -> Self {
        let capacity = form.section_capacity();
        let lanes: Vec<LaneSpec> = (0..capacity)
            .map(|i| LaneSpec::new(LaneId(i as u32)))
            .collect();
        let frames = (0..form.corner_count())
            .map(|ci| {
                let address = form.address_of(ci);
                Frame {
                    label: form.label_of(&address),
                    address,
                    values: vec![LaneValue::IDENTITY; capacity],
                }
            })
            .collect();
        Self {
            version: PROJECT_VERSION,
            name: name.into(),
            object: Some(Object {
                form,
                origin: Origin::Authored,
                lanes,
                frames,
                sample_rate_hz,
            }),
        }
    }

    pub fn form(&self) -> Option<ObjectForm> {
        self.object.as_ref().map(|o| o.form)
    }

    pub fn sample_rate(&self) -> f64 {
        self.object
            .as_ref()
            .map(|o| o.sample_rate_hz)
            .unwrap_or(44_100.0)
    }

    pub fn frames(&self) -> &[Frame] {
        self.object
            .as_ref()
            .map(|o| o.frames.as_slice())
            .unwrap_or(&[])
    }

    pub fn lane_capacity(&self) -> usize {
        self.object.as_ref().map(|o| o.lanes.len()).unwrap_or(0)
    }

    /// Sections that carry something at any corner. Pass-through capacity is
    /// not an authored section and is not counted as one.
    pub fn active_lanes(&self) -> Vec<usize> {
        let Some(o) = &self.object else {
            return Vec::new();
        };
        (0..o.lanes.len())
            .filter(|&li| o.frames.iter().any(|f| !f.values[li].is_identity()))
            .collect()
    }

    /// Order of the cascade actually carrying signal: two poles per section
    /// holding a conjugate pair, at the given corner.
    pub fn order_at(&self, frame: usize) -> usize {
        let Some(o) = &self.object else { return 0 };
        let sr = o.sample_rate_hz;
        o.frames
            .get(frame)
            .map(|f| {
                f.values
                    .iter()
                    .filter(|v| {
                        matches!(
                            v.geometry(sr).pole,
                            trench_core::stage_law::RootPair::Conjugate { r, .. } if r > 0.0
                        )
                    })
                    .count()
                    * 2
            })
            .unwrap_or(0)
    }

    /// The cascade at a position. `coords` is one value per axis of the form.
    pub fn cascade_at(&self, coords: &[f32]) -> Option<Vec<LaneValue>> {
        let o = self.object.as_ref()?;
        if coords.len() != o.form.axis_count() {
            return None;
        }
        let mut ordered: Vec<Option<&Frame>> = vec![None; o.form.corner_count()];
        for f in &o.frames {
            let i = o.form.index_of(&f.address)?;
            ordered[i] = Some(f);
        }
        let corner_lanes: Vec<Vec<LaneValue>> = ordered
            .into_iter()
            .map(|f| f.map(|f| f.values.clone()))
            .collect::<Option<Vec<_>>>()?;
        Some(super::interp::cascade_at(&corner_lanes, coords))
    }

    /// Imports a packed body. The byte length declares the form.
    pub fn from_packed(
        name: impl Into<String>,
        bytes: &[u8],
        sample_rate_hz: f64,
    ) -> Result<Self, String> {
        let form = ObjectForm::from_body_len(bytes.len()).ok_or_else(|| {
            format!(
                "{} bytes is neither a {}-byte square nor a {}-byte cube",
                bytes.len(),
                ObjectForm::Square.body_bytes(),
                ObjectForm::Cube.body_bytes()
            )
        })?;
        let capacity = form.section_capacity();
        // 240 legacy arrives through the crate, which mirrors the four corners
        // and pads the seventh section with the pass-through sentinel. Native
        // lengths are read straight, corner by corner.
        let words: Vec<Vec<[u16; 5]>> = if ObjectForm::is_legacy_len(bytes.len()) {
            let packed = PackedCorners::from_body_bytes(bytes).map_err(|e| e.to_string())?;
            (0..form.corner_count())
                .map(|ci| (0..capacity).map(|si| packed.words[ci][si]).collect())
                .collect()
        } else {
            let mut i = 0;
            (0..form.corner_count())
                .map(|_| {
                    (0..capacity)
                        .map(|_| {
                            let mut w = [0u16; 5];
                            for x in w.iter_mut() {
                                *x = u16::from_le_bytes([bytes[i], bytes[i + 1]]);
                                i += 2;
                            }
                            w
                        })
                        .collect()
                })
                .collect()
        };
        let lanes: Vec<LaneSpec> = (0..capacity)
            .map(|i| LaneSpec::new(LaneId(i as u32)))
            .collect();
        let frames = (0..form.corner_count())
            .map(|ci| {
                let address = form.address_of(ci);
                Frame {
                    label: form.label_of(&address),
                    address,
                    values: (0..capacity)
                        .map(|si| LaneValue::from_words(words[ci][si]))
                        .collect(),
                }
            })
            .collect();
        Ok(Self {
            version: PROJECT_VERSION,
            name: name.into(),
            object: Some(Object {
                form,
                origin: Origin::Imported,
                lanes,
                frames,
                sample_rate_hz,
            }),
        })
    }

    /// The bytes this object writes as. A square writes 240, a cube 560.
    pub fn to_body_bytes(&self) -> Result<Vec<u8>, String> {
        let o = self
            .object
            .as_ref()
            .ok_or_else(|| "there is no object to export".to_string())?;

        // Native layout for both forms: corner by corner, seven sections,
        // five little-endian words each. Corner topology differs; order does
        // not.
        let mut out = Vec::with_capacity(o.form.body_bytes());
        let mut ordered: Vec<Option<&Frame>> = vec![None; o.form.corner_count()];
        for (ci, frame) in o.frames.iter().enumerate() {
            let slot = o
                .form
                .index_of(&frame.address)
                .ok_or_else(|| format!("corner {ci} is not addressable in a {}", o.form.name()))?;
            ordered[slot] = Some(frame);
        }
        for (ci, frame) in ordered.iter().enumerate() {
            let frame = frame.ok_or_else(|| format!("corner {ci} is missing"))?;
            for si in 0..trench_core::cascade::NUM_STAGES {
                let w = frame
                    .values
                    .get(si)
                    .copied()
                    .unwrap_or(LaneValue::IDENTITY)
                    .words;
                for x in w {
                    out.extend_from_slice(&x.to_le_bytes());
                }
            }
        }
        Ok(out)
    }

    /// Swaps what two sections hold at one corner, crossing their trajectories
    /// deliberately without disturbing any other corner.
    pub fn swap_lanes_at_frame(&mut self, frame: usize, a: usize, b: usize) -> bool {
        if a == b {
            return false;
        }
        let Some(o) = &mut self.object else {
            return false;
        };
        let Some(f) = o.frames.get_mut(frame) else {
            return false;
        };
        if a >= f.values.len() || b >= f.values.len() {
            return false;
        }
        f.values.swap(a, b);
        true
    }

    /// Re-discretises every section onto a new rate, preserving the continuous
    /// frequencies the roots stand at.
    pub fn retune(&mut self, to_rate: f64) {
        let Some(o) = &mut self.object else { return };
        let from = o.sample_rate_hz;
        if !to_rate.is_finite() || to_rate <= 0.0 || (to_rate - from).abs() < 1e-9 {
            return;
        }
        for frame in &mut o.frames {
            for v in &mut frame.values {
                if v.is_identity() {
                    continue;
                }
                v.words = trench_core::stage_law::recompile_stage_words(v.words, from, to_rate);
            }
        }
        o.sample_rate_hz = to_rate;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn factory() -> Vec<u8> {
        std::fs::read("../ref/presets/P2k_013_talking_hedz.bin").expect("factory preset")
    }

    #[test]
    fn a_new_project_is_genuinely_empty() {
        let p = Project::empty();
        assert!(p.is_empty());
        assert_eq!(p.frames().len(), 0);
        assert_eq!(p.lane_capacity(), 0);
        assert!(p.active_lanes().is_empty());
        assert!(p.cascade_at(&[0.5, 0.5]).is_none());
        assert!(p.to_body_bytes().is_err());
    }

    /// A new object has capacity but nothing authored in it, and must not
    /// report sections it does not have.
    #[test]
    fn a_new_object_has_capacity_but_no_authored_sections() {
        for form in [ObjectForm::Square, ObjectForm::Cube] {
            let p = Project::new_object("t", form, 44_100.0);
            assert_eq!(p.frames().len(), form.corner_count());
            assert_eq!(p.lane_capacity(), form.section_capacity());
            assert!(
                p.active_lanes().is_empty(),
                "{form:?} reported authored sections in a new object"
            );
            assert_eq!(p.order_at(0), 0);
        }
    }

    #[test]
    fn body_length_selects_the_form_on_import() {
        let raw = factory();
        let p = Project::from_packed("f", &raw, 44_100.0).unwrap();
        let form = ObjectForm::from_body_len(raw.len()).unwrap();
        assert_eq!(p.form(), Some(form));
        assert_eq!(p.frames().len(), form.corner_count());
    }

    /// The six authored sections survive a legacy import unchanged.
    #[test]
    fn legacy_import_preserves_every_authored_word() {
        let raw = factory();
        let p = Project::from_packed("f", &raw, 44_100.0).unwrap();
        let packed = PackedCorners::from_body_bytes(&raw).unwrap();
        for ci in 0..4 {
            for si in 0..6 {
                assert_eq!(p.frames()[ci].values[si].words, packed.words[ci][si]);
            }
        }
    }

    /// Seven sections either way; only the corner count differs.
    #[test]
    fn a_square_writes_280_and_a_cube_560() {
        let sq = Project::new_object("s", ObjectForm::Square, 44_100.0);
        assert_eq!(sq.lane_capacity(), 7);
        assert_eq!(sq.to_body_bytes().unwrap().len(), 280);
        let cu = Project::new_object("c", ObjectForm::Cube, 44_100.0);
        assert_eq!(cu.lane_capacity(), 7);
        assert_eq!(cu.to_body_bytes().unwrap().len(), 560);
    }

    /// A legacy 240 body imports as a seven-section square whose seventh
    /// section is pass-through, and then writes native.
    #[test]
    fn legacy_import_becomes_a_seven_section_square() {
        let p = Project::from_packed("f", &factory(), 44_100.0).unwrap();
        assert_eq!(p.form(), Some(ObjectForm::Square));
        assert_eq!(p.lane_capacity(), 7);
        assert!(p.frames()[0].values[6].is_identity());
        let native = p.to_body_bytes().unwrap();
        assert_eq!(native.len(), 280);
        // And it reads back identically.
        let back = Project::from_packed("f", &native, 44_100.0).unwrap();
        assert_eq!(back.frames(), p.frames());
    }

    #[test]
    fn an_unrecognised_body_length_is_refused_with_the_reason() {
        let e = Project::from_packed("x", &[0u8; 100], 44_100.0).unwrap_err();
        assert!(e.contains("280"), "{e}");
        assert!(e.contains("560"), "{e}");
    }

    #[test]
    fn authoring_a_section_makes_it_active() {
        let mut p = Project::new_object("t", ObjectForm::Square, 44_100.0);
        assert!(p.active_lanes().is_empty());
        let o = p.object.as_mut().unwrap();
        o.frames[0].values[2]
            .set_roots(
                &trench_core::stage_law::StageRoots {
                    pole_hz: 700.0,
                    pole_r: 0.95,
                    zero_hz: 1400.0,
                    zero_r: 0.5,
                    scale: 1.0,
                },
                44_100.0,
            )
            .unwrap();
        assert_eq!(p.active_lanes(), vec![2]);
        assert_eq!(
            p.order_at(0),
            2,
            "one conjugate section is a 2nd-order path"
        );
    }

    #[test]
    fn interpolating_at_a_corner_returns_that_corner() {
        let p = Project::from_packed("f", &factory(), 44_100.0).unwrap();
        let form = p.form().unwrap();
        for ci in 0..form.corner_count() {
            let coords: Vec<f32> = form.address_of(ci).iter().map(|&c| c as f32).collect();
            assert_eq!(p.cascade_at(&coords).unwrap(), p.frames()[ci].values);
        }
    }

    #[test]
    fn retune_holds_root_frequencies_across_rates() {
        let mut p = Project::from_packed("f", &factory(), 39_062.5).unwrap();
        let before: Vec<Option<f64>> = p.frames()[1]
            .values
            .iter()
            .map(|v| match v.geometry(39_062.5).pole {
                trench_core::stage_law::RootPair::Conjugate { hz, .. } => Some(hz),
                _ => None,
            })
            .collect();
        p.retune(48_000.0);
        for (i, b) in before.iter().enumerate() {
            if let (Some(b), trench_core::stage_law::RootPair::Conjugate { hz, .. }) =
                (b, p.frames()[1].values[i].geometry(48_000.0).pole)
            {
                assert!(
                    (hz / b - 1.0).abs() < 0.06,
                    "section {i}: {b:.0} -> {hz:.0}"
                );
            }
        }
    }
}
