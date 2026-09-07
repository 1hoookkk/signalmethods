# Forensics: Span vs Spectrogram

Two preserved SGI/IRIX MIPS executables from `reality.sgi.com/dscott/audio.apps/public_apps/`.
All offsets are file offsets in the extracted binaries unless a virtual address is given.

| | Spectrogram | Span |
|---|---|---|
| file | `spectrogram/extracted/spectrogram` | `span/extracted/span` |
| size | 410,036 | 117,824 |
| tar member date | 1995-03-08T18:33:51Z | 1995-07-18 |
| README in tarball | yes, 14,213 bytes | none |
| ELF | 32-bit MSB MIPS, o32, not stripped, `.mdebug` 123,872 B @0x3c000 | same, `.mdebug` 26,260 B @0x13000 |
| sha256 | `1b8747fa…5057a3` (REPORT.md) | `445295a45f863f9d4c1dace871c421844d0132daac48ae035c751998c3ff5945` (sgi-archive/keyword-hits.json) |

## 1. Author and copyright strings

- Spectrogram README line 4 (offset 0x71/0x7a): `Author: Alan W. Peevers, E-mu Systems, Scotts Valley, CA.`
- Spectrogram README line 245: `Alan Peevers` (sign-off).
- Spectrogram README lines 6-8: `Spectrogram is a program which resulted from my MS project at UC Berkeley during the winter of 1993-94.`
- Spectrogram binary, offset **0x38298**, `.rodata`: `Copyright (c) 1993 The Regents of the University of California`. It is the only `Copyright` byte-run in the file.
- Span binary, offset **0x11d98**, `.rodata` vaddr **0x10000d98**: `Copyright (c) 1995 E-mu Systems, Inc.` It is the only `Copyright` byte-run in the file, and it is printed at startup: `span/decompiled/main.c` line 24, `printf("%s\n","Copyright (c) 1995 E-mu Systems, Inc.");`.
- Span carries no author name. Its author comes from the archive index page,
  `sgi-archive/mirror/dscott/audio.apps/public.html` (~offset 23878), where one
  `Author: <a href="mailto:alan@emu.com"> Alan Peevers</a>` block serves both the
  Spectrogram and Span download links, under the heading
  `<a href="http://www.emu.com/"> E-mu Systems, Inc.</a>`.
- Spectrogram's embedded FORMS toolkit credits: `FORMS LIBRARY`, `version 2.2a`, `Written by Mark Overmars` (REPORT.md, "Authorship/version strings").

## 2. Every E-mu / Emu / Emulator / Morpheus / Proteus / Z-plane / filter / coefficient / cube / morph occurrence

Byte search over both binaries and the README for each literal, case variants included:

| term | spectrogram binary | span binary | README |
|---|---|---|---|
| `E-mu` | **0 hits** | 1 @0x11dab (inside the copyright above) | 1 @0x7a |
| `Emu`/`EMU`/`emu` | 0 | 0 | 0 (except inside `E-mu`) |
| `Emulator` | 0 | 0 | 0 |
| `Morpheus` | 0 | 0 | 0 |
| `Proteus` | 0 | 0 | 0 |
| `Z-plane`/`z-plane` | 0 | 0 | 0 |
| `cube`/`Cube` | 0 | 0 | 0 |
| `morph`/`Morph` | 0 | 0 | 0 |
| `coeff`/`Coeff` | 0 | 0 | 0 |
| `filter` | 1 @0x38168 | 0 | 10 |
| `Filter` | 1 @0x38718 | 0 | 3 |

Verbatim, the only `filter` strings in either binary:

- spectrogram 0x38168: `    -t: do filter-function synthesis` (from `usage()`).
- spectrogram 0x38718: `Filter` — the FORMS menu label, bound to `SurfMenu` / `SurfMenu_C` (`spectrogram/FORM_Menu_Form.md`, row `menu | 1 | 130 | 340 | 55 | 20 | Filter | SurfMenu`).

README `filter` contexts, verbatim, lines 118-129 and 213-217:

> `The Filter menu allows you to load and save time-varying filters. These are raw binary floating point files representing the series of Fourier transform magnitudes used by spectrogram to do time-varying filtering.`

