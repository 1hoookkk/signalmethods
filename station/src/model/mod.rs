//! The Station's versioned model.
//!
//! The layers are kept apart on purpose: topology is not frame data, frame data
//! is not the interpolation law, and none of them is the import/export format's
//! capability. Collapsing any two is how a Station starts claiming a format can
//! store something it cannot.

pub mod interp;
pub mod lane;
pub mod project;
pub mod store;
pub mod topology;
