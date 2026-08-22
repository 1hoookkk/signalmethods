use crate::cartridge::Cartridge;
use crate::minifloat::LEGACY_BODY_BYTES;
use crate::cascade::{NUM_COEFFS, NUM_STAGES};
use crate::designer::{self, DesignerSection};
use crate::dsp::BASE_AGC_TABLE;
use crate::engine::{
    install_pending, CartridgeMailbox, EngineHandle, FilterEngine, InputMode, SpatialMode,
};
use crate::minifloat::{decode, encode, pole_radius, stage_words_to_biquad, PackedCorners};
use crate::runtime_preset::RuntimePreset;
use crate::stage_law::{
    authoring_limits_at, roots_from_words_at, validate_stage_roots_at, words_from_roots_at,
    StageRoots,
};
use std::ffi::CStr;
use std::ffi::{c_char, c_void};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
const FFI_PANIC: i32 = -100;
#[inline]
fn ffi_guard<R>(default: R, f: impl FnOnce() -> R) -> R {
    match catch_unwind(AssertUnwindSafe(f)) {
        Ok(v) => v,
        Err(_) => default,
    }
}
#[inline]
unsafe fn engine_mut<'a>(handle: *mut c_void) -> Option<&'a mut FilterEngine> {
    if handle.is_null() {
        return None;
    }
    let h = handle as *mut EngineHandle;
    Some(&mut *ptr::addr_of_mut!((*h).engine))
}
#[inline]
unsafe fn engine_ref<'a>(handle: *mut c_void) -> Option<&'a FilterEngine> {
    if handle.is_null() {
        return None;
    }
    let h = handle as *mut EngineHandle;
    Some(&*ptr::addr_of!((*h).engine))
}
#[inline]
unsafe fn mailbox_ref<'a>(handle: *mut c_void) -> Option<&'a CartridgeMailbox> {
    if handle.is_null() {
        return None;
    }
    let h = handle as *mut EngineHandle;
    Some(&*ptr::addr_of!((*h).mailbox))
}
#[no_mangle]
pub extern "C" fn trench_engine_create() -> *mut c_void {
    ffi_guard(ptr::null_mut(), || {
        Box::into_raw(Box::new(EngineHandle::new())) as *mut c_void
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_destroy(engine: *mut c_void) {
    ffi_guard((), || {
        if !engine.is_null() {
            let _ = unsafe { Box::from_raw(engine as *mut EngineHandle) };
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_prepare(engine: *mut c_void, sample_rate: f64) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.prepare(sample_rate);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_load_cartridge(
    engine: *mut c_void,
    json: *const c_char,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if json.is_null() {
            return -1;
        }
        let mailbox = match unsafe { mailbox_ref(engine) } {
            Some(m) => m,
            None => return -1,
        };
        let c_str = unsafe { CStr::from_ptr(json) };
        let r_str = match c_str.to_str() {
            Ok(s) => s,
            Err(_) => return -2,
        };
        match Cartridge::from_json(r_str) {
            Ok(cart) => {
                mailbox.stage(Box::new(cart));
                0
            }
            Err(_) => -3,
        }
    })
}
/// Loads a raw `.body240` at the ROM/heritage interchange datum of 44,100 Hz
/// — the factory law: formants keep their Hz at every host rate.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_load_body_bytes(
    engine: *mut c_void,
    bytes: *const u8,
    len: usize,
) -> i32 {
    unsafe {
        trench_engine_load_body_bytes_at(engine, bytes, len, crate::compiler::DEFAULT_AUTHORING_SR)
    }
}
/// Loads a raw `.body240` whose words are Hz-anchored interchange at a
/// positive `datum_rate` (measured/Praat bodies). The words are recompiled at
/// the engine's runtime rate and the recompiled body is certified at that
/// rate before it may stage. Returns -5 when certification fails.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_load_body_bytes_at(
    engine: *mut c_void,
    bytes: *const u8,
    len: usize,
    datum_rate: f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null() || !datum_rate.is_finite() || datum_rate <= 0.0 {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let (mailbox, compile_rate) = match (unsafe { mailbox_ref(engine) }, unsafe {
            engine_ref(engine)
        }) {
            (Some(m), Some(e)) => (m, e.sample_rate()),
            _ => return -1,
        };
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let mut cart = match Cartridge::from_body_bytes_at("audition", slice, 1.0, datum_rate) {
            Ok(cart) => cart,
            Err(_) => return -3,
        };
        cart.compile_at(compile_rate);
        let (pass, _, _, _) = certify_packed(&cart.packed, CERTIFY_GRID, 1.0);
        if pass == 0 {
            return -5;
        }
        mailbox.stage(Box::new(cart));
        0
    })
}
/// Reload a `.body240` without resetting filter states or AGC.
/// Used for body glide steps — the filter keeps singing while the
/// coefficients travel. On final landing, call `trench_engine_load_body_bytes`
/// for a clean voice start.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_reload_body_bytes(
    engine: *mut c_void,
    bytes: *const u8,
    len: usize,
    datum_rate: f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null() || !datum_rate.is_finite() || datum_rate <= 0.0 {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let (mailbox, compile_rate) = match (unsafe { mailbox_ref(engine) }, unsafe {
            engine_ref(engine)
        }) {
            (Some(m), Some(e)) => (m, e.sample_rate()),
            _ => return -1,
        };
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let mut cart = match Cartridge::from_body_bytes_at("audition", slice, 1.0, datum_rate) {
            Ok(cart) => cart,
            Err(_) => return -3,
        };
        cart.compile_at(compile_rate);
        mailbox.stage_reload(Box::new(cart));
        0
    })
}
/// Load a single X3 runtime-preset bank as a verbatim cartridge.
///
/// `words` is 4 corners × N active stages × 5 packed u16 words, minifloat
/// codec. Stages are identity-padded to 6. The cartridge is loaded verbatim
/// (`datum_rate = 0`) — no recompilation, because the X3 banks are distinct
/// rate-specific designs that cannot be decomposed for all stage types.
///
/// `authored_rate` is the xStream rate this bank was compiled for (e.g.
/// 44100.0). It is recorded in the cartridge for display but does not drive
/// any remapping. The caller is responsible for selecting the bank whose
/// rate is nearest the engine's host rate.
///
/// Returns 0 on success, negative on error.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_load_runtime_preset(
    engine: *mut c_void,
    name: *const c_char,
    words: *const u16,
    word_count: usize,
    active_stages: usize,
    authored_rate: f64,
    boost: f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if words.is_null() || name.is_null() {
            return -1;
        }
        if active_stages < 1 || active_stages > 3 {
            return -4;
        }
        if word_count != 4 * active_stages * NUM_COEFFS {
            return -4;
        }
        let mailbox = match unsafe { mailbox_ref(engine) } {
            Some(m) => m,
            None => return -1,
        };
        let c_str = unsafe { CStr::from_ptr(name) };
        let name_str = match c_str.to_str() {
            Ok(s) => s,
            Err(_) => return -2,
        };
        let ws = unsafe { std::slice::from_raw_parts(words, word_count) };
        // Determine rate_index from authored_rate — used by RuntimePreset
        // to tag which bank slot this occupies.
        let rate_index = match authored_rate as u32 {
            44100 => 0,
            48000 => 1,
            96000 => 2,
            192000 => 3,
            _ => return -4, // unknown rate family
        };
        let preset = match RuntimePreset::from_raw_words(name_str, ws, rate_index, active_stages) {
            Ok(p) => p,
            Err(_) => return -3,
        };
        // Select bank for the engine's host rate. Since we only loaded one
        // bank, this picks it regardless of host_rate.
        let host_rate = match unsafe { engine_ref(engine) } {
            Some(e) => e.sample_rate(),
            None => return -1,
        };
        let (cart, _bank_rate) = preset.to_cartridge(host_rate, boost);
        mailbox.stage(Box::new(cart));
        0
    })
}
/// The load-gate certification grid, one axis of the 33x33 Morph x Q sweep.
const CERTIFY_GRID: u32 = 33;
fn certify_packed(packed: &PackedCorners, res: u32, r_max: f64) -> (i32, f64, f64, f64) {
    let res = res.max(2);
    let rmax = if r_max > 0.0 { r_max } else { 1.0 };
    let denom = (res - 1) as f64;
    let mut max_r = 0.0f64;
    for qi in 0..res {
        let q = qi as f64 / denom;
        for mi in 0..res {
            let m = mi as f64 / denom;
            let rows = packed.interpolate_biquad(m as f32, q as f32, 0.0);
            for row in rows.iter() {
                if row.iter().any(|v| !v.is_finite()) {
                    return (0, f64::INFINITY, m, q);
                }
                let r = pole_radius(row[3], row[4]);
                if r > max_r {
                    max_r = r;
                }
                if r >= rmax {
                    return (0, max_r, m, q);
                }
            }
        }
    }
    (1, max_r, -1.0, -1.0)
}
/// Compiles interchange words at `datum_rate` into playable words at
/// `compile_rate` — the same transform the engine applies on load, exposed so
/// displays and tools can show exactly what plays.
#[no_mangle]
pub unsafe extern "C" fn trench_body_compile_at(
    bytes: *const u8,
    len: usize,
    datum_rate: f64,
    compile_rate: f64,
    out_body: *mut u8,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null()
            || out_body.is_null()
            || !datum_rate.is_finite()
            || datum_rate <= 0.0
            || !compile_rate.is_finite()
            || compile_rate <= 0.0
        {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let mut packed = match PackedCorners::from_body_bytes(slice) {
            Ok(p) => p,
            Err(_) => return -3,
        };
        for corner in packed.words.iter_mut() {
            for row in corner.iter_mut() {
                *row = crate::stage_law::recompile_stage_words(*row, datum_rate, compile_rate);
            }
        }
        let out = unsafe { std::slice::from_raw_parts_mut(out_body, LEGACY_BODY_BYTES) };
        out.copy_from_slice(&packed.to_rom_bytes());
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_reclaim(engine: *mut c_void) {
    ffi_guard((), || {
        if let Some(mailbox) = unsafe { mailbox_ref(engine) } {
            mailbox.reclaim();
        }
    })
}
#[no_mangle]
pub extern "C" fn trench_packed_decode(word: u16) -> f64 {
    ffi_guard(0.0, || decode(word))
}
#[no_mangle]
pub extern "C" fn trench_packed_encode(value: f64) -> u16 {
    ffi_guard(0u16, || encode(value))
}
#[no_mangle]
pub unsafe extern "C" fn trench_agc_table(out: *mut f32, len: usize) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if out.is_null() || len < BASE_AGC_TABLE.len() {
            return -1;
        }
        let dst = unsafe { std::slice::from_raw_parts_mut(out, BASE_AGC_TABLE.len()) };
        dst.copy_from_slice(&BASE_AGC_TABLE);
        BASE_AGC_TABLE.len() as i32
    })
}
/// Fixed Praat target preparation: 40 Hz..16 kHz logarithmic grid followed by
/// reflected-edge Gaussian background subtraction at exactly one octave FWHM.
#[no_mangle]
pub unsafe extern "C" fn trench_prepare_praat_target(
    freqs: *const f64,
    dbs: *const f64,
    n: usize,
    out_freqs: *mut f64,
    out_dbs: *mut f64,
    out_n: usize,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if freqs.is_null()
            || dbs.is_null()
            || out_freqs.is_null()
            || out_dbs.is_null()
            || n < 2
            || out_n < crate::praat_endpoint::TARGET_POINTS
        {
            return -1;
        }
        let fs = unsafe { std::slice::from_raw_parts(freqs, n) };
        let ds = unsafe { std::slice::from_raw_parts(dbs, n) };
        let source: Vec<(f64, f64)> = fs.iter().zip(ds).map(|(&f, &d)| (f, d)).collect();
        let Some(target) = crate::praat_endpoint::prepare_pitch_corrected_ltas(&source) else {
            return -2;
        };
        let out_f = unsafe {
            std::slice::from_raw_parts_mut(out_freqs, crate::praat_endpoint::TARGET_POINTS)
        };
        let out_d = unsafe {
            std::slice::from_raw_parts_mut(out_dbs, crate::praat_endpoint::TARGET_POINTS)
        };
        for (index, &(frequency, db)) in target.iter().enumerate() {
            out_f[index] = frequency;
            out_d[index] = db;
        }
        0
    })
}