> `This spectral envelope is computed by performing a 12th order LPC analysis, and taking the FFT of the impulse response of the LPC synthesis filter. The surface so created can be saved via the filter menu (go into Pause mode first), and used subsequently as a time-varying filter.`

**Correction to REPORT.md.** The 2026-09-04 addendum states that
`Copyright (c) 1995 E-mu Systems, Inc.` is "embedded in the `spectrogram` binary".
A byte search of `spectrogram/extracted/spectrogram` returns zero hits for `E-mu`,
`Emu` and `Copyright (c) 1995`. That string exists only in `span`. The
"E-mu owned, in 1995" reading therefore attaches to Span, not to Spectrogram;
Spectrogram's only copyright is the 1993 Regents notice, and its E-mu link is the
README byline and the `alan@emu.com` address on the index page.

## 3. Build dates, compiler invocations, source mounts, source file names

Neither binary has a compiler banner or a compilation timestamp. Dating evidence:

- Tar member dates: spectrogram 1995-03-08T18:33:51Z, span 1995-07-18.
- `.liblist` recorded link-time library stamps.
  Spectrogram: `libaudiofile.so` 1994-11-21T18:57:30Z, `libm.so` 1994-11-01T02:23:02Z,
  `libfm.so` 1994-11-01T02:43:55Z, `libgl.so` 1994-11-03T23:54:37Z,
  `libX11.so.1` 1994-11-01T02:28:09Z, `libc.so.1` 1994-11-01T02:18:58Z.
  Span: `libfm.so` 1994-11-01T02:43:55Z, `libgl.so` 1994-11-03T23:54:37Z,
  **`libaudiofile.so` 1995-05-22T20:32:08Z**, **`libaudioutil.so` 1995-05-22T20:32:08Z**,
  `libm.so` 1994-11-01T02:23:02Z, `libc.so.1` 1994-11-01T02:18:58Z.
  Span was therefore linked on or after 1995-05-22 — after the preserved Spectrogram build.

Spectrogram compile commands surviving in `.mdebug` (offsets 0x505fc, 0x50790):

```
//usr/lib/cc -I. -L. -g -prototypes -mips2 -float -c fof_transf.c -o32
//usr/lib/cc -I. -L. -g -prototypes -mips2 -float -c fof_value.c -o32
```

with working directory `/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram` (0x50643, 0x507d6).
Span retains **no** `cc` invocation string.

Spectrogram source mount paths, prefixed with the compile host `candiru:` and the
NFS-mounted source host `lamprey` (offsets 0x4d739 onward):

```
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/spectrogram.c
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/theforms.c
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/power.c
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/aiffio.c
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/fof_transf.c
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/fof_value.c
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/gal.c
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/./forms.h
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/./theforms.h
candiru:/tmp_mnt/hosts/lamprey/d3/audio/apps/Spectrogram/./spectrogram.h
```

Spectrogram application sources: `spectrogram.c`, `theforms.c`, `power.c`, `aiffio.c`,
`fof_transf.c`, `fof_value.c`, `gal.c`. Statically incorporated FORMS 2.2a sources:
`forms.c menu.c fselect.c objects.c input.c slider.c button.c support.c events.c box.c
text.c draw.c goodies.c browser.c sldraw.c symbols.c` (0x508eb-0x51c87). Statically
incorporated SGI AL sources: `writesamps.c readsamps.c getfilled.c getparams.c error.c
newconfig.c setwidth.c setchannels.c setqueuesize.c openport.c` (0x51da8-0x51fb8).

Span source names (`.mdebug`, offsets 0x16174-0x165f8):

```
span.c
power.c
getparams.c  newconfig.c  setwidth.c  setchannels.c  setqueuesize.c
openport.c   closeport.c  getfilled.c readsamps.c    error.c
globals.c    xfers.c
./../include/audio_private.h
```

Span's application sources are just `span.c` and `power.c`; the rest is the AL client
library. Span's paths carry **no** `candiru:` host prefix and no `/tmp_mnt/hosts/lamprey`
mount — a different build environment from Spectrogram's.

