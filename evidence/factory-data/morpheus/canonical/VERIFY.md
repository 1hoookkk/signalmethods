# Morpheus canonical bodies - verification

Order law: native corner index = Morph + 2 * Frequency + 4 * Transform, from raw corner index = Transform + 2 * Frequency + 4 * Morph; sections in the raw record's row order 0..6; five little-endian u16 words per section, corner-major.

Datum 39062.5 Hz. 289 cubes, 560 bytes each, written to canonical/.

Every canonical name matches a raw/bodies file of the same name.

## (a) corner order, cube 1 and cube 65

### cube 1 LPFlange.4

| native | m | f | t | raw = t + 2f + 4m | pole Hz, rows 0..6 | words exact |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | 815.1, 815.1, 815.1, 815.1, 815.1, 815.1, 815.1 | yes |
| 1 | 1 | 0 | 0 | 4 | 815.1, 815.1, 815.1, 815.1, 815.1, 815.1, 815.1 | yes |
| 2 | 0 | 1 | 0 | 2 | 815.1, 815.1, 815.1, 815.1, 815.1, 815.1, 815.1 | yes |
| 3 | 1 | 1 | 0 | 6 | 815.1, 815.1, 815.1, 815.1, 815.1, 815.1, 815.1 | yes |
| 4 | 0 | 0 | 1 | 1 | 79.2, 45.3, 90.6, 182.3, 367.0, 743.6, 1477.6 | yes |
| 5 | 1 | 0 | 1 | 5 | 9381.8, 2097.5, 367.0, 743.6, 1477.6, 2936.1, 5986.7 | yes |
| 6 | 0 | 1 | 1 | 3 | 9381.8, 11744.5, 13804.4, 16169.5, 17466.5, 19145.0, 19145.0 | yes |
| 7 | 1 | 1 | 1 | 7 | 9381.8, 11744.5, 13575.6, 16169.5, 17161.4, 18305.8, 18611.0 | yes |

### cube 65 Rev Peaks

| native | m | f | t | raw = t + 2f + 4m | pole Hz, rows 0..6 | words exact |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | 2764.5, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5 | yes |
| 1 | 1 | 0 | 0 | 4 | 657.7, 216.9, 433.8, 657.7, 867.5, 1082.1, 1286.9 | yes |
| 2 | 0 | 1 | 0 | 2 | 5033.0, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5 | yes |
| 3 | 1 | 1 | 0 | 6 | 18839.8, 1744.6, 3432.0, 5185.6, 6978.5, 8618.8, 10371.2 | yes |
| 4 | 0 | 0 | 1 | 1 | 300.3, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5 | yes |
| 5 | 1 | 0 | 1 | 5 | 653.0, 216.9, 433.8, 657.7, 867.5, 1082.1, 1286.9 | yes |
| 6 | 0 | 1 | 1 | 3 | 695.9, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5, 18229.5 | yes |
| 7 | 1 | 1 | 1 | 7 | 16779.9, 1744.6, 3432.0, 5185.6, 6978.5, 8618.8, 10371.2 | yes |

Corner order verification: PASS

## (b) trench_core round-trip

trench_core round-trip: 289 of 289 canonical bodies survive Body.from_native_bytes then to_native_bytes byte for byte.

## (c) canonical against raw/bodies of the same name

| result | count |
|---|---|
| differ | 289 |
| identical | 0 |
| no raw/bodies counterpart | 0 |

The raw/bodies files were encoded at a 44100 Hz datum while the Morpheus datum is 39062.5 Hz, they carry a per-cube corner scramble, their pole and zero slots disagree on row order, and their fifth word holds a seventh-root spread of a gain that does not match the record's per-corner gain field. The canonical bodies take the pole pair from raw field groups 0 and 1, the zero pair from groups 2 and 3, and put the record's per-corner gain in section 0 with unity in sections 1..6, so the cascade product is the recorded gain.
