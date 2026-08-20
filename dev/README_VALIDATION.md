# E-mu Z-Plane Root-Set & Body240 Validation Pack

This targeted archive contains everything required to independently validate the **Order-Independent Root-Set Hypothesis** and the **Exact Permutation Finding** across the E-mu / Ensoniq / Creative filter corpus.

---

## Pack Contents

### 1. Raw Binary Bodies (Targeted Priority 1)
* `P2k_010_ooh_to_eee.body240` (240 bytes raw DSP memory dump)
* `P2k_021_ubu_orator.body240` (240 bytes raw DSP memory dump)

### 2. Standalone Python Reference Decoder & Verifier
* `standalone_body240_decoder.py` (Self-contained pure Python script that parses 240-byte bodies, extracts minifloats, roots, raw words, and evaluates H(z)). Run with:
  ```bash
  python standalone_body240_decoder.py
  ```

### 3. Authoritative Rust DSP Source Code
* `trench-core/src/minifloat.rs` (Bit-exact Rossum minifloat exponent/mantissa decoding)
* `trench-core/src/stage_law.rs` (Rossum parameter geometry transform: (d_mag, d_rsq) -> (p, q) roots)
* `trench-core/src/response.rs` (Complex cascade H(z) evaluation & decibel transfer functions)

### 4. Canonical Decoded Representations
* `recipes/architectures/P2k_010_Ooh-To-Eee.json` (Full 6-stage x 4-corner decoded parameters)
* `recipes/architectures/P2k_021_UbuOrator.json` (Full 6-stage x 4-corner decoded parameters)
* All 33 P2K decoded architecture JSONs in `recipes/architectures/`

### 5. Corpus-Wide Mining Scripts & Complete Corpora
* `dev/mine_root_multisets.py` (Order-independent root multiset mining)
* `dev/mine_transpositions.py` (Transposition-invariant chord template mining)
* `ref/morpheus/cubes_decoded.json` (Complete decoded 289-cube Morpheus corpus, 2312 corners)

---

## How to Run Instant Validation

```bash
python standalone_body240_decoder.py
```
This tests `P2k_010` vs `P2k_021` at Corner 0:
1. Displays the raw 16-bit word quads for each stage.
2. Demonstrates the exact stage permutation.
3. Evaluates $H(z)$ across $40	ext{ Hz} 	o 16	ext{ kHz}$ to prove the composite transfer function difference is $< 10^{-6}	ext{ dB}$.