/// Fit one independently measured Praat endpoint. Sections 1..5 keep the
/// supplied FormantPath lane identity and exact pole conversion. Missing or
/// illegal lanes remain identity; section 6 is a material residual only.
///
/// `out_metrics4` is [target RMS dB, intended-packed RMS dB,
/// residual-used 0/1, residual improvement dB].
#[no_mangle]
pub unsafe extern "C" fn trench_fit_praat_endpoint(
    freqs: *const f64,
    dbs: *const f64,
    n: usize,
    lane_freqs: *const f64,
    lane_bandwidths: *const f64,
    lane_valid: *const u8,
    runtime_sr: f64,
    out_roots30: *mut f64,
    out_words30: *mut u16,
    out_metrics4: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if freqs.is_null()
            || dbs.is_null()
            || lane_freqs.is_null()
            || lane_bandwidths.is_null()
            || lane_valid.is_null()
            || out_roots30.is_null()
            || out_words30.is_null()
            || out_metrics4.is_null()
            || n < 32
        {
            return -1;
        }
        let fs = unsafe { std::slice::from_raw_parts(freqs, n) };
        let ds = unsafe { std::slice::from_raw_parts(dbs, n) };
        let lane_f = unsafe { std::slice::from_raw_parts(lane_freqs, 5) };
        let lane_b = unsafe { std::slice::from_raw_parts(lane_bandwidths, 5) };
        let lane_v = unsafe { std::slice::from_raw_parts(lane_valid, 5) };
        let target: Vec<(f64, f64)> = fs.iter().zip(ds).map(|(&f, &d)| (f, d)).collect();
        let lanes = std::array::from_fn(|index| {
            (lane_v[index] != 0).then_some(crate::praat_endpoint::FormantLane {
                frequency_hz: lane_f[index],
                bandwidth_hz: lane_b[index],
            })
        });
        let Some(fit) = crate::praat_endpoint::fit_endpoint(&target, &lanes, runtime_sr) else {
            return -2;
        };
        let roots = unsafe { std::slice::from_raw_parts_mut(out_roots30, 30) };
        let words = unsafe { std::slice::from_raw_parts_mut(out_words30, 30) };
        for section in 0..NUM_STAGES {
            let root = fit.roots[section];
            roots[section * 5..section * 5 + 5].copy_from_slice(&[
                root.pole_hz,
                root.pole_r,
                root.zero_hz,
                root.zero_r,
                root.scale,
            ]);
            words[section * 5..section * 5 + 5].copy_from_slice(&fit.words[section]);
        }
        let metrics = unsafe { std::slice::from_raw_parts_mut(out_metrics4, 4) };
        metrics.copy_from_slice(&[
            fit.target_rms_db,
            fit.intended_packed_rms_db,
            if fit.residual_used { 1.0 } else { 0.0 },
            fit.residual_improvement_db,
        ]);
        0
    })
}

/// Fit one source-agnostic ARMA endpoint: any measured magnitude spectrum in,
/// six jointly fitted pole/zero sections out. No formant lanes.
///
/// `out_metrics4` is [target RMS dB, intended-packed RMS dB, sections used, 0].
#[no_mangle]
pub unsafe extern "C" fn trench_fit_arma_endpoint(
    freqs: *const f64,
    dbs: *const f64,
    n: usize,
    runtime_sr: f64,
    out_roots30: *mut f64,
    out_words30: *mut u16,
    out_metrics4: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if freqs.is_null()
            || dbs.is_null()
            || out_roots30.is_null()
            || out_words30.is_null()
            || out_metrics4.is_null()
            || n < 32
        {
            return -1;
        }
        let fs = unsafe { std::slice::from_raw_parts(freqs, n) };
        let ds = unsafe { std::slice::from_raw_parts(dbs, n) };
        let target: Vec<(f64, f64)> = fs.iter().zip(ds).map(|(&f, &d)| (f, d)).collect();
        let Some(fit) = crate::arma_endpoint::fit_arma(&target, runtime_sr) else {
            return -2;
        };
        let roots = unsafe { std::slice::from_raw_parts_mut(out_roots30, 30) };
        let words = unsafe { std::slice::from_raw_parts_mut(out_words30, 30) };
        for section in 0..NUM_STAGES {
            let root = fit.roots[section];
            roots[section * 5..section * 5 + 5].copy_from_slice(&[
                root.pole_hz,
                root.pole_r,
                root.zero_hz,
                root.zero_r,
                root.scale,
            ]);
            words[section * 5..section * 5 + 5].copy_from_slice(&fit.words[section]);
        }
        let metrics = unsafe { std::slice::from_raw_parts_mut(out_metrics4, 4) };
        metrics.copy_from_slice(&[
            fit.target_rms_db,
            fit.intended_packed_rms_db,
            fit.sections_used as f64,
            0.0,
        ]);
        0
    })
}

