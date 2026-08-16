use crate::services::audio::Audio;
use crate::session::state::Session;

pub struct Jobs {}

impl Jobs {
    pub fn new() -> Self {
        Self {}
    }

    pub fn poll(&mut self, _session: &mut Session, _audio: &mut Audio) {}
}
