# XL-1 "Aud" multisamples (raw ROM pool)

Copied 2026-09-04 from the Digital Sound Factory Kontakt release of the E-MU Xtreme Lead-1 bank
(C:\Users\hooki\Downloads\42531\...\Xtreme Lead\Xtreme Lead Samples\Xtreme Lead 1 Bank 0..3),
the raw ROM multisample pool as DSF captured it (no preset filter). 224 WAVs, 43 families named
"Aud ...", sample rates 26-37 kHz, root notes in the file names (Kontakt mapping). No XL-1 ROM
dump exists in evidence; these are the nearest thing.

Finding (evidence/research-results/xl1_aud_resonance_census.*, xl1_pool_resonance_census.*,
xl1_keyframes_magnitude.png, xl1_frames_m0_6peq.png): a key-tracked resonant filter is frozen into these waves; whose filter is not in evidence:
the Audity 2000 ROM was marketed as sampled analog synths, and a sampled analog resonant filter
tracking the keyboard leaves the same fingerprint as E-mu's own Z-plane with key tracking. Aud Lead 2 carries a resonance at 7.9 x the note (Q 23-63, +30..37 dB) on every
multisample G1-G5; Aud Blend a fixed formant at 6.4 kHz; Aud Bell 4, Sync 2, Ring Mod 2 tracked
non-integer ratios. Integer ratios (2.0x) are the wave's own harmonics.
