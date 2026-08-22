use crate::agc::{active_agc_table, agc_step_stereo};
use crate::cartridge::{Cartridge, CornerData};
use crate::cascade::{Cascade, BLOCK_SIZE, NUM_COEFFS, NUM_STAGES};
use crate::cvsd_input::CvsdInput;
use crate::desk_drive::{DeskDrive, SUPPORTED_MODEL as DESK_SLAM_MODEL};
use crate::qsound_spatial::QSoundSpatial;
use crate::trench_matrix::TrenchMatrix;
use std::ptr;
use std::sync::atomic::{AtomicBool, AtomicPtr, Ordering};
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SpatialMode {
    QSound,
    Trench,
    Off,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InputMode {
    None,
    MackieDeskSlam,
    Cvsd,
}
#[derive(Debug, Clone, Copy)]
struct DcBlocker {
    x_prev: f32,
    y_prev: f32,
    r: f32,
}
impl DcBlocker {
    fn new(sample_rate: f64) -> Self {
        // 5 Hz, not 20 (Tyson 2026-08-10 "I want sub"): the blocker's one job
        // is killing 0 Hz drift from the nonlinear stages. At 20 Hz it was
        // shaving -1.6 dB at 30 Hz and rotating phase through the whole sub
        // octave; at 5 Hz the sub passes whole (-0.1 dB at 30 Hz) and DC
        // still dies.
        let fc = 5.0_f64;
        let r = 1.0 - (2.0 * std::f64::consts::PI * fc / sample_rate);
        Self {
            x_prev: 0.0,
            y_prev: 0.0,
            r: r as f32,
        }
    }
    #[inline(always)]
    fn process(&mut self, x: f32) -> f32 {
        let y = x - self.x_prev + self.r * self.y_prev;
        self.x_prev = x;
        self.y_prev = if y.is_finite() { y } else { 0.0 };
        self.y_prev
    }
}
#[derive(Debug, Clone, Copy)]
pub struct DebugToggles {
    pub agc_enabled: bool,
    pub dc_block_enabled: bool,
    pub saturation_enabled: bool,
    pub spatial_enabled: bool,
    pub agc_rate_scale: f32,
    pub agc_max_cut_db: f32,
    pub agc_bypass: bool,
    pub agc_makeup_gain: f32,
    /// The two nonlinearities inside every section (state saturation and the
    /// pole-radius modulator). They are on at every GRIT setting including
    /// zero; off, the cascade is a plain linear IIR. See Cascade::linear.
    pub nonlinearity_enabled: bool,
}
impl Default for DebugToggles {
    fn default() -> Self {
        Self {
            agc_enabled: true,
            dc_block_enabled: true,
            saturation_enabled: true,
            spatial_enabled: true,
            agc_rate_scale: 1.0,
            agc_max_cut_db: f32::INFINITY,
            agc_bypass: false,
            agc_makeup_gain: 1.0,
            nonlinearity_enabled: true,
        }
    }
}
pub const WIDTH_GUARD_LO_HZ: f64 = 2_000.0;
pub const WIDTH_GUARD_HI_HZ: f64 = 18_980.0;
/// Dry-copy capacity reserved for the width guard, so the audio thread never
/// allocates. Comfortably above any real host block size.
const WIDTH_GUARD_MAX_BLOCK: usize = 8192;
#[derive(Debug, Clone, Copy, Default)]
struct GuardBiquad {
    b0: f32,
    b1: f32,
    b2: f32,
    a1: f32,
    a2: f32,
    w1: f32,
    w2: f32,
}
impl GuardBiquad {
    fn high_pass(hz: f64, sr: f64) -> Self {
        let w0 = 2.0 * std::f64::consts::PI * hz.min(sr * 0.45) / sr;
        let (c, s) = (w0.cos(), w0.sin());
        let al = s / std::f64::consts::SQRT_2;
        let a0 = 1.0 + al;
        Self {
            b0: (((1.0 + c) * 0.5) / a0) as f32,
            b1: ((-(1.0 + c)) / a0) as f32,
            b2: (((1.0 + c) * 0.5) / a0) as f32,
            a1: ((-2.0 * c) / a0) as f32,
            a2: ((1.0 - al) / a0) as f32,
            w1: 0.0,
            w2: 0.0,
        }
    }
    fn low_pass(hz: f64, sr: f64) -> Self {
        let w0 = 2.0 * std::f64::consts::PI * hz.min(sr * 0.45) / sr;
        let (c, s) = (w0.cos(), w0.sin());
        let al = s / std::f64::consts::SQRT_2;
        let a0 = 1.0 + al;
        Self {
            b0: (((1.0 - c) * 0.5) / a0) as f32,
            b1: ((1.0 - c) / a0) as f32,
            b2: (((1.0 - c) * 0.5) / a0) as f32,
            a1: ((-2.0 * c) / a0) as f32,
            a2: ((1.0 - al) / a0) as f32,
            w1: 0.0,
            w2: 0.0,
        }
    }
    #[inline]
    fn process(&mut self, x: f32) -> f32 {
        let y = self.b0 * x + self.w1;
        self.w1 = self.b1 * x - self.a1 * y + self.w2;
        self.w2 = self.b2 * x - self.a2 * y;
        if y.is_finite() {
            y
        } else {
            self.w1 = 0.0;
            self.w2 = 0.0;
            0.0
        }
    }
    fn reset(&mut self) {
        self.w1 = 0.0;
        self.w2 = 0.0;
    }
}
pub const AGC_DRIVE: f32 = 1.0;
// No fixed broadband trim lives here. SCALE owns the body's authored level and
// the AGC may reduce only genuinely hot signal. A former -6.5 dB reference-match
// trim made every body quiet before the plugin's separately calibrated SLAM stage.
// saturate() is a SAFETY NET, not a tone stage.
const SATURATE_KNEE: f32 = 4.0; // +12.0 dBFS
const SATURATE_CEILING: f32 = 8.0; // +18.1 dBFS asymptote
const KEY_SNAP_MINOR_DEGREES: [i32; 7] = [0, 2, 3, 5, 7, 8, 10];
const KEY_SNAP_MAJOR_DEGREES: [i32; 7] = [0, 2, 4, 5, 7, 9, 11];
fn key_snap_spec(choice: i32) -> Option<(i32, &'static [i32; 7])> {
    match choice {
        1..=12 => Some((choice - 1, &KEY_SNAP_MINOR_DEGREES)),
        13..=24 => Some((choice - 13, &KEY_SNAP_MAJOR_DEGREES)),
        _ => None,
    }
}
fn conjugate_pair_hz(c1: f64, c2: f64, sample_rate: f64) -> Option<f64> {
    let discriminant = c1 * c1 - 4.0 * c2;
    if discriminant >= 0.0 || c2 <= 1.0e-18 {
        return None;
    }
    let radius = c2.sqrt();
    let angle = (-c1 / (2.0 * radius)).clamp(-1.0, 1.0).acos();
    Some(angle * sample_rate / core::f64::consts::TAU)
}
fn transpose_conjugate_pair(c1: f64, c2: f64, sample_rate: f64, ratio: f64) -> f64 {
    let Some(hz) = conjugate_pair_hz(c1, c2, sample_rate) else {
        return c1;
    };
    let radius = c2.sqrt();
    let moved_hz = (hz * ratio).clamp(20.0, 0.49 * sample_rate);
    -2.0 * radius * (core::f64::consts::TAU * moved_hz / sample_rate).cos()
}
/// PITCH-SHIFT LANE GUARD (2026-08-07).
///
/// A ratio applied to every lane keeps all the formant relationships locked,
/// which is the whole point — but two lane species must NOT travel with it, and
/// both are present in the shipping roster today:
///
/// SUB ANCHOR. Seven of the thirty shipping bodies pin a resonant pole at
/// 64 Hz — Peak Rise, Blade, Cliff, Low Rider, Crackle, Drift 2, Drift. That
/// anchor is what keeps the low end solid while the midband sweeps. Carry it up
/// an octave and it lands at 128 Hz and the weight is gone.
///
/// ZERO WALL. Three park a near-circle zero above 9 kHz — Peak Rise at
/// 11506 Hz, Opium 11241, Crackle 10449. At 44.1 kHz an octave above 11506 is
/// 23012 Hz, past Nyquist. The clamp inside `transpose_conjugate_pair` would
/// fold it onto the wall instead of moving it, which mangles the phase response
/// rather than preserving it.
///
/// Both are decided from the geometry, not from a preset label: a body authored
/// next month is protected without anyone classifying it, and on the twenty
/// bodies carrying neither hazard this does nothing at all.
const SUB_ANCHOR_HZ: f64 = 70.0;
/// A zero this close to the circle is a wall, not a shaping zero.
const ZERO_WALL_RADIUS: f64 = 0.8;
/// Head-room under Nyquist; past this a wall is folding, not moving.
const ZERO_WALL_CEILING: f64 = 0.45;
fn lane_travels(c1: f64, c2: f64, sample_rate: f64, ratio: f64, is_zero: bool) -> bool {
    let Some(hz) = conjugate_pair_hz(c1, c2, sample_rate) else {
        return false;
    };
    if !is_zero {
        return hz >= SUB_ANCHOR_HZ;
    }
    c2.sqrt() < ZERO_WALL_RADIUS || hz * ratio < ZERO_WALL_CEILING * sample_rate
}
fn nearest_scale_hz(hz: f64, tonic: i32, degrees: &[i32; 7]) -> f64 {
    let midi = 69.0 + 12.0 * (hz / 440.0).log2();
    let centre = midi.round() as i32;
    let mut best_note = centre;
    let mut best_distance = f64::INFINITY;
    for note in (centre - 12)..=(centre + 12) {
        let pitch_class = note.rem_euclid(12);
        let degree = (pitch_class - tonic).rem_euclid(12);
        if !degrees.contains(&degree) {
            continue;
        }
        let distance = (note as f64 - midi).abs();
        if distance < best_distance - 1.0e-12
            || ((distance - best_distance).abs() <= 1.0e-12 && note < best_note)
        {
            best_note = note;
            best_distance = distance;
        }
    }
    440.0 * 2.0_f64.powf((best_note as f64 - 69.0) / 12.0)
}
fn snap_stage_to_key(stage: &mut [f64; NUM_COEFFS], sample_rate: f64, choice: i32) {
    let Some((tonic, degrees)) = key_snap_spec(choice) else {
        return;
    };
    let Some(pole_hz) = conjugate_pair_hz(stage[3], stage[4], sample_rate) else {
        return;
    };
    let ratio = nearest_scale_hz(pole_hz, tonic, degrees) / pole_hz;
    stage[3] = transpose_conjugate_pair(stage[3], stage[4], sample_rate, ratio);
    let b0 = stage[0];
    if b0.abs() > 1.0e-12 {
        stage[1] = transpose_conjugate_pair(stage[1] / b0, stage[2] / b0, sample_rate, ratio) * b0;
    }
}
#[inline]
pub(crate) fn saturate(x: f32) -> f32 {
    let a = x.abs();
    if a <= SATURATE_KNEE {
        x
    } else {
        let span = SATURATE_CEILING - SATURATE_KNEE;
        x.signum() * (SATURATE_KNEE + span * ((a - SATURATE_KNEE) / span).tanh())
    }
}
/// Everything the decoded coefficient set is a pure function of. Bit-exact
/// equality here means the previously decoded cascade coefficients are the
/// correct ones for this sample too.
#[derive(Clone, Copy, PartialEq, Eq)]
struct CoeffStamp {
    words: [crate::minifloat::PackedStage; NUM_STAGES],
    boost_bits: u64,
    key_snap: i32,
    ratio_bits: u64,
    amount_bits: u32,
}
pub struct FilterEngine {
    cascade_l: Cascade,
    cascade_r: Cascade,
    // X3 PRESET SWITCH (Tyson 2026-08-15): a hard cut. The new body's words
    // snap in whole and the DF-II states keep ringing through them — the
    // machine's frozen/static install on a running voice. The 100 ms
    // outgoing-voice crossfade this replaced was ours, not the X3's.
    // Per-voice AGC stays, like the hardware: the incoming body's AGC
    // starts fresh (FUN_1802d1600 runs AGC inside the voice loop).
    snap_next_targets: bool,
    // AUDIO-RATE MOVEMENT LAW (2026-08-10): the wheel position arrives as an
    // authored per-sample trajectory. Every sample the packed u16 words are
    // interpolated Morph-first then Q, decoded, and installed whole — the
    // audio only ever passes through real points on the encoded morph path,
    // and the complete coefficient delta is consumed on the sample it was
    // authored for. No control-voltage lag, no catch-up ramp.
    //
    // The stamp caches the last-applied inputs: when the interpolated words,
    // boost and every post-decode transform input are bit-identical to the
    // previous sample, the decoded coefficients are reused. Pure execution
    // optimisation — it can never change an output sample.
    coeff_stamp: Option<CoeffStamp>,
    current_morph: f64,
    // Third corner axis (Morpheus 2^3). Zero on every legacy body, whose two
    // planes are identical, so the position cannot change a sample there.
    third_axis: f64,
    // GROWL (Tyson 2026-08-10 "sounds like a speaker in a trunk"): the wheel
    // oscillates at HALF the detected note — a sub-octave, pitch-locked
    // audio-rate movement. One dumb button: amount 0 = exactly off.
    growl_amount: f32,
    growl_phase: f64,
    control_phase: usize,
    // X3 movement parity (X3_MOVEMENT_SPEC.md): block-rate rebuild + kernel
    // ramp. SHIPPING DEFAULT since 2026-08-15 (Tyson: "the morph wheel
    // doesnt feel like the x3 ... do what they did verbatim in voice
    // processing"); the per-sample path stays reachable from the dev desk
    // for A/B. The morph one-pole (FUN_1802c0430) lives HERE, on the
    // complete summed Morph destination — trajectory + FOLLOW + GROWL —
    // stepped once per control tick before the packed words are
    // interpolated. Nothing upstream may pre-smooth its own contributor.
    x3_movement: bool,
    // FUN_1802c0430 state (+0x2b8) and its seed flag. Seeded to steady state
    // at the current target on the first tick (and on an A/B toggle) so a
    // parked wheel passes through exactly — the machine's cold-start glide
    // from zero is a voice event we do not have.
    morph_pole_s: f64,
    morph_pole_seeded: bool,
    output_gain_delta: f32,
    cartridge: Option<Box<Cartridge>>,
    sample_rate: f64,
    output_gain: f32,
    pre_drive_gain: f32,
    // PREAMP: the desk BEFORE the cascade (community "clip in → filter" stage,
    // adopted by ear 2026-07-30). Independent of SLAM, which is the desk
    // AFTER the filter. preamp_drive feeds InputMode::MackieDeskSlam.
    preamp_drive: f32,
    target_preamp_drive: f32,
    delta_preamp_drive: f32,
    target_grit: f32,
    input_mode: InputMode,
    desk_drive_configured: bool,
    desk_drive_l: DeskDrive,
    desk_drive_r: DeskDrive,
    cvsd_l: CvsdInput,
    cvsd_r: CvsdInput,
    agc_gain: f32,
    agc_mix: f32,
    active_agc_table: [f32; 16],
    agc_drive: f32,
    dc_blocker_l: DcBlocker,
    dc_blocker_r: DcBlocker,
    spatial: QSoundSpatial,
    pub trench_matrix: TrenchMatrix,
    spatial_mode: SpatialMode,
    width_dry_l: Vec<f32>,
    width_dry_r: Vec<f32>,
    width_hp_l: GuardBiquad,
    width_lp_l: GuardBiquad,
    width_hp_r: GuardBiquad,
    width_lp_r: GuardBiquad,
    space: f32,
    amount: f32,
    pitch_ratio: f64,
    // The LISTENER: control-rate pitch follower. When amount > 0 it multiplies
    // a follow ratio into pitch_ratio — the body stays verbatim, the datum
    // moves, and the filter chases the melody.
    listener: crate::listener::PitchListener,
    // The ENV macro: control-rate envelope follower on the same input tap.
    // When amount > 0 it adds a MORPH offset — the body stays verbatim, the
    // wheel position travels with the material's dynamics. MORPH only.
    env: crate::env::EnvFollower,
    // What the ENV macro is allowed to hear. Off = the whole mix (the original
    // broadband tap). On = only the band the cascade is working at, so a carve
    // opens on energy in its own notch and ignores everything else.
    env_detector: crate::env::BandDetector,
    env_tracking: bool,
    // THE AIMER: finds the band that is sticking out and points both wheels at
    // it — Q to where, MORPH to how much. amount 0 = exact bypass.
    aimer: crate::env::Aimer,
    aim_amount: f64,
    // THE ANCHOR: a slow F3 reference that scales the whole body to the
    // singer's tract. Second source for the same ratio the LISTENER drives.
    anchor: crate::anchor::Anchor,
    key_snap: i32,
    pub debug: DebugToggles,
}
impl Default for FilterEngine {
    fn default() -> Self {
        Self::new()
    }
}
impl FilterEngine {
    pub fn new() -> Self {
        let sr = 44100.0;
        Self {
            cascade_l: Cascade::new(),
            cascade_r: Cascade::new(),
            snap_next_targets: false,
            coeff_stamp: None,
            current_morph: 0.0,
            third_axis: 0.0,
            growl_amount: 0.0,
            growl_phase: 0.0,
            control_phase: 0,
            x3_movement: true,
            morph_pole_s: 0.0,
            morph_pole_seeded: false,
            output_gain_delta: 0.0,
            cartridge: None,
            sample_rate: sr,
            output_gain: 1.0,
            pre_drive_gain: 1.0,
            preamp_drive: 0.0,
            target_preamp_drive: 0.0,
            delta_preamp_drive: 0.0,
            target_grit: 0.0,
            input_mode: InputMode::None,
            desk_drive_configured: false,
            desk_drive_l: DeskDrive::new(),
            desk_drive_r: DeskDrive::new(),
            cvsd_l: CvsdInput::new(),
            cvsd_r: CvsdInput::new(),
            agc_gain: 1.0,
            agc_mix: 1.0,
            active_agc_table: active_agc_table(sr),
            agc_drive: AGC_DRIVE,
            dc_blocker_l: DcBlocker::new(sr),
            dc_blocker_r: DcBlocker::new(sr),
            spatial: QSoundSpatial::new(sr as f32),
            trench_matrix: TrenchMatrix::new(sr as f32),
            spatial_mode: SpatialMode::Off,
            width_dry_l: Vec::new(),
            width_dry_r: Vec::new(),
            width_hp_l: GuardBiquad::default(),
            width_lp_l: GuardBiquad::default(),
            width_hp_r: GuardBiquad::default(),
            width_lp_r: GuardBiquad::default(),
            space: 0.0,
            amount: 1.0,
            pitch_ratio: 1.0,
            listener: crate::listener::PitchListener::new(sr),
            env: crate::env::EnvFollower::new(sr),
            env_detector: crate::env::BandDetector::new(),
            env_tracking: false,
            aimer: crate::env::Aimer::new(sr),
            aim_amount: 0.0,
            anchor: crate::anchor::Anchor::new(sr, crate::env::ENV_HOP),
            key_snap: 0,
            debug: DebugToggles::default(),
        }
    }
    pub fn sample_rate(&self) -> f64 {
        self.sample_rate
    }
    pub fn prepare(&mut self, sample_rate: f64) {
        self.sample_rate = sample_rate;
        // Geometry is the body authority: a rate change re-derives the
        // playable words from the datum words at the new runtime rate.
        if let Some(cart) = self.cartridge.as_mut() {
            cart.compile_at(sample_rate);
        }
        self.control_phase = 0;
        // The width guard copies the dry block into these on the AUDIO thread.
        // Reserve here or the first block with SPACE > 0 allocates under the
        // real-time deadline. clear() keeps capacity, so after this they never
        // grow again; an oversized block guards only what fits rather than
        // allocating (apply_width_guard already clamps to the shorter length).
        self.width_dry_l.reserve(WIDTH_GUARD_MAX_BLOCK);
        self.width_dry_r.reserve(WIDTH_GUARD_MAX_BLOCK);
        self.cascade_l.reset();
        self.cascade_r.reset();
        self.coeff_stamp = None;
        self.width_hp_l = GuardBiquad::high_pass(WIDTH_GUARD_LO_HZ, sample_rate);
        self.width_hp_r = GuardBiquad::high_pass(WIDTH_GUARD_LO_HZ, sample_rate);
        self.width_lp_l = GuardBiquad::low_pass(WIDTH_GUARD_HI_HZ, sample_rate);
        self.width_lp_r = GuardBiquad::low_pass(WIDTH_GUARD_HI_HZ, sample_rate);
        self.width_hp_l.reset();
        self.width_hp_r.reset();
        self.width_lp_l.reset();
        self.width_lp_r.reset();
        self.listener.prepare(sample_rate);
        self.env.prepare(sample_rate);
        self.output_gain = 1.0;
        self.output_gain_delta = 0.0;
        self.preamp_drive = 0.0;
        self.target_preamp_drive = 0.0;
        self.delta_preamp_drive = 0.0;
        self.target_grit = 0.0;
        self.input_mode = InputMode::None;
        self.desk_drive_configured = false;
        self.desk_drive_l.prepare(sample_rate as f32);
        self.desk_drive_r.prepare(sample_rate as f32);
        self.cvsd_l.prepare(sample_rate as f32);
        self.cvsd_r.prepare(sample_rate as f32);
        self.agc_gain = 1.0;
        self.active_agc_table = active_agc_table(sample_rate);
        self.dc_blocker_l = DcBlocker::new(sample_rate);
        self.dc_blocker_r = DcBlocker::new(sample_rate);
        self.spatial = QSoundSpatial::new(sample_rate as f32);
        self.spatial.reset();
        self.trench_matrix = TrenchMatrix::new(sample_rate as f32);
    }
    pub fn set_spatial_mode(&mut self, mode: SpatialMode) {
        self.spatial_mode = mode;
    }
    pub fn spatial_mode(&self) -> SpatialMode {
        self.spatial_mode
    }
    pub fn load_cartridge(&mut self, cart: Cartridge) {
        let _ = self.install_cartridge(Box::new(cart), true);
    }
    /// Reload corner words without resetting filter states or AGC.
    /// Used for body glide steps — the filter keeps singing while the
    /// coefficients travel. On final landing, call `load_cartridge` instead
    /// to get a clean voice start.
    pub fn reload_cartridge(&mut self, cart: Box<Cartridge>) {
        let _ = self.install_cartridge(cart, false);
    }
    fn install_cartridge(&mut self, mut cart: Box<Cartridge>, new_voice: bool) -> Option<Box<Cartridge>> {
        cart.compile_at(self.sample_rate);
        self.pre_drive_gain = 10.0_f32.powf(cart.drive.input_gain_db / 20.0);
        self.spatial.clear_profile();
        if new_voice {
            // A body switch is a new voice. EmulatorX's AGC runs per voice
            // (FUN_1802d1600 calls FUN_1802c04e0 inside the voice loop), so a new
            // sound NEVER inherits the previous sound's gain cut.
            self.agc_gain = 1.0;
            // X3 PRESET SWITCH (Tyson 2026-08-15 "make preset switching the
            // same as x3"): a HARD CUT. The 100 ms outgoing-voice crossfade
            // was ours, not the machine's — retired. The new body's words
            // SNAP in whole (never ramping through in-between filter shapes,
            // which is what fired the switch transient that slammed the AGC)
            // and the DF-II delay states keep ringing through them, exactly
            // the X3's frozen/static install on a running voice.
            self.snap_next_targets = true;
        }
        // Reload (new_voice=false): just swap corner words. Filter states,
        // AGC gain, and outgoing crossfade are left untouched — the body glide
        // travels smoothly without 60 Hz state-clearing transients.
        self.coeff_stamp = None; // new corner words: the cached decode is stale
        self.cartridge.replace(cart)
    }
    pub fn set_space(&mut self, space: f32) {
        self.space = space.clamp(0.0, 1.0);
    }
    pub fn set_qsound_fallback_pan(&mut self, pan: f32) {
        self.spatial.set_fallback_pan(pan);
    }
    pub fn set_input_preamp(&mut self, drive: f32) {
        self.target_preamp_drive = if drive.is_finite() {
            drive.clamp(0.0, 1.0)
        } else {
            0.0
        };
    }
    /// Sets the GRIT amount — one knob driving both per-section mechanisms
    /// (pole-radius modulation + state saturation in all six stages).
    pub fn set_grit(&mut self, amount: f32) {
        self.target_grit = if amount.is_finite() {
            amount.clamp(0.0, 1.0)
        } else {
            0.0
        };
    }
    pub fn grit_activity(&self) -> f32 {
        self.cascade_l
            .chew_activity()
            .max(self.cascade_r.chew_activity())
    }
    pub fn agc_reduction_db(&self) -> f32 {
        if self.agc_gain >= 1.0 || self.agc_gain <= 0.0 {
            0.0
        } else {
            -20.0 * self.agc_gain.log10()
        }
    }
    pub fn set_amount(&mut self, amount: f32) {
        self.amount = amount.clamp(0.0, 1.0);
    }
    pub fn set_pitch_ratio(&mut self, ratio: f32) {
        self.pitch_ratio = (ratio as f64).clamp(0.5, 2.0);
    }
    /// The LISTENER: amount 0 = off; speed_ms = glide time constant.
    pub fn set_listener(&mut self, amount: f32, speed_ms: f32) {
        let amount = if amount.is_finite() { amount } else { 0.0 };
        let speed = if speed_ms.is_finite() { speed_ms } else { 80.0 };
        self.listener.set(f64::from(amount), f64::from(speed));
    }
    /// Last voiced f0 the listener heard (Hz); 0 = unvoiced/off. Observation.
    pub fn listener_f0(&self) -> f64 {
        self.listener.f0()
    }
    /// The listener's current follow ratio (1.0 when off). Observation.
    pub fn listener_ratio(&self) -> f64 {
        self.listener.ratio()
    }
    /// The ENV macro: amount 0 = off (exact bypass); release_ms = the relax
    /// time constant. Attack is fixed at the transient (1 ms).
    /// The puck: `momentum` 0 keeps the position follower exactly as it was;
    /// above 0 the wheel becomes a mass that transients throw. `q_depth` is the
    /// second axis (signed - negative sends Q the other way on a hit).
    pub fn set_env_kinetics(&mut self, momentum: f32, friction: f32, q_depth: f32) {
        self.env.set_kinetics(f64::from(momentum), f64::from(friction));
        self.env.set_q(f64::from(q_depth));
    }
    /// The puck's position in wheel units - what the UI would draw.
    pub fn env_puck(&self) -> f64 {
        self.env.puck()
    }
    /// THE ANCHOR: fit the body to this singer. `amount` 0 = exact bypass.
    /// Above 0 a slow F3 reference measures the singer's tract size and scales
    /// every pole and zero by one ratio, so the vowel is preserved and only the
    /// throat it lives in changes size. Takes seconds to settle by design — see
    /// anchor.rs for why a fast one would read the vowel instead of the singer.
    pub fn set_anchor(&mut self, amount: f32) {
        let a = if amount.is_finite() { f64::from(amount) } else { 0.0 };
        self.anchor.set_amount(a);
    }

    /// The tract size the anchor has settled on, in Hz (0 when off). Observation.
    pub fn anchor_hz(&self) -> f64 {
        if self.anchor.amount() > 0.0 { self.anchor.tracked_hz() } else { 0.0 }
    }

    /// How settled the anchor is, 0..1. Observation.
    pub fn anchor_confidence(&self) -> f64 {
        self.anchor.confidence()
    }

    /// The scalar it is applying (1.0 when off or unsettled). Observation.
    pub fn anchor_ratio(&self) -> f64 {
        self.anchor.ratio()
    }

    /// THE AIMER: nothing gets parked by hand. `amount` 0 = exact bypass; above
    /// 0 the search band is hunted for whatever is sticking out above its own
    /// running average, Q is pointed there and MORPH opens by how far it stuck
    /// out. Default floor is the measured vowel ceiling (4.5 kHz, see env.rs);
    /// widen it and the same machine aims at boom, boxiness or whatever peaks.
    pub fn set_aim(&mut self, amount: f32, lo_hz: f32, hi_hz: f32) {
        let amount = if amount.is_finite() { f64::from(amount).clamp(0.0, 1.0) } else { 0.0 };
        if self.aim_amount <= 0.0 && amount > 0.0 {
            self.aimer.reset();
        }
        self.aim_amount = amount;
        if lo_hz.is_finite() && hi_hz.is_finite() && hi_hz > lo_hz {
            self.aimer.set_range(f64::from(lo_hz), f64::from(hi_hz));
        }
    }

    /// Where the aimer is currently pointing, in Hz (0 when off). Observation.
    pub fn aim_hz(&self) -> f64 {
        if self.aim_amount > 0.0 { self.aimer.hz() } else { 0.0 }
    }

    /// How far the aimed band is above its own average, 0..1. Observation.
    pub fn aim_excess(&self) -> f64 {
        if self.aim_amount > 0.0 { self.aimer.excess() } else { 0.0 }
    }

    /// BAND TRACKING: what the ENV follower hears. `false` (default) is the
    /// original broadband tap, bit for bit. `true` routes the tap through one
    /// bandpass tuned to the cascade's own working frequency, so the macro
    /// reacts to energy in the band the body is acting on and ignores the rest.
    /// The window follows the wheels automatically — no frequency to set.
    pub fn set_env_tracking(&mut self, on: bool) {
        if self.env_tracking != on {
            self.env_tracking = on;
            self.env_detector.reset();
        }
    }

    /// The band the detector is currently listening to, in Hz (0 when off).
    /// Observation.
    pub fn env_tracking_hz(&self) -> f64 {
        if self.env_tracking {
            self.env_detector.hz()
        } else {
            0.0
        }
    }

    pub fn set_env(&mut self, amount: f32, release_ms: f32) {
        let amount = if amount.is_finite() { amount } else { 0.0 };
        let release = if release_ms.is_finite() { release_ms } else { 200.0 };
        self.env.set(f64::from(amount), f64::from(release));
    }
    /// The envelope follower's level in normalised dB units. Observation.
    pub fn env_level(&self) -> f64 {
        self.env.level()
    }
    /// The follower's self-centring reference. Observation.
    pub fn env_reference(&self) -> f64 {
        self.env.reference()
    }
    /// The ENV macro's current MORPH offset in wheel units (0 when off).
    pub fn env_offset(&self) -> f64 {
        self.env.offset()
    }
    /// BLOOM: the ENV follower's second destination — the same level and the
    /// same self-centring reference ride the PREAMP desk drive.
    /// amount 0 = off (exact bypass; the desk sees the base preamp verbatim).
    pub fn set_bloom(&mut self, amount: f32) {
        let amount = if amount.is_finite() { amount } else { 0.0 };
        self.env.set_bloom(f64::from(amount));
    }
    /// BLOOM's current PREAMP drive offset (0 when off). Observation.
    pub fn bloom_offset(&self) -> f64 {
        self.env.bloom_offset()
    }
    /// The desk drive the input stage is actually running at. Observation.
    pub fn preamp_drive(&self) -> f32 {
        self.preamp_drive
    }
    /// The wheel position the cascade is actually built from, after the ENV
    /// offset. Observation.
    pub fn morph_position(&self) -> f64 {
        self.current_morph
    }
    pub fn set_key_snap(&mut self, choice: i32) {
        self.key_snap = choice.clamp(0, 24);
    }
    /// GROWL: 0 = exactly off. Above 0 the wheel oscillates around its
    /// position at HALF the detected note (sub-octave, pitch-locked); with no
    /// voiced pitch it holds the verdicted 27.5 Hz. The phase runs continuous
    /// so a note change bends the growl instead of clicking it.
    pub fn set_growl(&mut self, amount: f32) {
        self.growl_amount = if amount.is_finite() {
            amount.clamp(0.0, 1.0)
        } else {
            0.0
        };
    }
    pub fn set_third_axis(&mut self, z: f64) {
        self.third_axis = z.clamp(0.0, 1.0);
    }
    pub fn third_axis(&self) -> f64 {
        self.third_axis
    }
    pub fn set_agc_drive(&mut self, drive: f32) {
        self.agc_drive = drive.max(1.0);
    }
    /// X3 movement parity switch. Toggling either way arms a snap so no stale
    /// kernel ramp (or missing one) is left driving the cascade.
    pub fn set_x3_movement(&mut self, enabled: bool) {
        if self.x3_movement != enabled {
            self.x3_movement = enabled;
            self.snap_next_targets = true;
            self.output_gain_delta = 0.0;
            self.morph_pole_seeded = false;
        }
    }
    pub fn set_input_mode(&mut self, mode: InputMode) {
        if self.input_mode != mode {
            self.input_mode = mode;
            self.desk_drive_l.reset();
            self.desk_drive_r.reset();
            self.cvsd_l.reset();
            self.cvsd_r.reset();
        }
    }
    /// Decoded words -> transformed biquad corner + boost: the shared middle of
    /// both movement paths (KEY snap, TRACK/listener/anchor transposition, the
    /// AMOUNT blend). Pure — installation is the caller's.
    fn transformed_corner(
        &self,
        words: &[crate::minifloat::PackedStage; NUM_STAGES],
        raw_boost: f64,
    ) -> (CornerData, f32) {
        let mut corner: CornerData = [[0.0; NUM_COEFFS]; NUM_STAGES];
        for (row, w) in corner.iter_mut().zip(words.iter()) {
            *row = crate::minifloat::stage_words_to_biquad(*w);
        }
        let mut boost = raw_boost as f32;
        let corner = if self.key_snap != 0 {
            let mut snapped = corner;
            for stage in snapped.iter_mut() {
                snap_stage_to_key(stage, self.sample_rate, self.key_snap);
            }
            snapped
        } else {
            corner
        };
        let corner = if (self.pitch_ratio - 1.0).abs() > 1.0e-9
            || (self.listener.ratio() - 1.0).abs() > 1.0e-9
            || (self.anchor.ratio() - 1.0).abs() > 1.0e-9
        {
            let sr = self.sample_rate;
            let ratio = self.pitch_ratio * self.listener.ratio() * self.anchor.ratio();
            let mut t = corner;
            for stage in t.iter_mut() {
                // Sub anchors stay pinned; everything else travels.
                if lane_travels(stage[3], stage[4], sr, ratio, false) {
                    stage[3] = transpose_conjugate_pair(stage[3], stage[4], sr, ratio);
                }
                let b0 = stage[0];
                if b0.abs() > 1.0e-12
                    && lane_travels(stage[1] / b0, stage[2] / b0, sr, ratio, true)
                {
                    stage[1] =
                        transpose_conjugate_pair(stage[1] / b0, stage[2] / b0, sr, ratio) * b0;
                }
            }
            t
        } else {
            corner
        };
        let corner = if self.amount < 1.0 {
            let k = self.amount as f64;
            boost = boost.powf(self.amount);
            let mut blended = corner;
            for stage in blended.iter_mut() {
                let b0 = stage[0];
                let b0k = if b0 > 0.0 {
                    b0.powf(k)
                } else {
                    k * b0 + (1.0 - k)
                };
                let ratio = if b0.abs() > 1.0e-12 { b0k / b0 } else { 0.0 };
                let a2 = stage[4];
                let g = if a2 > 1.0e-9 {
                    a2.powf(0.5 * (1.0 / k.max(1.0e-3) - 1.0)).min(1.0)
                } else {
                    k
                };
                stage[1] *= g * ratio;
                stage[2] *= g * g * ratio;
                stage[0] = b0k;
                stage[3] *= g;
                stage[4] *= g * g;
            }
            let peak_full = compute_cascade_peak(&corner, self.sample_rate).max(1.0e-6);
            let peak_blended = compute_cascade_peak(&blended, self.sample_rate).max(1.0e-6);
            let expected_db = k as f32 * 20.0 * peak_full.log10();
            let actual_db = 20.0 * peak_blended.log10();
            boost *= 10.0_f32.powf((expected_db - actual_db) / 20.0);
            blended
        } else {
            corner
        };
        (corner, boost)
    }
    /// Rebuilds and installs the cascade coefficients for one sample's wheel
    /// position. The packed u16 words are interpolated Morph-first then Q
    /// (the one packed law, minifloat.rs), decoded, transformed (KEY is an
    /// explicit post-decode transform), and installed WHOLE — the complete
    /// delta is consumed on this sample, never divided over a ramp. DF-II
    /// delay states are preserved by the install (Cascade::snap_targets).
    fn rebuild_coefficients(&mut self, morph: f64, q: f64) {
        let cart = match &self.cartridge {
            Some(c) => c,
            None => return,
        };
        let words = cart
            .packed
            .interpolate_words(morph as f32, q as f32, self.third_axis as f32);
        let raw_boost = cart.interpolate_boost(morph, q, self.third_axis);
        let ratio = self.pitch_ratio * self.listener.ratio() * self.anchor.ratio();
        let stamp = CoeffStamp {
            words,
            boost_bits: raw_boost.to_bits(),
            key_snap: self.key_snap,
            ratio_bits: ratio.to_bits(),
            amount_bits: self.amount.to_bits(),
        };
        // Execution cache only: bit-identical inputs mean the installed
        // coefficients are already exactly right — reusing them cannot change
        // a single output sample.
        if !self.snap_next_targets && self.coeff_stamp == Some(stamp) {
            return;
        }
        let (corner, boost) = self.transformed_corner(&words, raw_boost);
        // Install WHOLE, this sample. snap_targets zeroes the coefficient
        // deltas and leaves the DF-II delay states (w1/w2) untouched — the
        // filter keeps ringing, only its coefficients move. boost is SCALE,
        // the ONE broadband level per corner; it lands with the coefficients.
        self.snap_next_targets = false;
        self.cascade_l.snap_targets(&corner);
        self.cascade_r.snap_targets(&corner);
        self.output_gain = boost;
        self.coeff_stamp = Some(stamp);
    }
    /// X3 movement parity rebuild: once per control block. Words are
    /// interpolated at the block's smoothed morph (FUN_1802c3d40), transformed
    /// like the shipping path, converted to kernel rows and RAMPED over the
    /// block (FUN_1802c41a0 / FUN_1802c1550). On the X3's change-test skip the
    /// ramp freezes; a pending body switch takes the frozen/static path.
    fn rebuild_coefficients_x3(&mut self, morph: f64, q: f64, ramp_samples: usize) {
        if self.snap_next_targets {
            return self.rebuild_coefficients(morph, q);
        }
        let cart = match &self.cartridge {
            Some(c) => c,
            None => return,
        };
        let words = cart
            .packed
            .interpolate_words(morph as f32, q as f32, self.third_axis as f32);
        let raw_boost = cart.interpolate_boost(morph, q, self.third_axis);
        let ratio = self.pitch_ratio * self.listener.ratio() * self.anchor.ratio();
        let stamp = CoeffStamp {
            words,
            boost_bits: raw_boost.to_bits(),
            key_snap: self.key_snap,
            ratio_bits: ratio.to_bits(),
            amount_bits: self.amount.to_bits(),
        };
        if self.coeff_stamp == Some(stamp) {
            // `+0x86 = 1`: nothing moved; nothing is recomputed.
            self.cascade_l.zero_kernel_deltas();
            self.cascade_r.zero_kernel_deltas();
            self.output_gain_delta = 0.0;
            return;
        }
        let (corner, boost) = self.transformed_corner(&words, raw_boost);
        let mut kernels: CornerData = [[0.0; NUM_COEFFS]; NUM_STAGES];
        for (k, row) in kernels.iter_mut().zip(corner.iter()) {
            *k = crate::minifloat::biquad_to_kernel(*row);
        }
        self.cascade_l.set_kernel_targets(&kernels, ramp_samples);
        self.cascade_r.set_kernel_targets(&kernels, ramp_samples);
        // Our per-corner boost has no X3 row to ride, so it ramps on the same
        // block schedule instead of stepping.
        self.output_gain_delta = (boost - self.output_gain) / ramp_samples.max(1) as f32;
        self.coeff_stamp = Some(stamp);
    }
    #[inline]
    fn process_input_stage(&mut self, l: f32, r: f32) -> (f32, f32) {
        match self.input_mode {
            InputMode::None => (l, r),
            InputMode::MackieDeskSlam => {
                // The desk is not unity at zero drive (baked character), so
                // PREAMP fades the desk in over its first 5% — exact unity at 0,
                // no step when the input stage engages.
                let wet = (self.preamp_drive * 20.0).clamp(0.0, 1.0);
                let dl = self.desk_drive_l.process(l, self.preamp_drive);
                let dr = self.desk_drive_r.process(r, self.preamp_drive);
                (l + (dl - l) * wet, r + (dr - r) * wet)
            }
            InputMode::Cvsd => (self.cvsd_l.process(l), self.cvsd_r.process(r)),
        }
    }
    fn configure_desk_drive(&mut self) {
        if !self.desk_drive_configured {
            self.desk_drive_l.configure(DESK_SLAM_MODEL);
            self.desk_drive_r.configure(DESK_SLAM_MODEL);
            self.desk_drive_configured = true;
        }
    }
    #[inline]
    fn process_sample_inner(&mut self, l: f32, r: f32) -> (f32, f32) {
        let (mut sl, mut sr) = self.process_input_stage(l, r);
        self.preamp_drive += self.delta_preamp_drive;
        sl *= self.pre_drive_gain;
        sr *= self.pre_drive_gain;
        // NO CROSSFADE (Tyson 2026-08-15 "make preset switching the same as
        // x3"): the 100 ms outgoing-voice blend was ours, not the machine's.
        // On the X3 a program change on a running voice swaps the coefficient
        // words and keeps the delay lines — the frozen/static install path —
        // so a body switch here is a hard cut: snap_targets installs the new
        // words whole, the DF-II states keep ringing through them.
        sl = self.cascade_l.tick(sl);
        sr = self.cascade_r.tick(sr);
        if self.debug.agc_enabled {
            if self.debug.agc_bypass {
                let mk = self.debug.agc_makeup_gain;
                sl += (sl * mk - sl) * self.agc_mix;
                sr += (sr * mk - sr) * self.agc_mix;
            } else if self.debug.agc_rate_scale == 1.0 && self.debug.agc_max_cut_db == f32::INFINITY
            {
                let d = self.agc_drive;
                let (agc_l, agc_r) =
                    agc_step_stereo(sl * d, sr * d, &mut self.agc_gain, &self.active_agc_table);
                let agc_l = agc_l / d;
                let agc_r = agc_r / d;
                sl += (agc_l - sl) * self.agc_mix;
                sr += (agc_r - sr) * self.agc_mix;
            } else {
                let d = self.agc_drive;
                let mag = (sl * d).abs().max((sr * d).abs());
                let idx = ((self.agc_gain * mag) as u32 & 0xF) as usize;
                let mut step = self.active_agc_table[idx];
                if self.debug.agc_rate_scale != 1.0 {
                    step = step.powf(self.debug.agc_rate_scale);
                }
                let floor = if self.debug.agc_max_cut_db == f32::INFINITY {
                    0.0
                } else {
                    10.0_f32.powf(-self.debug.agc_max_cut_db / 20.0)
                };
                self.agc_gain = (self.agc_gain * step).clamp(floor, 1.0);
                let agc_l = sl * self.agc_gain;
                let agc_r = sr * self.agc_gain;
                sl += (agc_l - sl) * self.agc_mix;
                sr += (agc_r - sr) * self.agc_mix;
            }
        }
        sl *= self.output_gain;
        sr *= self.output_gain;
        if self.debug.dc_block_enabled {
            sl = self.dc_blocker_l.process(sl);
            sr = self.dc_blocker_r.process(sr);
        }
        if self.debug.saturation_enabled {
            sl = saturate(sl);
            sr = saturate(sr);
        }
        (sl, sr)
    }
    /// Static wheel positions: the whole block is one authored coordinate.
    /// Same per-sample law as `process_trajectory` — the coefficient cache
    /// makes the constant case cost one rebuild.
    pub fn process_block(&mut self, left: &mut [f32], right: &mut [f32], morph: f64, q: f64) {
        self.process_span(left, right, q, |_| morph);
    }
    /// Audio-rate Movement: one authored Morph position per sample. Q stays
    /// the static authored second axis — Movement modulates Morph only.
    pub fn process_trajectory(
        &mut self,
        left: &mut [f32],
        right: &mut [f32],
        morph: &[f32],
        q: f64,
    ) {
        let n = left.len().min(right.len()).min(morph.len());
        self.process_span(&mut left[..n], &mut right[..n], q, |i| morph[i] as f64);
    }
    fn process_span(
        &mut self,
        left: &mut [f32],
        right: &mut [f32],
        q: f64,
        morph_at: impl Fn(usize) -> f64,
    ) {
        if self.cartridge.is_none() {
            return;
        }
        let q = q.clamp(0.0, 1.0);
        let len = left.len().min(right.len());
        if self.listener.amount() > 0.0 {
            self.listener.process_block(&left[..len]);
        } else if self.growl_amount > 0.0 {
            // GROWL needs the note, not a transposition: detector-only.
            self.listener.process_block_detect(&left[..len]);
        }
        if self.anchor.amount() > 0.0 {
            // The anchor reads the dry input on the same hop grid as everything
            // else. It is a seconds-long reference, so hop alignment is
            // irrelevant to it — this is just where the tap is.
            let mut i = 0;
            while i < len {
                let end = (i + crate::env::ENV_HOP).min(len);
                self.anchor.advance(&left[i..end]);
                i = end;
            }
        }
        for i in 0..len {
            if self.control_phase == 0 {
                // FOLLOW's detector advances on its authored hop grid — the
                // attack/release is the detector's authored behaviour, never a
                // second smoothing of the coefficients.
                if self.env.armed() {
                    let end = (i + crate::env::ENV_HOP).min(len);
                    let peak = if self.env_tracking {
                        // Listen where the cascade is working. The coefficients
                        // are last block's — 0.7 ms stale, far inside the 1 ms
                        // attack, so the transient still lands on the wheel the
                        // moment it is heard.
                        let mut rows = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
                        self.cascade_l.get_coeffs(&mut rows);
                        if let Some(hz) = crate::env::working_hz(&rows, self.sample_rate) {
                            self.env_detector.tune(hz, self.sample_rate);
                        }
                        self.env_detector.block_peak(&left[i..end])
                    } else {
                        crate::env::block_peak(&left[i..end])
                    };
                    self.env.advance(peak);
                }
                if self.input_mode == InputMode::MackieDeskSlam {
                    self.configure_desk_drive();
                }
                // Input gain de-zip only (PREAMP is a drive knob, not the
                // Morph trajectory): a short glide toward the target drive.
                let ramp = (self.sample_rate * 0.005).round().max(BLOCK_SIZE as f64);
                self.delta_preamp_drive =
                    (self.target_preamp_drive - self.preamp_drive) / ramp as f32;
                self.cascade_l.set_grit(self.target_grit);
                self.cascade_r.set_grit(self.target_grit);
                // Dev bypass, re-stated each control block so it survives a
                // body switch (which hands the ringing voice to the outgoing
                // pair). Costs a bool store per block.
                let linear = !self.debug.nonlinearity_enabled;
                self.cascade_l.set_linear(linear);
                self.cascade_r.set_linear(linear);
            }
            // Movement law: the authored per-sample position plus FOLLOW's
            // offset, clamped. No lag, no catch-up ramp, no Q modulation.
            // GROWL adds a pitch-locked sub-octave oscillation on the same
            // wheel — the one moving part — at audio rate.
            let growl = if self.growl_amount > 0.0 {
                const GROWL_DEPTH: f64 = 0.45;
                const GROWL_FALLBACK_HZ: f64 = 27.5;
                let f0 = self.listener.f0();
                let rate = if f0 > 20.0 { f0 * 0.5 } else { GROWL_FALLBACK_HZ };
                self.growl_phase += rate / self.sample_rate;
                if self.growl_phase >= 1.0 {
                    self.growl_phase -= 1.0;
                }
                self.growl_amount as f64
                    * GROWL_DEPTH
                    * (core::f64::consts::TAU * self.growl_phase).sin()
            } else {
                0.0
            };
            let m = (morph_at(i) + self.env.offset() + growl).clamp(0.0, 1.0);
            if self.x3_movement {
                // X3 parity: the complete summed Morph destination is read
                // once per block, smoothed by the morph one-pole
                // (FUN_1802c0430: s = s - s*R + target; out = R*s, stepped
                // once per control tick), and the coefficients ramp to the
                // smoothed coordinate in kernel space across the block
                // (FUN_1802c41a0 / FUN_1802c1550). Pattern, FOLLOW and GROWL
                // all arrive through this pole — it is the machine's
                // anti-zipper for a modulated wheel, and a fast modulator's
                // depth falling off with rate is its behaviour, not a defect.
                if self.control_phase == 0 {
                    const R: f64 = 0.4516276717185974;
                    let smoothed = if !self.morph_pole_seeded {
                        self.morph_pole_seeded = true;
                        self.morph_pole_s = m / R;
                        m
                    } else {
                        self.morph_pole_s = self.morph_pole_s - self.morph_pole_s * R + m;
                        R * self.morph_pole_s
                    };
                    self.current_morph = smoothed;
                    self.rebuild_coefficients_x3(smoothed, q, BLOCK_SIZE);
                }
            } else {
                self.current_morph = m;
                self.rebuild_coefficients(m, q);
            }
            let (out_l, out_r) = self.process_sample_inner(left[i], right[i]);
            left[i] = out_l;
            right[i] = out_r;
            if self.x3_movement {
                self.output_gain += self.output_gain_delta;
            }
            self.control_phase += 1;
            if self.control_phase >= BLOCK_SIZE {
                self.control_phase = 0;
            }
        }
        if self.debug.spatial_enabled && self.spatial_mode != SpatialMode::Off {
            let guarded = left.len().min(right.len()).min(self.width_dry_l.capacity());
            self.width_dry_l.clear();
            self.width_dry_r.clear();
            self.width_dry_l.extend_from_slice(&left[..guarded]);
            self.width_dry_r.extend_from_slice(&right[..guarded]);
            match self.spatial_mode {
                SpatialMode::QSound => {
                    self.spatial.set_space(self.space);
                    self.spatial.process_stereo(left, right);
                }
                SpatialMode::Trench => {
                    self.trench_matrix.space = self.space;
                    self.trench_matrix.process_stereo(left, right);
                }
                SpatialMode::Off => {}
            }
            self.apply_width_guard(left, right);
        }
    }
    fn apply_width_guard(&mut self, left: &mut [f32], right: &mut [f32]) {
        let n = left.len().min(right.len()).min(self.width_dry_l.len());
        for i in 0..n {
            let (dry_l, dry_r) = (self.width_dry_l[i], self.width_dry_r[i]);
            let add_l = left[i] - dry_l;
            let add_r = right[i] - dry_r;
            let band_l = self.width_lp_l.process(self.width_hp_l.process(add_l));
            let band_r = self.width_lp_r.process(self.width_hp_r.process(add_r));
            left[i] = dry_l + band_l;
            right[i] = dry_r + band_r;
        }
    }
    pub fn take_instability_flag(&mut self) -> bool {
        self.cascade_l.take_instability_flag() || self.cascade_r.take_instability_flag()
    }
    pub fn get_coeffs_for_ui(
        &self,
        out_coeffs: &mut [[f32; NUM_COEFFS]; NUM_STAGES],
        out_boost: &mut f32,
    ) {
        let mut d_coeffs = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
        self.cascade_l.get_coeffs(&mut d_coeffs);
        for (i, stage) in d_coeffs.iter().enumerate() {
            for (j, &c) in stage.iter().enumerate() {
                out_coeffs[i][j] = c as f32;
            }
        }
        *out_boost = self.output_gain;
    }
}
pub struct CartridgeMailbox {
    pending: AtomicPtr<Cartridge>,
    garbage: AtomicPtr<Cartridge>,
    reload: AtomicBool,
}
impl CartridgeMailbox {
    fn new() -> Self {
        Self {
            pending: AtomicPtr::new(ptr::null_mut()),
            garbage: AtomicPtr::new(ptr::null_mut()),
            reload: AtomicBool::new(false),
        }
    }
    pub fn stage(&self, cart: Box<Cartridge>) {
        self.reload.store(false, Ordering::Release);
        self.reclaim();
        let prev = self.pending.swap(Box::into_raw(cart), Ordering::AcqRel);
        if !prev.is_null() {
            drop(unsafe { Box::from_raw(prev) });
        }
    }
    /// Stage a reload: swap corner words without resetting filter states or AGC.
    pub fn stage_reload(&self, cart: Box<Cartridge>) {
        self.reload.store(true, Ordering::Release);
        self.reclaim();
        let prev = self.pending.swap(Box::into_raw(cart), Ordering::AcqRel);
        if !prev.is_null() {
            drop(unsafe { Box::from_raw(prev) });
        }
    }
    pub fn reclaim(&self) {
        let g = self.garbage.swap(ptr::null_mut(), Ordering::AcqRel);
        if !g.is_null() {
            drop(unsafe { Box::from_raw(g) });
        }
    }
    fn take(&self) -> Option<Box<Cartridge>> {
        if !self.garbage.load(Ordering::Acquire).is_null() {
            return None;
        }
        let p = self.pending.swap(ptr::null_mut(), Ordering::AcqRel);
        if p.is_null() {
            None
        } else {
            Some(unsafe { Box::from_raw(p) })
        }
    }
    fn retire(&self, old: Box<Cartridge>) {
        let prev = self.garbage.swap(Box::into_raw(old), Ordering::AcqRel);
        debug_assert!(
            prev.is_null(),
            "garbage slot overwritten: producer fell behind take()'s guard"
        );
        if !prev.is_null() {
            drop(unsafe { Box::from_raw(prev) });
        }
    }
}
impl Drop for CartridgeMailbox {
    fn drop(&mut self) {
        let p = self.pending.swap(ptr::null_mut(), Ordering::AcqRel);
        if !p.is_null() {
            drop(unsafe { Box::from_raw(p) });
        }
        self.reclaim();
    }
}
pub struct EngineHandle {
    pub engine: FilterEngine,
    pub mailbox: CartridgeMailbox,
}
impl EngineHandle {
    pub fn new() -> Self {
        Self {
            engine: FilterEngine::new(),
            mailbox: CartridgeMailbox::new(),
        }
    }
}
impl Default for EngineHandle {
    fn default() -> Self {
        Self::new()
    }
}
pub fn install_pending(engine: &mut FilterEngine, mailbox: &CartridgeMailbox) {
    if let Some(new_cart) = mailbox.take() {
        let is_reload = mailbox.reload.swap(false, Ordering::Acquire);
        let old = if is_reload {
            engine.reload_cartridge(new_cart);
            None
        } else {
            engine.install_cartridge(new_cart, true)
        };
        if let Some(old) = old {
            mailbox.retire(old);
        }
    }
}
fn compute_cascade_peak(corner: &CornerData, sample_rate: f64) -> f32 {
    const NUM_BINS: usize = 1024;
    let nyquist = sample_rate / 2.0;
    let mut max_mag: f64 = 0.0;
    for bin in 0..=NUM_BINS {
        let freq = nyquist * (bin as f64 / NUM_BINS as f64);
        let omega = 2.0 * std::f64::consts::PI * freq / sample_rate;
        let cos_w = omega.cos();
        let cos_2w = (2.0 * omega).cos();
        let sin_w = omega.sin();
        let sin_2w = (2.0 * omega).sin();
        let mut cascade_mag_sq: f64 = 1.0;
        for stage in corner.iter() {
            let b0 = stage[0];
            let b1 = stage[1];
            let b2 = stage[2];
            let a1 = stage[3];
            let a2 = stage[4];
            let num_real = b0 + b1 * cos_w + b2 * cos_2w;
            let num_imag = -(b1 * sin_w + b2 * sin_2w);
            let den_real = 1.0 + a1 * cos_w + a2 * cos_2w;
            let den_imag = -(a1 * sin_w + a2 * sin_2w);
            let den_mag_sq = den_real * den_real + den_imag * den_imag;
            if den_mag_sq > 1e-30 {
                let stage_mag_sq = (num_real * num_real + num_imag * num_imag) / den_mag_sq;
                cascade_mag_sq *= stage_mag_sq;
            }
        }
        let mag = cascade_mag_sq.sqrt();
        if mag > max_mag {
            max_mag = mag;
        }
    }
    max_mag as f32
}
#[cfg(test)]
mod tests {
    use super::*;
    use crate::cascade::PASSTHROUGH_COEFFS;
    fn conjugate_coefficients(hz: f64, radius: f64, sample_rate: f64) -> (f64, f64) {
        let angle = core::f64::consts::TAU * hz / sample_rate;
        (-2.0 * radius * angle.cos(), radius * radius)
    }
    #[test]
    fn a_sub_anchor_does_not_travel_with_the_pitch_shift() {
        let sr = 44_100.0;
        // The anchor seven shipping bodies carry: a resonant pole at 64 Hz.
        let (c1, c2) = pair(64.0, 0.995, sr);
        assert!(!lane_travels(c1, c2, sr, 2.0, false), "64 Hz anchor must pin");
        // A formant just above the threshold travels normally.
        let (m1, m2) = pair(300.0, 0.99, sr);
        assert!(lane_travels(m1, m2, sr, 2.0, false), "300 Hz formant must travel");
        let moved = transpose_conjugate_pair(m1, m2, sr, 2.0);
        let got = conjugate_pair_hz(moved, m2, sr).expect("conjugate");
        assert!((got - 600.0).abs() < 1.0, "expected 600 Hz, got {got}");
    }

    #[test]
    fn a_zero_wall_does_not_get_folded_onto_nyquist() {
        let sr = 44_100.0;
        // Peak Rise's wall, measured: a near-circle zero at 11506 Hz. An octave
        // up is 23012 Hz — past Nyquist, so it must pin rather than fold.
        let (c1, c2) = pair(11_506.0, 0.99, sr);
        assert!(!lane_travels(c1, c2, sr, 2.0, true), "the wall must pin at +1 oct");
        // The same wall shifted only slightly stays inside and travels.
        assert!(lane_travels(c1, c2, sr, 1.05, true), "a small shift stays legal");
        // A shaping zero (well off the circle) always travels, even up high.
        let (s1, s2) = pair(11_506.0, 0.4, sr);
        assert!(lane_travels(s1, s2, sr, 2.0, true), "a shaping zero is not a wall");
    }

    #[test]
    fn a_body_with_neither_hazard_is_untouched_by_the_guard() {
        let sr = 44_100.0;
        for hz in [120.0, 456.0, 857.0, 2344.0, 5000.0] {
            let (c1, c2) = pair(hz, 0.97, sr);
            assert!(lane_travels(c1, c2, sr, 1.5, false), "{hz} Hz pole should travel");
            assert!(lane_travels(c1, c2, sr, 1.5, true), "{hz} Hz zero should travel");
        }
    }

    /// Build a conjugate pair at `hz` with radius `r`, as (c1, c2).
    fn pair(hz: f64, r: f64, sr: f64) -> (f64, f64) {
        let w = core::f64::consts::TAU * hz / sr;
        (-2.0 * r * w.cos(), r * r)
    }

    #[test]
    fn manual_key_snap_removes_a_natural_from_c_minor() {
        let snapped = nearest_scale_hz(440.0, 0, &KEY_SNAP_MINOR_DEGREES);
        let expected_ab = 440.0 * 2.0_f64.powf(-1.0 / 12.0);
        assert!((snapped - expected_ab).abs() < 1.0e-9, "snapped={snapped}");
    }
    #[test]
    fn manual_key_snap_preserves_the_stage_pole_zero_interval() {
        let sample_rate = 48_000.0;
        let (b1, b2) = conjugate_coefficients(660.0, 0.82, sample_rate);
        let (a1, a2) = conjugate_coefficients(440.0, 0.96, sample_rate);
        let mut stage = [1.0, b1, b2, a1, a2];
        let before_pole = conjugate_pair_hz(stage[3], stage[4], sample_rate).unwrap();
        let before_zero = conjugate_pair_hz(stage[1], stage[2], sample_rate).unwrap();
        snap_stage_to_key(&mut stage, sample_rate, 1);
        let after_pole = conjugate_pair_hz(stage[3], stage[4], sample_rate).unwrap();
        let after_zero = conjugate_pair_hz(stage[1], stage[2], sample_rate).unwrap();
        let expected_ab = 440.0 * 2.0_f64.powf(-1.0 / 12.0);
        assert!((after_pole - expected_ab).abs() < 1.0e-8);
        assert!((after_zero / before_zero - after_pole / before_pole).abs() < 1.0e-10);
        assert!((stage[2] - b2).abs() < 1.0e-15, "zero radius changed");
        assert!((stage[4] - a2).abs() < 1.0e-15, "pole radius changed");
    }
    #[test]
    fn manual_key_snap_leaves_real_pole_rows_verbatim() {
        let mut stage = [1.0, -0.4, 0.03, -1.1, 0.28];
        let before = stage;
        snap_stage_to_key(&mut stage, 48_000.0, 1);
        assert_eq!(stage, before);
    }
    #[test]
    fn manual_key_snap_off_is_an_exact_noop() {
        let (b1, b2) = conjugate_coefficients(660.0, 0.82, 48_000.0);
        let (a1, a2) = conjugate_coefficients(440.0, 0.96, 48_000.0);
        let mut stage = [0.91, b1 * 0.91, b2 * 0.91, a1, a2];
        let before = stage;
        snap_stage_to_key(&mut stage, 48_000.0, 0);
        assert_eq!(stage, before);
    }
    fn cartridge_from_corner(name: &str, corner: CornerData) -> Cartridge {
        let mut word_corner =
            [[0u16; crate::cascade::NUM_COEFFS]; crate::minifloat::LEGACY_STAGES];
        for (stage_index, biquad) in corner.iter().take(crate::minifloat::LEGACY_STAGES).enumerate() {
            word_corner[stage_index] = crate::compiler::biquad_to_words(*biquad);
        }
        let packed = crate::minifloat::PackedCorners::from_legacy_words(&[word_corner; 4]);
        Cartridge::from_body_bytes(name, &packed.to_rom_bytes(), 1.0)
            .expect("packed test cartridge")
    }
    fn make_passthrough_cartridge() -> Cartridge {
        cartridge_from_corner(
            "passthrough",
            [PASSTHROUGH_COEFFS; crate::cascade::NUM_STAGES],
        )
    }
    #[test]
    fn install_and_prepare_compile_the_body_at_the_runtime_rate() {
        let roots = crate::stage_law::StageRoots {
            pole_hz: 1_200.0,
            pole_r: 0.96,
            zero_hz: 2_900.0,
            zero_r: 0.82,
            scale: 0.8,
        };
        let row = crate::stage_law::words_from_roots_at(&roots, 44_100.0);
        let identity = crate::stage_law::words_from_roots_at(&crate::stage_law::StageRoots::IDENTITY, 44_100.0);
        let packed = crate::minifloat::PackedCorners::from_legacy_words(&[[
            row, identity, identity, identity, identity, identity,
        ]; 4]);
        // An explicit verbatim carrier (datum 0) plays as stored at any rate.
        let verbatim =
            Cartridge::from_body_bytes_at("theta", &packed.to_rom_bytes(), 1.0, 0.0).unwrap();
        let mut engine = FilterEngine::new();
        engine.prepare(96_000.0);
        engine.load_cartridge(verbatim);
        assert_eq!(
            engine.cartridge.as_ref().unwrap().packed.words[0][0],
            row,
            "verbatim carriers must never be recompiled"
        );

        // The default ROM load is Hz-anchored at the 44,100 datum - the
        // factory law.
        let cart = Cartridge::from_body_bytes("rom", &packed.to_rom_bytes(), 1.0).unwrap();
        engine.load_cartridge(cart);
        let installed = engine.cartridge.as_ref().unwrap();
        let expected = crate::stage_law::recompile_stage_words(row, 44_100.0, 96_000.0);
        assert_eq!(installed.packed.words[0][0], expected);
        assert_ne!(installed.packed.words[0][0], row, "96 kHz must recompile the words");

        // A rate change back to the datum restores the interchange words
        // exactly - packed is always derived from the datum, never chained.
        engine.prepare(44_100.0);
        let installed = engine.cartridge.as_ref().unwrap();
        assert_eq!(installed.packed.words[0][0], row);
        assert_eq!(installed.datum_packed.words[0][0], row);
    }
    #[test]
    fn env_and_listener_are_orthogonal_sources() {
        // The LISTENER moves the frequency datum (pitch_ratio); ENV moves the
        // MORPH wheel. Running both must leave each exactly where it would be
        // alone — no shared state, nothing to fight over.
        let sr = 48_000.0;
        let n = (2.5 * sr) as usize;
        let mut signal = Vec::with_capacity(n);
        for i in 0..n {
            let t = i as f64 / sr;
            let mut s = 0.0;
            for k in 1..=8 {
                s += (core::f64::consts::TAU * 220.0 * k as f64 * t).sin() / k as f64;
            }
            // Pumping amplitude so the follower has something to follow.
            let env = 0.05 + 0.45 * (1.0 + (core::f64::consts::TAU * 2.0 * t).sin());
            signal.push((s * env) as f32);
        }
        let run = |env_on: bool, listen_on: bool| -> (f64, f64) {
            let mut e = FilterEngine::new();
            e.prepare(sr);
            e.load_cartridge(make_passthrough_cartridge());
            if env_on {
                e.set_env(1.0, 200.0);
            }
            if listen_on {
                e.set_listener(1.0, 80.0);
            }
            for chunk in signal.chunks(512) {
                let mut l = chunk.to_vec();
                let mut r = chunk.to_vec();
                e.process_block(&mut l, &mut r, 0.3, 0.5);
            }
            (e.env_offset(), e.listener_ratio())
        };
        let (env_alone, _) = run(true, false);
        let (_, listen_alone) = run(false, true);
        let (env_both, listen_both) = run(true, true);
        assert_eq!(env_both, env_alone, "the listener must not disturb ENV");
        assert_eq!(listen_both, listen_alone, "ENV must not disturb the listener");
        assert!(env_alone != 0.0 && listen_alone != 1.0, "both sources must be live");
    }
    #[test]
    fn env_off_is_an_exact_bypass_of_the_whole_path() {
        let sr = 48_000.0;
        let mut signal = Vec::with_capacity(4096);
        for i in 0..4096 {
            let t = i as f64 / sr;
            signal.push(((core::f64::consts::TAU * 300.0 * t).sin() * (t * 40.0).fract()) as f32);
        }
        // A body with real morph travel: M0 resonates low, M100 resonates high.
        let make_body = || {
            let low = crate::stage_law::words_from_roots_at(
                &crate::stage_law::StageRoots {
                    pole_hz: 400.0,
                    pole_r: 0.97,
                    zero_hz: 1_200.0,
                    zero_r: 0.8,
                    scale: 1.0,
                },
                sr,
            );
            let high = crate::stage_law::words_from_roots_at(
                &crate::stage_law::StageRoots {
                    pole_hz: 3_000.0,
                    pole_r: 0.97,
                    zero_hz: 800.0,
                    zero_r: 0.8,
                    scale: 1.0,
                },
                sr,
            );
            let id =
                crate::stage_law::words_from_roots_at(&crate::stage_law::StageRoots::IDENTITY, sr);
            let rows = |w| [w, id, id, id, id, id];
            let packed = crate::minifloat::PackedCorners::from_legacy_words(&[
                rows(low),
                rows(high),
                rows(low),
                rows(high),
            ]);
            Cartridge::from_body_bytes_at("env_travel", &packed.to_rom_bytes(), 1.0, 0.0).unwrap()
        };
        let render = |env_amount: f32| -> Vec<f32> {
            let mut e = FilterEngine::new();
            e.prepare(sr);
            e.load_cartridge(make_body());
            e.set_env(env_amount, 200.0);
            let mut out = Vec::new();
            for chunk in signal.chunks(256) {
                let mut l = chunk.to_vec();
                let mut r = chunk.to_vec();
                e.process_block(&mut l, &mut r, 0.4, 0.6);
                out.extend_from_slice(&l);
            }
            out
        };
        let mut baseline = FilterEngine::new();
        baseline.prepare(sr);
        baseline.load_cartridge(make_body());
        let mut untouched = Vec::new();
        for chunk in signal.chunks(256) {
            let mut l = chunk.to_vec();
            let mut r = chunk.to_vec();
            baseline.process_block(&mut l, &mut r, 0.4, 0.6);
            untouched.extend_from_slice(&l);
        }
        assert_eq!(render(0.0), untouched, "amount 0 must be sample-identical");
        assert_ne!(render(1.0), untouched, "amount 1 must change the render");
    }
    // BLOOM routing culled 2026-08-10 (no product surface): set_bloom must
    // never change the render, at any amount.
    #[test]
    fn bloom_is_culled_and_never_changes_the_render() {
        let sr = 48_000.0;
        const BASE_PREAMP: f32 = 0.3;
        // Drum pops: hits with quiet tails, the material BLOOM is built for.
        let mut signal = Vec::with_capacity(48_000);
        for i in 0..48_000 {
            let t = i as f64 / sr;
            let phase = t % 0.25;
            let hit = (core::f64::consts::TAU * 90.0 * phase).sin() * (-phase / 0.03).exp();
            signal.push((hit * 0.9 + 0.01 * (core::f64::consts::TAU * 200.0 * t).sin()) as f32);
        }
        let render = |bloom: f32| -> (Vec<f32>, f32, f32) {
            let mut e = FilterEngine::new();
            e.prepare(sr);
            e.load_cartridge(make_passthrough_cartridge());
            e.set_input_mode(InputMode::MackieDeskSlam);
            e.set_input_preamp(BASE_PREAMP);
            e.set_env(0.0, 200.0); // ENV->MORPH off: BLOOM alone
            e.set_bloom(bloom);
            let mut out = Vec::new();
            let (mut lo, mut hi) = (f32::INFINITY, f32::NEG_INFINITY);
            for chunk in signal.chunks(crate::env::ENV_HOP) {
                let mut l = chunk.to_vec();
                let mut r = chunk.to_vec();
                e.process_block(&mut l, &mut r, 0.5, 0.5);
                lo = lo.min(e.preamp_drive());
                hi = hi.max(e.preamp_drive());
                out.extend_from_slice(&l);
            }
            (out, lo, hi)
        };
        let (off, off_lo, off_hi) = render(0.0);
        let (on, _on_lo, _on_hi) = render(1.0);
        assert_eq!(on.len(), off.len());
        assert_eq!(on, off, "bloom is culled: amount must never change audio");
        assert!(
            off_hi <= BASE_PREAMP && off_hi > BASE_PREAMP - 1.0e-3,
            "the desk must sit on the base preamp exactly: {off_lo}..{off_hi}"
        );
        // ...and bloom 0 is byte-identical to never arming it at all.
        let mut never = FilterEngine::new();
        never.prepare(sr);
        never.load_cartridge(make_passthrough_cartridge());
        never.set_input_mode(InputMode::MackieDeskSlam);
        never.set_input_preamp(BASE_PREAMP);
        let mut untouched = Vec::new();
        for chunk in signal.chunks(crate::env::ENV_HOP) {
            let mut l = chunk.to_vec();
            let mut r = chunk.to_vec();
            never.process_block(&mut l, &mut r, 0.5, 0.5);
            untouched.extend_from_slice(&l);
        }
        assert_eq!(off, untouched, "bloom 0 must be sample-identical to disabled");
    }
    #[test]
    fn empty_engine_passes_buffer_through_unchanged() {
        let mut engine = FilterEngine::new();
        engine.prepare(44100.0);
        let mut l = vec![0.1_f32, -0.2, 0.3, -0.4];
        let mut r = vec![-0.5_f32, 0.6, -0.7, 0.8];
        let l_copy = l.clone();
        let r_copy = r.clone();
        engine.process_block(&mut l, &mut r, 0.0, 0.0);
        assert_eq!(l, l_copy);
        assert_eq!(r, r_copy);
    }
    #[test]
    fn passthrough_cartridge_preserves_energy() {
        let mut engine = FilterEngine::new();
        engine.prepare(44100.0);
        engine.load_cartridge(make_passthrough_cartridge());
        let mut l = vec![0.0_f32; 256];
        let mut r = vec![0.0_f32; 256];
        l[0] = 1.0;
        r[0] = 1.0;
        let input_sum_sq: f32 =
            l.iter().map(|&s| s * s).sum::<f32>() + r.iter().map(|&s| s * s).sum::<f32>();
        engine.process_block(&mut l, &mut r, 0.5, 0.5);
        let output_sum_sq: f32 =
            l.iter().map(|&s| s * s).sum::<f32>() + r.iter().map(|&s| s * s).sum::<f32>();
        let expected = input_sum_sq;
        assert!(
            (output_sum_sq - expected).abs() < 0.1,
            "impulse energy drifted: in={input_sum_sq} out={output_sum_sq} \
             expected={expected}"
        );
        assert!(!engine.take_instability_flag());
    }
    #[test]
    fn prepare_installs_sample_rate_adjusted_agc_table() {
        let mut engine = FilterEngine::new();
        engine.prepare(65_000.0);
        assert_eq!(engine.active_agc_table, active_agc_table(65_000.0));
        engine.prepare(65_000.1);
        assert_eq!(engine.active_agc_table, active_agc_table(65_000.1));
        engine.prepare(130_000.1);
        assert_eq!(engine.active_agc_table, active_agc_table(130_000.1));
    }
    fn make_resonant_cartridge() -> Cartridge {
        let mut corner = [PASSTHROUGH_COEFFS; crate::cascade::NUM_STAGES];
        corner[0] = [0.90, -0.20, 0.08, -0.72, 0.20];
        cartridge_from_corner("resonant", corner)
    }
    #[test]
    fn amount_zero_drives_cascade_targets_to_passthrough() {
        let mut engine = FilterEngine::new();
        engine.prepare(44100.0);
        engine.load_cartridge(make_resonant_cartridge());
        engine.set_amount(0.0);
        let mut l = vec![0.0_f32; BLOCK_SIZE * 8];
        let mut r = vec![0.0_f32; BLOCK_SIZE * 8];
        engine.process_block(&mut l, &mut r, 0.5, 0.5);
        let mut coeffs = [[0.0_f64; crate::cascade::NUM_COEFFS]; crate::cascade::NUM_STAGES];
        engine.cascade_l.get_coeffs(&mut coeffs);
        for stage in coeffs.iter() {
            for (i, &c) in stage.iter().enumerate() {
                assert!(
                    (c - PASSTHROUGH_COEFFS[i]).abs() < 1e-6,
                    "amount=0 should drive every stage to passthrough, got {stage:?}"
                );
            }
        }
    }
    #[test]
    fn amount_one_leaves_cascade_targets_at_full_strength() {
        let mut engine = FilterEngine::new();
        engine.prepare(44100.0);
        let cartridge = make_resonant_cartridge();
        let expected = cartridge.interpolate(0.5, 0.5, 0.0)[0];
        engine.load_cartridge(cartridge);
        engine.set_amount(1.0);
        let mut l = vec![0.0_f32; BLOCK_SIZE * 2048];
        let mut r = vec![0.0_f32; BLOCK_SIZE * 2048];
        engine.process_block(&mut l, &mut r, 0.5, 0.5);
        let mut coeffs = [[0.0_f64; crate::cascade::NUM_COEFFS]; crate::cascade::NUM_STAGES];
        engine.cascade_l.get_coeffs(&mut coeffs);
        for (i, &c) in coeffs[0].iter().enumerate() {
            assert!(
                (c - expected[i]).abs() < 1e-6,
                "amount=1 should leave stage 0 at full strength, got {:?}",
                coeffs[0]
            );
        }
    }
    #[test]
    fn amount_actually_changes_the_processed_output() {
        let mut full = FilterEngine::new();
        full.prepare(44100.0);
        full.load_cartridge(make_resonant_cartridge());
        full.set_amount(1.0);
        let mut flat = FilterEngine::new();
        flat.prepare(44100.0);
        flat.load_cartridge(make_resonant_cartridge());
        flat.set_amount(0.0);
        let make_input = || {
            let mut l = vec![0.0_f32; BLOCK_SIZE * 16];
            let mut r = vec![0.0_f32; BLOCK_SIZE * 16];
            for i in 0..l.len() {
                l[i] = 0.3 * ((i as f32) * 0.05).sin();
                r[i] = l[i];
            }
            (l, r)
        };
        let (mut l1, mut r1) = make_input();
        full.process_block(&mut l1, &mut r1, 0.5, 0.5);
        let (mut l0, mut r0) = make_input();
        flat.process_block(&mut l0, &mut r0, 0.5, 0.5);
        let tail = BLOCK_SIZE * 4;
        let diff: f32 = l1[tail..]
            .iter()
            .zip(l0[tail..].iter())
            .map(|(a, b)| (a - b).abs())
            .sum();
        assert!(
            diff > 0.5,
            "amount=1 vs amount=0 output should clearly differ, diff={diff}"
        );
        assert!(!full.take_instability_flag());
        assert!(!flat.take_instability_flag());
    }
    #[test]
    fn amount_tapers_the_cascade_peak_smoothly_and_monotonically() {
        let steps = [1.0f32, 0.75, 0.5, 0.25, 0.0];
        let mut peaks_db = Vec::new();
        for &amt in &steps {
            let mut engine = FilterEngine::new();
            engine.prepare(44100.0);
            engine.load_cartridge(make_resonant_cartridge());
            engine.set_amount(amt);
            let mut l = vec![0.0_f32; BLOCK_SIZE * 2048];
            let mut r = vec![0.0_f32; BLOCK_SIZE * 2048];
            engine.process_block(&mut l, &mut r, 0.5, 0.5);
            let mut coeffs = [[0.0_f64; NUM_COEFFS]; crate::cascade::NUM_STAGES];
            engine.cascade_l.get_coeffs(&mut coeffs);
            let peak = compute_cascade_peak(&coeffs, 44100.0);
            peaks_db.push(20.0 * peak.log10());
        }
        println!(
            "amount -> peak (dB): {:?}",
            steps.iter().zip(&peaks_db).collect::<Vec<_>>()
        );
        assert!(
            peaks_db[0] > 1.0,
            "amount=1 should show real resonance gain, got {} dB",
            peaks_db[0]
        );
        assert!(
            peaks_db[4].abs() < 0.01,
            "amount=0 should be exactly flat (0 dB), got {} dB",
            peaks_db[4]
        );
        for i in 1..peaks_db.len() {
            assert!(
                peaks_db[i] <= peaks_db[i - 1] + 0.05,
                "peak should taper monotonically as amount decreases: {:?}",
                peaks_db
            );
        }
    }
    #[test]
    fn num_coeffs_matches_corner_shape() {
        use crate::cascade::{NUM_COEFFS, NUM_STAGES};
        let _: [[f64; NUM_COEFFS]; NUM_STAGES] = [[0.0; NUM_COEFFS]; NUM_STAGES];
    }
    #[test]
    fn agc_drive_unity_is_identity_higher_compresses() {
        fn run(drive: f32) -> f32 {
            let mut eng = FilterEngine::new();
            eng.prepare(44_100.0);
            eng.load_cartridge(make_passthrough_cartridge());
            eng.debug.saturation_enabled = false;
            eng.debug.dc_block_enabled = false;
            eng.set_agc_drive(drive);
            let mut l = vec![0.7f32; 512];
            let mut r = l.clone();
            eng.process_block(&mut l, &mut r, 0.5, 0.5);
            let tail = &l[128..];
            (tail.iter().map(|s| s * s).sum::<f32>() / tail.len() as f32).sqrt()
        }
        let unity = run(1.0);
        let driven = run(8.0);
        // Unity is the shipped default: the AGC is asleep and the core is
        // broadband-unity for a passthrough body.
        let expected = 0.7;
        assert!(
            (unity - expected).abs() < 0.02,
            "unity drive must pass {expected} untouched (AGC asleep in float domain), got {unity}"
        );
        assert!(
            driven < unity * 0.85,
            "agc_drive=8 must visibly compress; got driven={driven} vs unity={unity}"
        );
    }
    #[test]
    #[ignore = "renders audition WAVs to target/agc_audition"]
    fn render_agc_audition() {
        use crate::cartridge::Cartridge;
        const BODY: &[u8; 240] = include_bytes!("../tests/fixtures/sf_mouth_frame.body240");
        let bytes = BODY.as_slice();
        let out = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("target/agc_audition");
        std::fs::create_dir_all(&out).expect("mkdir out");
        let sr_emu = 48_000.0f64;
        let out_sr = 48_000u32;
        let secs = 6.0f64;
        let total = (sr_emu * secs) as usize;
        let block = 256usize;
        let write_wav = |path: &std::path::Path, samples: &[f32]| {
            let data_len = (samples.len() * 2) as u32;
            let mut b: Vec<u8> = Vec::with_capacity(44 + data_len as usize);
            b.extend_from_slice(b"RIFF");
            b.extend_from_slice(&(36 + data_len).to_le_bytes());
            b.extend_from_slice(b"WAVE");
            b.extend_from_slice(b"fmt ");
            b.extend_from_slice(&16u32.to_le_bytes());
            b.extend_from_slice(&1u16.to_le_bytes());
            b.extend_from_slice(&1u16.to_le_bytes());
            b.extend_from_slice(&out_sr.to_le_bytes());
            b.extend_from_slice(&(out_sr * 2).to_le_bytes());
            b.extend_from_slice(&2u16.to_le_bytes());
            b.extend_from_slice(&16u16.to_le_bytes());
            b.extend_from_slice(b"data");
            b.extend_from_slice(&data_len.to_le_bytes());
            for &s in samples {
                b.extend_from_slice(&((s.clamp(-1.0, 1.0) * 32767.0) as i16).to_le_bytes());
            }
            std::fs::write(path, b).expect("write wav");
        };
        for &drive in &[1.0f32, 4.0, 8.0] {
            let mut eng = FilterEngine::new();
            eng.prepare(sr_emu);
            eng.load_cartridge(Cartridge::from_body_bytes("cleanroom", bytes, 1.0).unwrap());
            eng.set_agc_drive(drive);
            let mut rng = 0x2545_F491_4F6C_DD1Du64;
            let mut pb = [0f64; 7];
            let mut wet: Vec<f32> = Vec::with_capacity(total);
            let mut off = 0;
            while off < total {
                let len = block.min(total - off);
                let mut l = vec![0f32; len];
                for s in l.iter_mut() {
                    rng = rng
                        .wrapping_mul(6364136223846793005)
                        .wrapping_add(1442695040888963407);
                    let white = ((rng >> 40) as f64 / (1u64 << 23) as f64) - 1.0;
                    pb[0] = 0.99886 * pb[0] + white * 0.0555179;
                    pb[1] = 0.99332 * pb[1] + white * 0.0750759;
                    pb[2] = 0.96900 * pb[2] + white * 0.1538520;
                    pb[3] = 0.86650 * pb[3] + white * 0.3104856;
                    pb[4] = 0.55000 * pb[4] + white * 0.5329522;
                    pb[5] = -0.7616 * pb[5] - white * 0.0168980;
                    let p =
                        (pb[0] + pb[1] + pb[2] + pb[3] + pb[4] + pb[5] + pb[6] + white * 0.5362)
                            * 0.11;
                    pb[6] = white * 0.115926;
                    *s = (p as f32) * 0.85;
                }
                let mut r = l.clone();
                let morph = off as f64 / total as f64;
                eng.process_block(&mut l, &mut r, morph, 0.5);
                wet.extend_from_slice(&l);
                off += len;
            }
            let ratio = sr_emu / out_sr as f64;
            let out_n = (wet.len() as f64 / ratio) as usize;
            let resampled: Vec<f32> = (0..out_n)
                .map(|i| {
                    let pos = i as f64 * ratio;
                    let i0 = pos.floor() as usize;
                    let frac = (pos - i0 as f64) as f32;
                    let a = wet.get(i0).copied().unwrap_or(0.0);
                    let bb = wet.get(i0 + 1).copied().unwrap_or(a);
                    a + (bb - a) * frac
                })
                .collect();
            let peak = resampled.iter().fold(0.0f32, |m, &s| m.max(s.abs()));
            let path = out.join(format!("cleanroom_pink_sweep_drive{drive}.wav"));
            write_wav(&path, &resampled);
            println!("wrote {} (peak {:.3})", path.display(), peak);
        }
    }
    #[test]
    #[ignore = "diagnostic: pre-AGC scale vs AGC engagement"]
    fn diag_pre_agc_scaling() {
        use crate::agc::{active_agc_table, agc_step};
        use crate::cartridge::Cartridge;
        const BODY: &[u8; 240] = include_bytes!("../tests/fixtures/sf_mouth_frame.body240");
        let bytes = BODY.as_slice();
        let mut eng = FilterEngine::new();
        eng.prepare(48_000.0);
        eng.load_cartridge(Cartridge::from_body_bytes("d", &bytes, 1.0).unwrap());
        eng.debug.agc_enabled = false;
        eng.debug.saturation_enabled = false;
        eng.debug.dc_block_enabled = false;
        let n = 48_000usize;
        let mut ph = 0.0f64;
        let mut l: Vec<f32> = (0..n)
            .map(|_| {
                let s = ((ph * 2.0 - 1.0) * 0.36) as f32 * 0.85;
                ph = (ph + 110.0 / 48_000.0).fract();
                s
            })
            .collect();
        let mut r = l.clone();
        eng.process_block(&mut l, &mut r, 0.5, 1.0);
        let cascade = &l[4096..];
        let rms = |v: &[f32]| (v.iter().map(|s| s * s).sum::<f32>() / v.len() as f32).sqrt();
        let peak = |v: &[f32]| v.iter().fold(0.0f32, |m, &s| m.max(s.abs()));
        let cas_rms = rms(cascade);
        println!(
            "\n=== clean-room raw cascade (no AGC/sat/DC), q1, in=0.85: peak={:.3} rms={:.3} ===",
            peak(cascade),
            cas_rms
        );
        println!("pre-AGC scale -> compression after rescale, min agc_gain reached");
        let agc_table = active_agc_table(48_000.0);
        for scale in [1.0f32, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0] {
            let mut gain = 1.0f32;
            let mut min_gain = 1.0f32;
            let out: Vec<f32> = cascade
                .iter()
                .map(|&s| {
                    let y = agc_step(s * scale, &mut gain, &agc_table) / scale;
                    min_gain = min_gain.min(gain);
                    y
                })
                .collect();
            let red_db = 20.0 * (rms(&out) / cas_rms).max(1e-9).log10();
            println!("  x{scale:<6} -> {red_db:>7.2} dB   min_gain={min_gain:.4}");
        }
    }
    #[test]
    #[ignore = "diagnostic: which limiter is doing the work"]
    fn diag_limiter_handoff() {
        const SR: f64 = 48_000.0;
        const TAIL: usize = 12_288;
        let run = |cycles: f64, drive: f32, agc: bool, sat: bool, amp: f32| -> Probe {
            probe_body(cycles, Some(drive), agc, sat, amp)
        };
        let mut best = (0.0f64, 0.0f32);
        for k in 1..600 {
            let c = k as f64 * 4.0;
            let p = run(c, 1.0, false, false, 1.0).peak;
            if p > best.1 {
                best = (c, p);
            }
        }
        let (cyc, hot_peak) = best;
        println!("\n=== who is limiting? (real body, q=1, host rate) ===");
        println!(
            "AGC first tooth = |x| >= 2.0 (+6.0 dBFS)   saturate() knee = {SATURATE_KNEE} \
             (+{:.1} dBFS, safety net)   fixed post-AGC trim = none",
            20.0 * SATURATE_KNEE.log10()
        );
        println!(
            "body resonance at {:.0} Hz: full-scale input -> raw peak {:.3} (+{:.1} dBFS)\n",
            SR * cyc / TAIL as f64,
            hot_peak,
            20.0 * hot_peak.log10()
        );
        println!("  in    raw_peak | STOCK (agc+sat)                | AGC only  | sat only");
        println!("                 | peak    rms    resid    agc_g  | resid     | resid");
        for &amp in &[0.4f32, 0.6, 0.8, 0.9, 1.0] {
            let raw = run(cyc, 1.0, false, false, amp).peak;
            let s = run(cyc, 1.0, true, true, amp);
            let d_agc = run(cyc, 1.0, true, false, amp).resid_dbc;
            let d_sat = run(cyc, 1.0, false, true, amp).resid_dbc;
            println!(
                "  {amp:.1}   {raw:6.3}  | {:6.3} {:6.3} {:7.1}dBc {:6.4} | {d_agc:6.1}dBc | {d_sat:6.1}dBc",
                s.peak, s.rms, s.resid_dbc, s.agc_gain
            );
        }
        println!("\n=== agc_drive sweep at full-scale input ===");
        println!("  drive   peak    rms    resid     agc_gain | attack peak (AGC alone, sat off)");
        for &drive in &[1.0f32, 1.5, 2.0, AGC_DRIVE, 3.0, 4.0] {
            let s = run(cyc, drive, true, true, 1.0);
            let attack = run(cyc, drive, true, false, 1.0).attack;
            println!(
                "  x{drive:<5.2}  {:6.3} {:6.3} {:7.1}dBc {:6.4} | {attack:8.3}",
                s.peak, s.rms, s.resid_dbc, s.agc_gain
            );
        }
    }
    #[test]
    #[ignore = "diagnostic: which of the 16 AGC teeth the shipped drive uses"]
    fn diag_agc_curve() {
        use crate::agc::active_agc_table;
        use crate::dsp::BASE_AGC_TABLE;
        const SR: f64 = 48_000.0;
        let table = active_agc_table(SR);
        assert_eq!(
            table, BASE_AGC_TABLE,
            "host rate must use the verified 16 values unmodified"
        );
        println!("\n=== the AGC curve in use at {SR} Hz (16 values, verified) ===");
        for (i, v) in table.iter().enumerate() {
            let tag = if *v >= 1.0 { "no reduction" } else { "REDUCES" };
            println!("  [{i:2}] {v:.4}   {tag}");
        }
        println!("\nindex = (agc_gain * |x| * AGC_DRIVE) as int & 0xF");
        println!(
            "AGC_DRIVE = {AGC_DRIVE:.4} (RE-vault: the DLL path has no pre-scale; the old \
             2.0/0.9 = 2.2222 default was ours)\n"
        );
        const HOT_CYCLES: f64 = 964.0;
        println!("what the leveler hands to the saturator (safety-net knee = {SATURATE_KNEE}):");
        for &(label, drive) in &[
            ("SHIPPED (unity)", AGC_DRIVE),
            ("OLD  (drive 2.2222)", 2.0f32 / 0.9),
        ] {
            let leveller = probe_body(HOT_CYCLES, Some(drive), true, false, 1.0);
            let shipped = probe_body(HOT_CYCLES, Some(drive), true, true, 1.0);
            let verdict = if shipped.resid_dbc > leveller.resid_dbc + 1.0 {
                "-> tanh must finish the job, and it distorts doing it"
            } else {
                "-> tanh idle, the curve owns the level"
            };
            println!(
                "  {label:16} settles at peak {:.3}  {verdict}\n{:18}distortion: {:.1} dBc leveller alone -> {:.1} dBc with the tanh",
                leveller.peak, "", leveller.resid_dbc, shipped.resid_dbc
            );
        }
    }
    #[test]
    #[ignore = "diagnostic: why morph became musical"]
    fn diag_morph_musicality() {
        use crate::cartridge::Cartridge;
        const SR: f64 = 48_000.0;
        const N: usize = 8192;
        let saw: Vec<f32> = (0..N)
            .map(|i| {
                let ph = (110.0 * i as f64 / SR).fract();
                ((ph * 2.0 - 1.0) * 0.7) as f32
            })
            .collect();
        let at = |drive: f32, morph: f64| -> (f32, f32) {
            let mut eng = FilterEngine::new();
            eng.prepare(SR);
            eng.load_cartridge(
                Cartridge::from_body_bytes(
                    "d",
                    include_bytes!("../tests/fixtures/sf_mouth_frame.body240").as_slice(),
                    1.0,
                )
                .unwrap(),
            );
            eng.set_agc_drive(drive);
            let mut l = saw.clone();
            let mut r = saw.clone();
            eng.process_block(&mut l, &mut r, morph, 1.0);
            let mut l = saw.clone();
            let mut r = saw.clone();
            eng.process_block(&mut l, &mut r, morph, 1.0);
            let peak = l.iter().fold(0.0f32, |m, &s| m.max(s.abs()));
            let rms =
                ((l.iter().map(|&s| (s as f64).powi(2)).sum::<f64>()) / N as f64).sqrt() as f32;
            let crest = 20.0 * (peak / rms.max(1e-9)).log10();
            (rms, crest)
        };
        println!("\n=== a morph sweep, before and after ===");
        println!("crest: 3.01 dB = a sine.  ~1 dB = a square wave (fully clipped).\n");
        println!("  morph |   OLD (drive 1.0)      |   SHIPPED");
        println!("        |   rms     crest        |   rms     crest");
        let (mut old_rms, mut new_rms) = (Vec::new(), Vec::new());
        for k in 0..=10 {
            let m = k as f64 / 10.0;
            let (o_r, o_c) = at(1.0, m);
            let (n_r, n_c) = at(AGC_DRIVE, m);
            old_rms.push(o_r);
            new_rms.push(n_r);
            println!("   {m:.1}   |  {o_r:.3}   {o_c:5.2} dB     |  {n_r:.3}   {n_c:5.2} dB");
        }
        let spread = |v: &[f32]| {
            let (lo, hi) = v
                .iter()
                .fold((f32::MAX, 0.0f32), |(l, h), &x| (l.min(x), h.max(x)));
            20.0 * (hi / lo.max(1e-9)).log10()
        };
        println!(
            "\nlevel contour across the sweep:  OLD {:.2} dB   SHIPPED {:.2} dB",
            spread(&old_rms),
            spread(&new_rms)
        );
        println!("(a sweep with no level contour is a sweep you cannot feel)");
    }
    #[test]
    #[ignore = "diagnostic: is the identity body a true bypass"]
    fn diag_identity_transparency() {
        use crate::cartridge::Cartridge;
        const SR: f64 = 48_000.0;
        const N: usize = 8192;
        const IDENTITY: &[u8; 240] = include_bytes!("../../plugin/assets/bodies/identity.body240");
        let run = |amp: f32, agc: bool, sat: bool, dc: bool| -> (f32, f64) {
            let mut eng = FilterEngine::new();
            eng.prepare(SR);
            eng.load_cartridge(
                Cartridge::from_body_bytes("identity", IDENTITY.as_slice(), 1.0).unwrap(),
            );
            eng.debug.agc_enabled = agc;
            eng.debug.saturation_enabled = sat;
            eng.debug.dc_block_enabled = dc;
            let dry: Vec<f32> = (0..N)
                .map(|i| {
                    let ph = (220.0 * i as f64 / SR).fract();
                    (amp as f64 * (ph * 2.0 - 1.0)) as f32
                })
                .collect();
            let mut l = dry.clone();
            let mut r = dry.clone();
            eng.process_block(&mut l, &mut r, 0.5, 0.5);
            let resid = (l
                .iter()
                .zip(dry.iter())
                .map(|(&o, &d)| ((o - d) as f64).powi(2))
                .sum::<f64>()
                / N as f64)
                .sqrt();
            (
                l.iter().fold(0.0f32, |m, &s| m.max(s.abs())),
                20.0 * resid.max(1e-12).log10(),
            )
        };
        println!("\n=== identity body: is the CASCADE itself identity? ===");
        println!("(everything after it switched off — this is the packed body alone)");
        for &amp in &[0.3f32, 1.0] {
            let (p, d) = run(amp, false, false, false);
            let v = if d < -100.0 {
                "EXACT identity"
            } else {
                "NOT identity — the body is wrong"
            };
            println!("   in {amp:.2} -> peak {p:.4}   null {d:8.1} dBFS   {v}");
        }
        println!("\n=== which stage breaks the bypass? ===");
        println!("  in    | cascade only | +DC block | +AGC     | +saturator (full)");
        for &amp in &[0.3f32, 0.6, 0.9, 1.0] {
            let a = run(amp, false, false, false).1;
            let b = run(amp, false, false, true).1;
            let c = run(amp, true, false, true).1;
            let d = run(amp, true, true, true).1;
            println!("  {amp:.2}  | {a:9.1} dB | {b:6.1} dB | {c:6.1} dB | {d:6.1} dB");
        }
    }
    #[test]
    fn moving_morph_is_identical_at_any_buffer_size() {
        use crate::cartridge::Cartridge;
        const BODY: &[u8; 240] = include_bytes!("../tests/fixtures/sf_mouth_frame.body240");
        const N: usize = 8192;
        let render = |block: usize| -> Vec<f32> {
            let mut eng = FilterEngine::new();
            eng.prepare(48_000.0);
            eng.load_cartridge(Cartridge::from_body_bytes("d", BODY.as_slice(), 1.0).unwrap());
            let src: Vec<f32> = (0..N)
                .map(|i| ((i as f64 * 0.013).sin() * 0.4) as f32)
                .collect();
            let mut out = Vec::with_capacity(N);
            let mut off = 0;
            while off < N {
                let n = block.min(N - off);
                let mut l = src[off..off + n].to_vec();
                let mut r = l.clone();
                eng.process_block(&mut l, &mut r, 0.5, 1.0);
                out.extend_from_slice(&l);
                off += n;
            }
            out
        };
        let reference = render(512);
        for &block in &[16usize, 32, 64, 100, 128, 333, 1024] {
            let other = render(block);
            let peak = reference
                .iter()
                .zip(other.iter())
                .map(|(a, b)| (a - b).abs())
                .fold(0.0f32, f32::max);
            let db = if peak <= 0.0 {
                -300.0
            } else {
                20.0 * (peak as f64).log10()
            };
            assert!(
                db < -120.0,
                "block {block} vs 512: peak residual {db:.1} dBFS — the ramp is block-size \
                 dependent again (E3). The control grid phase must survive block boundaries."
            );
        }
    }
    #[test]
    fn saturator_never_adds_distortion_to_the_leveller() {
        const HOT_CYCLES: f64 = 964.0;
        let leveller_only = probe_body(HOT_CYCLES, None, true, false, 1.0);
        let shipped = probe_body(HOT_CYCLES, None, true, true, 1.0);
        assert!(
            shipped.resid_dbc <= leveller_only.resid_dbc + 1.0,
            "the saturator is doing the levelling: {:.1} dBc with it vs {:.1} dBc without \
             (steady peak {:.3} vs knee {SATURATE_KNEE}). Check AGC_DRIVE.",
            shipped.resid_dbc,
            leveller_only.resid_dbc,
            leveller_only.peak
        );
    }
    struct Probe {
        peak: f32,
        rms: f32,
        resid_dbc: f64,
        agc_gain: f32,
        attack: f32,
    }
    fn probe_body(cycles: f64, drive: Option<f32>, agc: bool, sat: bool, amp: f32) -> Probe {
        use crate::cartridge::Cartridge;
        use std::f64::consts::PI;
        const BODY: &[u8; 240] = include_bytes!("../tests/fixtures/sf_mouth_frame.body240");
        const SR: f64 = 48_000.0;
        const WARMUP: usize = 4096;
        const TAIL: usize = 12_288;
        let f0 = SR * cycles / TAIL as f64;
        let mut eng = FilterEngine::new();
        eng.prepare(SR);
        eng.load_cartridge(Cartridge::from_body_bytes("d", BODY.as_slice(), 1.0).unwrap());
        eng.debug.agc_enabled = agc;
        eng.debug.saturation_enabled = sat;
        eng.debug.dc_block_enabled = false;
        if let Some(d) = drive {
            eng.set_agc_drive(d);
        }
        let tone = |i: usize| (amp as f64 * (2.0 * PI * f0 * i as f64 / SR).sin()) as f32;
        let mut sl: Vec<f32> = (0..WARMUP).map(tone).collect();
        let mut sr_ = sl.clone();
        eng.process_block(&mut sl, &mut sr_, 0.5, 1.0);
        let attack = sl.iter().fold(0.0f32, |m, &s| m.max(s.abs()));
        let mut l: Vec<f32> = (0..TAIL).map(|i| tone(WARMUP + i)).collect();
        let mut r = l.clone();
        eng.process_block(&mut l, &mut r, 0.5, 1.0);
        let (mut cr, mut ci) = (0.0f64, 0.0f64);
        for (i, &s) in l.iter().enumerate() {
            let ph = 2.0 * PI * f0 * (WARMUP + i) as f64 / SR;
            cr += s as f64 * ph.cos();
            ci += s as f64 * ph.sin();
        }
        let (a, b) = (2.0 * cr / TAIL as f64, 2.0 * ci / TAIL as f64);
        let fund_rms = (a * a + b * b).sqrt() / 2.0f64.sqrt();
        let mut acc = 0.0f64;
        for (i, &s) in l.iter().enumerate() {
            let ph = 2.0 * PI * f0 * (WARMUP + i) as f64 / SR;
            acc += (s as f64 - (a * ph.cos() + b * ph.sin())).powi(2);
        }
        Probe {
            peak: l.iter().fold(0.0f32, |m, &s| m.max(s.abs())),
            rms: ((l.iter().map(|&s| (s as f64).powi(2)).sum::<f64>()) / TAIL as f64).sqrt() as f32,
            resid_dbc: 20.0
                * ((acc / TAIL as f64).sqrt() / fund_rms.max(1e-12))
                    .max(1e-12)
                    .log10(),
            agc_gain: eng.agc_gain,
            attack,
        }
    }
}
#[cfg(test)]
mod trajectory_law {
    use super::*;
    use crate::cartridge::Cartridge;
    const SR: f64 = 48_000.0;
    fn resonant_body() -> Vec<u8> {
        // Four distinct corners so Morph/Q travel actually moves poles.
        let mk = |hz: f64, r: f64| {
            let angle = core::f64::consts::TAU * hz / SR;
            let (a1, a2) = (-2.0 * r * angle.cos(), r * r);
            // kernel form: [2-a1-ish…] use biquad->kernel inverse via from_corner_data
            [1.0 - a2, 0.0, 0.0, a1, a2]
        };
        let corner = |hz: f64| {
            let mut c = [[1.0f64, 0.0, 0.0, 0.0, 0.0]; NUM_STAGES];
            c[0] = mk(hz, 0.92);
            c[3] = mk(hz * 2.0, 0.9);
            c
        };
        // biquad rows -> kernel encoding path: PackedCorners::from_corner_data
        // takes KERNEL rows; convert through the crate's own inverse by
        // encoding kernel k such that kernel_to_biquad(k) == biquad row.
        let to_kernel = |b: [f64; 5]| {
            let c4 = b[0];
            let c0 = if c4.abs() > 1.0e-12 { b[1] / c4 + 2.0 } else { 2.0 };
            let c1 = if c4.abs() > 1.0e-12 { 1.0 - b[2] / c4 } else { 1.0 };
            [c0, c1, b[3] + 2.0, 1.0 - b[4], c4]
        };
        let mut corners =
            [[[0.0f64; NUM_COEFFS]; crate::minifloat::LEGACY_STAGES]; crate::minifloat::LEGACY_CORNERS];
        for (ci, hz) in [(0usize, 300.0f64), (1, 1200.0), (2, 500.0), (3, 2400.0)] {
            let c = corner(hz);
            for si in 0..crate::minifloat::LEGACY_STAGES {
                corners[ci][si] = to_kernel(c[si]);
            }
        }
        crate::minifloat::PackedCorners::from_legacy_corner_data(&corners)
            .to_rom_bytes()
            .to_vec()
    }
    fn engine_with_body() -> FilterEngine {
        let mut e = FilterEngine::new();
        e.prepare(SR);
        e.load_cartridge(Cartridge::from_body_bytes_at("t", &resonant_body(), 1.0, 0.0).unwrap());
        e
    }
    fn saw(n: usize) -> Vec<f32> {
        (0..n)
            .map(|i| (((i as f64 * 110.0 / SR) % 1.0) * 2.0 - 1.0) as f32 * 0.25)
            .collect()
    }
    /// A constant trajectory is sample-identical to the static path — Movement
    /// OFF is the static filter, bit for bit.
    #[test]
    fn constant_trajectory_matches_static_path_exactly() {
        let n = 4096;
        let sig = saw(n);
        let mut a = engine_with_body();
        let (mut al, mut ar) = (sig.clone(), sig.clone());
        a.process_block(&mut al, &mut ar, 0.37, 0.61);
        let mut b = engine_with_body();
        let (mut bl, mut br) = (sig.clone(), sig.clone());
        let morph = vec![0.37f32; n];
        b.process_trajectory(&mut bl, &mut br, &morph, 0.61);
        assert_eq!(al, bl, "constant trajectory must equal the static path");
        assert_eq!(ar, br);
    }
    /// Rendering is block-size invariant: the same trajectory split at any
    /// host block size produces the same samples.
    #[test]
    fn rendering_is_block_size_invariant() {
        let n = 6144;
        let sig = saw(n);
        // A trajectory that moves every sample.
        let morph: Vec<f32> = (0..n)
            .map(|i| 0.5 + 0.45 * ((i as f32) * 0.001).sin())
            .collect();
        let render = |block: usize| -> Vec<f32> {
            let mut e = engine_with_body();
            let mut out = Vec::with_capacity(n);
            let mut off = 0;
            while off < n {
                let len = block.min(n - off);
                let mut l = sig[off..off + len].to_vec();
                let mut r = l.clone();
                e.process_trajectory(&mut l, &mut r, &morph[off..off + len], 0.5);
                out.extend_from_slice(&l);
                off += len;
            }
            out
        };
        let reference = render(512);
        for &block in &[32usize, 64, 128, 1024] {
            assert_eq!(render(block), reference, "block {block} must match 512");
        }
    }
    /// A step trajectory lands on the exact sample — the PER-SAMPLE path's
    /// law, pinned with x3_movement OFF. The shipping default is the X3
    /// parity path (2026-08-15 "do what they did verbatim in voice
    /// processing"), where a step deliberately arrives via the block-rate
    /// one-pole and kernel ramp; its shape is proven by the cascade's
    /// x3_kernel_ramp_shape test and the movement A/B plot. This test keeps
    /// the A/B path honest.
    #[test]
    fn step_transition_occurs_on_the_exact_sample() {
        let n = 2048;
        let step_at = 1000;
        let sig = saw(n);
        let mut stepped_morph = vec![0.1f32; n];
        for m in stepped_morph[step_at..].iter_mut() {
            *m = 0.9;
        }
        let mut a = engine_with_body();
        a.set_x3_movement(false);
        let (mut al, mut ar) = (sig.clone(), sig.clone());
        a.process_trajectory(&mut al, &mut ar, &stepped_morph, 0.5);
        let mut b = engine_with_body();
        b.set_x3_movement(false);
        let (mut bl, mut br) = (sig.clone(), sig.clone());
        let flat = vec![0.1f32; n];
        b.process_trajectory(&mut bl, &mut br, &flat, 0.5);
        assert_eq!(
            &al[..step_at],
            &bl[..step_at],
            "before the boundary the renders must be identical"
        );
        // The new coefficients are installed ON the boundary sample. In DF-II
        // the output is y = b0·x + w1, so with equal b0 and equal states the
        // first VISIBLE divergence is the state written on the boundary
        // sample, read one sample later — never any further out than that.
        assert_ne!(
            &al[step_at..step_at + 2],
            &bl[step_at..step_at + 2],
            "the step must be audible within one sample of the boundary"
        );
    }
    /// X3_MOVEMENT_SPEC.md, FUN_1802c0430: the summed Morph target passes
    /// through the one-pole (s = s - s*R + target; out = R*s), which steps
    /// exactly once per 32-sample control tick — a mid-block trajectory
    /// change is not even read until the next tick.
    #[test]
    fn x3_one_pole_smooths_the_summed_target_once_per_tick() {
        const R: f64 = 0.4516276717185974;
        let n = 96;
        let sig = saw(n);
        let mut morph = vec![0.2f32; n];
        for m in morph[40..].iter_mut() {
            *m = 0.9;
        }
        let mut e = engine_with_body();
        let (mut l, mut r) = (sig.clone(), sig.clone());
        e.process_trajectory(&mut l[..64], &mut r[..64], &morph[..64], 0.5);
        let m0 = 0.2f32 as f64;
        let s0 = m0 / R;
        let s1 = s0 - s0 * R + m0;
        assert_eq!(
            e.morph_position(),
            R * s1,
            "the change at sample 40 must not be read before the next tick"
        );
        e.process_trajectory(&mut l[64..], &mut r[64..], &morph[64..], 0.5);
        let s2 = s1 - s1 * R + 0.9f32 as f64;
        assert_eq!(
            e.morph_position(),
            R * s2,
            "the tick 3 coordinate is the one-pole of the summed target, not the raw wheel"
        );
        assert!(e.morph_position() < 0.9);
    }
    /// GROWL is a Morph contributor, and it arrives through the one-pole —
    /// never around it.
    #[test]
    fn growl_arrives_through_the_one_pole_not_around_it() {
        const R: f64 = 0.4516276717185974;
        let n = 64;
        let mut e = engine_with_body();
        e.set_growl(1.0);
        let (mut l, mut r) = (vec![0.0f32; n], vec![0.0f32; n]);
        let morph = vec![0.5f32; n];
        e.process_trajectory(&mut l, &mut r, &morph, 0.5);
        let base = 0.5f32 as f64;
        let mut phase = 0.0f64;
        let mut targets = [0.0f64; 2];
        for i in 0..n {
            phase += 27.5 / SR;
            if phase >= 1.0 {
                phase -= 1.0;
            }
            let g = 1.0f32 as f64 * 0.45 * (core::f64::consts::TAU * phase).sin();
            if i % 32 == 0 {
                targets[i / 32] = (base + 0.0 + g).clamp(0.0, 1.0);
            }
        }
        assert_ne!(targets[1], base, "growl must actually move the target");
        let s0 = targets[0] / R;
        let s1 = s0 - s0 * R + targets[1];
        assert_eq!(
            e.morph_position(),
            R * s1,
            "growl must enter the pole, not bypass it"
        );
    }
    /// FOLLOW's offset is summed into the Morph target BEFORE the one-pole.
    #[test]
    fn follow_arrives_through_the_one_pole_not_around_it() {
        const R: f64 = 0.4516276717185974;
        let mut e = engine_with_body();
        e.set_env(1.0, 200.0);
        let quiet = vec![0.0f32; 32];
        let loud: Vec<f32> = (0..32).map(|i| if i % 2 == 0 { 0.9 } else { -0.9 }).collect();
        let morph = vec![0.1f32; 32];
        let (mut l, mut r) = (quiet.clone(), quiet.clone());
        e.process_trajectory(&mut l, &mut r, &morph, 0.5);
        let coord0 = e.morph_position();
        let (mut l, mut r) = (loud.clone(), loud.clone());
        e.process_trajectory(&mut l, &mut r, &morph, 0.5);
        let off = e.env.offset();
        assert!(off > 0.0, "the detector must have heard the loud hop");
        let m1 = (0.1f32 as f64 + off + 0.0).clamp(0.0, 1.0);
        let s0 = coord0 / R;
        let s1 = s0 - s0 * R + m1;
        assert!(
            (e.morph_position() - R * s1).abs() < 1e-9,
            "FOLLOW must enter the pole, not bypass it: got {}, want {}",
            e.morph_position(),
            R * s1
        );
    }
    /// One-block arrival: at sample 0 of block k+1 the running coefficients
    /// sit exactly on block k's smoothed target (vtable 0x08 replay).
    #[test]
    fn x3_coefficients_arrive_one_block_late_exactly() {
        const R: f64 = 0.4516276717185974;
        let n = 65;
        let sig = saw(n);
        let mut morph = vec![0.2f32; n];
        for m in morph[32..].iter_mut() {
            *m = 0.8;
        }
        let mut a = engine_with_body();
        let (mut al, mut ar) = (sig.clone(), sig.clone());
        a.process_trajectory(&mut al, &mut ar, &morph, 0.5);
        let s0 = (0.2f32 as f64) / R;
        let s1 = s0 - s0 * R + 0.8f32 as f64;
        let c1 = R * s1;
        let mut b = engine_with_body();
        b.set_x3_movement(false);
        let (mut bl, mut br) = (vec![0.0f32; 4], vec![0.0f32; 4]);
        b.process_block(&mut bl, &mut br, c1, 0.5);
        let mut rows_a = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
        let mut rows_b = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
        a.cascade_l.get_coeffs(&mut rows_a);
        b.cascade_l.get_coeffs(&mut rows_b);
        for (si, (ra, rb)) in rows_a.iter().zip(rows_b.iter()).enumerate() {
            let want = crate::minifloat::kernel_to_biquad(crate::minifloat::biquad_to_kernel(*rb));
            for (j, (&got, &w)) in ra.iter().zip(want.iter()).enumerate() {
                assert!(
                    (got - w).abs() < 1e-12,
                    "stage {si} coef {j}: got {got}, want {w}"
                );
            }
        }
    }
    /// The whole Morph surface stays finite when swept at audio rate.
    #[test]
    fn full_surface_sweep_stays_finite() {
        let n = 8192;
        let sig = saw(n);
        for q in [0.0f64, 0.5, 1.0] {
            let mut e = engine_with_body();
            let morph: Vec<f32> = (0..n).map(|i| i as f32 / (n - 1) as f32).collect();
            let (mut l, mut r) = (sig.clone(), sig.clone());
            e.process_trajectory(&mut l, &mut r, &morph, q);
            assert!(
                l.iter().chain(r.iter()).all(|s| s.is_finite()),
                "sweep at q={q} must stay finite"
            );
        }
    }
    /// Artifact hunt: is there anything in the output that is NOT the
    /// authored movement? Renders a steady tone through (a) a parked wheel,
    /// (b) a smooth audio-rate sweep, (c) a hard step phrase, and measures
    /// sample-to-sample discontinuities away from the authored step samples.
    /// A spurious click is a derivative outlier where nothing was authored.
    #[test]
    #[ignore = "prints the artifact scan for the audio-rate path"]
    fn scan_for_spurious_discontinuities() {
        const SR: f64 = 48_000.0;
        let n = 96_000usize; // 2 s
        let tone: Vec<f32> = (0..n)
            .map(|i| {
                let t = i as f64 / SR;
                (0.4 * (core::f64::consts::TAU * 220.0 * t).sin()) as f32
            })
            .collect();
        let render = |morph: &[f32]| -> Vec<f32> {
            let mut e = engine_with_body();
            let (mut l, mut r) = (tone.clone(), tone.clone());
            let mut out = Vec::with_capacity(n);
            let mut off = 0;
            while off < n {
                let len = 512.min(n - off);
                e.process_trajectory(
                    &mut l[off..off + len],
                    &mut r[off..off + len],
                    &morph[off..off + len],
                    0.5,
                );
                out.extend_from_slice(&l[off..off + len]);
                off += len;
            }
            out
        };
        // A click is a derivative that is large RELATIVE TO ITS NEIGHBOURHOOD.
        // A resonance sweeping across the tone legitimately raises both the
        // local level and the local derivative together, so the detector
        // compares each sample's Δ against the RMS of Δ in a ±5 ms window
        // around it, excluding a guard after authored steps.
        let scan = |out: &[f32], steps: &[usize], label: &str| {
            // An authored hard step re-rings the cascade; its own settling is
            // signal, not artifact. 10 ms covers the ring at these radii.
            let guard = (SR * 0.010) as usize;
            let win = (SR * 0.005) as usize;
            let d: Vec<f32> = out.windows(2).map(|w| (w[1] - w[0]).abs()).collect();
            let mut worst = 0.0f32;
            let mut worst_at = 0usize;
            'outer: for i in (SR as usize / 10)..d.len() {
                for &s in steps {
                    if i + 1 >= s && i + 1 < s + guard {
                        continue 'outer;
                    }
                }
                let lo = i.saturating_sub(win);
                let hi = (i + win).min(d.len());
                let local_rms = (d[lo..hi].iter().map(|&x| (x as f64) * (x as f64)).sum::<f64>()
                    / (hi - lo) as f64)
                    .sqrt() as f32;
                let ratio = d[i] / local_rms.max(1.0e-9);
                if ratio > worst {
                    worst = ratio;
                    worst_at = i;
                }
            }
            println!("{label}: worst local Δ ratio {:.2}x at sample {}", worst, worst_at);
            worst
        };
        // (a) parked
        let parked = render(&vec![0.4f32; n]);
        let r_parked = scan(&parked, &[], "parked wheel   ");
        // (b) smooth audio-rate sweep, two full passes
        let smooth: Vec<f32> = (0..n)
            .map(|i| 0.5 + 0.45 * ((i as f32) * 2.0 * core::f32::consts::PI / 48_000.0).sin())
            .collect();
        let r_smooth = scan(&render(&smooth), &[], "smooth sweep   ");
        // (c) hard steps every quarter second (the step-phrase law)
        let step_len = SR as usize / 4;
        let stepped: Vec<f32> = (0..n)
            .map(|i| if (i / step_len) % 2 == 0 { 0.1 } else { 0.9 })
            .collect();
        let steps: Vec<usize> = (1..(n / step_len)).map(|k| k * step_len).collect();
        let r_step = scan(&render(&stepped), &steps, "steps (guarded)");
        // Against a ±5 ms local window a clean signal sits low single digits;
        // a genuine click is an order of magnitude above its neighbourhood.
        // The moving paths may not be dirtier than the parked baseline by more
        // than a small margin.
        assert!(r_smooth < r_parked * 1.5, "smooth sweep adds discontinuities over parked");
        assert!(r_step < r_parked * 1.5, "steps leak artifacts outside their boundary");
    }
    /// WHERE THE DIRT COMES FROM. `scan_for_spurious_discontinuities` shows a
    /// PARKED wheel already carrying local-derivative outliers, which means the
    /// morph path is not what is soiling the output. This one feeds a pure
    /// 220 Hz tone through a parked wheel and takes the chain apart stage by
    /// stage: for each configuration it reports the worst local Δ ratio and the
    /// INHARMONIC residual (everything that is not a multiple of 220 Hz, in dBc).
    /// A linear filter on a sine has no inharmonic energy at all, so whatever
    /// stage moves that number is the one making the noise.
    #[test]
    #[ignore = "diagnostic: which stage dirties a parked tone"]
    fn diag_parked_tone_stage_by_stage() {
        const SR: f64 = 48_000.0;
        const N: usize = 48_000; // 1 s -> 1 Hz bins, 220 Hz lands exactly
        const F0: f64 = 220.0;
        let tone = |amp: f32| -> Vec<f32> {
            (0..N)
                .map(|i| (amp as f64 * (core::f64::consts::TAU * F0 * i as f64 / SR).sin()) as f32)
                .collect()
        };
        // Goertzel at one exact bin.
        let bin_energy = |x: &[f32], hz: f64| -> f64 {
            let w = core::f64::consts::TAU * hz / SR;
            let (c, s) = (w.cos(), w.sin());
            let coeff = 2.0 * c;
            let (mut s1, mut s2) = (0.0f64, 0.0f64);
            for &v in x {
                let s0 = v as f64 + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            let re = s1 - s2 * c;
            let im = s2 * s;
            (re * re + im * im) * 4.0 / (x.len() as f64 * x.len() as f64)
        };
        let measure = |out: &[f32]| -> (f64, f32) {
            let total: f64 = out.iter().map(|&v| (v as f64) * (v as f64)).sum::<f64>()
                / out.len() as f64
                * 2.0;
            let mut harmonic = 0.0f64;
            let mut k = 1.0f64;
            while F0 * k < SR * 0.5 {
                harmonic += bin_energy(out, F0 * k);
                k += 1.0;
            }
            let resid = (total - harmonic).max(1.0e-30) / harmonic.max(1.0e-30);
            // worst local derivative outlier, same detector as the artifact scan
            let win = (SR * 0.005) as usize;
            let d: Vec<f32> = out.windows(2).map(|w| (w[1] - w[0]).abs()).collect();
            let mut worst = 0.0f32;
            for i in (SR as usize / 10)..d.len() {
                let lo = i.saturating_sub(win);
                let hi = (i + win).min(d.len());
                let rms = (d[lo..hi].iter().map(|&x| (x as f64) * (x as f64)).sum::<f64>()
                    / (hi - lo) as f64)
                    .sqrt() as f32;
                worst = worst.max(d[i] / rms.max(1.0e-9));
            }
            (10.0 * resid.log10(), worst)
        };
        println!("\n=== parked wheel, 220 Hz sine, stage by stage ===");
        println!("  in     configuration        peak    inharm dBc   worst Δ");
        for &amp in &[0.25f32, 0.7] {
            for (label, agc, sat) in [
                ("SHIPPED", true, true),
                ("AGC off", false, true),
                ("saturate off", true, false),
                ("cascade only", false, false),
            ] {
                let mut e = engine_with_body();
                e.debug.agc_enabled = agc;
                e.debug.saturation_enabled = sat;
                let (mut l, mut r) = (tone(amp), tone(amp));
                let mut off = 0;
                while off < N {
                    let len = 512.min(N - off);
                    e.process_block(&mut l[off..off + len], &mut r[off..off + len], 0.4, 0.5);
                    off += len;
                }
                let peak = l.iter().fold(0.0f32, |a, &v| a.max(v.abs()));
                let (resid, worst) = measure(&l);
                println!("  {amp:.2}   {label:20} {peak:6.3}  {resid:9.1}   {worst:6.2}x");
            }
        }
    }
    /// DOES THE MORPH ITSELF MOVE THE LEVEL? Both sources say the interpolation
    /// is right: Rossum's ARMAdillo closes with "it linearly interpolates the
    /// coefficients in the encoded space at the sample rate", which is what
    /// `interpolate_words` + the per-sample install do. So if a sweep sounds
    /// bad, the suspect is what sits AFTER it. This parks the wheel at 21
    /// positions and reports the settled level and inharmonic residual at each,
    /// so level pumping across the morph can be told apart from the morph.
    #[test]
    #[ignore = "diagnostic: level and dirt across the morph axis"]
    fn diag_level_across_morph() {
        const SR: f64 = 48_000.0;
        const N: usize = 48_000;
        const F0: f64 = 220.0;
        let tone: Vec<f32> = (0..N)
            .map(|i| (0.25 * (core::f64::consts::TAU * F0 * i as f64 / SR).sin()) as f32)
            .collect();
        let bin = |x: &[f32], hz: f64| -> f64 {
            let w = core::f64::consts::TAU * hz / SR;
            let (c, s) = (w.cos(), w.sin());
            let (mut s1, mut s2) = (0.0f64, 0.0f64);
            for &v in x {
                let s0 = v as f64 + 2.0 * c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            let (re, im) = (s1 - s2 * c, s2 * s);
            (re * re + im * im) * 4.0 / (x.len() as f64 * x.len() as f64)
        };
        println!("\n=== level across the morph axis, 220 Hz sine, Q 0.5 ===");
        println!("  morph |  SHIPPED  peak   dBFS   inharm | AGC OFF  peak   dBFS   inharm");
        for step in 0..=20 {
            let m = step as f64 / 20.0;
            let mut row = String::new();
            for agc in [true, false] {
                let mut e = engine_with_body();
                e.debug.agc_enabled = agc;
                let (mut l, mut r) = (tone.clone(), tone.clone());
                let mut off = 0;
                while off < N {
                    let len = 512.min(N - off);
                    e.process_block(&mut l[off..off + len], &mut r[off..off + len], m, 0.5);
                    off += len;
                }
                // settled half only: skip the attack transient
                let tail = &l[N / 2..];
                let peak = tail.iter().fold(0.0f32, |a, &v| a.max(v.abs()));
                let total: f64 =
                    tail.iter().map(|&v| (v as f64) * (v as f64)).sum::<f64>() / tail.len() as f64 * 2.0;
                let mut harm = 0.0f64;
                let mut k = 1.0f64;
                while F0 * k < SR * 0.5 {
                    harm += bin(tail, F0 * k);
                    k += 1.0;
                }
                let resid = 10.0
                    * ((total - harm).max(1.0e-30) / harm.max(1.0e-30)).log10();
                row += &format!(
                    "  {peak:6.3} {:6.1} {resid:8.1} |",
                    20.0 * peak.max(1.0e-9).log10()
                );
            }
            println!("  {m:5.2} |{row}");
        }
    }
    /// CPU baseline for the audio-rate path, per the acceptance list: run
    /// with `cargo test --release -p trench-core measure_trajectory_cpu -- --ignored --nocapture`.
    #[test]
    #[ignore = "prints the audio-rate CPU baseline at 48/96/192 kHz"]
    fn measure_trajectory_cpu() {
        for rate in [48_000.0f64, 96_000.0, 192_000.0] {
            let mut e = FilterEngine::new();
            e.prepare(rate);
            e.load_cartridge(
                Cartridge::from_body_bytes_at("cpu", &resonant_body(), 1.0, 0.0).unwrap(),
            );
            let n = 512usize;
            let sig = saw(n);
            let morph: Vec<f32> = (0..n)
                .map(|i| 0.5 + 0.45 * ((i as f32) * 0.003).sin())
                .collect();
            let blocks = (rate / n as f64).ceil() as usize; // one second of audio
            let start = std::time::Instant::now();
            let (mut l, mut r) = (sig.clone(), sig.clone());
            for _ in 0..blocks {
                e.process_trajectory(&mut l, &mut r, &morph, 0.5);
            }
            let moving = start.elapsed().as_secs_f64();
            let flat = vec![0.37f32; n];
            let start = std::time::Instant::now();
            for _ in 0..blocks {
                e.process_trajectory(&mut l, &mut r, &flat, 0.5);
            }
            let parked = start.elapsed().as_secs_f64();
            println!(
                "trajectory CPU at {:>6} Hz: moving {:.2}% of one core, parked {:.2}%",
                rate as u64,
                moving * 100.0,
                parked * 100.0
            );
        }
    }
    /// The coefficient cache is a pure execution optimisation: disabling it by
    /// jittering the stamp is not possible from outside, so instead prove a
    /// re-sent identical position leaves the installed coefficients untouched
    /// while a moved position changes them.
    #[test]
    fn identical_position_reuses_coefficients_and_movement_changes_them() {
        let mut e = engine_with_body();
        let sig = saw(64);
        let (mut l, mut r) = (sig.clone(), sig.clone());
        e.process_block(&mut l, &mut r, 0.5, 0.5);
        let mut before = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
        e.cascade_l.get_coeffs(&mut before);
        let (mut l2, mut r2) = (sig.clone(), sig.clone());
        e.process_block(&mut l2, &mut r2, 0.5, 0.5);
        let mut same = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
        e.cascade_l.get_coeffs(&mut same);
        assert_eq!(before, same);
        let (mut l3, mut r3) = (sig.clone(), sig.clone());
        e.process_block(&mut l3, &mut r3, 0.9, 0.5);
        let mut moved = [[0.0f64; NUM_COEFFS]; NUM_STAGES];
        e.cascade_l.get_coeffs(&mut moved);
        assert_ne!(before, moved, "a moved wheel must rebuild the cascade");
    }
}
#[cfg(test)]
mod stage_taste {
    use super::*;
    use crate::cartridge::Cartridge;
    use crate::desk_drive::{trench_saturate, MACKITY_CURVE_DRIVE};
    #[test]
    #[ignore = "renders SLAM / QSound / soft-desk auditions"]
    fn render_stage_taste() {
        const SR: f64 = 48_000.0;
        const OUT: u32 = 44_100;
        const SECS: f64 = 6.0;
        const HB: usize = 256;
        let body = std::fs::read("../filters/bodies/CAVL_mason_jar_to_stone_pipe.body240").unwrap();
        let n = (SECS * SR) as usize;
        let render = |slam: f32, hard: bool, qsound: bool| -> (Vec<f32>, Vec<f32>) {
            let mut eng = FilterEngine::new();
            eng.prepare(SR);
            eng.load_cartridge(Cartridge::from_body_bytes("d", &body, 1.0).unwrap());
            eng.set_spatial_mode(if qsound {
                SpatialMode::QSound
            } else {
                SpatialMode::Off
            });
            if qsound {
                eng.set_space(1.0);
            }
            let mut rng = 0x2545_F491_4F6C_DD1Du64;
            let mut pb = [0f64; 7];
            let mut pr = [0f64; 7];
            let (mut ol, mut or_) = (Vec::with_capacity(n), Vec::with_capacity(n));
            let mut off = 0;
            while off < n {
                let len = HB.min(n - off);
                let mut l: Vec<f32> = (0..len)
                    .map(|_| {
                        rng = rng
                            .wrapping_mul(6364136223846793005)
                            .wrapping_add(1442695040888963407);
                        let w = ((rng >> 40) as f64 / (1u64 << 23) as f64) - 1.0;
                        pb[0] = 0.99886 * pb[0] + w * 0.0555179;
                        pb[1] = 0.99332 * pb[1] + w * 0.0750759;
                        pb[2] = 0.96900 * pb[2] + w * 0.1538520;
                        pb[3] = 0.86650 * pb[3] + w * 0.3104856;
                        pb[4] = 0.55000 * pb[4] + w * 0.5329522;
                        pb[5] = -0.7616 * pb[5] - w * 0.0168980;
                        let s =
                            (pb[0] + pb[1] + pb[2] + pb[3] + pb[4] + pb[5] + pb[6] + w * 0.5362)
                                * 0.11;
                        pb[6] = w * 0.115926;
                        (s * 0.6) as f32
                    })
                    .collect();
                let mut r: Vec<f32> = (0..len)
                    .map(|_| {
                        rng = rng
                            .wrapping_mul(6364136223846793005)
                            .wrapping_add(1442695040888963407);
                        let w = ((rng >> 40) as f64 / (1u64 << 23) as f64) - 1.0;
                        pr[0] = 0.99886 * pr[0] + w * 0.0555179;
                        pr[1] = 0.99332 * pr[1] + w * 0.0750759;
                        pr[2] = 0.96900 * pr[2] + w * 0.1538520;
                        pr[3] = 0.86650 * pr[3] + w * 0.3104856;
                        pr[4] = 0.55000 * pr[4] + w * 0.5329522;
                        pr[5] = -0.7616 * pr[5] - w * 0.0168980;
                        let s =
                            (pr[0] + pr[1] + pr[2] + pr[3] + pr[4] + pr[5] + pr[6] + w * 0.5362)
                                * 0.11;
                        pr[6] = w * 0.115926;
                        (s * 0.6) as f32
                    })
                    .collect();
                let t = (off + len / 2) as f64 / n as f64;
                let morph = 1.0 - (2.0 * t - 1.0).abs();
                eng.process_block(&mut l, &mut r, morph, 1.0);
                if slam > 1e-4 {
                    let drive = 10f32.powf(12.0 * slam / 20.0);
                    for v in l.iter_mut().chain(r.iter_mut()) {
                        let x = *v * drive;
                        *v = if hard {
                            x.clamp(-1.0, 1.0)
                        } else {
                            trench_saturate(x as f64, MACKITY_CURVE_DRIVE) as f32
                        };
                    }
                }
                ol.extend_from_slice(&l);
                or_.extend_from_slice(&r);
                off += len;
            }
            (ol, or_)
        };
        let dir = "C:/Users/hooki/df2-workstation/out/stage_taste";
        std::fs::create_dir_all(dir).unwrap();
        let write = |name: &str, l: &[f32], r: &[f32]| {
            let ratio = SR / OUT as f64;
            let on = (l.len() as f64 / ratio) as usize;
            let rs = |s: &[f32]| -> Vec<f32> {
                (0..on)
                    .map(|i| {
                        let p = i as f64 * ratio;
                        let i0 = p.floor() as usize;
                        let f = (p - i0 as f64) as f32;
                        let a = s.get(i0).copied().unwrap_or(0.0);
                        let b = s.get(i0 + 1).copied().unwrap_or(a);
                        a + (b - a) * f
                    })
                    .collect()
            };
            let (mut a, mut b) = (rs(l), rs(r));
            let pk = a
                .iter()
                .chain(b.iter())
                .fold(0.0f32, |m, &x| m.max(x.abs()))
                .max(1e-9);
            let g = 0.5012 / pk;
            for x in a.iter_mut().chain(b.iter_mut()) {
                *x *= g;
            }
            let mut w = Vec::new();
            let dl = (a.len() * 4) as u32;
            w.extend_from_slice(b"RIFF");
            w.extend_from_slice(&(36 + dl).to_le_bytes());
            w.extend_from_slice(b"WAVEfmt ");
            w.extend_from_slice(&16u32.to_le_bytes());
            w.extend_from_slice(&1u16.to_le_bytes());
            w.extend_from_slice(&2u16.to_le_bytes());
            w.extend_from_slice(&OUT.to_le_bytes());
            w.extend_from_slice(&(OUT * 4).to_le_bytes());
            w.extend_from_slice(&4u16.to_le_bytes());
            w.extend_from_slice(&16u16.to_le_bytes());
            w.extend_from_slice(b"data");
            w.extend_from_slice(&dl.to_le_bytes());
            for i in 0..a.len() {
                w.extend_from_slice(&((a[i].clamp(-1.0, 1.0) * 32767.0) as i16).to_le_bytes());
                w.extend_from_slice(&((b[i].clamp(-1.0, 1.0) * 32767.0) as i16).to_le_bytes());
            }
            let p = format!("{dir}/{name}.wav");
            std::fs::write(&p, w).unwrap();
            println!("  {p}");
        };
        println!("\nmason_jar -> stone_pipe, Q100, morph 0->1->0, 80 ms ramp. Level-matched.\n");
        let (l, r) = render(0.0, true, false);
        write("1_baseline_clean", &l, &r);
        let (l, r) = render(0.5, true, false);
        write("2_SLAM_50_hardclip_SHIPPED", &l, &r);
        let (l, r) = render(1.0, true, false);
        write("3_SLAM_100_hardclip_SHIPPED", &l, &r);
        let (l, r) = render(0.5, false, false);
        write("4_SLAM_50_soft_mackie_UNUSED", &l, &r);
        let (l, r) = render(1.0, false, false);
        write("5_SLAM_100_soft_mackie_UNUSED", &l, &r);
        let (l, r) = render(0.0, true, true);
        write("6_QSOUND", &l, &r);
    }
}
