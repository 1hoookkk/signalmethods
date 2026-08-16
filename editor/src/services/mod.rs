pub mod audio;
pub mod jobs;
pub mod repository;

pub struct Services {
    pub repository: repository::Repository,
    pub jobs: jobs::Jobs,
    pub audio: audio::Audio,
}

impl Services {
    pub fn new() -> Self {
        let mut audio = audio::Audio::new();
        audio.ensure_stream();
        Self {
            repository: repository::Repository::new(),
            jobs: jobs::Jobs::new(),
            audio,
        }
    }
}
