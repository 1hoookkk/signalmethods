pub mod engine;
pub mod shared;

use std::sync::Arc;

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use trench_core::minifloat::PackedCorners;

use engine::{Engine, ENGINE_SR};
use shared::Shared;

pub struct Audio {
    shared: Arc<Shared>,
    stream: Option<cpal::Stream>,
    pub status: String,
}

impl Audio {
    pub fn silent() -> Self {
        Self {
            shared: Arc::new(Shared::new()),
            stream: None,
            status: "audio off".into(),
        }
    }

    pub fn open() -> Self {
        let shared = Arc::new(Shared::new());
        let mut audio = Self {
            shared: shared.clone(),
            stream: None,
            status: String::new(),
        };
        match Self::build(shared) {
            Ok((stream, status)) => {
                audio.stream = Some(stream);
                audio.status = status;
            }
            Err(e) => audio.status = e,
        }
        audio
    }

    fn build(shared: Arc<Shared>) -> Result<(cpal::Stream, String), String> {
        let host = cpal::default_host();
        let device = host.default_output_device().ok_or("no output device")?;
        let default = device
            .default_output_config()
            .map_err(|e| format!("no output config: {e}"))?;
        let channels = default.channels();
        let config = cpal::StreamConfig {
            channels,
            sample_rate: ENGINE_SR as u32,
            buffer_size: cpal::BufferSize::Default,
        };
        let mut engine = Engine::new(shared);
        let stream = device
            .build_output_stream(
                config,
                move |data: &mut [f32], _: &cpal::OutputCallbackInfo| {
                    engine.fill(data, channels as usize);
                },
                |e| eprintln!("audio: {e}"),
                None,
            )
            .map_err(|e| format!("stream: {e}"))?;
        stream.play().map_err(|e| format!("play: {e}"))?;
        Ok((stream, format!("{} Hz", ENGINE_SR as u32)))
    }

    pub fn publish(&self, packed: &PackedCorners) {
        self.shared.store_words(packed);
        self.shared.bump_activity();
    }

    pub fn set_ride(&self, ride: [f32; 3]) {
        self.shared.set_ride(ride);
    }

    pub fn moved(&self) {
        self.shared.bump_activity();
    }
}
