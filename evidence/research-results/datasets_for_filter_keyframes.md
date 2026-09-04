# Datasets that could seed filter keyframes (web search 2026-09-04)

Question (Tyson): what other datasets could be great for a filter. Search by a Sonnet agent, links seen to
resolve unless marked.

## 1. Measured analog synthesizer filters: no ready corpus exists
| Name | Link | Licence | Format | Parameters |
|---|---|---|---|---|
| DGMD, Dataset Generator for Musical Devices (Fasciani et al.) | github.com/stefanofasciani/DGMD | open, see repo | Max/JS toolkit, records WAV + CSV | you point it at hardware; used on a Behringer Neutron LPF as 10 cutoff x 5 resonance |
| Modeling and measuring a Moog VCF (Paschou, Esqueda, Valimaki, APSIPA 2017) | acris.aalto.fi/ws/portalfiles/portal/27503975 | academic | PDF only | cutoff + resonance sweeps described, raw WAVs not published |
| KVR "Analog Synth Filter Test" thread | kvraudio.com/forum/viewtopic.php?t=453481 | forum | video, no files | SH-101, MiniBrute, Minitaur, Analog Four sweeps at 0/50/100 % resonance |

Verdict: no downloadable cutoff x resonance corpus of classic analog filters was found. Huovilainen 2004,
Fontana and Civolani (VCS3) and Paschou 2017 publish the method, not the data. DGMD is the tool to build
one from real hardware.

## 2. Brass and woodwind bore input impedance
| Name | Link | Licence | Format | Parameters |
|---|---|---|---|---|
| UNSW saxophone acoustics database | phys.unsw.edu.au/jw/SiteMap.html, newt.phys.unsw.edu.au/music/saxophone | free academic use, cite Wolfe et al. | impedance spectra + sound files per fingering | fingering x note, impedance at the mouthpiece |
| UNSW clarinet acoustics database | phys.unsw.edu.au/music/clarinet | same | same | fingering |
| UNSW flute electronic supplement | phys.unsw.edu.au/music/flute | same | same | fingering |

Direct .html paths 404 after a site restructure; enter through the site map or the newt mirror. No public
BIAS brass corpus: BIAS is a proprietary Vienna IWK instrument. Brass impedance data is scattered across
JASA papers.

## 3. Heads beyond SONICOM
| Name | Link | Licence | Format | Subjects | Parameters |
|---|---|---|---|---|---|
| CIPIC | sofaconventions.org/mediawiki/index.php/Files (mirror) | academic, free | SOFA | 45, 1250 directions | azimuth x elevation |
| ARI (Vienna) | projects.ari.oeaw.ac.at/research/experimental_audiology/hrtf | CC BY-SA 3.0 | SOFA | ~221, 1550 directions | azimuth x elevation |
| SADIE II | york.ac.uk/sadie-project/database.html, zenodo.org/records/12092466 | Apache 2.0 | WAV + SOFA | 20 (18 human + KU100/KEMAR) | azimuth x elevation, BRIRs |
| HUTUBS (TU Berlin) | doi.org/10.14279/depositonce-8487 | open | SOFA + head meshes | 96 | measured and simulated, anthropometry |
| RIEC (Tohoku) | riec.tohoku.ac.jp/pub/hrtf/hrtf_data.html | none stated, free download | SOFA 48 kHz | 105 | azimuth x elevation |

## 4. Singing voice
| Name | Link | Licence | Format | Size | Parameters |
|---|---|---|---|---|---|
| VocalSet | zenodo.org/records/1442513 | CC BY 4.0 | WAV | 10.1 h, 20 singers, 6 to 8 GB | singer x 17 techniques x 5 vowels |
| Annotated-VocalSet | zenodo.org/records/7061507 | CC BY 4.0 | WAV + annotations | | adds f0 and note timing |
| Vocadito | zenodo.org/records/5578807 | CC BY 4.0 | WAV + f0/note/lyric CSV | 40 excerpts, 7 languages | solo singing |
| NUS-48E | smcnus.comp.nus.edu.sg (request) | academic request | WAV + phone-level annotation | 169 min, 12 singers | sung vs spoken |

## 5. Vowel formant tables, raw files
| Name | Link | Licence | Format | Size | Parameters |
|---|---|---|---|---|---|
| Hillenbrand et al. 1995 | homepages.wmich.edu/~hillenbr/voweldata.html; data http://homepages.wmich.edu/~hillenbr/voweldata/vowdata.dat (http, the https cert is for redirect.wmich.edu) | free academic use | .dat text tables: vowdata.dat, bigdata.dat, vowdata.ds, iddata.dat | 1668 tokens, 139 speakers x 12 vowels | F0 to F4 at steady state and 20/50/80 % of duration |
| Peterson and Barney 1952 | R phonTools::pb52; Praat "Create formant table" | public-domain classic | R data frame / Praat table | 1520 vowels, 76 speakers | f0, F1 to F3 by vowel x speaker type |

Copied: evidence/factory-data/hillenbrand-1995/ (vowdata.dat + SOURCE.md).

## 6. Small resonant objects
| Name | Link | Licence | Format | Parameters |
|---|---|---|---|---|
| EchoThief | echothief.com/downloads | free, credit requested | WAV, 100+ spaces | caves, culverts, stairwells; rooms more than objects |
| OpenAIR | openair.hosted.york.ac.uk, openairlib.net | CC (mostly) | WAV, B-format | UNCERTAIN: both domains answered "Account suspended" at fetch time |
| Freesound packs (megaphone, misc IR packs: peter1955, djericmark, NoiseCollector) | freesound.org | CC0 / CC BY per file | WAV | ad hoc, no grid; raw material |
| IR64-CAR (car cabin, Eigenmike64) | dael.euracoustics.org/confs/fa2025/data/articles/000508.pdf | academic | dataset host not confirmed | mic position x car zone |

## 7. Animal sound archives
| Name | Link | Licence | Format | Parameters |
|---|---|---|---|---|
| xeno-canto | xeno-canto.org, API wrappers github.com/ntivirikin/xeno-canto-py, bghani/xcapi | per recording CC BY / BY-NC / BY-SA | MP3/WAV via API | species, call type |
| ESC-50 | github.com/karolpiczak/ESC-50 | CC BY-NC 3.0 | WAV zip ~600 MB, 2000 clips | 50 classes incl. animals |
| Macaulay Library | macaulaylibrary.org | request-gated, no bulk download | WAV | species |

## Best pick per category, as Morph x Q
1. A DGMD capture of a real filter: Morph = cutoff, Q = resonance. The only true two-axis grid, and it has
   to be recorded, not downloaded.
2. UNSW saxophone or clarinet impedance: Morph = fingering (bore length), Q = peak sharpness per note.
3. HUTUBS or SADIE II: Morph = azimuth, Q = elevation.
4. VocalSet: Morph = vowel, Q = technique (breathy to belt as the sharpening axis).
5. Hillenbrand vowdata.dat: Morph = vowel, Q = speaker type (men, women, children). Raw numbers, no fitting.
