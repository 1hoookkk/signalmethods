# Hillenbrand 1995 and Peterson & Barney 1952 vowel tables

Fetched 2026-09-04 from the CRAN source package phonTools 0.2-2.2 (BSD 2-clause), data/h95.rda and
data/pb52.rda, converted to CSV with the Python rdata reader. Dr Hillenbrand's own page
(homepages.wmich.edu/~hillenbr/voweldata.html, the original vowdata.dat) no longer exists: every path
redirects to the university home page.

h95.csv: 1668 tokens, 139 speakers (45 men, 48 women, 27 boys, 19 girls), 12 American English vowels,
steady-state F1..F3, f0 and duration. phonTools imputed 10 missing F2 and 41 missing F3 values.
Columns: type (b boy, g girl, m man, w woman), speaker, vowel (X-SAMPA), dur ms, f0, f1, f2, f3 Hz.

pb52.csv: 1520 tokens, 76 speakers (33 men, 28 women, 15 children), 10 vowels, two repetitions each,
f0 and F1..F3 from the Praat table. Columns: type (c child, m man, w woman), sex, speaker, vowel,
repetition, f0, f1, f2, f3.

vowel_medians.csv: per source, speaker type and vowel: n, median f0, F1, F2, F3 (and duration for h95).
Twelve vowels x four speaker types = 48 keyframes from h95; three formants each, no fitting.

X-SAMPA vowel key: i heed, I hid, e hayed, E head, { had, A hod, O hawed, o hoed, U hood, u who'd,
V hud, 3' heard.

Hillenbrand, Getty, Clark, Wheeler (1995) JASA 97, 3099-3111. Peterson & Barney (1952) JASA 24, 175-184.

Keyframes: keyframe_library_build.py (evidence/research-results) adds the group VOWEL H95: 12 vowels x
man/woman/boy/girl = 48 entries, three POLE rows each at the median F1, F2, F3, bandwidth 50 + F/20 Hz
(about 75 Hz at F1 500, 125 at F2 1500, 175 at F3 2500; the table gives no bandwidths). Morph = vowel,
Q = speaker type.
