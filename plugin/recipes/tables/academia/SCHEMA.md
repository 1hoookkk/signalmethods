# academia/ — normalized acoustic-source tables, in
# `C:\Users\hooki\trench-x3-clean\recipes\tables\academia\`

Everything here is *clean room*: peer-reviewed / academic measurements and
open instrument data. NEVER E-mu ROM / P2K bytes — those stay reference-only
and are never a compile source. S2–S5 of a compiled body must come from OUR
sources; conditioning anchors come from our own dossier medians.

## Folder layout (per dataset)

```
academia/
  etl/                         # Mokhtari & Tanaka 2000 (ETL 4-formant tracker)
    etl_formantdata.txt        # raw as downloaded
    etl_formantdoc.txt
    etl.json                   # normalized (below)
  hillenbrand_1995/
    h95.csv                    # raw as exported from phonTools
    h95.json                   # normalized
  peterson_barney_1952/
    pb52.csv
    pb52.json
  kent_2018/ ...
  iowa/ ...                    # per-instrument WAVs + partial tables
  cran/ ...                    # UNSW clarinet impedance, CCRMA bells, etc.
```

## Normalized JSON format (one file per dataset)

Top level:
```json
{
  "schema": "acoustic-source-v1",
  "dataset": "hillenbrand_1995",
  "category": "vowels",                // vowels | instruments | objects | hrtf
  "citation": "Hillenbrand et al. 1995, JASA 97(5)",
  "source": "https://... or phonTools data(h95)",
  "measurement_method": "LPC formant tracking, 186 speakers x 12 vowels",
  "objects": [ ... ]                   // each object is ONE selectable endpoint
}
```

Each object = one endpoint (one speaker×vowel, one instrument note, one bell)
ready to be selected as an M0 or M100 source in the workstation:

```json
{
  "object_id": "m01_i",                // stable id
  "label": "i",                        // human name / note / mode set
  "f0_hz": 270.0,
  "formants": [
    { "mode_or_formant": "F1", "frequency_hz": 309.5, "bandwidth_hz": 55.3, "q": 5.6 },
    { "mode_or_formant": "F2", "frequency_hz": 2284.3, "bandwidth_hz": 154.6, "q": 14.8 },
    { "mode_or_formant": "F3", "frequency_hz": 2904.4, "bandwidth_hz": 400.1, "q": 7.3 },
    { "mode_or_formant": "F4", "frequency_hz": 3386.0, "bandwidth_hz": 450.0, "q": 7.5 }
  ]
}
```

For instruments/objects where there is no f0 or no formant framing, use
`modes` instead of `formants`, same per-matrix shape:
```json
{ "object_id": "carillon_bell_01", "label": "bell 1",
  "modes": [ { "mode_or_formant": "M1", "frequency_hz": 304.12,
               "bandwidth_hz": 3.7, "q": 82, "decay_time_s": 2.1 } ] }
```

## What the compiler expects

`tools/batch_compiler.py` ingests **two objects** (M0 low, M100 high) — each a
formant/mode list of (frequency, bandwidth). Bandwidth absent in the source
(Hillenbrand/PB have none) is filled with the Klatt 1980 default-BW rule:
B1=90, B2=110, B3=170, B4=250 (constant high-formant B4) unless the dataset
carries its own B.

Rules when converting a raw dataset to this schema:
- Keep the original file untouched next to the normalized JSON.
- `object_id` must be unique within the dataset file.
- Formants are ascending in frequency; cap at the 4 the grammar uses
  (S2–S5); extra modes may be kept but compiler reads the first four.
- `q = frequency_hz / bandwidth_hz` when BW present, else null.
- Leave `null` (not 0) when a field is unknown — never invent values.
