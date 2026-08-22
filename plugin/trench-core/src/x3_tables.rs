//! Tables extracted from EmulatorX.dll — the X3 runtime voice processor.
//!
//! Source: `EmulatorX.dll` via Ghidra decompilation, 2026-08-06.
//! These are the profile tables used by the three-row writer
//! (`FUN_1802c59b0`) and its callers.
//!
//! ## Writer dispatch
//!
//! | Writer | Function | Description |
//! |---|---|---|
//! | 1 | `FUN_1802c5d60` | Fixed fallback words + spread |
//! | 2 | `FUN_1802c5e40` | 16 × 6-word profiles (independent Q0/Q100) |
//! | 3 | `FUN_1802c5f10` | 16 × 3-word profiles (Q100 mirrors Q0) |
//! | 4 | `FUN_1802c6020` | Custom 2-pair spread writer |
//!
//! ## Profile-to-filter mapping (observed)
//!
//! | Filter | Writer | Active stages | Profile table |
//! |---|---|---|---|
//! | 2-Pole Lowpass  | 2 | 1 | `PROFILES_6` |
//! | 4-Pole Lowpass  | 2 | 2 | `PROFILES_6` |
//! | 6-Pole Lowpass  | 2 | 3 | `PROFILES_6` |
//! | 2-Pole Highpass | 3 | 1 | `PROFILES_3` |
//! | 4-Pole Highpass | 3 | 2 | `PROFILES_3` |
//! | 4-Pole Bandpass | 4 | 3 | custom |
//! | Contrary Bandpass| 4 | 3 | custom |
//! | Swept EQ 1-Oct  | 4 | 3 | custom |
//! | Phaser 2        | 3 | 2 | `PROFILES_3` |

/// 8-entry sample-rate frequency table (int32 LE).
/// Indexed by `*(u16*)(param_1 + 0x0c)` — the rate index.
/// Address: `DAT_1806d73a0`
pub const RATE_FREQ_TABLE_A: [i32; 8] = [
    4896, // 0
    4500, // 1
    900,  // 2
    220,  // 3
    442,  // 4
    440,  // 5
    405,  // 6
    350,  // 7
];

/// 8-entry sample-rate multiplier table (int32 LE).
/// Indexed by rate index. First 4 entries are int32, remaining 4 are raw u16s.
/// Address: `DAT_1806d73b0`
pub const RATE_FREQ_TABLE_B: [i32; 8] = [
    442,  // 0
    440,  // 1
    405,  // 2
    350,  // 3
    0x11ed, // 4 — actually u16 profile data starts here
    0x94fc, // 5
    0x11ed, // 6
    0xdffc, // 7
];

/// 16 × 6-word profiles for `FUN_1802c5e40`.
/// Each profile is [word0, word1, word2, word3, word4, word5].
/// Address: `DAT_1806d73c0`
pub const PROFILES_6: [[u16; 6]; 16] = [
    [0x11ed, 0x94fc, 0x11ed, 0xdffc, 0xfffd, 0xdffc], // 0
    [0x11ee, 0x90fc, 0x11ed, 0xe2fc, 0xfdfd, 0xe2fc], // 1
    [0x11ed, 0x8dfc, 0x11ed, 0xe5fc, 0xfc7d, 0xe5fc], // 2
    [0x11ee, 0x8afc, 0x11ed, 0xe8fc, 0xfafd, 0xe8fc], // 3
    [0x11ed, 0x87fc, 0x11ed, 0xebfc, 0xf97d, 0xebfc], // 4
    [0x11ed, 0x83fc, 0x11ed, 0xedfc, 0xf87d, 0xedfc], // 5
    [0x11ee, 0x80fc, 0x11ed, 0xf07d, 0xf77d, 0xf07d], // 6
    [0x0fee, 0x7dfc, 0x0fef, 0xf17d, 0xf67d, 0xf17d], // 7
    [0x0fef, 0x7afc, 0x0fef, 0xf27d, 0xf47d, 0xf27d], // 8
    [0x0fee, 0x77fc, 0x0fef, 0xf3fd, 0xf27d, 0xf3fd], // 9
    [0x0fee, 0x73fc, 0x0fef, 0xf47d, 0xf17d, 0xf47d], // 10
    [0x0dee, 0x70fc, 0x0def, 0xf4fd, 0xf07d, 0xf4fd], // 11
    [0x0ded, 0x6dfc, 0x0def, 0xf5fd, 0xeffc, 0xf5fd], // 12
    [0x0bef, 0x6afc, 0x0bef, 0xf67d, 0xeefc, 0xf67d], // 13
    [0x0bef, 0x62fc, 0x0bef, 0xf6fd, 0xecfc, 0xf6fd], // 14
    [0x0bef, 0x62fc, 0x0bef, 0xf6fd, 0xecfc, 0xf6fd], // 15 (same as 14)
];

/// 16 × 3-word profiles for `FUN_1802c5f10`.
/// Each profile is [word0, word1, word2]. Q100 = Q0 (mirrored by writer).
/// Address: `DAT_1806d7480`
pub const PROFILES_3: [[u16; 3]; 16] = [
    [0x11ed, 0x94fc, 0x11ed], // 0
    [0x24f5, 0x9bfc, 0x24f5], // 1
    [0x30f9, 0xa3fc, 0x30f9], // 2
    [0x3ef9, 0xacfc, 0x3ef9], // 3
    [0x4bfb, 0xb3fc, 0x4bfb], // 4
    [0x59fc, 0xbcfc, 0x59fc], // 5
    [0x65fc, 0xc4fc, 0x65fc], // 6
    [0x73fc, 0xccfc, 0x73fc], // 7
    [0x80fc, 0xd4fc, 0x80fc], // 8
    [0x8efc, 0xddfc, 0x8efc], // 9
    [0x9afc, 0xe4fc, 0x9afc], // 10
    [0xa8fc, 0xedfc, 0xa8fc], // 11
    [0xb5fc, 0xf2fd, 0xb5fc], // 12
    [0xc3fc, 0xf6fd, 0xc3fc], // 13
    [0xcffc, 0xfafd, 0xcffc], // 14
    [0xddfc, 0xff7d, 0xddfc], // 15
];

/// Spread table for `FUN_1802c6020` (custom writer).
/// Address: `DAT_1806d74e0`
pub const SPREAD_TABLE_A: [i32; 8] = [18, 18, 4, 1, 220, 220, 200, 177];

/// Second spread/multiplier table for `FUN_1802c6020`.
/// Address: `DAT_1806d74f0`
pub const SPREAD_TABLE_B: [i32; 8] = [220, 220, 200, 177, 0xdfff, 0xffff, 0xdfff, 0xffff];

/// The universal identity row.
pub const IDENTITY_ROW: [u16; 5] = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff];

/// Pad sentinel from Morph Designer padding (`FUN_1802c6590`).
/// Address: `DAT_1806d7500`
pub const PAD_ROW: [u16; 5] = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000];
