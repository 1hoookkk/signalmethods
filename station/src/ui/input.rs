//! The interaction core.
//!
//! Every control in the Station is custom painted, so every control also needs
//! hit testing, press tracking and focus. That logic lives here once. Views ask
//! `Ui::region` for a response and never test the pointer themselves, which is
//! what keeps hover, press-and-drag-off, click-release and keyboard focus
//! behaving the same way everywhere.

use std::collections::HashMap;
use std::hash::{Hash, Hasher};

use eframe::egui::{Key, Painter, Pos2, Rect, Vec2};

/// Identity of an interactive region, derived from a caller-supplied path so
/// it is stable across frames without a retained widget tree.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Id(pub u64);

impl Id {
    pub fn of(path: impl Hash) -> Self {
        let mut h = std::collections::hash_map::DefaultHasher::new();
        path.hash(&mut h);
        Id(h.finish())
    }

    pub fn child(self, part: impl Hash) -> Self {
        let mut h = std::collections::hash_map::DefaultHasher::new();
        self.0.hash(&mut h);
        part.hash(&mut h);
        Id(h.finish())
    }
}

/// What happened to one region this frame.
#[derive(Clone, Copy, Debug)]
pub struct Response {
    pub rect: Rect,
    pub hovered: bool,
    /// The button went down inside this region this frame.
    pub pressed: bool,
    /// The button was released inside the region that received the press.
    pub clicked: bool,
    /// This region owns the drag, whether or not the pointer is still inside.
    pub held: bool,
    pub drag_delta: Vec2,
    /// Pointer position, when there is one.
    pub pointer: Option<Pos2>,
    pub double_clicked: bool,
    pub scroll: f32,
}

impl Default for Response {
    fn default() -> Self {
        Self {
            rect: Rect::NOTHING,
            hovered: false,
            pressed: false,
            clicked: false,
            held: false,
            drag_delta: Vec2::ZERO,
            pointer: None,
            double_clicked: false,
            scroll: 0.0,
        }
    }
}

impl Response {
    pub fn empty() -> Self {
        Self::default()
    }
}

/// Per-frame pointer and keyboard snapshot, taken once and passed down.
#[derive(Clone, Default)]
pub struct InputFrame {
    pub pointer: Option<Pos2>,
    pub down: bool,
    pub pressed: bool,
    pub released: bool,
    pub double_click: bool,
    pub delta: Vec2,
    pub scroll: Vec2,
    pub keys: Vec<Key>,
    pub text: String,
    /// Clipboard content delivered by the host this frame.
    pub paste: Option<String>,
    pub shift: bool,
    pub ctrl: bool,
    pub alt: bool,
}

impl InputFrame {
    pub fn read(ctx: &eframe::egui::Context) -> Self {
        ctx.input(|i| {
            let keys = [
                Key::ArrowLeft, Key::ArrowRight, Key::ArrowUp, Key::ArrowDown,
                Key::Backspace, Key::Delete, Key::Enter, Key::Tab, Key::Escape,
                Key::Home, Key::End, Key::PageUp, Key::PageDown,
                Key::A, Key::C, Key::V, Key::X, Key::Z, Key::Y, Key::S,
                Key::N, Key::O, Key::E, Key::R, Key::F,
            ]
            .into_iter()
            .filter(|k| i.key_pressed(*k))
            .collect();
            let text = i
                .events
                .iter()
                .filter_map(|e| match e {
                    eframe::egui::Event::Text(t) => Some(t.clone()),
                    _ => None,
                })
                .collect::<String>();
            let paste = i.events.iter().find_map(|e| match e {
                eframe::egui::Event::Paste(t) => Some(t.clone()),
                _ => None,
            });
            Self {
                paste,
                pointer: i.pointer.interact_pos(),
                down: i.pointer.primary_down(),
                pressed: i.pointer.primary_pressed(),
                released: i.pointer.primary_released(),
                double_click: i.pointer.button_double_clicked(eframe::egui::PointerButton::Primary),
                delta: i.pointer.delta(),
                scroll: i.raw_scroll_delta,
                keys,
                text,
                shift: i.modifiers.shift,
                ctrl: i.modifiers.ctrl || i.modifiers.mac_cmd,
                alt: i.modifiers.alt,
            }
        })
    }

