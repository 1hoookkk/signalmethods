pub mod edit;
pub mod history;

use std::sync::Arc;

use trench_core::cascade::NUM_STAGES;
use trench_core::minifloat::NUM_CORNERS;
use trench_core::stage_law::{RootPair, StageGeometry};

pub const CORNERS: usize = NUM_CORNERS;
pub const SECTIONS: usize = NUM_STAGES;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub struct Hold {
    pub pole: bool,
    pub zero: bool,
}

impl Hold {
    pub const FREE: Hold = Hold { pole: false, zero: false };

    pub fn any(&self) -> bool {
        self.pole || self.zero
    }

    pub fn all(&self) -> bool {
        self.pole && self.zero
    }
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Section {
    pub geometry: StageGeometry,
    pub hold: Hold,
}

impl Section {
    pub const EMPTY: Section = Section {
        geometry: StageGeometry::IDENTITY,
        hold: Hold::FREE,
    };

    pub fn is_empty(&self) -> bool {
        matches!(self.geometry.pole, RootPair::Degenerate)
            && matches!(self.geometry.zero, RootPair::Degenerate)
    }

    pub fn is_conjugate_expressible(&self) -> bool {
        !matches!(self.geometry.pole, RootPair::RealPair { .. })
            && !matches!(self.geometry.zero, RootPair::RealPair { .. })
    }
}

#[derive(PartialEq, Debug)]
pub struct Target {
    pub name: String,
    pub db: Vec<f64>,
}

#[derive(Clone, PartialEq, Debug)]
pub struct Corner {
    pub sections: [Section; SECTIONS],
    pub target: Option<Arc<Target>>,
    pub provenance: Option<Arc<str>>,
}

impl Corner {
    pub fn empty() -> Self {
        Self {
            sections: [Section::EMPTY; SECTIONS],
            target: None,
            provenance: None,
        }
    }
}

#[derive(Clone, PartialEq, Debug)]
pub struct Body {
    pub corners: [Corner; CORNERS],
    pub datum_sr_hz: f64,
    pub name: Option<String>,
}

impl Body {
    pub fn empty(datum_sr_hz: f64) -> Self {
        Self {
            corners: std::array::from_fn(|_| Corner::empty()),
            datum_sr_hz,
            name: None,
        }
    }

    pub fn seated(&self, corner: usize) -> bool {
        self.corners[corner].sections.iter().any(|s| !s.is_empty())
    }
}

pub struct Doc {
    body: Body,
    rev: u64,
}

impl Doc {
    pub fn new(body: Body) -> Self {
        Self { body, rev: 1 }
    }

    pub fn body(&self) -> &Body {
        &self.body
    }

    pub fn rev(&self) -> u64 {
        self.rev
    }

    pub(super) fn edit_in_place(&mut self, f: impl FnOnce(&mut Body)) {
        f(&mut self.body);
        self.rev = self.rev.wrapping_add(1);
    }

}
