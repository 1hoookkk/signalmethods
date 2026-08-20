use trench_core::cascade::NUM_COEFFS;
use trench_core::minifloat::PackedCorners;
use trench_core::stage_law::words_from_geometry_at;

use crate::document::{Body, Doc, CORNERS, SECTIONS};

pub fn pack(body: &Body) -> PackedCorners {
    let mut words = [[[0u16; NUM_COEFFS]; SECTIONS]; CORNERS];
    for (ci, corner) in words.iter_mut().enumerate() {
        for (si, stage) in corner.iter_mut().enumerate() {
            *stage = words_from_geometry_at(
                &body.corners[ci].sections[si].geometry,
                body.datum_sr_hz,
            );
        }
    }
    PackedCorners { words }
}

pub struct Derived {
    rev: u64,
    corner: usize,
    ride_key: [u16; 3],
    repacked: bool,
    pub packed: PackedCorners,
    pub biquads: [[f64; NUM_COEFFS]; SECTIONS],
    pub ride_biquads: [[f64; NUM_COEFFS]; SECTIONS],
}

fn ride_key(ride: [f32; 3]) -> [u16; 3] {
    std::array::from_fn(|i| (ride[i].clamp(0.0, 1.0) * 65535.0) as u16)
}

impl Derived {
    pub fn new(doc: &Doc, corner: usize, ride: [f32; 3]) -> Self {
        let mut d = Self {
            rev: u64::MAX,
            corner: usize::MAX,
            ride_key: [u16::MAX; 3],
            repacked: true,
            packed: pack(doc.body()),
            biquads: [[1.0, 0.0, 0.0, 0.0, 0.0]; SECTIONS],
            ride_biquads: [[1.0, 0.0, 0.0, 0.0, 0.0]; SECTIONS],
        };
        d.sync(doc, corner, ride);
        d
    }

    pub fn sync(&mut self, doc: &Doc, corner: usize, ride: [f32; 3]) {
        let key = ride_key(ride);
        let doc_changed = doc.rev() != self.rev;
        if doc_changed {
            self.packed = pack(doc.body());
            self.repacked = true;
        }
        if doc_changed || corner != self.corner {
            let body = doc.body();
            for (si, row) in self.biquads.iter_mut().enumerate() {
                *row = body.corners[corner].sections[si]
                    .geometry
                    .biquad_at(body.datum_sr_hz);
            }
        }
        if doc_changed || key != self.ride_key {
            self.ride_biquads = self.packed.interpolate_biquad(ride[0], ride[1], ride[2]);
        }
        self.rev = doc.rev();
        self.corner = corner;
        self.ride_key = key;
    }

    pub fn took_repack(&mut self) -> bool {
        std::mem::take(&mut self.repacked)
    }
}
