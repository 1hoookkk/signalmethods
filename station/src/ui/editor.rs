//! A custom-painted text editor.
//!
//! This is where laws and grammar are edited, so it has to be a real editor:
//! caret movement, selection, clipboard, scrolling, and errors reported at the
//! line and column that caused them. It is painted mark by mark like everything
//! else — there is no egui text field anywhere in the Station.

use eframe::egui::{Key, Pos2, Rect, Vec2};

use super::input::{Id, Ui};
use super::paint::{self, fill, hairline, outline, text};
use super::theme;

/// A parse or validation failure, located in the buffer.
#[derive(Clone, Debug, PartialEq)]
pub struct Located {
    /// 1-based, as an operator counts them.
    pub line: usize,
    pub column: usize,
    pub message: String,
    /// False for a warning that does not block applying.
    pub fatal: bool,
}

/// The editable buffer. `text` is the authority; caret and anchor are byte
/// offsets into it and are kept on character boundaries.
pub struct TextEditor {
    pub text: String,
    pub caret: usize,
    pub anchor: usize,
    pub scroll: f32,
    /// The text as last applied, for dirty state and revert.
    pub committed: String,
    pub errors: Vec<Located>,
}

impl TextEditor {
    pub fn new(text: impl Into<String>) -> Self {
        let text = text.into();
        Self {
            caret: 0,
            anchor: 0,
            scroll: 0.0,
            committed: text.clone(),
            errors: Vec::new(),
            text,
        }
    }

    pub fn dirty(&self) -> bool {
        self.text != self.committed
    }

    pub fn commit(&mut self) {
        self.committed = self.text.clone();
    }

    pub fn revert(&mut self) {
        self.text = self.committed.clone();
        self.clamp();
    }

    pub fn set(&mut self, text: impl Into<String>) {
        self.text = text.into();
        self.committed = self.text.clone();
        self.caret = 0;
        self.anchor = 0;
        self.scroll = 0.0;
    }

    fn clamp(&mut self) {
        self.caret = self.caret.min(self.text.len());
        self.anchor = self.anchor.min(self.text.len());
        while self.caret > 0 && !self.text.is_char_boundary(self.caret) {
            self.caret -= 1;
        }
        while self.anchor > 0 && !self.text.is_char_boundary(self.anchor) {
            self.anchor -= 1;
        }
    }

    pub fn selection(&self) -> (usize, usize) {
        (self.caret.min(self.anchor), self.caret.max(self.anchor))
    }

    pub fn has_selection(&self) -> bool {
        self.caret != self.anchor
    }

    pub fn selected_text(&self) -> String {
        let (a, b) = self.selection();
        self.text[a..b].to_string()
    }

    fn delete_selection(&mut self) {
        let (a, b) = self.selection();
        if a != b {
            self.text.replace_range(a..b, "");
            self.caret = a;
            self.anchor = a;
        }
    }

    pub fn insert(&mut self, s: &str) {
        self.delete_selection();
        self.text.insert_str(self.caret, s);
        self.caret += s.len();
        self.anchor = self.caret;
    }

    /// Byte offset of the start of `line` (0-based).
    fn line_start(&self, line: usize) -> usize {
        if line == 0 {
            return 0;
        }
        let mut seen = 0;
        for (i, c) in self.text.char_indices() {
            if c == '\n' {
                seen += 1;
                if seen == line {
                    return i + 1;
                }
            }
        }
        self.text.len()
    }

    /// Line and column (both 0-based) of a byte offset.
    pub fn line_col(&self, offset: usize) -> (usize, usize) {
        let upto = &self.text[..offset.min(self.text.len())];
        let line = upto.matches('\n').count();
        let col = upto.len() - upto.rfind('\n').map(|i| i + 1).unwrap_or(0);
        (line, col)
    }

    fn offset_of(&self, line: usize, col: usize) -> usize {
        let start = self.line_start(line);
        let rest = &self.text[start..];
        let len = rest.find('\n').unwrap_or(rest.len());
        start + col.min(len)
    }

    fn prev_boundary(&self, i: usize) -> usize {
        let mut j = i.saturating_sub(1);
        while j > 0 && !self.text.is_char_boundary(j) {
            j -= 1;
        }
        j
    }

    fn next_boundary(&self, i: usize) -> usize {
        let mut j = (i + 1).min(self.text.len());
        while j < self.text.len() && !self.text.is_char_boundary(j) {
            j += 1;
        }
        j
    }