Both binaries carry the same IRIX release-root header paths, e.g.
`/joist/5.3MR/root/usr/include/regdef.h`, `/joist/5.3MR/root/usr/include/sgidefs.h`,
`/joist/5.3MR/root/usr/include/sys/fpregdef.h`, `/joist/5.3MR/root/usr/include/asm.h`
(spectrogram 0x4d66f-0x4d6ee, span 0x160c3-0x16142) — IRIX 5.3 maintenance tree.

## 4. UI toolkit

Neither program uses Motif: no `libXm`, no `libXt`, no `Xm*` symbol in either dynsym.

**Spectrogram = SGI FORMS Library 2.2a (statically linked) over IRIS GL.**
`.symtab` holds **178** defined `fl_*` functions, e.g. `fl_addto_form` @0x4173c4,
`fl_do_forms` @0x4197e0, `fl_check_only_forms` @0x419884, `fl_replace_menu_item`,
`fl_delete_menu_item`, `fl_set_menu_item_shortcut`, `fl_show_menu_symbol`; callbacks use
`fl_get_input` and `fl_get_slider_value` (`spectrogram/decompiled/cursor_C.c`).
The panel is generated code: `create_form_Menu_Form` / `create_the_forms` from `theforms.c`.
IRIS GL imports (dynsym undefined): `winopen winset qdevice qread getvaluator getbutton
getorigin getsize RGBmode mapcolor color doublebuffer zbuffer swapbuffers ortho2 perspective
polarview reshapeviewport rectf cmov2 bgnline v2f v3f`, plus `glcompat`. No OpenGL
`gl*` entry points. Fonts via fmclient: `fminit fmfindfont fmscalefont fmsetfont fmprstr
fmgetfontinfo fmgetstrwidth fmfreefont`. `.liblist` declares `libX11.so.1`, but no `X*`
symbol is imported.

**Span = raw IRIS GL, no toolkit at all.** Zero `fl_*` symbols. Imports only
`winopen winset qdevice qread getsize reshapeviewport ortho2 color mapcolor bgnline v2f`
and the five fmclient calls. `.liblist`: `libfm.so libgl.so libaudiofile.so libaudioutil.so
libm.so libc.so.1` — no `libX11`. Its whole UI is `draw_axes` + `draw_graph` + typed
answers on stdin.

Both pick the same font: `Times-Roman` (spectrogram 0x37d44 and 0x398d0, span 0x11dc4);
`span/decompiled/main.c` calls `fmfindfont("Times-Roman")`.

## 5. FFT lengths, windows, averaging and smoothing constants, numeric defaults

**Shared window table.** Both binaries hold the same 9 x 4 array of cosine-series window
coefficients, laid out identically and read by `win_calc(buf, type, len)`:

| idx | window | spectrogram @0x100000d4+ | span @0x1000004c+ |
|---|---|---|---|
| 0 | exact Blackman | 0.42659, -0.49656001, 0.076849997, 0 | identical |
| 1 | Blackman | 0.41999999, -0.5, 0.079999998, 0 | identical |
| 2 | Blackman-Harris 1 | 0.42322999, -0.49755001, 0.079219997, 0 | identical |
| 3 | Blackman-Harris 2 | 0.44959, -0.49364001, 0.056770001, 0 | identical |
| 4 | Blackman-Harris 3 | 0.35874999, -0.48829001, 0.14128, 0.01168 | identical |
| 5 | Blackman-Harris 4 | 0.40217, -0.49702999, 0.09392, 0.00183 | identical |
| 6 | Hamming | 0.54000002, -0.46000001, 0, 0 | identical |
| 7 | Hanning | 0.5, -0.5, 0, 0 | identical |
| 8 | rectangular | 0, 0, 0, 0 | **1, 0, 0, 0** |

Index 8 is the only differing row. Both `win_calc` bodies guard with `if (param_2 < 9)`
and otherwise call `perror("bad window type! Try again...\n")` (span 0x100009a8), and both
sum the same three harmonics: `cos(n*6.283185307179586/N)`, `cos(n*12.566370614359172/N)`,
`cos(n*18.84955592153876/N)`.