/// `trench_fit_arma_endpoint_pinned` with the zero frequencies frozen too.
/// `pinned_zero_hz6[s] > 0` claims section `s`'s zero; 0.0 leaves it free.
/// A zero pin needs no pole pin at the same index. This is the caller stating
/// the whole geometry and leaving only radii and gains to the fitter.
#[no_mangle]
pub unsafe extern "C" fn trench_fit_arma_endpoint_pinned_pairs(
    freqs: *const f64,
    dbs: *const f64,
    n: usize,
    pinned_pole_hz: *const f64,
    n_pole: usize,
    pinned_zero_hz: *const f64,
    n_zero: usize,
    runtime_sr: f64,
    out_roots30: *mut f64,
    out_words30: *mut u16,
    out_metrics4: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if freqs.is_null()
            || dbs.is_null()
            || out_roots30.is_null()
            || out_words30.is_null()
            || out_metrics4.is_null()
            || n < 32
            || n_pole > NUM_STAGES
            || n_zero > NUM_STAGES
        {
            return -1;
        }
        let fs = unsafe { std::slice::from_raw_parts(freqs, n) };
        let ds = unsafe { std::slice::from_raw_parts(dbs, n) };
        let poles: Vec<f64> = if n_pole == 0 || pinned_pole_hz.is_null() {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(pinned_pole_hz, n_pole) }.to_vec()
        };
        let zeros: Vec<f64> = if n_zero == 0 || pinned_zero_hz.is_null() {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(pinned_zero_hz, n_zero) }.to_vec()
        };
        let target: Vec<(f64, f64)> = fs.iter().zip(ds).map(|(&f, &d)| (f, d)).collect();
        let Some(fit) = crate::arma_endpoint::fit_arma_pinned_pairs(
            &target, runtime_sr, &poles, &zeros)
        else {
            return -2;
        };
        let roots = unsafe { std::slice::from_raw_parts_mut(out_roots30, 30) };
        let words = unsafe { std::slice::from_raw_parts_mut(out_words30, 30) };
        for section in 0..NUM_STAGES {
            let root = fit.roots[section];
            roots[section * 5..section * 5 + 5].copy_from_slice(&[
                root.pole_hz,
                root.pole_r,
                root.zero_hz,
                root.zero_r,
                root.scale,
            ]);
            words[section * 5..section * 5 + 5].copy_from_slice(&fit.words[section]);
        }
        let metrics = unsafe { std::slice::from_raw_parts_mut(out_metrics4, 4) };
        metrics.copy_from_slice(&[
            fit.target_rms_db,
            fit.intended_packed_rms_db,
            fit.sections_used as f64,
            0.0,
        ]);
        0
    })
}

/// `trench_fit_arma_endpoint` with pole frequencies locked to measured
/// resonances. `pinned_hz`/`n_pinned` (up to 6) each claim one section whose
/// pole frequency is frozen there; radii, zeros, and scale stay free.
/// Warm-start ARMA refinement (the SCULPT fast path): descend from
/// `seed_roots30` (6 x [pole_hz, pole_r, zero_hz, zero_r, scale]) toward the
/// target instead of fitting from scratch. Lane slots are preserved — no
/// seeding, no reseating, no reassignment. `pinned_mask6[s] != 0` freezes
/// that lane's pole frequency. Outputs match `trench_fit_arma_endpoint`.
#[no_mangle]
pub unsafe extern "C" fn trench_fit_arma_refine(
    freqs: *const f64,
    dbs: *const f64,
    n: usize,
    seed_roots30: *const f64,
    pinned_mask6: *const u8,
    runtime_sr: f64,
    out_roots30: *mut f64,
    out_words30: *mut u16,
    out_metrics4: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if freqs.is_null()
            || dbs.is_null()
            || seed_roots30.is_null()
            || out_roots30.is_null()
            || out_words30.is_null()
            || out_metrics4.is_null()
            || n < 32
        {
            return -1;
        }
        let fs = unsafe { std::slice::from_raw_parts(freqs, n) };
        let ds = unsafe { std::slice::from_raw_parts(dbs, n) };
        let sr = unsafe { std::slice::from_raw_parts(seed_roots30, 30) };
        let target: Vec<(f64, f64)> = fs.iter().zip(ds).map(|(&f, &d)| (f, d)).collect();
        let mut seed = [crate::stage_law::StageRoots::IDENTITY; NUM_STAGES];
        for section in 0..NUM_STAGES {
            let row = &sr[section * 5..section * 5 + 5];
            seed[section] = crate::stage_law::StageRoots {
                pole_hz: row[0],
                pole_r: row[1],
                zero_hz: row[2],
                zero_r: row[3],
                scale: row[4],
            };
        }
        let mut pinned = [false; NUM_STAGES];
        if !pinned_mask6.is_null() {
            let pm = unsafe { std::slice::from_raw_parts(pinned_mask6, NUM_STAGES) };
            for section in 0..NUM_STAGES {
                pinned[section] = pm[section] != 0;
            }
        }
        let Some(fit) = crate::arma_endpoint::fit_arma_refine(&target, runtime_sr, &seed, &pinned)
        else {
            return -2;
        };
        let roots = unsafe { std::slice::from_raw_parts_mut(out_roots30, 30) };
        let words = unsafe { std::slice::from_raw_parts_mut(out_words30, 30) };
        for section in 0..NUM_STAGES {
            let root = fit.roots[section];
            roots[section * 5..section * 5 + 5].copy_from_slice(&[
                root.pole_hz,
                root.pole_r,
                root.zero_hz,
                root.zero_r,
                root.scale,
            ]);
            words[section * 5..section * 5 + 5].copy_from_slice(&fit.words[section]);
        }
        let metrics = unsafe { std::slice::from_raw_parts_mut(out_metrics4, 4) };
        metrics.copy_from_slice(&[
            fit.target_rms_db,
            fit.intended_packed_rms_db,
            fit.sections_used as f64,
            0.0,
        ]);
        0
    })
}

#[no_mangle]
pub unsafe extern "C" fn trench_fit_arma_endpoint_pinned(
    freqs: *const f64,
    dbs: *const f64,
    n: usize,
    pinned_hz: *const f64,
    n_pinned: usize,
    runtime_sr: f64,
    out_roots30: *mut f64,
    out_words30: *mut u16,
    out_metrics4: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if freqs.is_null()
            || dbs.is_null()
            || out_roots30.is_null()
            || out_words30.is_null()
            || out_metrics4.is_null()
            || n < 32
            || (pinned_hz.is_null() && n_pinned > 0)
        {
            return -1;
        }
        let fs = unsafe { std::slice::from_raw_parts(freqs, n) };
        let ds = unsafe { std::slice::from_raw_parts(dbs, n) };
        let pins = if n_pinned == 0 {
            &[][..]
        } else {
            unsafe { std::slice::from_raw_parts(pinned_hz, n_pinned) }
        };
        let target: Vec<(f64, f64)> = fs.iter().zip(ds).map(|(&f, &d)| (f, d)).collect();
        let Some(fit) = crate::arma_endpoint::fit_arma_pinned(&target, runtime_sr, pins) else {
            return -2;
        };
        let roots = unsafe { std::slice::from_raw_parts_mut(out_roots30, 30) };
        let words = unsafe { std::slice::from_raw_parts_mut(out_words30, 30) };
        for section in 0..NUM_STAGES {
            let root = fit.roots[section];
            roots[section * 5..section * 5 + 5].copy_from_slice(&[
                root.pole_hz,
                root.pole_r,
                root.zero_hz,
                root.zero_r,
                root.scale,
            ]);
            words[section * 5..section * 5 + 5].copy_from_slice(&fit.words[section]);
        }
        let metrics = unsafe { std::slice::from_raw_parts_mut(out_metrics4, 4) };
        metrics.copy_from_slice(&[
            fit.target_rms_db,
            fit.intended_packed_rms_db,
            fit.sections_used as f64,
            0.0,
        ]);
        0
    })
}

