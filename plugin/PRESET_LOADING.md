# Canonical authored-body path

Morph6 (`C:\Users\hooki\morph6-experiment`) authors the four corners. TRENCH (`C:\Users\hooki\trench-native`) is the VST3 player. The repositories exchange files through the Windows Documents known folder, never through each other's source or factory rosters.

**Library: Documents\TRENCH\User Bodies**

On this machine Windows redirects Documents, so the actual library is:

`C:\Users\hooki\OneDrive\Documents\TRENCH\User Bodies`

Only top-level `.body240` files of exactly 240 bytes are listed. The datum is 44,100 Hz. Names come from filenames. Hidden files and names beginning with `_` are ignored. Sidecars are documentation, not DSP input. The old `Documents\TRENCH\bodies` shelves and `authoring_slot.json` are not scanned by this route.

## Author, publish, play

1. Start Morph6 with `C:\Users\hooki\morph6-experiment\Launch Morph6.cmd` after saving and closing an older Morph6 instance.
2. Export the body. The existing export/level policy runs once. Morph6 keeps its `.body240`, `.m6strip` and provenance in `output\exports`; it publishes the identical `.body240` and provenance to User Bodies. Publication replaces the destination atomically after the complete file is written. Authored intermediate strip rows are not representable in the four-corner plugin body.
3. Open TRENCH's BODY menu. It rescans this library. User bodies follow factory bodies. New files append without renumbering existing entries during the running process.
4. Select the filename-derived name. Editing/re-exporting the selected file reloads it on the plugin's 250 ms control timer. This is an authoring audition connection: overwriting a selected file changes the playing body.
5. DAW state saves the body's absolute file identity. Keep the file at that path for recall; the state does not embed its bytes. For a finished project, use a distinct filename and retain that file unchanged. Missing files are not portable presets.

The current `body.body240` came from Morph6's 20 September export: `fujimura-schwa.hedzrel-M1` to `s1-15-bass-lax-a`. It appears as **Body**. Its original bytes and gain were preserved. Its provenance reports a +51.8 dB interior response peak, so library installation is not a claim that it passed the separate production motion gate.

## Install the player update once

Build in trench-native with `cmake --build out/build/vst3 --target TRENCH_VST3 TRENCH_UserBodyTests` from an MSVC environment.

Close the DAW, then run `C:\Users\hooki\trench-native\Install TRENCH.cmd`. The installer checks for locked files, preserves the previous files under `out\install-backups`, copies the built plugin to `C:\Program Files\Common Files\VST3\TRENCH.vst3`, and verifies hashes. It never closes processes or removes old files. Reopen the DAW normally. No separate Dev plugin or second VST3 identity is required.

Subsequent body exports do not require rebuilding or reinstalling TRENCH.

## Headless verification

`out\build\vst3\plugin\TRENCH_UserBodyTests_artefacts\Release\TRENCH_UserBodyTests.exe "C:\Users\hooki\OneDrive\Documents\TRENCH\User Bodies\body.body240"`

Tests cover discovery, append-only indices, byte-exact selection and DAW-state recall with the file present, explicit 44.1 kHz decoding, and finite impulse processing. Host UI and listening acceptance remain separate.

Verified 20 September 2026:

- `cmake --build out/build/vst3 --target TRENCH_VST3 TRENCH_UserBodyTests --parallel 3`: exit 0.
- The user-body test command above: exit 0, 12 PASS, 0 failures, using the actual exported body.
- Morph6: `cmake --build build/source-lab --config Release --target morph6 surface_check --parallel 3`: exit 0.
- Morph6: `ctest --test-dir build/source-lab -C Release -R "^surface$" --output-on-failure`: exit 0, 1/1 passed.
- Export and installed library body share SHA-256 `5D55CF2BFB94E38190A05CD2CBDC139BF8E7F0810DEFF99F715105AF68E31DEB`.
- Installer preflight correctly refused the DLL held by FL Studio; the built update is staged, not installed. Both running applications were left untouched.
