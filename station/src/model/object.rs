//! The two object forms.
//!
//! There are exactly two, and they are the two the packed format stores:
//!
//! - A **square** holds four corners — morph low/high against Q low/high — and
//!   six sections. That is the 240-byte body, and it is what a `.4d` file is.
//! - A **cube** holds eight corners, adding a third axis, and seven sections.
//!   That is the 560-byte body.
//!
//! Corner index is `m | q<<1 | t<<2`, which is how the runtime addresses them.
//! Nothing here generalises to more forms: an object is a square or a cube.

use serde::{Deserialize, Serialize};
use trench_core::minifloat::{LEGACY_CORNERS, LEGACY_STAGES, NUM_CORNERS};

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

    /// Sections the form carries. A square is a six-section object; a cube
    /// carries a seventh.
    pub fn section_capacity(self) -> usize {
        match self {
            ObjectForm::Square => LEGACY_STAGES,
            ObjectForm::Cube => trench_core::cascade::NUM_STAGES,
        }
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

    /// Byte length of the packed body for this form.
    pub fn body_bytes(self) -> usize {
        match self {
            ObjectForm::Square => trench_core::minifloat::LEGACY_BODY_BYTES,
            ObjectForm::Cube => trench_core::minifloat::BODY_BYTES,
        }
    }

    /// The form a body of this length is.
    pub fn from_body_len(len: usize) -> Option<Self> {
        match len {
            n if n == trench_core::minifloat::LEGACY_BODY_BYTES => Some(ObjectForm::Square),
            n if n == trench_core::minifloat::BODY_BYTES => Some(ObjectForm::Cube),
            _ => None,
        }
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

    #[test]
    fn body_length_identifies_the_form() {
        assert_eq!(ObjectForm::from_body_len(240), Some(ObjectForm::Square));
        assert_eq!(ObjectForm::from_body_len(560), Some(ObjectForm::Cube));
        assert_eq!(ObjectForm::from_body_len(241), None);
        assert_eq!(ObjectForm::Square.body_bytes(), 240);
        assert_eq!(ObjectForm::Cube.body_bytes(), 560);
    }

    #[test]
    fn a_square_is_written_as_4d_and_a_cube_as_cube() {
        assert_eq!(ObjectForm::Square.extension(), "4d");
        assert_eq!(ObjectForm::Cube.extension(), "cube");
    }
}