**Span defaults** (`.data`, addresses from `span/decompiled/GLOBALS.md` names):
`wintype` 0x10000008 = 1 (Blackman); `winsize` 0x1000000c = 1024; `order` 0x10000010 = 1024
(FFT size); `expg` 0x10000018 = 0.01 and `expk` 0x10000020 = 0.99 (the exponential
smoother pair used in `xavg`); `power_mode` 0x10000028 = 0; `removeDC` 0x1000002c = 0;
`fmin` = 0; `amin` = -120 dB; `amax` = +40 dB; `fsc` = `asc` = 1; `foff` = `aoff` = 0.
`xavg(buf, n, reset)` computes `out = prev*expk + out*expg` per bin — a one-pole
exponential average, enabled by `-ak` (`main.c` case `'a'`: `expk = atof(argv+2)`,
`expavg = 1`, `expg = 1.0 - expk`). `rescale()` fixes the frame: `asc = 160.0/(amax-amin)`,
`aoff = (-120.0 - amin) * (height*0.8*0.125/20.0)`, viewport inset 0.1/0.9 of the window.
`log_of` (span) clamps to a floor derived from `winsize/squark` and emits
`10*log10(x*scale) - 20*log10(order)*power_mode`.

**Spectrogram defaults** (`init_the_parms`, 0x4131bc): FFT order `theParms` = 0x100 = 256;
window size `DAT_1000b978` = 0x100 = 256; stride `DAT_1000b97c` = 0x80 = 128;
`DAT_1000b974` = 0x80; frame budget `DAT_1000b980` = 0x1000; window type `DAT_1000b990` = 7
(Hanning). `usage()` prints the same numbers: `-d n: number of frames (500)`,
`-n n: number of samples to analyze/frame (256)`, `-o n: FFT size (256)`,
`-s n: stride: no. of samples to advance each frame (128)`, `-w n: type of window to use (7)`.
`log_of` (spectrogram) clamps at `1e-05` (stores 0x3727c5ac) then applies globals
`m` (0x1000b9b0) and `b` (0x1000b9b8): `out = log10f(x)*m + b`.
`spectrum()` normalises by `1/(N*N)`; span's `spectrum()` does not normalise, and
`inspect()` in both scales the IFFT by `1.0/N`.
`init_logftbl` builds the log-frequency axis as `log(i+1) * (N/log(N))` over `DAT_1000b97c` bins.

**LPC / adaptive lattice constants.** `gal()` (0x416f60, from `gal.c`) is a 12th-order
gradient adaptive lattice: both loops run over orders 1..12 (`while (iStack_4 < 0xd)`,
`iStack_4 = 0xc` counting down), with a leakage/step pair of **0.998** and **0.002**
(`*p = *p * 0.998; *p += (f*f + b*b) * 0.002;` and the reflection-coefficient update
`k += ((…)*0.002)/power`). `lattice()` (0x4171c0) runs 12 stages (`iStack_4 = 0xb` down to 0).
This is the "12th order LPC analysis" the README names.
`fof_value()` (0x416df0) uses the literal **3.1416** in `exp(-a*(3.1416/bw))` and
`cos(w*(3.1416/bw))`; `fofbw` is settable by the undocumented `-B` switch.
`drw_flttbl` loads the double constant 0x3f589374bc6a7efa (0.0015) and works on a
1024-double buffer.

Shared power-of-two table `pwr_two` in both: `1 2 4 8 16 32 64 128 256 512 1024 2048
4096 8192 16384 32768`, indexed by FFT order in `fft`, `ifft`, `bitreverse`, `buildtable`.

## 6. Mouse and key bindings

**Span** — keyboard, from the embedded help text (0x10000c30-0x10000ce4) and the
`qdevice`/`qread` switch in `main.c`:

```
	While active, you can press the following keys:
	C: 	reset the averager
	Q: 	quit
	R: 	rewind to beginning of file
	S: 	take a snapshot
	X: 	change x-axis limits
	Y: 	change y-axis limits
```

