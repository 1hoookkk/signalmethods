# INPUT versus OUTPUT audition set, 25 September 2026

Source: `C:\Users\hooki\Downloads\trench_capture\dry_saw_49hz_-12dBFS.wav` (12 s, 44.1 kHz, -12 dBFS peak).
Every render: Q 100, MORPH swept 0 to 100 over the file, KEY off, movement off, block 256, 44.1 kHz.
Rendered with `TRENCH_Render` from the build that hashes `bcc9f7d4...234c7c` (the INPUT desk build).

Two files per case: the raw render, and `__matched.wav` scaled to the RMS of that body's `ref_in0_out0` so the pairs can be compared at the same loudness. `metrics.tsv` holds the numbers.

| case | what it is |
| --- | --- |
| `ref_in0_out0` | INPUT 0, OUTPUT 0 |
| `gain50_then_out50slam` | plain +5 dB gain into the filter (the old INPUT law at 50), then OUTPUT desk at 50 |
| `gain100_then_out100slam` | plain +20 dB gain into the filter (old INPUT at 100), then OUTPUT desk at 100 |
| `slam50_both` | Mackity desk at 50 into the filter, Mackity desk at 50 after it |
| `slam100_both` | desk at 100 into the filter, desk at 100 after it |
| `slam100_in_only` | desk at 100 into the filter, OUTPUT 0 |
| `gain100_plain` | plain +20 dB gain into the filter, OUTPUT 0 (old INPUT at 100) |

What the numbers say, Talking Hedz (guard = fraction of samples inside the final soft guard):

| case | crest dB | centroid Hz | energy above 5 kHz dB | guard % |
| --- | --- | --- | --- | --- |
| ref | 7.7 | 487 | -47.4 | 21 |
| gain100 plain | 0.7 | 614 | -22.2 | 90 |
| slam100 in only | 1.9 | 497 | -27.7 | 74 |
| gain100 then out100 slam | 2.5 | 886 | -15.8 | 98 |
| slam100 both | 2.4 | 797 | -17.0 | 96 |

Reading: Talking Hedz at Q 100 already drives the final guard on this saw with both knobs at zero. The desk into the filter keeps more crest and less top than plain gain at the same drive; the OUTPUT desk is what adds the top. Slam-both and gain-then-slam land within 0.1 dB crest and 90 Hz centroid of each other on Hedz and within 0.5 dB on Millennium. These are measurements of renders, not listening results.
