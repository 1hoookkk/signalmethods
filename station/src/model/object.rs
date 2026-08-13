//! The two object forms.
//!
//! The form decides corner topology and nothing else.
//!
//! - A **square** holds four corners: morph low/high against Q low/high. That
//!   is a `.4d` file, 280 native bytes.
//! - A **cube** holds eight corners, adding a third axis. 560 native bytes.
//!
//! Both carry seven equal pole/zero sections, up to 14th order. There is no
//! privileged section. The 240-byte six-section body is read for import only.
//!
//! Corner index is `m | q<<1 | t<<2`, which is how the runtime addresses them.
//! Nothing here generalises to more forms: an object is a square or a cube.

use serde::{Deserialize, Serialize};
use trench_core::cascade::NUM_COEFFS;
use trench_core::minifloat::{LEGACY_BODY_BYTES, LEGACY_CORNERS, NUM_CORNERS};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum ObjectForm {
    /// Four corners, morph against Q.
    Square,
    /// Eight corners, morph against Q against the third axis.
    Cube,
}

impl ObjectForm {
    pub fn corner_count(self) -> usize {
        match self {
            ObjectForm::Square => LEGACY_CORNERS,
            ObjectForm::Cube => NUM_CORNERS,
        }
    }

    /// Sections are seven whatever the form. Corner topology and filter order
    /// are separate concerns: a square has fewer corners than a cube, not a
    /// shorter cascade. Seven equal pole/zero sections, up to 14th order.
    pub fn section_capacity(self) -> usize {
        trench_core::cascade::NUM_STAGES
    }

    pub fn axis_count(self) -> usize {
        match self {
            ObjectForm::Square => 2,
            ObjectForm::Cube => 3,
        }
    }

    pub fn axis_names(self) -> &'static [&'static str] {
        match self {
            ObjectForm::Square => &["MORPH", "Q"],
            ObjectForm::Cube => &["MORPH", "Q", "T"],
        }
    }

    pub fn name(self) -> &'static str {
        match self {
            ObjectForm::Square => "square",
            ObjectForm::Cube => "cube",
        }
    }

    /// The file extension this form is written as.
    pub fn extension(self) -> &'static str {
        match self {
            ObjectForm::Square => "4d",
            ObjectForm::Cube => "cube",
        }
    }

    /// Native body length: corners x 7 sections x 5 words x 2 bytes.
    pub fn body_bytes(self) -> usize {
        self.corner_count() * trench_core::cascade::NUM_STAGES * NUM_COEFFS * 2
    }

    /// The form a body of this length is.
    ///
    /// 240 bytes is the legacy six-section square, read for import only; it
    /// arrives as a square and gains a seventh section that starts as
    /// pass-through, so the object is a full seven-lane one from then on.
    pub fn from_body_len(len: usize) -> Option<Self> {
        match len {
            n if n == LEGACY_BODY_BYTES => Some(ObjectForm::Square),
            n if n == ObjectForm::Square.body_bytes() => Some(ObjectForm::Square),
            n if n == ObjectForm::Cube.body_bytes() => Some(ObjectForm::Cube),
            _ => None,
        }
    }

    /// True when this length is the legacy import-only body.
    pub fn is_legacy_len(len: usize) -> bool {
        len == LEGACY_BODY_BYTES
    }

    /// One 0/1 coordinate per axis for a corner, morph varying fastest.
    pub fn address_of(self, index: usize) -> Vec<u8> {
        (0..self.axis_count())
            .map(|a| ((index >> a) & 1) as u8)
            .collect()
    }

    pub fn index_of(self, address: &[u8]) -> Option<usize> {
        if address.len() != self.axis_count() {
            return None;
        }
        let i: usize = address
            .iter()
            .enumerate()
            .map(|(a, &c)| ((c != 0) as usize) << a)
            .sum();
        (i < self.corner_count()).then_some(i)
    }

    /// How a corner reads: `M0 Q100`, and on a cube also the third axis.
    pub fn label_of(self, address: &[u8]) -> String {
        self.axis_names()
            .iter()
            .zip(address)
            .map(|(n, &c)| format!("{}{}", &n[..1], if c != 0 { "100" } else { "0" }))
            .collect::<Vec<_>>()
            .join(" ")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_square_is_four_corners_and_a_cube_is_eight() {
        assert_eq!(ObjectForm::Square.corner_count(), 4);
        assert_eq!(ObjectForm::Cube.corner_count(), 8);
        assert_eq!(ObjectForm::Square.axis_count(), 2);
        assert_eq!(ObjectForm::Cube.axis_count(), 3);
    }

    /// The four corners of a square are morph against Q, in that order.
    #[test]
    fn square_corners_are_morph_against_q() {
        let f = ObjectForm::Square;
        assert_eq!(f.label_of(&f.address_of(0)), "M0 Q0");
        assert_eq!(f.label_of(&f.address_of(1)), "M100 Q0");
        assert_eq!(f.label_of(&f.address_of(2)), "M0 Q100");
        assert_eq!(f.label_of(&f.address_of(3)), "M100 Q100");
    }

    #[test]
    fn every_corner_round_trips_through_its_address() {
        for f in [ObjectForm::Square, ObjectForm::Cube] {
            for i in 0..f.corner_count() {
                assert_eq!(f.index_of(&f.address_of(i)), Some(i));
            }
            assert_eq!(f.index_of(&[0]), None, "wrong rank must not address");
        }
    }

    /// Corner topology is the only thing the form decides. Order is seven
    /// sections either way.
    #[test]
    fn form_sets_corners_not_order() {
        assert_eq!(ObjectForm::Square.section_capacity(), 7);
        assert_eq!(ObjectForm::Cube.section_capacity(), 7);
    }

    #[test]
    fn native_lengths_are_corners_times_seven_sections() {
        assert_eq!(ObjectForm::Square.body_bytes(), 280);
        assert_eq!(ObjectForm::Cube.body_bytes(), 560);
        assert_eq!(ObjectForm::from_body_len(280), Some(ObjectForm::Square));
        assert_eq!(ObjectForm::from_body_len(560), Some(ObjectForm::Cube));
        assert_eq!(ObjectForm::from_body_len(241), None);
    }

    #[test]
    fn the_legacy_body_is_import_only() {
        assert!(ObjectForm::is_legacy_len(240));
        assert_eq!(ObjectForm::from_body_len(240), Some(ObjectForm::Square));
        assert!(!ObjectForm::is_legacy_len(280));
    }

    #[test]
    fn a_square_is_written_as_4d_and_a_cube_as_cube() {
        assert_eq!(ObjectForm::Square.extension(), "4d");
        assert_eq!(ObjectForm::Cube.extension(), "cube");
    }
}
