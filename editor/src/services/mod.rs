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
        Self {
            repository: repository::Repository::new(),
            jobs: jobs::Jobs::new(),
            audio: audio::Audio::new(),
        }
    }
}
