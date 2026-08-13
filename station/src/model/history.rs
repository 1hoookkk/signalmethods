//! Undo and redo.
//!
//! Snapshots rather than inverse operations: a project is small enough that a
//! clone is cheap, and a snapshot cannot disagree with the edit that produced
//! it the way a hand-written inverse can.

use super::project::Project;

pub struct History {
    past: Vec<Project>,
    future: Vec<Project>,
    /// The state last saved to disk, for dirty tracking.
    saved: Option<Project>,
    limit: usize,
}

impl History {
    pub fn new(current: &Project) -> Self {
        Self {
            past: Vec::new(),
            future: Vec::new(),
            saved: Some(current.clone()),
            limit: 128,
        }
    }

    /// Records the state *before* an edit. Call this immediately before
    /// mutating, with the unmodified project.
    pub fn record(&mut self, before: &Project) {
        self.past.push(before.clone());
        if self.past.len() > self.limit {
            self.past.remove(0);
        }
        self.future.clear();
    }

    pub fn can_undo(&self) -> bool {
        !self.past.is_empty()
    }

    pub fn can_redo(&self) -> bool {
        !self.future.is_empty()
    }

    /// Swaps `current` for the previous state, keeping the replaced one for
    /// redo.
    pub fn undo(&mut self, current: &mut Project) -> bool {
        if let Some(prev) = self.past.pop() {
            self.future.push(std::mem::replace(current, prev));
            true
        } else {
            false
        }
    }

    pub fn redo(&mut self, current: &mut Project) -> bool {
        if let Some(next) = self.future.pop() {
            self.past.push(std::mem::replace(current, next));
            true
        } else {
            false
        }
    }

    pub fn mark_saved(&mut self, current: &Project) {
        self.saved = Some(current.clone());
    }

    /// Dirty is measured against what is on disk, not against whether an edit
    /// happened: undoing back to the saved state is not dirty.
    pub fn dirty(&self, current: &Project) -> bool {
        self.saved.as_ref() != Some(current)
    }

    /// Forgets all history, for a newly opened project.
    pub fn reset(&mut self, current: &Project) {
        self.past.clear();
        self.future.clear();
        self.saved = Some(current.clone());
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::lane::LaneValue;
    use crate::model::topology::Topology;

    fn proj() -> Project {
        Project::blank("t", Topology::packed_runtime(), 3)
    }

    fn edit(p: &mut Project, w: u16) {
        p.frames[0].values[0] = LaneValue::from_words([w; 5]);
    }

    #[test]
    fn undo_and_redo_walk_the_edit_chain() {
        let mut p = proj();
        let mut h = History::new(&p);
        assert!(!h.can_undo() && !h.can_redo());

        h.record(&p);
        edit(&mut p, 111);
        h.record(&p);
        edit(&mut p, 222);

        assert!(h.undo(&mut p));
        assert_eq!(p.frames[0].values[0].words[0], 111);
        assert!(h.undo(&mut p));
        assert!(p.frames[0].values[0].is_identity());
        assert!(!h.can_undo());

        assert!(h.redo(&mut p));
        assert_eq!(p.frames[0].values[0].words[0], 111);
        assert!(h.redo(&mut p));
        assert_eq!(p.frames[0].values[0].words[0], 222);
        assert!(!h.can_redo());
    }

    #[test]
    fn a_new_edit_discards_the_redo_branch() {
        let mut p = proj();
        let mut h = History::new(&p);
        h.record(&p);
        edit(&mut p, 1);
        h.undo(&mut p);
        assert!(h.can_redo());
        h.record(&p);
        edit(&mut p, 2);
        assert!(!h.can_redo(), "redo survived a divergent edit");
    }

    #[test]
    fn undoing_back_to_the_saved_state_is_not_dirty() {
        let mut p = proj();
        let mut h = History::new(&p);
        assert!(!h.dirty(&p));
        h.record(&p);
        edit(&mut p, 9);
        assert!(h.dirty(&p));
        h.undo(&mut p);
        assert!(
            !h.dirty(&p),
            "undo returned to the saved state but reads dirty"
        );
    }

    #[test]
    fn saving_clears_dirty_at_the_current_state() {
        let mut p = proj();
        let mut h = History::new(&p);
        h.record(&p);
        edit(&mut p, 5);
        assert!(h.dirty(&p));
        h.mark_saved(&p);
        assert!(!h.dirty(&p));
    }
}