`main.c` queues devices `0x210` (redraw, handled by `rescale`), `10`, `0x18`, `0x1c`,
`0x15`, `0x20`, and `0x0c` only when `-f<file>` supplied an xgraph file. By behaviour:
`case 10` → `fclose(fd); exit(0)` = Q; `case 0x18` → `AFseekframe(file, 0x3e9, 0)` = R;
`case 0x1c` → sets the averager-reset flag = C; `case 0x0c` → sets the snapshot flag = S;
`case 0x15` → `printf("Min Freq:\n"); scanf("%f",&fmin); printf("Max Freq:\n"); scanf("%f",&fmax)` = X;
`case 0x20` → the same for `Min Amp:` / `Max Amp:` = Y. Span binds no mouse buttons.

**Spectrogram** — mouse only; all keyboard entry goes through FORMS input boxes.
`pEvent_Handler(dev, val)` (0x40d43c) switches on GL device ids `0x65` (RIGHTMOUSE, 101),
`0x66` (MIDDLEMOUSE, 102) and `0x67` (LEFTMOUSE, 103), reading
`getvaluator(0x10a)`/`getvaluator(0x10b)` (MOUSEX/MOUSEY):

- `0x67` down: begins a FOF trajectory — `bgnline/v3f/endline`, then a drag loop
  `while (getbutton(0x67))` streaming `bgnpoint/v3f/endpoint` and writing
  `freqarry[bin*3 + fof] = y/4.0`; if the surface mode flag is set instead, it starts
  a view rotation (`function = 2`).
- `0x66`: places one point into `freqarry`, or starts a view rotation (`function = 1`).
- `0x65` down: advances the FOF index `DAT_1000ac40` cyclically over 0,1,2 (three FOFs)
  and zeroes that column of `freqarry`.
- README lines 41-50: `When the middle mouse button is depressed, you will see a rectangular grid displayed on the screen… Move the mouse while holding the middle button… I 'borrowed' the mouse input routines from the amesh demo program… These values get used in a call to the 'polarview' gl function.`
- README lines 86-89: `They can be opened up by clicking with any mouse button. To select any of the options, you must use the right mouse button.`

Panel controls (labels and callbacks) are enumerated in `spectrogram/FORM_Menu_Form.md`:
menus `File`/`Colors`/`Hamm`/`Filter`; buttons `Clear LiveIn Mesh Monitor Scope Meter
Peaks Surface F0 Env SynWin Modify LogF Impulse ZB DB`; inputs `Length FFT Size Win Size
Stride` plus the two cursor boxes; sliders `R G B` x2, `gain floor twist zoom tscale
ascale fscale Cursor time amp freq`.

## 7. File formats read and written

Spectrogram:
- AIFF in and out via the Audio File Library — imports `AFopenfile AFreadframes
  AFwriteframes AFseekframe AFnewfilesetup AFinitfilefmt AFinitchannels AFinitrate
  AFgetchannels AFgetframecnt AFgetrate AFgetsampfmt AFsyncfile AFclosefile`;
  strings `*.aiff` (0x37b14, 0x38314), `.aiff` (0x37b1c), `testout.aiff` (0x38288, 0x3832c).
- `*.filt` (0x37c2c, 0x37c5c) — headerless raw native float FFT magnitudes.
  README lines 118-123: `These are raw binary floating point files representing the series of Fourier transform magnitudes used by spectrogram to do time-varying filtering. The size of this file is 4 * number_of_fft_frames * (FFT_order/2 + 1).`
- `*.map` (0x37b6c) SGI colour maps, loaded by shelling out to `/usr/sbin/loadmap` (0x37b7c).
- Live audio in/out through statically linked SGI AL (`openport.c`, `readsamps.c`, `writesamps.c`).

Span:
- AIFF in only — imports `AFopenfile AFreadframes AFseekframe AFnewfilesetup AFgetframecnt
  AFgetrate`; `aifopen()` is a two-line `AFnewfilesetup` + `AFopenfile`. Usage:
  `if aifffile is specified, input will be read from the named file.` and
  `NOTE: aifffile must be the LAST argument!`
- Writes an **xgraph** text file: `filename is an xgraph file to write snapshots to.`
  `save_graph()` emits `fprintf(fd,"%f %f\n", freq, amp)` per bin (format string `%f %f`
  at 0x100009b8) followed by a blank line per snapshot.
