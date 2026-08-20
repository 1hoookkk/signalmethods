use std::collections::VecDeque;

use super::Body;

const CAP: usize = 256;

#[derive(Default)]
pub struct History {
    undo: VecDeque<Body>,
    redo: Vec<Body>,
}

impl History {
    pub fn push(&mut self, before: Body) {
        self.redo.clear();
        if self.undo.len() == CAP {
            self.undo.pop_front();
        }
        self.undo.push_back(before);
    }

    pub fn undo(&mut self, current: &Body) -> Option<Body> {
        let previous = self.undo.pop_back()?;
        self.redo.push(current.clone());
        Some(previous)
    }

    pub fn redo(&mut self, current: &Body) -> Option<Body> {
        let next = self.redo.pop()?;
        self.undo.push_back(current.clone());
        Some(next)
    }

    pub fn depth(&self) -> usize {
        self.undo.len()
    }
}
