use trench_core::arma_endpoint::{ArmaFit, Freedom, Zones, NO_ZONES};
use trench_core::cascade::NUM_STAGES;
use trench_core::response::biquad_cascade_mag_db;
use trench_core::stage_law::{RootPair, StageGeometry, StageRoots};

use crate::document::{Corner, Section, SECTIONS};

pub struct Plan {
    pub target: Vec<(f64, f64)>,
    pub seed: [StageRoots; NUM_STAGES],
    pub freedom: Freedom,
    pub writable: [bool; NUM_STAGES],
    pub grow: [bool; NUM_STAGES],
    pub zones: Zones,
    pub fixed: [bool; NUM_STAGES],
}

fn roots_of(g: &StageGeometry) -> Option<StageRoots> {
    let (pole_hz, pole_r) = match g.pole {
        RootPair::Conjugate { hz, r } => (hz, r),
        RootPair::Degenerate => (0.0, 0.0),
        RootPair::RealPair { .. } => return None,
    };
    let (zero_hz, zero_r) = match g.zero {
        RootPair::Conjugate { hz, r } => (hz, r),
        RootPair::Degenerate => (0.0, 0.0),
        RootPair::RealPair { .. } => return None,
    };
    Some(StageRoots {
        pole_hz,
        pole_r,
        zero_hz,
        zero_r,
        scale: g.scale,
    })
}

pub fn geometry_of(r: &StageRoots) -> StageGeometry {
    let pair = |hz: f64, rad: f64| {
        if rad <= 0.0 {
            RootPair::Degenerate
        } else {
            RootPair::Conjugate { hz, r: rad }
        }
    };
    StageGeometry {
        pole: pair(r.pole_hz, r.pole_r),
        zero: pair(r.zero_hz, r.zero_r),
        scale: r.scale,
    }
}

pub fn plan(corner: &Corner, grid: &[f64], sr: f64) -> Result<Plan, &'static str> {
    let target_db = corner.target.as_ref().ok_or("no target")?;
    if target_db.db.len() != grid.len() {
        return Err("target grid mismatch");
    }

    let mut seed = [StageRoots::IDENTITY; NUM_STAGES];
    let mut fixed = [false; NUM_STAGES];
    let mut writable = [false; NUM_STAGES];
    let mut grow = [false; NUM_STAGES];
    let mut freedom: Freedom = [[false; 4]; NUM_STAGES];

    for si in 0..SECTIONS {
        let Section { geometry, hold } = corner.sections[si];
        match roots_of(&geometry) {
            Some(r) => {
                seed[si] = r;
                writable[si] = !(hold.pole && hold.zero);
                grow[si] = corner.sections[si].is_empty() && !hold.pole && !hold.zero;
                freedom[si] = [!hold.pole, !hold.pole, !hold.zero, !hold.zero];
            }
            None => {
                fixed[si] = true;
                seed[si] = StageRoots::IDENTITY;
            }
        }
    }

    let mut target: Vec<(f64, f64)> = Vec::with_capacity(grid.len());
    for (i, &hz) in grid.iter().enumerate() {
        let mut db = target_db.db[i];
        for si in 0..SECTIONS {
            if fixed[si] {
                let row = corner.sections[si].geometry.biquad_at(sr);
                db -= biquad_cascade_mag_db(std::slice::from_ref(&row), hz, sr);
            }
        }
        target.push((hz, db));
    }

    Ok(Plan {
        target,
        seed,
        freedom,
        writable,
        grow,
        zones: NO_ZONES,
        fixed,
    })
}

pub fn install(plan: &Plan, fit: &ArmaFit, into: &mut [Section; SECTIONS]) {
    for si in 0..SECTIONS {
        if plan.fixed[si] || !plan.writable[si] {
            continue;
        }
        into[si].geometry = geometry_of(&fit.roots[si]);
    }
}