- Live audio in through statically linked AL; `check_sr()` refuses odd rates with
  `Whoa there! Weird sampling rate.` (0x100008e0, duplicated at 0x10000904).

Neither reads or writes WAV, Sound Designer, MIDI, a preset bank, a cube, or any
coefficient container.

## 8. Source files and functions shared between Span and Spectrogram

**`power.c` is the same file in both**, named in both `.mdebug` file lists
(spectrogram 0x4f914 / `candiru:…/Spectrogram/power.c` 0x4f927; span 0x162be). It supplies
the FFT core in both binaries.

| function | spectrogram | span | evidence of sameness |
|---|---|---|---|
| `buildtable` | 0x41600c | 0x4069e0 | identical body: frees and re-mallocs `N*8`, `printf("malloc failed\n"); exit(0)`, fills cos/-sin twiddles at `2*pi*i/N` |
| `bitreverse` | 0x4161a8 | 0x406b7c | identical algorithm over `pwr_two[]`, same swap of the 8-byte complex pair |
| `fft` | 0x4162e0 | 0x406c30 | same decimation loop, same `pwr_two[order]` and twiddle-table indexing `(i << (m & 0x1f)) & (N-1)` |
| `ifft` | 0x41653c | 0x406d84 | same loop with the conjugated butterfly |
| `spectrum` | 0x415c10 | 0x4065b0 | same shape: derive order by shifting, `if (order != fftinitialized) buildtable()`, copy real into interleaved `tmpc`, zero-pad, `bitreverse`, `fft`, then `re*re + im*im`. Spectrogram additionally divides by `N*N`; span does not |
| `inspect` | 0x415e98 | 0x406878 | same shape: order derivation, `bitreverse`, `ifft`, then `out[i] = in[2i] * (1.0/N)` |
| `win_calc` | 0x40b904 | 0x404298 | same 9x4 coefficient table copied to stack, same `if (type < 9) … else perror("bad window type! Try again...\n")`, same three-harmonic sum. Span additionally accumulates `squark += w*w` |
| `winmult` | 0x40be48 | 0x4049dc | same name and role; span multiplies `short` samples by the window into a float buffer, spectrogram multiplies float by float and then applies zero-phase rotation for `theParms != winsize` — diverged |
| `log_of` | 0x40c380 | 0x404af0 | same name and role; different constants (spectrogram `m`/`b` globals, span dB with `power_mode`) |
| `redraw_everything_and_resize` | 0x40d16c | 0x405484 | same unusual name in both |
| `usage` | 0x413238 | 0x40596c | shared help text, verbatim in both: `0 = exact blackman, 1 = blackman, 2-5 = blackman-harris` / `6 = hamming, 7 = hanning, 8 = rectangular (no windowing!)` |
| globals `pwr_two`, `fftinitialized`, `tmpc` | `.data`/`.bss` | `.data`/`.bss` | same three names, same roles, same 16-entry `pwr_two` contents |
| AL client sources | `getparams.c newconfig.c setwidth.c setchannels.c setqueuesize.c openport.c getfilled.c readsamps.c error.c writesamps.c` | `getparams.c newconfig.c setwidth.c setchannels.c setqueuesize.c openport.c closeport.c getfilled.c readsamps.c error.c globals.c xfers.c` | same statically linked SGI library, overlapping module names and `_ALWriteMonoToStereo_*` / `_ALRead*` symbol families |

Span-only application functions: `check_sr oinit draw_axes draw_graph demean xavg clr_buf
save_graph rescale aifopen winhelp`.
Spectrogram-only: the whole FORMS panel, `gal lattice fof_transf fof_value olap ola
drawsurf draw_surf_slice draw_slice draw_grid drw_flttbl init_flttbl init_lut edit_lut
findmax normalize mixdown read_adc_samps read_file_samps aiff_open/read/write/seek`, etc.

## 9. Function names suggesting filter design, coefficient tables or companion programs

