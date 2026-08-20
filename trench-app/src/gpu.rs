pub struct Gpu {
    pub device: wgpu::Device,
    pub queue: wgpu::Queue,
    pub target_format: wgpu::TextureFormat,
}

impl Gpu {
    pub fn from_creation_context(cc: &eframe::CreationContext<'_>) -> Option<Self> {
        let state = cc.wgpu_render_state.as_ref()?;
        Some(Self {
            device: state.device.clone(),
            queue: state.queue.clone(),
            target_format: state.target_format,
        })
    }
}