    /// Applies one frame of keyboard and clipboard input. Returns true when the
    /// buffer changed.
    pub fn handle_input(&mut self, ui: &mut Ui, id: Id) -> bool {
        let before = self.text.clone();
        let shift = ui.input.shift;
        let ctrl = ui.input.ctrl;

        if ctrl && ui.input.key(Key::A) {
            self.anchor = 0;
            self.caret = self.text.len();
        }
        if ctrl && (ui.input.key(Key::C) || ui.input.key(Key::X)) && self.has_selection() {
            let s = self.selected_text();
            ui.p.ctx().copy_text(s);
            if ui.input.key(Key::X) {
                self.delete_selection();
            }
        }
        if let Some(p) = &ui.input.paste {
            if ui.focused(id) {
                let p = p.clone();
                self.insert(&p);
            }
        }

        for ch in ui.input.text.chars() {
            if !ch.is_control() {
                let mut b = [0u8; 4];
                self.insert(ch.encode_utf8(&mut b));
            }
        }
        if ui.input.key(Key::Enter) {
            self.insert("\n");
        }
        if ui.input.key(Key::Tab) {
            self.insert("  ");
        }
        if ui.input.key(Key::Backspace) {
            if self.has_selection() {
                self.delete_selection();
            } else if self.caret > 0 {
                let p = self.prev_boundary(self.caret);
                self.text.replace_range(p..self.caret, "");
                self.caret = p;
                self.anchor = p;
            }
        }
        if ui.input.key(Key::Delete) {
            if self.has_selection() {
                self.delete_selection();
            } else if self.caret < self.text.len() {
                let n = self.next_boundary(self.caret);
                self.text.replace_range(self.caret..n, "");
            }
        }

        let (line, col) = self.line_col(self.caret);
        let mut moved = None;
        if ui.input.key(Key::ArrowLeft) {
            moved = Some(self.prev_boundary(self.caret));
        }
        if ui.input.key(Key::ArrowRight) {
            moved = Some(self.next_boundary(self.caret));
        }
        if ui.input.key(Key::ArrowUp) && line > 0 {
            moved = Some(self.offset_of(line - 1, col));
        }
        if ui.input.key(Key::ArrowDown) {
            moved = Some(self.offset_of(line + 1, col));
        }
        if ui.input.key(Key::Home) {
            moved = Some(self.line_start(line));
        }
        if ui.input.key(Key::End) {
            moved = Some(self.offset_of(line, usize::MAX));
        }
        if let Some(m) = moved {
            self.caret = m;
            if !shift {
                self.anchor = m;
            }
        }

        self.clamp();
        self.text != before
    }