- `init_flttbl` (0x41176c) and `drw_flttbl` (0x411a4c) — the time-varying **filt**er
  **t**a**bl**e: built from FFT magnitudes, drawn on the surface, saved/loaded through the
  `Filter` menu, applied by the `Modify` button. This is the closest thing in either binary
  to a coefficient table, and it holds FFT magnitudes, not biquad coefficients.
- `gal` / `lattice` — 12th-order gradient adaptive lattice and its synthesis filter; the
  README's `Env` mode. `lpcenvB` is the FORMS button variable bound to it.
- `fof_transf` / `fof_value` and globals `foffreq`, `fofbw`, `fofamp`, `freqarry` — formant
  (FOF) frequency, bandwidth and amplitude, three FOFs, hand-drawn with the mouse.
- `Peaks` (`partialB`) and `F0` (`F0B`) buttons — peak picking and pitch track.
- No function in either binary is named for pole/zero placement, biquad section design,
  cascade coefficients, or preset export.

## 10. Hard-coded paths, hosts, users, and companion programs

- Interpreter, both: `/usr/lib/libc.so.1`.
- Audio devices, both: `/dev/hdsp/hdsp0r%d` (spectrogram 0x39e80, span 0x11fb0) and
  `/dev/hdsp/hdsp0master` (spectrogram 0x39ecd, span 0x11fe0) — from the AL client library.
- Spectrogram shells out to `/usr/sbin/loadmap` (0x37b7c) and defaults its writeback file to
  `testout.aiff` (0x38288).
- Build hosts, spectrogram only: compile host `candiru`, NFS source host `lamprey`, tree
  `/d3/audio/apps/Spectrogram`, automount path `/tmp_mnt/hosts/lamprey/…`.
- Release root, both: `/joist/5.3MR/root/usr/include/…`.
- No usernames appear in either binary. The tarball's ownership is user `cook`,
  group `nuucp` (REPORT.md); `alan@emu.com` appears only on the archive index page.
- Named companion programs, all from the Spectrogram README, none shipped in the tarballs:
  `makemap` (line 94, `'makemap' at the UNIX command line (or clicking the makemap icon)`),
  the `SGI savemap` utility (line 111), `loadmap` (called at runtime), the `amesh` demo whose
  mouse routines were borrowed (line 46), and `Xgraph` for Span's snapshots. Directory
  conventions assumed but not created: a `sounds` directory of `.aiff` files (line 17) and a
  `filt` directory (line 123).
- No reference in either binary to a filter-design tool, a coefficient compiler, an editor,
  a librarian, or any internal E-mu hostname or program name.

## What this establishes and what it does not

Span and Spectrogram share one FFT source file, `power.c`, and one window-coefficient table,
so they are two programs from one person's toolbox rather than unrelated finds. The archive
index attributes both to `alan@emu.com`, Alan Peevers, under an E-mu Systems heading, and
Span alone carries `Copyright (c) 1995 E-mu Systems, Inc.` while Spectrogram carries only
`Copyright (c) 1993 The Regents of the University of California`. REPORT.md's addendum
places the E-mu copyright in the Spectrogram binary; a byte search shows it is not there,
so the "E-mu owned in 1995" reading belongs to Span. Span's `libaudiofile.so` link stamp of
1995-05-22 puts its build after the preserved Spectrogram build, and its source paths carry
none of Spectrogram's `candiru`/`lamprey` mount, so it was built in a different environment.
The words E-mu, Emulator, Morpheus, Proteus, Z-plane, cube, morph and coefficient appear
nowhere in either binary except inside Span's one copyright line. The only filter machinery
present is an FFT-magnitude surface (`init_flttbl`, `drw_flttbl`, the `.filt` file whose size
the README gives as `4 * number_of_fft_frames * (FFT_order/2 + 1)`) and a 12th-order
gradient adaptive lattice (`gal`, `lattice`, leakage 0.998, step 0.002) whose FFT'd impulse
response is the `Env` display. That confirms, at artifact level, that an E-mu employee's
1995 toolbox looked at a sound as a low-order all-pole envelope and re-applied it as a filter.
It does not show any cube, corner, section-word, or hardware-coefficient path, and it names
no companion filter-design or coefficient program beyond `makemap`, `savemap`, `loadmap`,
`amesh` and `Xgraph`.
