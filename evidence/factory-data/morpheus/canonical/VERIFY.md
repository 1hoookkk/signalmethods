# Morpheus canonical bodies - verification

Order law: native corner index = Morph + 2 * Frequency + 4 * Transform, from raw corner index = Transform + 2 * Frequency + 4 * Morph; sections in the raw record's row order 0..6; five little-endian u16 words per section, corner-major.

Datum 48000.0 Hz. 289 cubes, 560 bytes each, written to canonical/.

Every canonical name matches a raw/bodies file of the same name.

## (a) corner order, cube 1 and cube 65

### cube 1 LPFlange.4

| native | m | f | t | raw = t + 2f + 4m | pole Hz, rows 0..6 | words exact |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6 | yes |
| 1 | 1 | 0 | 0 | 4 | 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6 | yes |
| 2 | 0 | 1 | 0 | 2 | 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6 | yes |
| 3 | 1 | 1 | 0 | 6 | 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6, 1001.6 | yes |
| 4 | 0 | 0 | 1 | 1 | 97.4, 55.6, 111.3, 224.0, 451.0, 913.7, 1815.7 | yes |
| 5 | 1 | 0 | 1 | 5 | 11528.3, 2577.4, 451.0, 913.7, 1815.7, 3607.9, 7356.4 | yes |
| 6 | 0 | 1 | 1 | 3 | 11528.3, 14431.6, 16962.9, 19869.1, 21462.9, 23525.4, 23525.4 | yes |
| 7 | 1 | 1 | 1 | 7 | 11528.3, 14431.6, 16681.6, 19869.1, 21087.9, 22494.1, 22869.1 | yes |

### cube 65 Rev Peaks

| native | m | f | t | raw = t + 2f + 4m | pole Hz, rows 0..6 | words exact |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | 3397.0, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4 | yes |
| 1 | 1 | 0 | 0 | 4 | 808.2, 266.5, 533.0, 808.2, 1066.0, 1329.7, 1581.3 | yes |
| 2 | 0 | 1 | 0 | 2 | 6184.6, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4 | yes |
| 3 | 1 | 1 | 0 | 6 | 23150.4, 2143.8, 4217.3, 6372.1, 8575.2, 10590.8, 12744.1 | yes |
| 4 | 0 | 0 | 1 | 1 | 369.0, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4 | yes |
| 5 | 1 | 0 | 1 | 5 | 802.4, 266.5, 533.0, 808.2, 1066.0, 1329.7, 1581.3 | yes |
| 6 | 0 | 1 | 1 | 3 | 855.1, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4, 22400.4 | yes |
| 7 | 1 | 1 | 1 | 7 | 20619.1, 2143.8, 4217.3, 6372.1, 8575.2, 10590.8, 12744.1 | yes |

Corner order verification: PASS

## (b) trench_core round-trip

trench_core round-trip: 289 of 289 canonical bodies survive Body.from_native_bytes then to_native_bytes byte for byte.

## (c) canonical against raw/bodies of the same name

| result | count |
|---|---|
| differ | 289 |
| identical | 0 |
| no raw/bodies counterpart | 0 |

The raw/bodies files were encoded at a 44100 Hz datum while the hardware datum is 48000 Hz (angles are normalised radians, f = theta / pi * 24000), they carry a per-cube corner scramble, their pole and zero slots disagree on row order, and their fifth word holds a seventh-root spread of a gain that does not match the record's per-corner gain field. The canonical bodies take the pole pair from raw field groups 0 and 1, the zero pair from groups 2 and 3, and put the record's per-corner gain in section 0 with unity in sections 1..6, so the cascade product is the recorded gain.
