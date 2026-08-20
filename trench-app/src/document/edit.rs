use super::history::History;
use super::{Body, Doc};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Gesture {
    pub corner: usize,
    pub section: usize,
}

pub struct Session {
    pub doc: Doc,
    pub history: History,
    open: Option<(Gesture, Option<Body>)>,
}

impl Session {
    pub fn new(body: Body) -> Self {
        Self {
            doc: Doc::new(body),
            history: History::default(),
            open: None,
        }
    }

    pub fn edit(&mut self, f: impl FnOnce(&mut Body)) {
        match &mut self.open {
            Some((_, pending)) => {
                if let Some(before) = pending.take() {
                    self.history.push(before);
                }
            }
            None => self.history.push(self.doc.body().clone()),
        }
        self.doc.edit_in_place(f);
    }

    pub fn begin_gesture(&mut self, id: Gesture) {
        self.open = Some((id, Some(self.doc.body().clone())));
    }

    pub fn gesture(&self) -> Option<Gesture> {
        self.open.as_ref().map(|(g, _)| *g)
    }

    pub fn end_gesture(&mut self) {
        self.open = None;
    }

    pub fn replace(&mut self, body: Body) {
        self.open = None;
        self.history.push(self.doc.body().clone());
        self.doc.edit_in_place(|b| *b = body);
    }

    pub fn seat(&mut self, body: Body) {
        self.open = None;
        self.history = History::default();
        self.doc.edit_in_place(|b| *b = body);
    }

    pub fn undo(&mut self) -> bool {
        let Some(previous) = self.history.undo(self.doc.body()) else {
            return false;
        };
        self.doc.edit_in_place(|b| *b = previous);
        true
    }

    pub fn redo(&mut self) -> bool {
        let Some(next) = self.history.redo(self.doc.body()) else {
            return false;
        };
        self.doc.edit_in_place(|b| *b = next);
        true
    }
}