    pub fn key(&self, k: Key) -> bool {
        self.keys.contains(&k)
    }
}

/// State that must outlive a frame: what is being dragged, what has focus,
/// and where each scrollable region is parked.
#[derive(Default)]
pub struct UiState {
    pub active: Option<Id>,
    pub focus: Option<Id>,
    pub scroll: HashMap<Id, f32>,
    /// Value a drag started from, so a drag is absolute rather than an
    /// accumulation of rounded deltas.
    pub drag_origin: Option<(Id, Pos2, f64)>,
    /// Regions claimed this frame, innermost last, for overlap resolution.
    claimed: Vec<(Id, Rect)>,
}

impl UiState {
    pub fn begin_frame(&mut self) {
        self.claimed.clear();
    }

    pub fn has_focus(&self, id: Id) -> bool {
        self.focus == Some(id)
    }
}

/// The painting and interaction context handed to every view.
pub struct Ui<'a> {
    pub p: &'a Painter,
    pub input: &'a InputFrame,
    pub state: &'a mut UiState,
    /// Regions outside this clip do not receive the pointer. Set by scrollers.
    pub clip: Rect,
}

impl<'a> Ui<'a> {
    pub fn new(p: &'a Painter, input: &'a InputFrame, state: &'a mut UiState, clip: Rect) -> Self {
        Self {
            p,
            input,
            state,
            clip,
        }
    }

    /// Claims a rectangle for `id` and reports what happened to it.
    ///
    /// A region that owns the active drag keeps reporting `held` even when the
    /// pointer leaves it, which is what makes a slider survive a fast drag.
    pub fn region(&mut self, id: Id, rect: Rect) -> Response {
        let pointer = self.input.pointer.filter(|q| self.clip.contains(*q));
        let inside = pointer.map(|q| rect.contains(q)).unwrap_or(false);
        let active = self.state.active == Some(id);

        let mut r = Response {
            rect,
            hovered: inside && self.state.active.is_none(),
            pointer,
            scroll: if inside { self.input.scroll.y } else { 0.0 },
            ..Default::default()
        };

        if inside && self.input.pressed && self.state.active.is_none() {
            self.state.active = Some(id);
            self.state.focus = Some(id);
            r.pressed = true;
            r.double_clicked = self.input.double_click;
        }
        if active {
            r.held = true;
            r.hovered = inside;
            r.drag_delta = self.input.delta;
            if self.input.released {
                r.clicked = inside;
                self.state.active = None;
            }
        }
        r
    }

    /// Clicking empty space clears focus, so a text editor gives it up the way
    /// an operator expects.
    pub fn background(&mut self, rect: Rect) {
        if self.input.pressed
            && self.state.active.is_none()
            && self.input.pointer.map(|q| rect.contains(q)).unwrap_or(false)
        {
            self.state.focus = None;
        }
    }

    pub fn focused(&self, id: Id) -> bool {
        self.state.focus == Some(id)
    }

    pub fn set_focus(&mut self, id: Id) {
        self.state.focus = Some(id);
    }

    /// Re-borrows the painter with a clip rectangle, for scrolled content.
    pub fn clipped(&self, rect: Rect) -> Painter {
        self.p.with_clip_rect(rect)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ids_are_stable_and_distinct() {
        assert_eq!(Id::of("lane"), Id::of("lane"));
        assert_ne!(Id::of("lane"), Id::of("frame"));
        assert_ne!(Id::of("lane").child(1), Id::of("lane").child(2));
        assert_eq!(Id::of("lane").child(3), Id::of("lane").child(3));
    }
}