    /// Paints the editor and handles pointer selection. Returns true when the
    /// buffer changed this frame.
    pub fn show(&mut self, ui: &mut Ui, id: Id, rect: Rect) -> bool {
        fill(ui.p, rect, theme::PANEL);
        outline(
            ui.p,
            rect,
            if ui.focused(id) {
                theme::RULE_HI
            } else {
                theme::RULE
            },
        );

        let fs = theme::T_BODY;
        let lh = paint::line_height(ui.p, fs).max(12.0);
        let cw = paint::char_width(ui.p, fs);
        let gutter = cw * 4.0 + 8.0;
        let inner = Rect::from_min_max(
            Pos2::new(rect.left() + gutter, rect.top() + 4.0),
            Pos2::new(rect.right() - 6.0, rect.bottom() - 4.0),
        );

        let r = ui.region(id, rect);
        if r.hovered || r.held {
            self.scroll -= ui.input.scroll.y;
        }
        let mut line_len: Vec<usize> = self.text.split('\n').map(|l| l.len()).collect();
        let content_h = line_len.len() as f32 * lh;
        self.scroll = self
            .scroll
            .clamp(0.0, (content_h - inner.height()).max(0.0));

        // Pointer places the caret, and dragging extends the selection.
        if let Some(q) = r.pointer {
            if r.pressed || (r.held && ui.input.down) {
                let li = (((q.y - inner.top() + self.scroll) / lh).floor().max(0.0) as usize)
                    .min(line_len.len().saturating_sub(1));
                let ci = (((q.x - inner.left()) / cw).round().max(0.0) as usize).min(line_len[li]);
                let off = self.offset_of(li, ci);
                self.caret = off;
                if r.pressed && !ui.input.shift {
                    self.anchor = off;
                }
            }
        }

        let changed = if ui.focused(id) {
            self.handle_input(ui, id)
        } else {
            false
        };
        if changed {
            line_len = self.text.split('\n').map(|l| l.len()).collect();
        }
        let lines: Vec<String> = self.text.split('\n').map(|s| s.to_string()).collect();
        let content_h = line_len.len() as f32 * lh;

        // Keep the caret in view after any movement.
        let (cl, _) = self.line_col(self.caret);
        let cy = cl as f32 * lh;
        if cy < self.scroll {
            self.scroll = cy;
        } else if cy + lh > self.scroll + inner.height() {
            self.scroll = cy + lh - inner.height();
        }

        let clip = ui.clipped(rect);
        let (sa, sb) = self.selection();
        let first = (self.scroll / lh).floor().max(0.0) as usize;
        let last = ((self.scroll + inner.height()) / lh).ceil() as usize + 1;

        for (li, line_text) in lines
            .iter()
            .enumerate()
            .take(last.min(lines.len()))
            .skip(first)
        {
            let y = inner.top() + li as f32 * lh - self.scroll;
            let ls = self.line_start(li);
            let le = ls + line_text.len();

            // Error rows are marked across the whole line, in the gutter and
            // behind the text, so a located failure is impossible to miss.
            if let Some(e) = self.errors.iter().find(|e| e.line == li + 1) {
                let c = if e.fatal { theme::BAD } else { theme::WARN };
                fill(
                    &clip,
                    Rect::from_min_size(Pos2::new(rect.left(), y), Vec2::new(rect.width(), lh)),
                    theme::mix(theme::PANEL, c, 0.16),
                );
            }

            // Selection behind the glyphs.
            if sb > ls && sa < le + 1 {
                let a = sa.clamp(ls, le) - ls;
                let b = sb.clamp(ls, le) - ls;
                if b > a {
                    fill(
                        &clip,
                        Rect::from_min_size(
                            Pos2::new(inner.left() + a as f32 * cw, y),
                            Vec2::new((b - a) as f32 * cw, lh),
                        ),
                        theme::SELECT,
                    );
                }
            }

            text(
                &clip,
                Pos2::new(rect.left() + 6.0, y),
                format!("{:>3}", li + 1),
                theme::T_MICRO,
                if self.errors.iter().any(|e| e.line == li + 1) {
                    theme::BAD
                } else {
                    theme::FAINT
                },
            );
            text(
                &clip,
                Pos2::new(inner.left(), y),
                &lines[li],
                fs,
                theme::INK,
            );
        }

        // Caret.
        if ui.focused(id) {
            let (cl, cc) = self.line_col(self.caret);
            let x = inner.left() + cc as f32 * cw;
            let y = inner.top() + cl as f32 * lh - self.scroll;
            hairline(&clip, Pos2::new(x, y), Pos2::new(x, y + lh), theme::ACCENT);
        }

        // Scrollbar.
        if content_h > inner.height() {
            let frac = inner.height() / content_h;
            let h = (inner.height() * frac).max(18.0);
            let t = self.scroll / (content_h - inner.height());
            fill(
                ui.p,
                Rect::from_min_size(
                    Pos2::new(rect.right() - 4.0, inner.top() + (inner.height() - h) * t),
                    Vec2::new(3.0, h),
                ),
                theme::RULE_HI,
            );
        }
        changed
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ed(s: &str) -> TextEditor {
        TextEditor::new(s)
    }

    #[test]
    fn line_col_locates_offsets_the_way_an_operator_counts() {
        let e = ed("abc\ndefg\nhi");
        assert_eq!(e.line_col(0), (0, 0));
        assert_eq!(e.line_col(4), (1, 0));
        assert_eq!(e.line_col(6), (1, 2));
        assert_eq!(e.line_col(9), (2, 0));
    }

    #[test]
    fn insert_replaces_the_selection() {
        let mut e = ed("hello world");
        e.anchor = 0;
        e.caret = 5;
        e.insert("bye");
        assert_eq!(e.text, "bye world");
        assert_eq!(e.caret, 3);
        assert!(!e.has_selection());
    }

    #[test]
    fn dirty_tracks_committed_text_and_revert_restores_it() {
        let mut e = ed("a");
        assert!(!e.dirty());
        e.insert("b");
        assert!(e.dirty());
        e.revert();
        assert_eq!(e.text, "a");
        assert!(!e.dirty());
        e.insert("c");
        e.commit();
        assert!(!e.dirty());
    }

    #[test]
    fn caret_stays_on_character_boundaries() {
        let mut e = ed("héllo");
        e.caret = 2; // inside the two-byte 'é'
        e.clamp();
        assert!(e.text.is_char_boundary(e.caret));
    }

    #[test]
    fn offset_of_clamps_to_the_end_of_a_short_line() {
        let e = ed("abc\nx");
        // Column 99 on line 1 is the end of that line, not the next one.
        assert_eq!(e.offset_of(1, 99), 5);
        assert_eq!(e.offset_of(0, 99), 3);
    }
}