#[no_mangle]
pub unsafe extern "C" fn trench_packed_interpolate(
    bytes: *const u8,
    len: usize,
    morph: f64,
    q: f64,
    out: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null() || out.is_null() {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let packed = match PackedCorners::from_body_bytes(slice) {
            Ok(p) => p,
            Err(_) => return -3,
        };
        let kernel = packed.interpolate(morph as f32, q as f32, 0.0);
        let out_slice = unsafe { std::slice::from_raw_parts_mut(out, NUM_STAGES * NUM_COEFFS) };
        for si in 0..NUM_STAGES {
            for ci in 0..NUM_COEFFS {
                out_slice[si * NUM_COEFFS + ci] = kernel[si][ci];
            }
        }
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_packed_probe(
    bytes: *const u8,
    len: usize,
    morph: f64,
    q: f64,
    out_biquad: *mut f64,
    out_max_pole_radius: *mut f64,
    out_unstable_mask: *mut u32,
    out_nonfinite_mask: *mut u32,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null()
            || out_biquad.is_null()
            || out_max_pole_radius.is_null()
            || out_unstable_mask.is_null()
            || out_nonfinite_mask.is_null()
        {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let packed = match PackedCorners::from_body_bytes(slice) {
            Ok(p) => p,
            Err(_) => return -3,
        };
        let biquad_rows = packed.interpolate_biquad(morph as f32, q as f32, 0.0);
        let out_bq = unsafe { std::slice::from_raw_parts_mut(out_biquad, NUM_STAGES * NUM_COEFFS) };
        let mut max_r = 0.0f64;
        let mut unstable_mask = 0u32;
        let mut nonfinite_mask = 0u32;
        for si in 0..NUM_STAGES {
            let row = biquad_rows[si];
            for ci in 0..NUM_COEFFS {
                out_bq[si * NUM_COEFFS + ci] = row[ci];
            }
            if row.iter().any(|v| !v.is_finite()) {
                nonfinite_mask |= 1u32 << si;
            } else {
                let r = pole_radius(row[3], row[4]);
                if r > max_r {
                    max_r = r;
                }
                if r >= 1.0 {
                    unstable_mask |= 1u32 << si;
                }
            }
        }
        unsafe {
            *out_max_pole_radius = max_r;
            *out_unstable_mask = unstable_mask;
            *out_nonfinite_mask = nonfinite_mask;
        }
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_packed_probe_at(
    bytes: *const u8,
    len: usize,
    morph: f64,
    q: f64,
    target_rate: f64,
    out_biquad: *mut f64,
    out_max_pole_radius: *mut f64,
    out_unstable_mask: *mut u32,
    out_nonfinite_mask: *mut u32,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null()
            || !target_rate.is_finite()
            || target_rate <= 0.0
            || out_biquad.is_null()
            || out_max_pole_radius.is_null()
            || out_unstable_mask.is_null()
            || out_nonfinite_mask.is_null()
        {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let packed = match PackedCorners::from_body_bytes(slice) {
            Ok(p) => p,
            Err(_) => return -3,
        };
        let biquad_rows = packed.interpolate_biquad(morph as f32, q as f32, 0.0);
        let out_bq = unsafe { std::slice::from_raw_parts_mut(out_biquad, NUM_STAGES * NUM_COEFFS) };
        let mut max_r = 0.0f64;
        let mut unstable_mask = 0u32;
        let mut nonfinite_mask = 0u32;
        for si in 0..NUM_STAGES {
            let row = biquad_rows[si];
            for ci in 0..NUM_COEFFS {
                out_bq[si * NUM_COEFFS + ci] = row[ci];
            }
            if row.iter().any(|v| !v.is_finite()) {
                nonfinite_mask |= 1u32 << si;
            } else {
                let r = pole_radius(row[3], row[4]);
                max_r = max_r.max(r);
                if r >= 1.0 {
                    unstable_mask |= 1u32 << si;
                }
            }
        }
        unsafe {
            *out_max_pole_radius = max_r;
            *out_unstable_mask = unstable_mask;
            *out_nonfinite_mask = nonfinite_mask;
        }
        0
    })
}
#[no_mangle]
pub extern "C" fn trench_num_stages() -> u32 {
    NUM_STAGES as u32
}
#[no_mangle]
pub extern "C" fn trench_num_coeffs() -> u32 {
    NUM_COEFFS as u32
}
#[no_mangle]
pub extern "C" fn trench_legacy_body_bytes() -> u32 {
    LEGACY_BODY_BYTES as u32
}
#[no_mangle]
pub unsafe extern "C" fn trench_pack_body_from_corner_words(
    words: *const u16,
    n: usize,
    out_body: *mut u8,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if words.is_null() || out_body.is_null() {
            return -1;
        }
        const NWORDS: usize =
            crate::minifloat::LEGACY_CORNERS * crate::minifloat::LEGACY_STAGES * NUM_COEFFS;
        if n != NWORDS {
            return -4;
        }
        let src = unsafe { std::slice::from_raw_parts(words, n) };
        let mut body = [0u8; LEGACY_BODY_BYTES];
        for (slot, &w) in body.chunks_exact_mut(2).zip(src.iter()) {
            slot.copy_from_slice(&w.to_le_bytes());
        }
        let bytes = body;
        let out = unsafe { std::slice::from_raw_parts_mut(out_body, LEGACY_BODY_BYTES) };
        out.copy_from_slice(&bytes);
        0
    })
}
/// SCALE law: closed analytical unity-DC normalization, applied post-hoc.
///
/// For each corner, the packed cascade's gain at z=1 is measured through the
/// real decode, and the six SCALE words are multiplied by one 1/6-root factor
/// so the corner passes DC at unity. Shape is untouched - SCALE is pure
/// broadband level. Never a search parameter (docs/BODY_PIPELINE_INTENT.md).
#[no_mangle]
pub unsafe extern "C" fn trench_body_dc_anchor(
    bytes: *const u8,
    len: usize,
    out_body: *mut u8,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null() || out_body.is_null() {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let mut packed = match PackedCorners::from_body_bytes(slice) {
            Ok(p) => p,
            Err(_) => return -3,
        };
        for corner in packed.words.iter_mut() {
            let mut dc = 1.0f64;
            for stage in corner.iter() {
                let [b0, b1, b2, a1, a2] = stage_words_to_biquad(*stage);
                let den = 1.0 + a1 + a2;
                dc *= (b0 + b1 + b2) / den;
            }
            if !dc.is_finite() || dc == 0.0 {
                return -3;
            }
            let factor = (1.0 / dc.abs()).powf(1.0 / NUM_STAGES as f64);
            for stage in corner.iter_mut() {
                stage[NUM_COEFFS - 1] = encode(decode(stage[NUM_COEFFS - 1]) * factor);
            }
        }
        let out = unsafe { std::slice::from_raw_parts_mut(out_body, LEGACY_BODY_BYTES) };
        out.copy_from_slice(&packed.to_rom_bytes());
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_certify_body(
    bytes: *const u8,
    len: usize,
    res: u32,
    r_max: f64,
    out_pass: *mut i32,
    out_max_radius: *mut f64,
    out_fail_morph: *mut f64,
    out_fail_q: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if bytes.is_null() || out_pass.is_null() || out_max_radius.is_null() {
            return -1;
        }
        if !crate::minifloat::is_body_len(len) {
            return -4;
        }
        let slice = unsafe { std::slice::from_raw_parts(bytes, len) };
        let packed = match PackedCorners::from_body_bytes(slice) {
            Ok(p) => p,
            Err(_) => return -3,
        };
        let (pass, max_r, fail_m, fail_q) = certify_packed(&packed, res, r_max);
        unsafe {
            *out_pass = pass;
            *out_max_radius = max_r;
            if !out_fail_morph.is_null() {
                *out_fail_morph = fail_m;
            }
            if !out_fail_q.is_null() {
                *out_fail_q = fail_q;
            }
        }
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_stage_roots_from_words_at(
    words: *const u16,
    sample_rate_hz: f64,
    out_roots: *mut f64,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if words.is_null()
            || out_roots.is_null()
            || !sample_rate_hz.is_finite()
            || sample_rate_hz <= 0.0
        {
            return -1;
        }
        let w = unsafe { std::slice::from_raw_parts(words, NUM_COEFFS) };
        let Some(roots) = roots_from_words_at([w[0], w[1], w[2], w[3], w[4]], sample_rate_hz)
        else {
            return -3;
        };
        let out = unsafe { std::slice::from_raw_parts_mut(out_roots, 5) };
        out.copy_from_slice(&[
            roots.pole_hz,
            roots.pole_r,
            roots.zero_hz,
            roots.zero_r,
            roots.scale,
        ]);
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_stage_words_from_roots_at(
    roots: *const f64,
    sample_rate_hz: f64,
    out_words: *mut u16,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if roots.is_null()
            || out_words.is_null()
            || !sample_rate_hz.is_finite()
            || sample_rate_hz <= 0.0
        {
            return -1;
        }
        let r = unsafe { std::slice::from_raw_parts(roots, 5) };
        let stage = StageRoots {
            pole_hz: r[0],
            pole_r: r[1],
            zero_hz: r[2],
            zero_r: r[3],
            scale: r[4],
        };
        if validate_stage_roots_at(&stage, sample_rate_hz) != crate::stage_law::RootValidity::Ok {
            return -2;
        }
        let words = words_from_roots_at(&stage, sample_rate_hz);
        let out = unsafe { std::slice::from_raw_parts_mut(out_words, NUM_COEFFS) };
        out.copy_from_slice(&words);
        0
    })
}
/// `sections`: array of `n` DesignerSection (repr(C)), n <= 6. Writes 30 words.
#[no_mangle]
pub unsafe extern "C" fn trench_designer_compile_corner(
    sections: *const DesignerSection,
    n: usize,
    morph: f64,
    shift: i32,
    out_words: *mut u16,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if (sections.is_null() && n != 0) || out_words.is_null() || !morph.is_finite() {
            return -1;
        }
        let secs: &[DesignerSection] = if n == 0 {
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(sections, n) }
        };
        let words = match designer::compile_corner(secs, morph, shift) {
            Ok(w) => w,
            Err(_) => return -2,
        };
        let out = unsafe { std::slice::from_raw_parts_mut(out_words, designer::WORDS_PER_CORNER) };
        out.copy_from_slice(&words);
        0
    })
}
/// `q0`/`q100`: the two Q-page section sets (pass the same pointer twice for
/// heritage Q-collapsed bodies). Writes 240 bytes.
#[no_mangle]
pub unsafe extern "C" fn trench_designer_body(
    q0: *const DesignerSection,
    q100: *const DesignerSection,
    n: usize,
    shift: i32,
    out_body: *mut u8,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if ((q0.is_null() || q100.is_null()) && n != 0) || out_body.is_null() {
            return -1;
        }
        let (a, b): (&[DesignerSection], &[DesignerSection]) = if n == 0 {
            (&[], &[])
        } else {
            unsafe {
                (
                    std::slice::from_raw_parts(q0, n),
                    std::slice::from_raw_parts(q100, n),
                )
            }
        };
        let bytes = match designer::body_bytes(a, b, shift) {
            Ok(b) => b,
            Err(_) => return -2,
        };
        let out = unsafe { std::slice::from_raw_parts_mut(out_body, LEGACY_BODY_BYTES) };
        out.copy_from_slice(&bytes);
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_cartridge_json_to_body(
    json: *const c_char,
    out_body: *mut u8,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if json.is_null() || out_body.is_null() {
            return -1;
        }
        let r_str = match unsafe { CStr::from_ptr(json) }.to_str() {
            Ok(s) => s,
            Err(_) => return -2,
        };
        let cart = match Cartridge::from_json(r_str) {
            Ok(c) => c,
            Err(_) => return -3,
        };
        let bytes = cart.packed.to_rom_bytes();
        let out = unsafe { std::slice::from_raw_parts_mut(out_body, LEGACY_BODY_BYTES) };
        out.copy_from_slice(&bytes);
        0
    })
}
/// The authoring domain, derived from the encoder and this crate's constants.
/// `pole_radius_max` is the largest radius below which *every* radius is
/// representable; the encoder is not monotone above it, so a UI may hold a
/// pointer against this bound but must still call `trench_validate_stage_roots_at`
/// for the verdict on any particular value.
#[repr(C)]
pub struct TrenchAuthoringLimits {
    pub sample_rate_hz: f64,
    pub display_freq_min_hz: f64,
    pub display_freq_max_hz: f64,
    pub authoring_freq_max_hz: f64,
    pub pole_radius_max: f64,
    pub zero_radius_max: f64,
    pub scale_min: f64,
    pub scale_max: f64,
}
#[no_mangle]
pub unsafe extern "C" fn trench_authoring_limits_at(
    sample_rate_hz: f64,
    out: *mut TrenchAuthoringLimits,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if out.is_null() || !sample_rate_hz.is_finite() || sample_rate_hz <= 0.0 {
            return -1;
        }
        let lim = authoring_limits_at(sample_rate_hz);
        unsafe {
            *out = TrenchAuthoringLimits {
                sample_rate_hz: lim.sample_rate_hz,
                display_freq_min_hz: lim.display_freq_min_hz,
                display_freq_max_hz: lim.display_freq_max_hz,
                authoring_freq_max_hz: lim.authoring_freq_max_hz,
                pole_radius_max: lim.pole_radius_max,
                zero_radius_max: lim.zero_radius_max,
                scale_min: lim.scale_min,
                scale_max: lim.scale_max,
            };
        }
        0
    })
}
/// The single authority on whether authored roots may be written.
/// `roots` is `[pole_hz, pole_r, zero_hz, zero_r, scale]`. Writes the reason
/// code (see `RootValidity`) to `out_reason`. Returns 0 when valid, -2 when
/// refused, or a negative argument error.
#[no_mangle]
pub unsafe extern "C" fn trench_validate_stage_roots_at(
    roots: *const f64,
    sample_rate_hz: f64,
    out_reason: *mut i32,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if roots.is_null() || !sample_rate_hz.is_finite() || sample_rate_hz <= 0.0 {
            return -1;
        }
        let r = unsafe { std::slice::from_raw_parts(roots, 5) };
        let verdict = validate_stage_roots_at(
            &StageRoots {
                pole_hz: r[0],
                pole_r: r[1],
                zero_hz: r[2],
                zero_r: r[3],
                scale: r[4],
            },
            sample_rate_hz,
        );
        if !out_reason.is_null() {
            unsafe { *out_reason = verdict as i32 };
        }
        if verdict == crate::stage_law::RootValidity::Ok {
            0
        } else {
            -2
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_compile_body_typed(
    cards: *const f64,
    n_values: usize,
    out_body: *mut u8,
) -> i32 {
    ffi_guard(FFI_PANIC, || {
        if cards.is_null() || out_body.is_null() {
            return -1;
        }
        if n_values != crate::compiler::TYPED_PARAM_LEN {
            return -4;
        }
        let p = unsafe { std::slice::from_raw_parts(cards, n_values) };
        let body = crate::compiler::pack_typed_body(p);
        let out = unsafe { std::slice::from_raw_parts_mut(out_body, crate::compiler::BODY_LEN) };
        out.copy_from_slice(&body);
        0
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_parameters(
    engine: *mut c_void,
    morph: f32,
    q: f32,
    slam_drive: f32,
    five_d: f32,
    amount: f32,
) {
    // slam_drive was updated-but-never-applied state; culled 2026-08-10.
    // The argument stays for ABI compatibility with the authoring tools.
    let _ = (morph, q, slam_drive);
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_space(five_d);
            eng.set_amount(amount);
        }
    })
}
/// Retired (response-peak cap culled 2026-08-10). ABI-compatible no-op.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_response_peak_cap(engine: *mut c_void, cap_db: f32) {
    let _ = (engine, cap_db);
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_input_preamp(engine: *mut c_void, amount: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_input_preamp(amount);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_grit_activity(engine: *mut c_void) -> f32 {
    ffi_guard(0.0, || {
        match unsafe { engine_mut(engine) } {
            Some(eng) => eng.grit_activity(),
            None => 0.0,
        }
    })
}
/// Current AGC gain reduction in positive dB (0 = idle, ~18.4 = table floor).
#[no_mangle]
pub unsafe extern "C" fn trench_engine_agc_reduction_db(engine: *mut c_void) -> f32 {
    ffi_guard(0.0, || {
        match unsafe { engine_mut(engine) } {
            Some(eng) => eng.agc_reduction_db(),
            None => 0.0,
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_input_mode(engine: *mut c_void, mode: i32) {
    ffi_guard((), || {
        let eng = match unsafe { engine_mut(engine) } {
            Some(e) => e,
            None => return,
        };
        let m = match mode {
            0 => InputMode::None,
            1 => InputMode::MackieDeskSlam,
            2 => InputMode::Cvsd,
            _ => return,
        };
        eng.set_input_mode(m);
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_qsound_fallback_pan(engine: *mut c_void, pan: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_qsound_fallback_pan(pan);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_process_block(
    engine: *mut c_void,
    left: *mut f32,
    right: *mut f32,
    num_samples: i32,
    morph: f64,
    q: f64,
) {
    ffi_guard((), || {
        if engine.is_null() || left.is_null() || right.is_null() || num_samples <= 0 {
            return;
        }
        let h = engine as *mut EngineHandle;
        let eng = unsafe { &mut *ptr::addr_of_mut!((*h).engine) };
        let mailbox = unsafe { &*ptr::addr_of!((*h).mailbox) };
        install_pending(eng, mailbox);
        let n = num_samples as usize;
        let left_slice = unsafe { std::slice::from_raw_parts_mut(left, n) };
        let right_slice = unsafe { std::slice::from_raw_parts_mut(right, n) };
        eng.process_block(left_slice, right_slice, morph, q);
    })
}
/// Audio-rate Movement: one authored Morph position per sample, Q static.
/// `morph_per_sample` must hold `num_samples` values in [0,1]. The engine
/// interpolates the packed words per sample, decodes, and installs the whole
/// coefficient set on that exact sample — no smoothing anywhere downstream.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_process_trajectory(
    engine: *mut c_void,
    left: *mut f32,
    right: *mut f32,
    num_samples: i32,
    morph_per_sample: *const f32,
    q: f64,
) {
    ffi_guard((), || {
        if engine.is_null()
            || left.is_null()
            || right.is_null()
            || morph_per_sample.is_null()
            || num_samples <= 0
        {
            return;
        }
        let h = engine as *mut EngineHandle;
        let eng = unsafe { &mut *ptr::addr_of_mut!((*h).engine) };
        let mailbox = unsafe { &*ptr::addr_of!((*h).mailbox) };
        install_pending(eng, mailbox);
        let n = num_samples as usize;
        let left_slice = unsafe { std::slice::from_raw_parts_mut(left, n) };
        let right_slice = unsafe { std::slice::from_raw_parts_mut(right, n) };
        let morph_slice = unsafe { std::slice::from_raw_parts(morph_per_sample, n) };
        eng.process_trajectory(left_slice, right_slice, morph_slice, q);
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_spatial_mode(engine: *mut c_void, mode: i32) {
    ffi_guard((), || {
        let eng = match unsafe { engine_mut(engine) } {
            Some(e) => e,
            None => return,
        };
        let m = match mode {
            0 => SpatialMode::QSound,
            1 => SpatialMode::Trench,
            2 => SpatialMode::Off,
            _ => return,
        };
        eng.set_spatial_mode(m);
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_agc_enabled(engine: *mut c_void, enabled: i32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.debug.agc_enabled = enabled != 0;
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_dc_block_enabled(engine: *mut c_void, enabled: i32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.debug.dc_block_enabled = enabled != 0;
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_saturation_enabled(engine: *mut c_void, enabled: i32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.debug.saturation_enabled = enabled != 0;
        }
    })
}
/// DEV BYPASS: 0 runs the sections as plain linear biquads (no state
/// saturation, no pole-radius modulation). Those two are on at every GRIT
/// setting including zero, so this is the only way to hear the filter alone.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_nonlinearity_enabled(
    engine: *mut c_void,
    enabled: i32,
) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.debug.nonlinearity_enabled = enabled != 0;
        }
    })
}
/// The cube's third axis (Transform 2 — corner bit z of `m | q<<1 | z<<2`).
/// Meaningful only on a native 560-byte cube body; every 240-byte body
/// duplicates its far plane, so z moves nothing there by construction. The
/// face control for this appears ONLY when a cube is loaded (Tyson
/// 2026-08-15) — no cube load path ships yet, so no knob exists yet.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_third_axis(engine: *mut c_void, z: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_third_axis(z as f64);
        }
    })
}
/// X3 movement parity (X3_MOVEMENT_SPEC.md): 1 = block-rate rebuild + kernel
/// ramp + morph one-pole, 0 = the shipping per-sample path. Dev A/B switch.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_x3_movement(engine: *mut c_void, enabled: i32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_x3_movement(enabled != 0);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_agc_drive(engine: *mut c_void, drive: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_agc_drive(drive);
        }
    })
}
/// Retired (coefficient ramps culled 2026-08-10: the complete delta is
/// consumed on its authored sample). ABI-compatible no-op.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_coeff_ramp_scale(engine: *mut c_void, scale: f32) {
    let _ = (engine, scale);
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_grit(engine: *mut c_void, amount: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_grit(amount);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_pitch_ratio(engine: *mut c_void, ratio: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_pitch_ratio(if ratio.is_finite() { ratio } else { 1.0 });
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_listener(engine: *mut c_void, amount: f32, speed_ms: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_listener(amount, speed_ms);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_env(engine: *mut c_void, amount: f32, release_ms: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_env(amount, release_ms);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_bloom(engine: *mut c_void, amount: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_bloom(amount);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_key_snap(engine: *mut c_void, choice: i32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_key_snap(choice);
        }
    })
}
/// GROWL: pitch-locked sub-octave wheel oscillation. 0 = exactly off.
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_growl(engine: *mut c_void, amount: f32) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.set_growl(amount);
        }
    })
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_set_trench_matrix(
    engine: *mut c_void,
    target_delay_samples: i32,
    allpass_delay_samples: i32,
    allpass_g: f32,
    mu: f32,
) {
    ffi_guard((), || {
        if let Some(eng) = unsafe { engine_mut(engine) } {
            eng.trench_matrix.target_delay_samples = target_delay_samples.max(0) as usize;
            eng.trench_matrix.allpass_delay_samples = allpass_delay_samples.max(1) as usize;
            eng.trench_matrix.allpass_g = allpass_g;
            eng.trench_matrix.mu = mu;
        }
    })
}
/// How many f32 `trench_engine_get_coeffs` writes, and how many f64
/// `trench_packed_probe_at` writes: NUM_STAGES * NUM_COEFFS. Exported so the
/// host side can assert its buffers instead of hardcoding a number that goes
/// stale the next time a section is added - which is exactly what happened when
/// the cascade went from six sections to seven and every C++ buffer stayed at
/// 30, overrunning the stack on the editor's own curve probe.
#[no_mangle]
pub extern "C" fn trench_engine_coeff_count() -> i32 {
    (NUM_STAGES * NUM_COEFFS) as i32
}
#[no_mangle]
pub unsafe extern "C" fn trench_engine_get_coeffs(
    engine: *mut c_void,
    out_coeffs: *mut f32,
    out_boost: *mut f32,
) {
    ffi_guard((), || {
        if out_boost.is_null() {
            return;
        }
        let eng = match unsafe { engine_ref(engine) } {
            Some(e) => e,
            None => {
                unsafe { *out_boost = 1.0 };
                return;
            }
        };
        if out_coeffs.is_null() {
            unsafe { *out_boost = 1.0 };
            return;
        }
        let mut r_coeffs = [[0.0f32; NUM_COEFFS]; NUM_STAGES];
        let mut r_boost = 1.0f32;
        eng.get_coeffs_for_ui(&mut r_coeffs, &mut r_boost);
        let out_ptr = out_coeffs;
        for i in 0..NUM_STAGES {
            for j in 0..NUM_COEFFS {
                unsafe { *out_ptr.add(i * NUM_COEFFS + j) = r_coeffs[i][j] };
            }
        }
        unsafe { *out_boost = r_boost };
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    use crate::stage_law::words_from_roots;
    fn passthrough_json() -> String {
        let corners = [[[2.0f64, 1.0, 2.0, 1.0, 1.0]; crate::minifloat::LEGACY_STAGES]; 4];
        let packed = PackedCorners::from_legacy_corner_data(&corners);
        let labels = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"];
        let keyframes = labels
            .iter()
            .enumerate()
            .map(|(corner_index, label)| {
                serde_json::json!({
                    "label": label,
                    "boost": 1.0,
                    "packedWords": &packed.words[corner_index][..crate::minifloat::LEGACY_STAGES]
                })
            })
            .collect::<Vec<_>>();
        serde_json::json!({
            "format": "compiled-v1",
            "name": "passthrough",
            "sampleRate": 44_100.0,
            "keyframes": keyframes
        })
        .to_string()
    }
    #[test]
    fn exported_limits_match_the_geometry_authority() {
        let rate = 48_000.0;
        let mut lim = std::mem::MaybeUninit::<TrenchAuthoringLimits>::uninit();
        assert_eq!(unsafe { trench_authoring_limits_at(rate, lim.as_mut_ptr()) }, 0);
        let lim = unsafe { lim.assume_init() };
        let want = authoring_limits_at(rate);
        assert_eq!(lim.sample_rate_hz, want.sample_rate_hz);
        assert_eq!(lim.display_freq_min_hz, want.display_freq_min_hz);
        assert_eq!(lim.display_freq_max_hz, want.display_freq_max_hz);
        assert_eq!(lim.authoring_freq_max_hz, want.authoring_freq_max_hz);
        assert_eq!(lim.pole_radius_max, want.pole_radius_max);
        assert_eq!(lim.zero_radius_max, want.zero_radius_max);
        assert_eq!(lim.scale_min, want.scale_min);
        assert_eq!(lim.scale_max, want.scale_max);
        assert_eq!(
            unsafe { trench_authoring_limits_at(rate, std::ptr::null_mut()) },
            -1
        );
    }
    #[test]
    fn validation_reports_each_refusal_reason_over_the_boundary() {
        let rate = 44_100.0;
        let mut reason = -1i32;
        let ok = [1000.0f64, 0.95, 3000.0, 0.5, 1.0];
        assert_eq!(
            unsafe { trench_validate_stage_roots_at(ok.as_ptr(), rate, &mut reason) },
            0
        );
        assert_eq!(reason, 0);
        // [pole_hz, pole_r, zero_hz, zero_r, scale] -> expected reason code
        let cases: [([f64; 5], i32); 5] = [
            ([f64::NAN, 0.95, 3000.0, 0.5, 1.0], 1),
            ([1.0, 0.95, 3000.0, 0.5, 1.0], 2),
            ([1.0e9, 0.95, 3000.0, 0.5, 1.0], 3),
            ([1000.0, 1.5, 3000.0, 0.5, 1.0], 5),
            ([1000.0, 0.95, 3000.0, 0.5, 99.0], 7),
        ];
        for (roots, want) in cases {
            reason = -1;
            assert_eq!(
                unsafe { trench_validate_stage_roots_at(roots.as_ptr(), rate, &mut reason) },
                -2,
                "expected refusal for {roots:?}"
            );
            assert_eq!(reason, want, "wrong reason for {roots:?}");
        }
        assert_eq!(
            unsafe { trench_validate_stage_roots_at(std::ptr::null(), rate, &mut reason) },
            -1
        );
    }
    #[test]
    fn stage_roots_words_ffi_round_trip_is_a_fixed_point() {
        let rate = 44_100.0;
        let roots = [1000.0f64, 0.98, 2000.0, 0.9, 0.5];
        let mut words = [0u16; 5];
        let mut decoded = [0.0f64; 5];
        let mut words2 = [0u16; 5];
        unsafe {
            assert_eq!(
                trench_stage_words_from_roots_at(roots.as_ptr(), rate, words.as_mut_ptr()),
                0
            );
            assert_eq!(
                trench_stage_roots_from_words_at(words.as_ptr(), rate, decoded.as_mut_ptr()),
                0
            );
            assert_eq!(
                trench_stage_words_from_roots_at(decoded.as_ptr(), rate, words2.as_mut_ptr()),
                0
            );
        }
        assert_eq!(words, words2);
        let identity_words = words_from_roots(&StageRoots::IDENTITY);
        let mut identity_decoded = [0.0f64; 5];
        unsafe {
            assert_eq!(
                trench_stage_roots_from_words_at(
                    identity_words.as_ptr(),
                    rate,
                    identity_decoded.as_mut_ptr()
                ),
                0
            );
        }
        assert_eq!(identity_decoded[4], 1.0);
        unsafe {
            assert_eq!(
                trench_stage_words_from_roots_at(ptr::null(), rate, words.as_mut_ptr()),
                -1
            );
            assert_eq!(
                trench_stage_roots_from_words_at(ptr::null(), rate, decoded.as_mut_ptr()),
                -1
            );
        }
    }
    #[test]
    fn null_pointers_are_rejected_not_dereferenced() {
        unsafe {
            assert_eq!(
                trench_engine_load_cartridge(ptr::null_mut(), ptr::null()),
                -1
            );
            assert_eq!(
                trench_engine_load_body_bytes(ptr::null_mut(), ptr::null(), 240),
                -1
            );
            trench_engine_prepare(ptr::null_mut(), 44100.0);
            trench_engine_process_block(
                ptr::null_mut(),
                ptr::null_mut(),
                ptr::null_mut(),
                64,
                0.5,
                0.5,
            );
            trench_engine_reclaim(ptr::null_mut());
            let mut coeffs = [0.0f32; 30];
            let mut boost = 0.0f32;
            trench_engine_get_coeffs(ptr::null_mut(), coeffs.as_mut_ptr(), &mut boost);
            assert_eq!(boost, 1.0, "null engine must report unity boost");
        }
        let mut morph = 0.0f32;
        let mut q = 0.0f32;
        assert_eq!(
            trench_motion_path_value_timed(
                std::ptr::null(),
                2,
                0.5,
                0,
                0,
                0.2,
                0.3,
                1.0,
                &mut morph,
                &mut q,
            ),
            -1
        );
    }
    #[test]
    fn timed_motion_ffi_matches_the_core_sampler() {
        let points = [
            0.0f32, 0.0, 0.0, 0.2, 0.8, 0.4, 0.7, 0.1, 0.9, 1.0, 0.0, 0.0,
        ];
        let expected = crate::motion::path_value_timed(&points, 0.2, false, 0.1, 0.1, 1.0, 4);
        let mut morph = 0.0f32;
        let mut q = 0.0f32;
        let rc = trench_motion_path_value_timed(
            points.as_ptr(),
            4,
            0.2,
            0,
            4,
            0.1,
            0.1,
            1.0,
            &mut morph,
            &mut q,
        );
        assert_eq!(rc, 0);
        assert!((morph - expected.0).abs() < 1.0e-6);
        assert!((q - expected.1).abs() < 1.0e-6);
    }
    #[test]
    fn wrong_length_body_is_rejected() {
        unsafe {
            let engine = trench_engine_create();
            assert!(!engine.is_null());
            let bytes = [0u8; 16];
            assert_eq!(
                trench_engine_load_body_bytes(engine, bytes.as_ptr(), bytes.len()),
                -4
            );
            trench_engine_destroy(engine);
        }
    }
    #[test]
    fn staged_load_installs_on_next_process_block() {
        unsafe {
            let engine = trench_engine_create();
            assert!(!engine.is_null());
            trench_engine_prepare(engine, 44100.0);
            let json = std::ffi::CString::new(passthrough_json()).unwrap();
            assert_eq!(trench_engine_load_cartridge(engine, json.as_ptr()), 0);
            let mut l = vec![0.25f32; 256];
            let mut r = vec![0.25f32; 256];
            trench_engine_process_block(
                engine,
                l.as_mut_ptr(),
                r.as_mut_ptr(),
                l.len() as i32,
                0.5,
                0.5,
            );
            assert!(l.iter().all(|s| s.is_finite()));
            let mut coeffs = [0.0f32; 30];
            let mut boost = 0.0f32;
            trench_engine_get_coeffs(engine, coeffs.as_mut_ptr(), &mut boost);
            assert!(boost.is_finite());
            trench_engine_reclaim(engine);
            trench_engine_reclaim(engine);
            trench_engine_destroy(engine);
        }
    }
    #[test]
    fn rapid_restage_then_install_is_clean() {
        unsafe {
            let engine = trench_engine_create();
            trench_engine_prepare(engine, 44100.0);
            let json = std::ffi::CString::new(passthrough_json()).unwrap();
            for _ in 0..8 {
                assert_eq!(trench_engine_load_cartridge(engine, json.as_ptr()), 0);
            }
            let mut l = vec![0.1f32; 128];
            let mut r = vec![0.1f32; 128];
            trench_engine_process_block(engine, l.as_mut_ptr(), r.as_mut_ptr(), 128, 0.5, 0.5);
            assert!(l.iter().all(|s| s.is_finite()));
            trench_engine_destroy(engine);
        }
    }
    #[test]
    fn pack_body_from_corner_words_roundtrips_bytes() {
        let mut body = [0u8; LEGACY_BODY_BYTES];
        for (i, w) in body.chunks_exact_mut(2).enumerate() {
            let word = (i as u16).wrapping_mul(7).wrapping_add(3);
            w.copy_from_slice(&word.to_le_bytes());
        }
        let pc = PackedCorners::from_body_bytes(&body).unwrap();
        let mut words =
            [0u16; crate::minifloat::LEGACY_CORNERS * crate::minifloat::LEGACY_STAGES * NUM_COEFFS];
        let mut i = 0;
        for ci in 0..crate::minifloat::LEGACY_CORNERS {
            for si in 0..crate::minifloat::LEGACY_STAGES {
                for wi in 0..NUM_COEFFS {
                    words[i] = pc.words[ci][si][wi];
                    i += 1;
                }
            }
        }
        let mut out = [0u8; LEGACY_BODY_BYTES];
        let rc = unsafe {
            trench_pack_body_from_corner_words(words.as_ptr(), words.len(), out.as_mut_ptr())
        };
        assert_eq!(rc, 0);
        assert_eq!(
            &out[..],
            &body[..],
            "packed bytes must equal the original body"
        );
    }
    #[test]
    fn pack_body_rejects_wrong_word_count() {
        let words = [0u16; 10];
        let mut out = [0u8; LEGACY_BODY_BYTES];
        let rc = unsafe {
            trench_pack_body_from_corner_words(words.as_ptr(), words.len(), out.as_mut_ptr())
        };
        assert_eq!(rc, -4);
    }
    #[test]
    fn certify_passes_unity_body() {
        let corners = [[[2.0f64, 1.0, 2.0, 1.0, 1.0]; crate::minifloat::LEGACY_STAGES]; 4];
        let body = PackedCorners::from_legacy_corner_data(&corners).to_rom_bytes();
        let (mut pass, mut maxr, mut fm, mut fq) = (0i32, 0.0f64, 0.0f64, 0.0f64);
        let rc = unsafe {
            trench_certify_body(
                body.as_ptr(),
                body.len(),
                33,
                0.9999,
                &mut pass,
                &mut maxr,
                &mut fm,
                &mut fq,
            )
        };
        assert_eq!(rc, 0);
        assert_eq!(pass, 1, "unity body must certify pass");
        assert!(maxr < 0.5, "unity poles sit near radius 0, got {maxr}");
    }
    #[test]
    fn certify_fails_rim_body() {
        let corners = [[[2.0f64, 1.0, 2.0, 0.0, 1.0]; crate::minifloat::LEGACY_STAGES]; 4];
        let body = PackedCorners::from_legacy_corner_data(&corners).to_rom_bytes();
        let (mut pass, mut maxr, mut fm, mut fq) = (1i32, 0.0f64, -1.0f64, -1.0f64);
        let rc = unsafe {
            trench_certify_body(
                body.as_ptr(),
                body.len(),
                9,
                0.9999,
                &mut pass,
                &mut maxr,
                &mut fm,
                &mut fq,
            )
        };
        assert_eq!(rc, 0);
        assert_eq!(pass, 0, "radius-1 body must certify fail");
        assert!(fm >= 0.0 && fq >= 0.0, "fail coordinate must be reported");
    }
    #[test]
    fn cartridge_json_to_body_yields_loadable_bytes() {
        let json = std::ffi::CString::new(passthrough_json()).unwrap();
        let mut out = [0u8; LEGACY_BODY_BYTES];
        let rc = unsafe { trench_cartridge_json_to_body(json.as_ptr(), out.as_mut_ptr()) };
        assert_eq!(rc, 0);
        assert!(
            PackedCorners::from_body_bytes(&out).is_ok(),
            "must be a valid 240-byte body"
        );
    }
}
#[no_mangle]
pub unsafe extern "C" fn trench_desk_saturate_stereo(
    left: *mut f32,
    right: *mut f32,
    num_samples: i32,
    drive: f32,
) {
    if left.is_null() || num_samples <= 0 {
        return;
    }
    let n = num_samples as usize;
    let l = std::slice::from_raw_parts_mut(left, n);
    for x in l.iter_mut() {
        *x = crate::desk_drive::trench_saturate(
            (*x * drive) as f64,
            crate::desk_drive::MACKITY_CURVE_DRIVE,
        ) as f32;
    }
    if right.is_null() || std::ptr::eq(right, left) {
        return;
    }
    let r = std::slice::from_raw_parts_mut(right, n);
    for x in r.iter_mut() {
        *x = crate::desk_drive::trench_saturate(
            (*x * drive) as f64,
            crate::desk_drive::MACKITY_CURVE_DRIVE,
        ) as f32;
    }
}
#[no_mangle]
pub extern "C" fn trench_keyframe_value(
    a: f32,
    b: f32,
    leg_bars: f32,
    ppq: f64,
    beats_per_bar: f64,
    mode: u32,
) -> f32 {
    crate::keyframe::keyframe_loop_value_mode(
        a,
        b,
        leg_bars,
        ppq,
        beats_per_bar,
        crate::keyframe::LoopMode::from_u32(mode),
    )
}
#[no_mangle]
pub extern "C" fn trench_motion_path_value(
    points: *const f32,
    point_count: usize,
    phase: f64,
    closed: i32,
    base_morph: f32,
    base_q: f32,
    amount: f32,
    out_morph: *mut f32,
    out_q: *mut f32,
) -> i32 {
    if out_morph.is_null() || out_q.is_null() || point_count == 0 {
        return -1;
    }
    if points.is_null() {
        return -1;
    }
    let count = point_count.min(crate::motion::MAX_PATH_POINTS);
    let values = unsafe { std::slice::from_raw_parts(points, count * 2) };
    let (morph, q) =
        crate::motion::path_value(values, phase, closed != 0, base_morph, base_q, amount);
    unsafe {
        *out_morph = morph;
        *out_q = q;
    }
    0
}
#[no_mangle]
pub extern "C" fn trench_motion_path_value_timed(
    points: *const f32,
    point_count: usize,
    phase: f64,
    closed: i32,
    grid_steps: usize,
    base_morph: f32,
    base_q: f32,
    amount: f32,
    out_morph: *mut f32,
    out_q: *mut f32,
) -> i32 {
    if out_morph.is_null() || out_q.is_null() || point_count == 0 {
        return -1;
    }
    if points.is_null() {
        return -1;
    }
    let count = point_count.min(crate::motion::MAX_PATH_POINTS);
    let values =
        unsafe { std::slice::from_raw_parts(points, count * crate::motion::TIMED_POINT_STRIDE) };
    let (morph, q) = crate::motion::path_value_timed(
        values,
        phase,
        closed != 0,
        base_morph,
        base_q,
        amount,
        grid_steps,
    );
    unsafe {
        *out_morph = morph;
        *out_q = q;
    }
    0
}
