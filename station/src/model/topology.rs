//! Declared topology. Nothing here knows how many axes an object has.
//!
//! A frame address is one coordinate per declared axis. Corner addressing is
//! binary and little-endian in axis order — axis 0 varies fastest — which is
//! the addressing the packed runtime uses for `m | q<<1 | z<<2` and which
//! extends to any axis count without a second rule.

use serde::{Deserialize, Serialize};

/// One declared axis. `id` is the stable identity; `name` is what the
/// operator reads.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Axis {
    pub id: String,
    pub name: String,
}

impl Axis {
    pub fn new(id: impl Into<String>, name: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            name: name.into(),
        }
    }
}

/// The axes an object declares. A cube declares three; a 4D object declares
/// four. No count is privileged.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Topology {
    pub axes: Vec<Axis>,
}

impl Topology {
    pub fn new(axes: Vec<Axis>) -> Self {
        Self { axes }
    }

    pub fn axis_count(&self) -> usize {
        self.axes.len()
    }

    /// Corners of the declared object: two per axis.
    pub fn corner_count(&self) -> usize {
        1usize << self.axes.len()
    }

    /// The address of corner `index`, one 0/1 coordinate per axis, axis 0
    /// varying fastest.
    pub fn address_of(&self, index: usize) -> Vec<u8> {
        (0..self.axes.len())
            .map(|a| ((index >> a) & 1) as u8)
            .collect()
    }

    /// The corner index of an address. Returns `None` when the address does
    /// not match the declared axis count.
    pub fn index_of(&self, address: &[u8]) -> Option<usize> {
        if address.len() != self.axes.len() {
            return None;
        }
        Some(
            address
                .iter()
                .enumerate()
                .map(|(a, &c)| ((c != 0) as usize) << a)
                .sum(),
        )
    }

    /// Short operator-facing label for an address, e.g. `MORPH0 Q100 T1`.
    pub fn label_of(&self, address: &[u8]) -> String {
        self.axes
            .iter()
            .zip(address)
            .map(|(ax, &c)| format!("{}{}", ax.name, if c != 0 { "100" } else { "0" }))
            .collect::<Vec<_>>()
            .join(" ")
    }

    /// The three axes the packed runtime declares, in its own order.
    pub fn packed_runtime() -> Self {
        Self::new(vec![
            Axis::new("morph", "M"),
            Axis::new("q", "Q"),
            Axis::new("t2", "T"),
        ])
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cube() -> Topology {
        Topology::packed_runtime()
    }

    #[test]
    fn corner_count_follows_declared_axes() {
        assert_eq!(cube().corner_count(), 8);
        assert_eq!(Topology::new(vec![]).corner_count(), 1);
        let four = Topology::new(vec![
            Axis::new("a", "A"),
            Axis::new("b", "B"),
            Axis::new("c", "C"),
            Axis::new("d", "D"),
        ]);
        assert_eq!(four.corner_count(), 16);
    }

    #[test]
    fn address_matches_packed_runtime_bit_order() {
        // The runtime addresses corners as m | q<<1 | z<<2.
        let t = cube();
        for i in 0..t.corner_count() {
            let a = t.address_of(i);
            assert_eq!(a[0] as usize, i & 1);
            assert_eq!(a[1] as usize, (i >> 1) & 1);
            assert_eq!(a[2] as usize, (i >> 2) & 1);
        }
    }

    #[test]
    fn index_round_trips_every_corner_at_four_axes() {
        let t = Topology::new(vec![
            Axis::new("a", "A"),
            Axis::new("b", "B"),
            Axis::new("c", "C"),
            Axis::new("d", "D"),
        ]);
        for i in 0..t.corner_count() {
            assert_eq!(t.index_of(&t.address_of(i)), Some(i));
        }
    }

    #[test]
    fn index_refuses_an_address_of_the_wrong_rank() {
        assert_eq!(cube().index_of(&[0, 1]), None);
        assert_eq!(cube().index_of(&[0, 1, 0, 1]), None);
    }
}
