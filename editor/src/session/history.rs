use crate::domain::document::Document;

pub struct History {
    undo: Vec<Document>,
    redo: Vec<Document>,
}

impl History {
    pub fn new() -> Self {
        Self {
            undo: Vec::new(),
            redo: Vec::new(),
        }
    }

    pub fn push(&mut self, doc: &Document) {
        self.undo.push(doc.clone());
        self.redo.clear();
        if self.undo.len() > 64 {
            self.undo.remove(0);
        }
    }

    pub fn undo(&mut self, current: &Document) -> Option<Document> {
        let prev = self.undo.pop()?;
        self.redo.push(current.clone());
        Some(prev)
    }

    pub fn redo(&mut self, current: &Document) -> Option<Document> {
        let next = self.redo.pop()?;
        self.undo.push(current.clone());
        Some(next)
    }

    pub fn depth(&self) -> usize {
        self.undo.len()
    }
}
